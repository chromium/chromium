// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mac_mps.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/mps_protocols_impl.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/interfaces/constants.mojom.h"

namespace ml {

namespace {

NSString* API_AVAILABLE(macosx(10.13)) KernelFor(const MPSImage* X,
                                                 NSString* arrayKernel,
                                                 NSString* nonArrayKernel) {
  if (X.featureChannels > 4) {
    return arrayKernel;
  }
  if (X.numberOfImages > 1) {
    return arrayKernel;
  }
  return nonArrayKernel;
}

auto divRoundUp(uint x, uint y) -> uint {
  return (x + y - 1) / y;
}

struct LaunchParams {
  MTLSize threadsPerThreadgroup;
  MTLSize threadgroupsPerGrid;
};

LaunchParams API_AVAILABLE(macosx(10.13))
    SpatialPointwiseKernelLaunchParams(id<MTLComputePipelineState> pipeline,
                                       const MPSImage* im) {
  // const auto maxThreadsPerThreadgroup =
  //[pipeline maxTotalThreadsPerThreadgroup];
  // const auto threadExecutionWidth = [pipeline threadExecutionWidth];
  const auto threadsPerThreadgroup =
      MTLSizeMake(8 /* threadExecutionWidth */,
                  4 /* maxThreadsPerThreadgroup / threadExecutionWidth */, 1);
  const auto threadgroupsPerGrid =
      MTLSizeMake(divRoundUp(im.width, threadsPerThreadgroup.width),
                  divRoundUp(im.height, threadsPerThreadgroup.height),
                  im.numberOfImages * divRoundUp(im.featureChannels, 4));
  return {threadsPerThreadgroup, threadgroupsPerGrid};
};

MPSImageDescriptor* API_AVAILABLE(macosx(10.13))
    CreateMPSImageDescriptor(const OperandMac& operand) {
  int32_t type = operand.type;
  MPSImageDescriptor* mpsimage_desc = nullptr;
  if (type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "type " << type << " is not supported";
    return mpsimage_desc;
  }
  uint32_t n, width, height, channels;
  if (!ml::GetMPSImageInfo(operand, n, width, height, channels)) {
    return mpsimage_desc;
  }
  mpsimage_desc = [MPSImageDescriptor
      imageDescriptorWithChannelFormat:MPSImageFeatureChannelFormatFloat16
                                 width:width
                                height:height
                       featureChannels:channels
                        numberOfImages:n
                                 usage:MTLTextureUsageShaderRead |
                                       MTLTextureUsageShaderWrite];
  LOG(INFO) << "Create MPSImageDescriptor " << mpsimage_desc << " [" << width
            << ", " << height << ", " << channels << "]";
  return mpsimage_desc;
}

}  // namespace

ExecutionImplMacMPS::ExecutionImplMacMPS(
    base::WeakPtr<CompilationImplMac> compilation,
    mojo::ScopedSharedBufferHandle memory) {
  compilation_ = compilation;
  uint32_t mapped_length = 0;
  SetupOperandInfoForOperands(inputs_info_, compilation_->operands_,
                              compilation_->inputs_, memory, mapped_length);
  SetupOperandInfoForOperands(outputs_info_, compilation_->operands_,
                              compilation_->outputs_, memory, mapped_length);

  if (@available(macOS 10.13, *)) {
    SetupMPSImageForOperands(input_mpsimages_, input_mtlbuffers_,
                             compilation_->inputs_);
    SetupMPSImageForOperands(constant_mpsimages_, constant_mtlbuffers_,
                             compilation_->constants_);
  }
}

ExecutionImplMacMPS::~ExecutionImplMacMPS() = default;

bool ExecutionImplMacMPS::IsValid() const {
  bool valid = compilation_ != nil &&
               inputs_info_.size() == compilation_->inputs_.size() &&
               outputs_info_.size() == compilation_->outputs_.size();
  if (compilation_) {
    if (@available(macOS 10.13, *)) {
      valid &= compilation_->inputs_.size() == input_mpsimages_.size() &&
               compilation_->constants_.size() == constant_mpsimages_.size();
    }
  }
  return valid;
}

void API_AVAILABLE(macosx(10.13)) ExecutionImplMacMPS::SetupMPSImageForOperands(
    std::vector<base::scoped_nsobject<MPSImage>>& mps_image_array,
    std::vector<id<MTLBuffer>>& mtl_buffer_array,
    const std::vector<uint32_t>& operands_index_array) {
  for (size_t i = 0; i < operands_index_array.size(); ++i) {
    const OperandMac& operand =
        compilation_->operands_[operands_index_array[i]];
    if (@available(macOS 10.13, *)) {
      MPSImageDescriptor* descriptor = CreateMPSImageDescriptor(operand);
      if (!descriptor)
        return;
      base::scoped_nsobject<MPSImage> mps_img([[MPSImage alloc]
           initWithDevice:GetMPSCNNContext().device
          imageDescriptor:descriptor]);
      mps_image_array.push_back(std::move(mps_img));
      mtl_buffer_array.push_back([GetMPSCNNContext().device
          newBufferWithLength:operand.requiredSize()
                      options:MTLResourceOptionCPUCacheModeWriteCombined]);
    }
  }
}

void ExecutionImplMacMPS::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMac::StartCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        id<MTLCommandBuffer> command_buffer =
            [GetMPSCNNContext().command_queue commandBuffer];

