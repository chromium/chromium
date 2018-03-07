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

bool GetMPSImageInfo(const Operand& operand, uint32_t& n, uint32_t& width, uint32_t& height, uint32_t& channels) {
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

MPSImageDescriptor* API_AVAILABLE(macosx(10.13)) CreateMPSImageDescriptor(const Operand& operand) {
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
  for (size_t i = 0; i < compilation_->inputs_.size(); ++i) {
    Operand& operand = compilation_->operands_[compilation_->inputs_[i]];
    uint32_t offset = total_length;
    uint32_t length = operand.requiredSize();
    mojo::ScopedSharedBufferMapping mapping = memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(new OperandInfo(offset, length, std::move(mapping)));
    inputs_info_.push_back(std::move(info));
    total_length += length;
  }
  for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
    Operand& operand = compilation_->operands_[compilation_->outputs_[i]];
    uint32_t offset = total_length;
    uint32_t length = operand.requiredSize();
    mojo::ScopedSharedBufferMapping mapping = memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(new OperandInfo(offset, length, std::move(mapping)));
    outputs_info_.push_back(std::move(info));
    total_length += length;
  }
}

ExecutionImplMac::~ExecutionImplMac() {}

void ExecutionImplMac::startCompute(startComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMac::startCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      base::mac::ScopedNSAutoreleasePool scoped_pool;
      id<MTLCommandBuffer> command_buffer = [GetMPSCNNContext().command_queue commandBuffer];

      if (compilation_->inputs_.size() > 1 || compilation_->outputs_.size() > 1) {
        DLOG(ERROR) << "Only input size and output size 1 is supported";
        success = false;
        break;
      }

      std::map<uint32_t, MPSImage*> mpsimage_cache;
      uint32_t input_idx = compilation_->inputs_[0];
      uint32_t output_idx = compilation_->outputs_[0];
      const Operand& input = compilation_->operands_[input_idx];
      const Operand& output = compilation_->operands_[output_idx];
      MPSImage* input_img = [[MPSImage alloc]
          initWithDevice:GetMPSCNNContext().device
          imageDescriptor:CreateMPSImageDescriptor(input)];
      DLOG(INFO) << "Create MPSImage for input " << input_idx << " " << input_img;
      mpsimage_cache[input_idx] = input_img;
      MPSImage* output_img = [[MPSImage alloc]
          initWithDevice:GetMPSCNNContext().device
          imageDescriptor:CreateMPSImageDescriptor(output)];
      DLOG(INFO) << "Create MPSImage for output " << output_idx << " " << output_img;
      mpsimage_cache[output_idx] = output_img;

      if (inputs_info_.size() > 1) {
        DLOG(ERROR) << "Input size " << inputs_info_.size() << " is not supported";
        break;
      }
      uint32_t input_n, input_width, input_height, input_channels;
      if (!GetMPSImageInfo(input, input_n, input_width, input_height, input_channels)) {
        DLOG(ERROR) << "Input shape is not supported";
        break;
      }
      std::unique_ptr<OperandInfo>& input_data = inputs_info_[0];
      id<MTLBuffer> input_buffer = [GetMPSCNNContext().device
          newBufferWithLength:input_data->length
          options:MTLResourceOptionCPUCacheModeWriteCombined];
      DLOG(INFO) << "Copy data to input buffer with length " << input_data->length;
      PrintOperand(input, input_data);
      memcpy([input_buffer contents], input_data->mapping.get(), input_data->length);
      
      {
        id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];
        id<MTLComputePipelineState> state =
            GetMPSCNNContext().GetSpecializedPipelineState(
                KernelFor(input_img, @"copy_nhwc_to_metal", @"copy_nhwc_to_metal_nonarray"),
                {{ushort(input_height), ushort(input_width), ushort(input_channels)}});
        [encoder setComputePipelineState:state];
        [encoder setBuffer:input_buffer offset:0 atIndex:0];
        [encoder setTexture:[input_img texture] atIndex:0];
        const auto& inputLaunchParams =
            SpatialPointwiseKernelLaunchParams(state, input_img);
        [encoder dispatchThreadgroups:inputLaunchParams.threadgroupsPerGrid
                threadsPerThreadgroup:inputLaunchParams.threadsPerThreadgroup];
        [encoder endEncoding];
      }

      for (size_t i = 0; i < compilation_->operations_.size(); i++) {
        const OperationMac& operation = compilation_->operations_[i];
        MPSCNNKernel* kernel = operation.mpscnn_kernel.get();
        if (!kernel) {
          DLOG(INFO) << "No kernel compiled for operation " << i << " type " << operation.type;
          continue;
        }
        uint32_t operation_input_idx = operation.inputs[0];
        const Operand& operation_input = compilation_->operands_[operation_input_idx];
        uint32_t operation_output_idx = operation.outputs[0];
        const Operand& operation_output = compilation_->operands_[operation_output_idx];
        if (mpsimage_cache.find(operation_input_idx) == mpsimage_cache.end()) {
          mpsimage_cache[operation_input_idx] = [MPSTemporaryImage
              temporaryImageWithCommandBuffer:command_buffer
              imageDescriptor:CreateMPSImageDescriptor(operation_input)];
        }
        if (mpsimage_cache.find(operation_output_idx) == mpsimage_cache.end()) {
          mpsimage_cache[operation_output_idx] = [MPSTemporaryImage
              temporaryImageWithCommandBuffer:command_buffer
              imageDescriptor:CreateMPSImageDescriptor(operation_output)];
        }
        MPSImage* src_img = mpsimage_cache[operation_input_idx];
        MPSImage* dst_img = mpsimage_cache[operation_output_idx];
        [kernel encodeToCommandBuffer:command_buffer
            sourceImage:src_img
            destinationImage:dst_img];
        DLOG(INFO) << "Encode operation " << i << " with kernel " << 
            kernel << " src " << operation_input_idx << " sourceImage " << src_img <<
            " dst " << operation_output_idx << " destinationImage " << dst_img;
      }
      
      if (outputs_info_.size() > 1) {
        DLOG(ERROR) << "Output size " << outputs_info_.size() << " is not supported";
        break;
      }
      uint32_t output_n, output_width, output_height, output_channels;
      if (!GetMPSImageInfo(output, output_n, output_width, output_height, output_channels)) {
        DLOG(ERROR) << "Output shape is not supported";
        break;
      }
      std::unique_ptr<OperandInfo>& output_data = outputs_info_[0];
      id<MTLBuffer> output_buffer = [GetMPSCNNContext().device
          newBufferWithLength:output_data->length
          options:MTLResourceOptionCPUCacheModeWriteCombined];

      {
        id<MTLComputeCommandEncoder> encoder = [command_buffer computeCommandEncoder];
        id<MTLComputePipelineState> state = GetMPSCNNContext().GetSpecializedPipelineState(
            KernelFor(output_img, @"copy_metal_to_nhwc", @"copy_metal_to_nhwc_nonarray"),
            {{ushort(output_height), ushort(output_width), ushort(output_channels)}});
              
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

      DLOG(INFO) << "Copy memory back from output buffer with length " << output_buffer.length;
      memcpy(output_data->mapping.get(), [output_buffer contents], output_buffer.length);
      PrintOperand(output, output_data);
    } while(0);
  }

  if (success) {
    std::move(callback).Run(mojom::NO_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

}  // namespace ml
