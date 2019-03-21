
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MPSCNNCONTEXT_H_
#define SERVICES_ML_MPSCNNCONTEXT_H_

#import <Metal/MTLBuffer.h>
#import <Metal/MTLDevice.h>
#import <Metal/MTLLibrary.h>

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

}

#endif  // SERVICES_ML_MPSCNNCONTEXT_H_