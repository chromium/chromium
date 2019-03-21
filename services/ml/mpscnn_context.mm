// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/mpscnn_context.h"
#include "base/logging.h"

#import <Metal/MTLFunctionConstantValues.h>

namespace ml {

static const char* MPSCNN_KERNELS = R"V0G0N(


using namespace metal;

constant ushort ushort_arg_0[[function_constant(0)]];
constant ushort ushort_arg_1[[function_constant(1)]];
constant ushort ushort_arg_2[[function_constant(2)]];
constant ushort ushort_arg_3[[function_constant(3)]];
constant ushort ushort_arg_4[[function_constant(4)]];
constant ushort ushort_arg_5[[function_constant(5)]];
constant ushort ushort_arg_6[[function_constant(6)]];
constant ushort ushort_arg_7[[function_constant(7)]];
constant ushort ushort_arg_8[[function_constant(8)]];
constant ushort ushort_arg_9[[function_constant(9)]];

inline constexpr ushort divRoundUp(ushort x, ushort y) { return (x + (y - 1)) / y; }

kernel void copy_nhwc_to_metal(constant float* in[[buffer(0)]],
                               texture2d_array<half, access::write> out[[texture(0)]],
                               ushort3 gid[[thread_position_in_grid]]) {
    const ushort H = ushort_arg_0;
    const ushort W = ushort_arg_1;
    const ushort C = ushort_arg_2;
    if (gid.x >= W || gid.y >= H) {
        return;
    }
    
    const ushort n = gid.z / divRoundUp(C, 4);
    const ushort c = gid.z - n * divRoundUp(C, 4);
    
    // TODO: are the `else` branches needed?
    // TODO: trick the optimizer for case where C == 4?
#define HWC_TO_CHWP4(idx, n, c_, h, w)                                     \
if ((c_) < C) {                                                          \
trns[idx] = in[n * H * W * C + int(h) * W * C + int(w) * C + int(c_)]; \
} else {                                                                 \
trns[idx] = 0.0h;                                                      \
}
    
    half4 trns;
    HWC_TO_CHWP4(0, n, c * 4 + 0, gid.y, gid.x);
    HWC_TO_CHWP4(1, n, c * 4 + 1, gid.y, gid.x);
    HWC_TO_CHWP4(2, n, c * 4 + 2, gid.y, gid.x);
    HWC_TO_CHWP4(3, n, c * 4 + 3, gid.y, gid.x);
#undef HWC_TO_CHWP4
    
    out.write(trns, gid.xy, gid.z);
}

kernel void copy_nhwc_to_metal_nonarray(constant float* in[[buffer(0)]],
                                        texture2d<half, access::write> out[[texture(0)]],
                                        ushort2 gid[[thread_position_in_grid]]) {
    const ushort H = ushort_arg_0;
    const ushort W = ushort_arg_1;
    const ushort C = ushort_arg_2;
    
    if (gid.x >= W || gid.y >= H) {
        return;
    }
    
    half4 trns;
    // TODO: are the `else` branches needed?
    // TODO: trick the optimizer for case where C % 4 == 0?
    
#define HWC_TO_CHWP4(idx, c, h, w)                      \
if ((c) < C) {                                          \
  trns[idx] = in[int(h) * W * C + int(w) * C + int(c)]; \
} else {                                                \
  trns[idx] = 0.0h;                                     \
}
    
    HWC_TO_CHWP4(0, 0, gid.y, gid.x);
    HWC_TO_CHWP4(1, 1, gid.y, gid.x);
    HWC_TO_CHWP4(2, 2, gid.y, gid.x);
    HWC_TO_CHWP4(3, 3, gid.y, gid.x);
#undef HWC_TO_CHWP4
    
    out.write(trns, gid.xy);
}

kernel void copy_metal_to_nhwc(texture2d_array<half, access::read> in[[texture(0)]],
                               device float* out[[buffer(0)]],
                               ushort3 gid[[thread_position_in_grid]]) {
    const ushort H = ushort_arg_0;
    const ushort W = ushort_arg_1;
    const ushort C = ushort_arg_2;
    
    if (gid.x >= W || gid.y >= H) {
        return;
    }
    const ushort n = gid.z / divRoundUp(C, 4);
    const ushort c = gid.z - n * divRoundUp(C, 4);
    
    half4 cs = in.read(gid.xy, gid.z);
    
#define CHWP4_TO_HWC(idx, n, c_, h, w)                                  \
if ((c_) < C) {                                                         \
  out[n * H * W * C + int(h) * W * C + int(w) * C + int(c_)] = cs[idx];     \
}
    
    CHWP4_TO_HWC(0, n, c * 4 + 0, gid.y, gid.x);
    CHWP4_TO_HWC(1, n, c * 4 + 1, gid.y, gid.x);
    CHWP4_TO_HWC(2, n, c * 4 + 2, gid.y, gid.x);
    CHWP4_TO_HWC(3, n, c * 4 + 3, gid.y, gid.x);
#undef CHWP4_TO_HWC
}

kernel void copy_metal_to_nhwc_nonarray(texture2d<half, access::read> in[[texture(0)]],
                                        device float* out[[buffer(0)]],
                                        ushort2 gid[[thread_position_in_grid]]) {
    const ushort H = ushort_arg_0;
    const ushort W = ushort_arg_1;
    const ushort C = ushort_arg_2;
    
    if (gid.x >= W || gid.y >= H) {
        return;
    }
    
    half4 cs = in.read(gid.xy);
    
#define CHWP4_TO_HWC(idx, c, h, w)                       \
if ((c) < C) {                                         \
out[int(h) * W * C + int(w) * C + int(c)] = cs[idx];  \
}
    
    CHWP4_TO_HWC(0, 0, gid.y, gid.x);
    CHWP4_TO_HWC(1, 1, gid.y, gid.x);
    CHWP4_TO_HWC(2, 2, gid.y, gid.x);
    CHWP4_TO_HWC(3, 3, gid.y, gid.x);
#undef CHWP4_TO_HWC
}

)V0G0N";

