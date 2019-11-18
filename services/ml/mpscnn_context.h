
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MPSCNNCONTEXT_H_
#define SERVICES_ML_MPSCNNCONTEXT_H_

#import <Metal/MTLBuffer.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLLibrary.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace ml {

struct API_AVAILABLE(macosx(10.13)) MPSCNNContext {
 public:
  MPSCNNContext();
  ~MPSCNNContext();
  id<MTLDevice> device;
  id<MTLCommandQueue> command_queue;
  id<MTLLibrary> library;
  bool initialized;

  bool IsValid() const {
    return initialized && device != nil && library != nil;
  }

  id<MTLComputePipelineState> GetPipelineState(NSString* kernel);
  id<MTLComputePipelineState> GetSpecializedPipelineState(
      NSString* kernel,
      const std::vector<ushort>& constants);

 private:
  std::unordered_map<std::string, id<MTLComputePipelineState>> pipelineCache_;
};

MPSCNNContext& API_AVAILABLE(macosx(10.13)) GetMPSCNNContext();

struct LaunchParams {
  MTLSize threadsPerThreadgroup;
  MTLSize threadgroupsPerGrid;
};

uint divRoundUp(uint x, uint y);

API_AVAILABLE(macosx(10.13))
LaunchParams SpatialPointwiseKernelLaunchParams(
    id<MTLComputePipelineState> pipeline,
    const MPSImage* im);

API_AVAILABLE(macosx(10.13))
NSString* KernelFor(const MPSImage* X,
                    NSString* arrayKernel,
                    NSString* nonArrayKernel);

API_AVAILABLE(macosx(10.13))
void UploadToMPSImage(const MPSImage*,
                      const id<MTLCommandBuffer>&,
                      const void*,
                      size_t);
}

#endif  // SERVICES_ML_MPSCNNCONTEXT_H_