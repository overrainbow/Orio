void VecScaleMult(int n, double a, double *x) {

    register int i;

    /*@ begin Loop (
          transform CUDA(threadCount=16, cacheBlocks=True, pinHostMem=False, streamCount=2)
        for (i=0; i<=n-1; i++)
          x[i]=a*x[i];
    ) @*/
    {
      /*declare variables*/
      double *dev_x;
      int nthreads=16;
      int nstreams=2;
      /*calculate device dimensions*/
      dim3 dimGrid, dimBlock;
      dimBlock.x=nthreads;
      dimGrid.x=(n+nthreads-1)/nthreads;
      /*create streams*/
      int istream, soffset;
      cudaStream_t stream[nstreams+1];
      for (istream=0; istream<=nstreams; istream++ ) 
        cudaStreamCreate(&stream[istream]);
      int chunklen=n/nstreams;
      int chunkrem=n%nstreams;
      /*allocate device memory*/
      int nbytes=n*sizeof(double);
      cudaMalloc((void**)&dev_x,nbytes);
      cudaHostRegister(x,nbytes,cudaHostRegisterPortable);
      /*copy data from host to device*/
      for (istream=0; istream<nstreams; istream++ ) {
        soffset=istream*chunklen;
        cudaMemcpyAsync(dev_x+soffset,x+soffset,chunklen*sizeof(double),cudaMemcpyHostToDevice,stream[istream]);
      }
      if (chunkrem!=0) {
        soffset=istream*chunklen;
        cudaMemcpyAsync(dev_x+soffset,x+soffset,chunkrem*sizeof(double),cudaMemcpyHostToDevice,stream[istream]);
      }
      cudaEvent_t start, stop;
      cudaEventCreate(&start);
      cudaEventCreate(&stop);
      cudaEventRecord(start,0);
      /*invoke device kernel*/
      int blks4chunk=dimGrid.x/nstreams;
      if (dimGrid.x%nstreams!=0) 
        blks4chunk++ ;
      for (istream=0; istream<nstreams; istream++ ) {
        soffset=istream*chunklen;
        orcu_kernel3<<<blks4chunk,dimBlock,0,stream[istream]>>>(chunklen,a,dev_x+soffset);
      }
      if (chunkrem!=0) {
        soffset=istream*chunklen;
        orcu_kernel3<<<blks4chunk,dimBlock,0,stream[istream]>>>(chunkrem,a,dev_x+soffset);
      }
      cudaEventRecord(stop,0);
      cudaEventSynchronize(stop);
      cudaEventElapsedTime(&orcu_elapsed,start,stop);
      cudaEventDestroy(start);
      cudaEventDestroy(stop);
      /*copy data from device to host*/
      for (istream=0; istream<nstreams; istream++ ) {
        soffset=istream*chunklen;
        cudaMemcpyAsync(x+soffset,dev_x+soffset,chunklen*sizeof(double),cudaMemcpyDeviceToHost,stream[istream]);
      }
      if (chunkrem!=0) {
        soffset=istream*chunklen;
        cudaMemcpyAsync(x+soffset,dev_x+soffset,chunkrem*sizeof(double),cudaMemcpyDeviceToHost,stream[istream]);
      }
      for (istream=0; istream<=nstreams; istream++ ) 
        cudaStreamSynchronize(stream[istream]);
      cudaHostUnregister(x);
      for (istream=0; istream<=nstreams; istream++ ) 
        cudaStreamDestroy(stream[istream]);
      /*free allocated memory*/
      cudaFree(dev_x);
    }
/*@ end @*/
}
__global__ void orcu_kernel3(int n, double a, double* x) {
  int tid=blockIdx.x*blockDim.x+threadIdx.x;
  __shared__ double shared_x[16];
  if (tid<=n-1) {
    shared_x[threadIdx.x]=x[tid];
    shared_x[threadIdx.x]=a*shared_x[threadIdx.x];
    x[tid]=shared_x[threadIdx.x];
  }
}