MPSCNNContext::MPSCNNContext() = default;
MPSCNNContext::~MPSCNNContext() = default;

MPSCNNContext& GetMPSCNNContext() {
  static MPSCNNContext ctx;
  if (!ctx.initialized) {
    ctx.initialized = true;

    ctx.device = MTLCreateSystemDefaultDevice();
    if (ctx.device == nil) {
      DLOG(ERROR) << "Cannot create MTLDevice";
      return ctx;
    } else {
      DLOG(INFO) << "Created MTLDevice: " << ctx.device.name.UTF8String;
    }

    NSError* compileError = nil;
    ctx.library = [ctx.device
        newLibraryWithSource:[NSString stringWithUTF8String:MPSCNN_KERNELS]
                     options:nil
                       error:&compileError];
    if (compileError != nil || ctx.library == nil) {
      DLOG(ERROR) << "Failed to load kernels: "
                  << [[compileError localizedDescription] UTF8String];
      return ctx;
    }

    ctx.command_queue = [ctx.device newCommandQueue];
  };
  return ctx;
}

id<MTLComputePipelineState> MPSCNNContext::GetPipelineState(NSString* kernel) {
  std::string kernelStr = std::string([kernel UTF8String]);
  if (pipelineCache_.find(kernelStr) != pipelineCache_.end()) {
    DLOG(INFO) << "Hit in pipeline cache for: " << kernelStr;
    return pipelineCache_[kernelStr];
  }
  DLOG(INFO) << "Miss in pipeline cache for: " << kernelStr;
  id<MTLFunction> func = [library newFunctionWithName:kernel];
  if (!func) {
    DLOG(ERROR) << "Couldn't get function: " << kernelStr;
    return nullptr;
  }
  NSError* errors;
  id<MTLComputePipelineState> state =
      [device newComputePipelineStateWithFunction:func error:&errors];
  if (!state) {
    DLOG(ERROR) << "Couldn't get state: " << kernelStr;
    return nullptr;
  }
  pipelineCache_[kernelStr] = state;
  return state;
}

id<MTLComputePipelineState> MPSCNNContext::GetSpecializedPipelineState(
    NSString* kernel,
    const std::vector<ushort>& constants) {
  std::string kernelStr = std::string([kernel UTF8String]);
  for (size_t i = 0; i < constants.size(); ++i) {
    kernelStr += "_" + std::to_string(constants[i]);
  }
  if (pipelineCache_.find(kernelStr) != pipelineCache_.end()) {
    DLOG(INFO) << "Hit in pipeline cache for: " << kernelStr;
    return pipelineCache_[kernelStr];
  }
  MTLFunctionConstantValues* constantValues = [MTLFunctionConstantValues new];
  for (size_t i = 0; i < constants.size(); ++i) {
    [constantValues setConstantValue:&constants[i]
                                type:MTLDataTypeUShort
                             atIndex:i];
  }
  NSError* errors;

  DLOG(INFO) << "Miss in pipeline cache for: " << kernelStr;
  id<MTLFunction> func = [library newFunctionWithName:kernel
                                       constantValues:constantValues
                                                error:&errors];
  if (!func) {
    DLOG(ERROR) << "Couldn't get function: " << kernelStr
                << " error: " << [[errors localizedDescription] UTF8String];
    return nullptr;
  }
  id<MTLComputePipelineState> state =
      [device newComputePipelineStateWithFunction:func error:&errors];
  if (!state) {
    DLOG(ERROR) << "Couldn't get function: " << kernelStr
                << " error: " << [[errors localizedDescription] UTF8String];
    return nullptr;
  }
  pipelineCache_[kernelStr] = state;
  return state;
}

}