        NSMutableArray<MPSImage*>* image_array =
            [NSMutableArray arrayWithCapacity:1];
        for (size_t i = 0; i < compilation_->inputs_.size(); ++i) {
          std::unique_ptr<OperandInfo>& input_data = inputs_info_[i];
          MPSImage* mps_img = input_mpsimages_[i].get();
          const id<MTLBuffer> mtl_buffer = input_mtlbuffers_[i];
          UploadToMPSImage(mps_img, mtl_buffer, command_buffer,
                           input_data->mapping.get(), input_data->length);
          [image_array addObject:mps_img];
        }

        for (size_t i = 0; i < compilation_->constants_.size(); ++i) {
          uint32_t index = compilation_->constants_[i];
          if (compilation_->values_.find(index) ==
              compilation_->values_.end()) {
            DLOG(ERROR) << "Can't find constant " << index;
            success = false;
            break;
          }
          const ValueInfo& value_info =
              compilation_->values_[compilation_->constants_[i]];
          MPSImage* mps_img = constant_mpsimages_[i].get();
          const id<MTLBuffer> mtl_buffer = constant_mtlbuffers_[i];
          const void* cpu_buffer = static_cast<const void*>(
              compilation_->memory_.get() + value_info.offset);
          UploadToMPSImage(mps_img, mtl_buffer, command_buffer, cpu_buffer,
                           value_info.length);
          [image_array addObject:mps_img];
        }

        MPSImage* output_image_graph =
            [compilation_->graph_ encodeToCommandBuffer:command_buffer
                                           sourceImages:image_array];
        const OperandMac& operand =
            compilation_->operands_[compilation_->outputs_[0]];
        id<MTLBuffer> output_buffer = [GetMPSCNNContext().device
            newBufferWithLength:operand.requiredSize()
                        options:MTLResourceOptionCPUCacheModeWriteCombined];
        DCHECK(compilation_->outputs_.size() == 1);
        for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
          MPSImage* output_img = output_image_graph;

          id<MTLComputeCommandEncoder> encoder =
              [command_buffer computeCommandEncoder];
          id<MTLComputePipelineState> state =
              GetMPSCNNContext().GetSpecializedPipelineState(
                  KernelFor(output_img, @"copy_metal_to_nhwc",
                            @"copy_metal_to_nhwc_nonarray"),
                  {{ushort(output_img.height), ushort(output_img.width),
                    ushort(output_img.featureChannels)}});

          [encoder setComputePipelineState:state];
          [encoder setBuffer:output_buffer offset:0 atIndex:0];
          [encoder setTexture:[output_img texture] atIndex:0];

          const auto& outputLaunchParams =
              SpatialPointwiseKernelLaunchParams(state, output_img);
          [encoder
               dispatchThreadgroups:outputLaunchParams.threadgroupsPerGrid
              threadsPerThreadgroup:outputLaunchParams.threadsPerThreadgroup];
          [encoder endEncoding];
        }

        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
          std::unique_ptr<OperandInfo>& output_data = outputs_info_[i];
          memcpy(output_data->mapping.get(), [output_buffer contents],
                 output_data->length);
        }
      }  // @autoreleasepool
    } while (0);
  }

  if (success) {
    std::move(callback).Run(mojom::NOT_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

void ExecutionImplMacMPS::UploadToMPSImage(
    const MPSImage* mps_image,
    const id<MTLBuffer>& mtl_buffer,
    const id<MTLCommandBuffer>& command_buffer,
    const void* cpu_buffer,
    size_t length) {
  if (@available(macOS 10.13, *)) {
    memcpy([mtl_buffer contents], cpu_buffer, length);
    id<MTLComputeCommandEncoder> encoder =
        [command_buffer computeCommandEncoder];
    id<MTLComputePipelineState> state =
        GetMPSCNNContext().GetSpecializedPipelineState(
            KernelFor(mps_image, @"copy_nhwc_to_metal",
                      @"copy_nhwc_to_metal_nonarray"),
            {{ushort(mps_image.height), ushort(mps_image.width),
              ushort(mps_image.featureChannels)}});
    [encoder setComputePipelineState:state];
    [encoder setBuffer:mtl_buffer offset:0 atIndex:0];
    [encoder setTexture:[mps_image texture] atIndex:0];
    const auto& inputLaunchParams =
        SpatialPointwiseKernelLaunchParams(state, mps_image);
    [encoder dispatchThreadgroups:inputLaunchParams.threadgroupsPerGrid
            threadsPerThreadgroup:inputLaunchParams.threadsPerThreadgroup];
    [encoder endEncoding];
  }
}

}  // namespace ml
