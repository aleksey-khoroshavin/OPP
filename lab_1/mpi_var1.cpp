#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <cmath>

#define N 20
#define eps 0.00001

//Count of rows to one process
int get_rows_count_process(int curr_rank, int size){
        int general = N/size;
        int rest = N % size;
        return general + (curr_rank < rest ? 1 : 0);
}

//Offset from the first element in matrix
int get_offset(int curr_rank, int size){
        int res = 0;
        for(int i = 0; i < curr_rank; i++){
                res += get_rows_count_process(i, size);
        }
        return res;
}

//Create matrix A
double* create_matrix_A(int rank, int size){
        int row_cnt = get_rows_count_process(rank, size);
        double* part_process = (double*)calloc(row_cnt*N, sizeof(double));
        int offset = get_offset(rank, size);
        for(int i = 0; i < row_cnt; i++){
                for(int j = 0; j < N; j++){
                        part_process[i*N + j] = (i == (j - offset))? 2 : 1;
                }
        }
        return part_process;
}

//Create vector b
double* create_vector_b(int rank, int size){
        double* vector = (double*)calloc(N, sizeof(double));
                for(int i = 0; i < N; i++){
                        vector[i] = N + 1;
                }
        return vector;
}


//Multiple matrix by vector
double* Matrix_by_vector(double *M, double *V, int rank, int size)
{
    int row_count_proc = get_rows_count_process(rank, size);
    double *result = (double*)calloc(row_count_proc, sizeof(double));
    for(int i = 0; i < row_count_proc; i++){
        for(int j = 0; j < N; j++)
        {
            result[i] += M[N*i + j]*V[j];
        }
    }
    return result;
}

//Array of beginigns of parts_processes
int* get_begin_positions(int size){
        int* positions = (int*)calloc(size, sizeof(int));
        for(int i = 0; i < size; i++){
                positions[i] = get_offset(i, size);
        }
        return  positions;
}

//Array of sizes of blocks of matrix
int* get_sizes(int size){
        int* sizes = (int*)calloc(size, sizeof(int));
        for(int i = 0; i < size; i++){
                sizes[i] = get_rows_count_process(i, size);
        }
        return sizes;
}

double* differ_vectros(double* a, double* b, int rank, int size){
        double* res = (double*)calloc(N, sizeof(double));
                for(int i = 0; i < N; i++){
                        res[i] = a[i] - b[i];
                }
        return res;
}

void print_matrix(double* matrix, int rank, int size){
        int part_size = get_rows_count_process(rank, size);
        for(int process = 0; process < size; process++){
                MPI_Barrier(MPI_COMM_WORLD);
                if(rank == process){
                        for(int i = 0; i < part_size; i++){
                                for(int j = 0; j < N; j++){
                                        printf("%0.0f ", matrix[i*N + j]);
                                }
                                printf("\n");
                        }
                }
        }
}

double get_chisl_tau(double *Ay, double *y, int rank, int* sizes){
        double chisl;
        double part_chisl = 0;
        for(int i = 0; i < sizes[rank]; i++){
                part_chisl += Ay[i]*y[i];
        }

        MPI_Allreduce(&part_chisl, &chisl, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        return chisl;
}

double get_del_tau(double *Ay, int rank, int* sizes){
        double del;
        double part_del = 0;
        for(int i = 0; i < sizes[rank]; i++){
                part_del += Ay[i]*Ay[i];
        }

        MPI_Allreduce(&part_del, &del, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        return del;
}

double get_crit_1(double *Ax, double *b, int rank, int* sizes){
        double crit_1;
        double part_crit_1 = 0;
        for(int i = 0; i < sizes[rank]; i++){
                part_crit_1 += pow(Ax[i] - b[i], 2);
        }

        MPI_Allreduce(&part_crit_1, &crit_1, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        return crit_1;
}

double get_crit_2(double *b, int rank, int* sizes){
        double crit_2;
        double part_crit_2 = 0;
        for(int i = 0; i < sizes[rank]; i++){
                part_crit_2 += pow(b[i], 2);
        }

        MPI_Allreduce(&part_crit_2, &crit_2, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        return crit_2;
}

double* Minimal_Nevazki(double *A, double *b, int rank, int size)
{
    double* X = (double*)calloc(N, sizeof(double));

    int* positions = get_begin_positions(size);
    int* part_sizes = get_sizes(size);

    double crit_module;
    double chisl_Tau;
    double del_Tau;

    while(1){
        double* AbX = Matrix_by_vector(A, X, rank, size);
        double* Ax = (double*)calloc(N, sizeof(double));
        MPI_Allgatherv(AbX, part_sizes[rank], MPI_DOUBLE, Ax, part_sizes, positions, MPI_DOUBLE, MPI_COMM_WORLD);

        double* Y =  differ_vectros(Ax, b, rank, size);

        double* AbY = Matrix_by_vector(A, Y, rank, size);
        double* Ay = (double*)calloc(N, sizeof(double));
        MPI_Allgatherv(AbY, part_sizes[rank], MPI_DOUBLE, Ay, part_sizes, positions, MPI_DOUBLE, MPI_COMM_WORLD);

        chisl_Tau = get_chisl_tau(Ay, Y, rank, part_sizes);
        del_Tau = get_del_tau(Ay, rank, part_sizes);

        double crit_1 = get_crit_1(Ax, b, rank, part_sizes);
        double crit_2 = get_crit_2(b, rank, part_sizes);

        crit_1 = sqrt(crit_1);
        crit_2 = sqrt(crit_2);
        crit_module = crit_1/crit_2;

        if(crit_module < eps){
                free(Y);
                free(AbY);
                free(Ay);
                free(AbX);
                free(Ax);
                break;
        }

        chisl_Tau = chisl_Tau/del_Tau;
        for(int i = 0; i < N; i++){
            X[i] = X[i] - chisl_Tau*Y[i];
        }
    }
    return X;
}

int main(int argc, char **argv) {
    int size, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    double *A = create_matrix_A(rank, size);
    double *b = create_vector_b(rank, size);
    if(rank == 0)
    std::cout << "Curr epsilon:" << eps << std::endl;

    //print_matrix(A, rank, size);
    double start = MPI_Wtime();
    double *X =  Minimal_Nevazki(A, b, rank, size);
    double end = MPI_Wtime();

    if(rank == 0){
     for(int i = 0; i < N; i++)
        printf("X[%d]= %lf\n", i, X[i]);
    }

    printf("Time taken: %lf sec.\n", end - start);

    free(A);
    free(b);
    free(X);

    MPI_Finalize();
    return 0;
}
