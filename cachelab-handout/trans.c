/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void transpose_32(int M, int N, int A[N][M], int B[M][N]);
void transpose_64(int M, int N, int A[N][M], int B[M][N]);
void transpose_61(int M, int N, int A[N][M], int B[M][N]);
/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if(M == 32 && N == 32) 
    {
        transpose_32(M, N, A, B);
    }
    if(M == 64 && N ==64)
    {
        transpose_64(M, N, A, B);
    }
    if(M == 61 && N == 67)
    {
        transpose_61(M, N, A, B);
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */
void transpose_32(int M, int N, int A[N][M], int B[M][N])
{
    //由于对角线部分的冲突不命中增多，导致miss的数量较大，使用的方法是将A中的一组元素（8个）使用局部变量存储在程序寄存器中，避免了A和B的读取和写回的冲突不命中
    int a0, a1, a2, a3, a4, a5, a6, a7;
    for(int i=0; i<N; i+=8)
    {
        for(int j=0; j<M; j+=8)
        {
            for(int k=0; k<8; ++k) 
            { 
                //将一组的8个Aij都存放到寄存器文件中，然后直接传递给Bji，对于对角线元素虽然仍有冲突，但是除此之外，对角线块不会出现冲突
                //估计约287misses，较上面的对象块冲突情况（处在对角线上的块，每一组都会有两次冲突），这种方法只有处在对角线上的元素会出现冲突
                //一次取出8*8的元素，由4个高速缓存组存储，A与B的高速缓存组正好错开了，而且B=32bytes，充分利用了块的内存空间。核心在于降低了B的平均冲突不命中
                //原来要存放32个元素，有32次不命中；现在存放64个元素，有8次不命中，降低不命中率至1/8（除对角线块外，但对角块经过优化，只有一个对角元素不命中，所以不命中率也近似于1/8）
                a0 = A[i+k][j+0];
                a1 = A[i+k][j+1];
                a2 = A[i+k][j+2];
                a3 = A[i+k][j+3];
                a4 = A[i+k][j+4];
                a5 = A[i+k][j+5];
                a6 = A[i+k][j+6];
                a7 = A[i+k][j+7];

                B[j+0][i+k] = a0;
                B[j+1][i+k] = a1;
                B[j+2][i+k] = a2;
                B[j+3][i+k] = a3;
                B[j+4][i+k] = a4;
                B[j+5][i+k] = a5;
                B[j+6][i+k] = a6;
                B[j+7][i+k] = a7;
            }
        }
    }   
}

void transpose_64(int M, int N, int A[N][M], int B[M][N])
{
   int a0, a1, a2, a3, a4, a5, a6, a7;
   for(int i=0; i<N; i+=8) 
   {
        for(int j=0; j<M; j+=8)
        {
            //首先取出A1、A2，然后将它们转置到B1、B3
            for(int k=i; k<i+4; ++k)
            {
                //读取A1
                a0 = A[k][j+0];
                a1 = A[k][j+1];
                a2 = A[k][j+2];
                a3 = A[k][j+3];
                //读取A2
                a4 = A[k][j+4];
                a5 = A[k][j+5];
                a6 = A[k][j+6];
                a7 = A[k][j+7];
                //将A1转置到B1
                B[j+0][k] = a0;
                B[j+1][k] = a1;
                B[j+2][k] = a2;
                B[j+3][k] = a3; 
                //将A2转置到B3
                B[j+0][k+4] = a4;
                B[j+1][k+4] = a5;
                B[j+2][k+4] = a6;
                B[j+3][k+4] = a7;
            }
            //先用局部变量存放B3中一组的数据（设为B3中行a的数据），然后将A3的一列赋给B3的行a（给B3的这一组修改为A3的转置，没有miss），然后处理B2中的行b，其中，行b与B3中刚刚处理的行a有相同的组索引
            //所以局部变量存放的正是B2中行b对应的A2转置，所以直接将局部变量存放的值赋给B2中的行b即可，而且由于驱逐的只有行a，所以这一步没有导致B3中其他数据丢失；由于行a与行b有相同的组索引，所以只有一次miss，
            //对应的是行b未命中，同时驱逐行a，而行a既将数据存放到局部变量，传给了行b（这是一个很好的性质，即行a与行b有相同的组索引，且行a与行b恰好在各自块中的行号相同，所以行b对应的A的转置存放在行a中）
            //又修改成了对应的A3转置，所以行a已经被处理好了，不需要再加载到缓存中进行处理，而行b也已经加载到缓存中了
            for(int k=j; k<4+j; ++k)
            {
                //按行读取行a的数据，并存放到局部变量中
                a4 = B[k][i+4];
                a5 = B[k][i+5];
                a6 = B[k][i+6];
                a7 = B[k][i+7];
                //将对应A3中的转置传给行a
                a0 = A[i+4][k];
                a1 = A[i+5][k];
                a2 = A[i+6][k];
                a3 = A[i+7][k];
                B[k][i+4] = a0;
                B[k][i+5] = a1;
                B[k][i+6] = a2;
                B[k][i+7] = a3;
                //处理B2中的行b，将局部变量a4~a7的值传给行b
                B[k+4][i+0] = a4;
                B[k+4][i+1] = a5;
                B[k+4][i+2] = a6;
                B[k+4][i+3] = a7;
            }
            //最后，处理B4，直接将A4转置，给B4，由于缓存中存在A2、A4，所以不会miss
            //由于有8个局部变量，而每个组中只有4个变量待处理，所以我利用循环展开，每两组进行处理
            for(int k=i+4; k<i+8; k+=2)
            {
                a0 = A[k][j+4];
                a1 = A[k][j+5];
                a2 = A[k][j+6];
                a3 = A[k][j+7];
                a4 = A[k+1][j+4];
                a5 = A[k+1][j+5];
                a6 = A[k+1][j+6];
                a7 = A[k+1][j+7];

                B[j+4][k] = a0;
                B[j+5][k] = a1;
                B[j+6][k] = a2;
                B[j+7][k] = a3;
                B[j+4][k+1] = a4;
                B[j+5][k+1] = a5;
                B[j+6][k+1] = a6;
                B[j+7][k+1] = a7;
            }

        }
   }
}

void transpose_61(int M, int N, int A[N][M], int B[M][N])
{
     for (int i=0; i<N; i+=17)
     {
         for(int j=0; j<M; j+=17)
         {
             for(int k=i; k<i+17 && k<N; ++k)
             {
                 for(int s=j; s<j+17 && s<M; ++s)
                 {
                     B[s][k] = A[k][s];
                 }
             }  
         }
     }
}

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}