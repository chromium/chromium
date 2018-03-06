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

MPSImageDescriptor* API_AVAILABLE(macosx(10.13)) CreateMPSImageDescriptor(const Operand& operand) {
  int32_t type = operand.type;
  const std::vector<uint32_t>& dimensions = operand.dimensions;
  MPSImageDescriptor* mpsimage_desc = nullptr;
  if (type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "type " << type << " is not supported";
    return mpsimage_desc;
  }
  uint32_t n, width, height, channels;
  if (dimensions.size() == 4) {
    n = dimensions[0];
    height = dimensions[1];
    width = dimensions[2];
    channels = dimensions[3];
  } else if (dimensions.size() == 2) {
    n = dimensions[0];
    channels = dimensions[1];
    height = 1;
    width = 1;
  } else {
    DLOG(ERROR) << "dimension " << dimensions.size() << " is not supported";
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

  for (size_t i = 0; i < compilation_->inputs_.size(); ++i) {
    DLOG(INFO) << "inputs[" << i << "]:";
    Operand& operand = compilation_->operands_[compilation_->inputs_[i]];
    std::unique_ptr<OperandInfo>& info = inputs_info_[i];
    PrintOperand(operand, info);
  }
  for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    DLOG(INFO) << "outputs[" << i << "]: length " << info->length;
    memset(static_cast<void*>(info->mapping.get()), 1, info->length);
  }
  for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
    DLOG(INFO) << "outputs[" << i << "]:";
    Operand& operand = compilation_->operands_[compilation_->outputs_[i]];
    std::unique_ptr<OperandInfo>& info = outputs_info_[i];
    PrintOperand(operand, info);
  }

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
      uint32_t input_idx = compilation_->inputs_[0];
      uint32_t output_idx = compilation_->outputs_[0];
      const Operand& input = compilation_->operands_[input_idx];
      const Operand& output = compilation_->operands_[output_idx];
      MPSImage* input_img = [[MPSImage alloc]
          initWithDevice:GetMPSCNNContext().device
          imageDescriptor:CreateMPSImageDescriptor(input)];
      MPSImage* output_img = [[MPSImage alloc]
          initWithDevice:GetMPSCNNContext().device
          imageDescriptor:CreateMPSImageDescriptor(output)];
      for (size_t i = 0; i < compilation_->operations_.size(); i++) {
        const OperationMac& operation = compilation_->operations_[i];
        MPSImage* src = nullptr;
        MPSImage* dst = nullptr;
        uint32_t operation_input_idx = operation.inputs[0];
        const Operand& operation_input = compilation_->operands_[operation_input_idx];
        uint32_t operation_output_idx = operation.outputs[0];
        const Operand& operation_output = compilation_->operands_[operation_output_idx];
        if (operation_input_idx == input_idx) {
          src = input_img;
        }
        if (operation_output_idx == output_idx) {
          dst = output_img;
        }
        if (src == nullptr) {
          src = [MPSTemporaryImage
              temporaryImageWithCommandBuffer:command_buffer
              imageDescriptor:CreateMPSImageDescriptor(operation_input)];
        }
        if (dst == nullptr) {
          dst = [MPSTemporaryImage
              temporaryImageWithCommandBuffer:command_buffer
              imageDescriptor:CreateMPSImageDescriptor(operation_output)];
        }
        MPSCNNKernel* kernel = operation.mpscnn_kernel.get();
        if (kernel) {
          [kernel encodeToCommandBuffer:command_buffer
              sourceImage:src
              destinationImage:dst];
          DLOG(INFO) << "Encode operation " << i << " with kernel " << 
              kernel << " src " << operation_input_idx << " sourceImage " << src <<
              " dst " << operation_output_idx << " destinationImage " << dst;
        } else {
          // TODO: handle null kernel
          DLOG(INFO) << "Null operation " << i << " with kernel " << 
              kernel << " src " << operation_input_idx << " sourceImage " << src <<
              " dst " << operation_output_idx << " destinationImage " << dst;
        }
      }

      [command_buffer commit];
      [command_buffer waitUntilCompleted];
    } while(0);
  }

  if (success) {
    std::move(callback).Run(mojom::NO_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

}  // namespace ml
