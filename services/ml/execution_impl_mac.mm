// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/ml/execution_impl_mac.h"
#include "services/ml/mpscnn_context.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

namespace ml {

NSString*
API_AVAILABLE(macosx(10.13)) KernelFor(const MPSImage* X, NSString* arrayKernel, NSString* nonArrayKernel) {
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

LaunchParams API_AVAILABLE(macosx(10.13)) SpatialPointwiseKernelLaunchParams(
                                                id<MTLComputePipelineState> pipeline,
                                                const MPSImage* im) {
    //const auto maxThreadsPerThreadgroup =
    //[pipeline maxTotalThreadsPerThreadgroup];
    //const auto threadExecutionWidth = [pipeline threadExecutionWidth];
    const auto threadsPerThreadgroup = MTLSizeMake(
                                                   8 /* threadExecutionWidth */,
                                                   4 /* maxThreadsPerThreadgroup / threadExecutionWidth */,
                                                   1);
    const auto threadgroupsPerGrid = MTLSizeMake(
                                                 divRoundUp(im.width, threadsPerThreadgroup.width),
                                                 divRoundUp(im.height, threadsPerThreadgroup.height),
                                                 im.numberOfImages * divRoundUp(im.featureChannels, 4));
    return {threadsPerThreadgroup, threadgroupsPerGrid};
};

bool GetMPSImageInfo(const OperandMac& operand, uint32_t& n, uint32_t& width, uint32_t& height, uint32_t& channels) {
  const std::vector<uint32_t>& dimensions = operand.dimensions;
  if (dimensions.size() == 4) {
    n = dimensions[0];
    height = dimensions[1];
    width = dimensions[2];
    channels = dimensions[3];
    return true;
  } else if (dimensions.size() == 2) {
    n = dimensions[0];
    channels = dimensions[1];
    height = 1;
    width = 1;
    return true;
  } else {
    DLOG(ERROR) << "dimension " << dimensions.size() << " is not supported";
    return false;
  }
}

MPSImageDescriptor* API_AVAILABLE(macosx(10.13)) CreateMPSImageDescriptor(const OperandMac& operand) {
  int32_t type = operand.type;
  MPSImageDescriptor* mpsimage_desc = nullptr;
  if (type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "type " << type << " is not supported";
    return mpsimage_desc;
  }
  uint32_t n, width, height, channels;
  if (!GetMPSImageInfo(operand, n, width, height, channels)) {
    return mpsimage_desc;
  }
  if (n != 1) {
    DLOG(ERROR) << "number of images " << n << " is not supported";
    return mpsimage_desc;
  }
  mpsimage_desc = [MPSImageDescriptor
      imageDescriptorWithChannelFormat:MPSImageFeatureChannelFormatFloat16
      width:width
      height:height
      featureChannels:channels
      numberOfImages:n
      usage:MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite];
  DLOG(INFO) << "Create MPSImageDescriptor " << mpsimage_desc
      << " [" << width << ", " << height << ", " << channels << "]";
  return mpsimage_desc;
}

ExecutionImplMac::ExecutionImplMac(CompilationImplMac* compilation, mojo::ScopedSharedBufferHandle memory) {
  compilation_ = compilation;
  memory_ = std::move(memory);
  uint32_t total_length = 0;
  uint32_t inputs_size = compilation_->inputs_.size();
  if (@available(macOS 10.13, *)) {
    input_mpsimages_.resize(inputs_size);
    input_mtlbuffers_.resize(inputs_size);
  }
  for (size_t i = 0; i < inputs_size; ++i) {
    OperandMac& operand = compilation_->operands_[compilation_->inputs_[i]];
    uint32_t offset = total_length;
    uint32_t length = operand.requiredSize();
    mojo::ScopedSharedBufferMapping mapping = memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(new OperandInfo(offset, length, std::move(mapping)));
    inputs_info_.push_back(std::move(info));
    total_length += length;
    if (@available(macOS 10.13, *)) {
      MPSImage* mps_img = [[MPSImage alloc]
          initWithDevice:GetMPSCNNContext().device
          imageDescriptor:CreateMPSImageDescriptor(operand)];
      input_mpsimages_[i].reset(mps_img);
      input_mtlbuffers_[i] = [GetMPSCNNContext().device
          newBufferWithLength:length
          options:MTLResourceOptionCPUCacheModeWriteCombined];
    }
  }
  uint32_t outputs_size = compilation_->outputs_.size();
  if (@available(macOS 10.13, *)) {
    output_mpsimages_.resize(outputs_size);
    output_mtlbuffers_.resize(outputs_size);
  }
  for (size_t i = 0; i < outputs_size; ++i) {
    OperandMac& operand = compilation_->operands_[compilation_->outputs_[i]];
    uint32_t offset = total_length;
    uint32_t length = operand.requiredSize();
    mojo::ScopedSharedBufferMapping mapping = memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(new OperandInfo(offset, length, std::move(mapping)));
    outputs_info_.push_back(std::move(info));
    total_length += length;
    if (@available(macOS 10.13, *)) {
      MPSImage* mps_img = [[MPSImage alloc]
          initWithDevice:GetMPSCNNContext().device
          imageDescriptor:CreateMPSImageDescriptor(operand)];
      output_mpsimages_[i].reset(mps_img);
      output_mtlbuffers_[i] = [GetMPSCNNContext().device
          newBufferWithLength:length
          options:MTLResourceOptionCPUCacheModeWriteCombined];
    }
  }
}

ExecutionImplMac::~ExecutionImplMac() {}

void ExecutionImplMac::startCompute(startComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMac::startCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        id<MTLCommandBuffer> command_buffer = [GetMPSCNNContext().command_queue commandBuffer];

        if (compilation_->inputs_.size() > 1 || compilation_->outputs_.size() > 1) {
          DLOG(ERROR) << "Only input size and output size 1 is supported";
          success = false;
          break;
        }

        const uint32_t input_idx = compilation_->inputs_[0];
        const uint32_t output_idx = compilation_->outputs_[0];
        std::unique_ptr<OperandInfo>& input_data = inputs_info_[0];
        std::unique_ptr<OperandInfo>& output_data = outputs_info_[0];
        MPSImage* input_img = input_mpsimages_[0].get();
        MPSImage* output_img = output_mpsimages_[0].get();
        id<MTLBuffer> input_buffer = input_mtlbuffers_[0];
        id<MTLBuffer> output_buffer = output_mtlbuffers_[0];

        {
          memcpy([input_buffer contents], input_data->mapping.get(), input_data->length);
          id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];
          id<MTLComputePipelineState> state =
              GetMPSCNNContext().GetSpecializedPipelineState(
                  KernelFor(input_img, @"copy_nhwc_to_metal", @"copy_nhwc_to_metal_nonarray"),
                  {{ushort(input_img.height), ushort(input_img.width), ushort(input_img.featureChannels)}});
          [encoder setComputePipelineState:state];
          [encoder setBuffer:input_buffer offset:0 atIndex:0];
          [encoder setTexture:[input_img texture] atIndex:0];
          const auto& inputLaunchParams =
              SpatialPointwiseKernelLaunchParams(state, input_img);
          [encoder dispatchThreadgroups:inputLaunchParams.threadgroupsPerGrid
                  threadsPerThreadgroup:inputLaunchParams.threadsPerThreadgroup];
          [encoder endEncoding];
        }

        std::map<uint32_t, MPSTemporaryImage*> tmp_mpsimage_cache;
        for (size_t i = 0; i < compilation_->operations_.size(); i++) {
          const OperationMac& operation = compilation_->operations_[i];
          MPSCNNKernel* kernel = operation.mpscnn_kernel.get();
          if (!kernel) {
            DLOG(INFO) << "No kernel compiled for operation " << i << " type " << operation.type;
            continue;
          }
          MPSImage* src_img = nullptr;
          MPSImage* dst_img = nullptr;
          uint32_t operation_input_idx = operation.inputs[0];
          const OperandMac& operation_input = compilation_->operands_[operation_input_idx];
          if (operation_input_idx == input_idx) {
            src_img = input_img;
          }
          uint32_t operation_output_idx = operation.outputs[0];
          const OperandMac& operation_output = compilation_->operands_[operation_output_idx];
          if (operation_output_idx == output_idx) {
            dst_img = output_img;
          }
          if (!src_img) {
            if (tmp_mpsimage_cache.find(operation_input_idx) == tmp_mpsimage_cache.end()) {
              MPSTemporaryImage* temp_image = [MPSTemporaryImage
                  temporaryImageWithCommandBuffer:command_buffer
                  imageDescriptor:CreateMPSImageDescriptor(operation_input)];
              DLOG(INFO) << "Set readCount as " << operation_input.read_count;
              temp_image.readCount = operation_input.read_count;
              tmp_mpsimage_cache[operation_input_idx] = temp_image;
            }
            src_img = tmp_mpsimage_cache[operation_input_idx];
          }
          if (!dst_img) {
            if (tmp_mpsimage_cache.find(operation_output_idx) == tmp_mpsimage_cache.end()) {
              MPSTemporaryImage* temp_image = [MPSTemporaryImage
                  temporaryImageWithCommandBuffer:command_buffer
                  imageDescriptor:CreateMPSImageDescriptor(operation_output)];
              DLOG(INFO) << "Set readCount as " << operation_output.read_count;
              temp_image.readCount = operation_output.read_count;
              tmp_mpsimage_cache[operation_output_idx] = temp_image;
            }
            dst_img = tmp_mpsimage_cache[operation_output_idx];
          }
          DLOG(INFO) << "Encode operation " << i << " with kernel " <<
              kernel << " src " << operation_input_idx << " sourceImage " << src_img <<
              " dst " << operation_output_idx << " destinationImage " << dst_img;
          [kernel encodeToCommandBuffer:command_buffer
              sourceImage:src_img
              destinationImage:dst_img];
        }

        {
          id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];
          id<MTLComputePipelineState> state = GetMPSCNNContext().GetSpecializedPipelineState(
              KernelFor(output_img, @"copy_metal_to_nhwc", @"copy_metal_to_nhwc_nonarray"),
              {{ushort(output_img.height), ushort(output_img.width), ushort(output_img.featureChannels)}});
                
          [encoder setComputePipelineState:state];
          [encoder setBuffer:output_buffer offset:0 atIndex:0];
          [encoder setTexture:[output_img texture] atIndex:0];
              
          const auto& outputLaunchParams = SpatialPointwiseKernelLaunchParams(state, output_img);
          [encoder dispatchThreadgroups:outputLaunchParams.threadgroupsPerGrid
                  threadsPerThreadgroup:outputLaunchParams.threadsPerThreadgroup];
          [encoder endEncoding];
        }

        [command_buffer commit];
        [command_buffer waitUntilCompleted];

        //DLOG(INFO) << "Copy memory back from output buffer with length " << output_buffer.length;
        memcpy(output_data->mapping.get(), [output_buffer contents], output_data->length);
      }  // @autoreleasepool
    } while(0);
  }

  if (success) {
    std::move(callback).Run(mojom::NO_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

}  // namespace ml
