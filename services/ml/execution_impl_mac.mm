// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mac.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/ml/mpscnn_context.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

namespace ml {

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

bool GetMPSImageInfo(const OperandMac& operand,
                     uint32_t& n,
                     uint32_t& width,
                     uint32_t& height,
                     uint32_t& channels) {
  const std::vector<uint32_t>& dimensions = operand.dimensions;
  if (dimensions.size() == 4) {
    n = dimensions[0];
    height = dimensions[1];
    width = dimensions[2];
    channels = dimensions[3];
    return true;
  } else if (dimensions.size() == 3) {
    n = 1;
    height = dimensions[0];
    width = dimensions[1];
    channels = dimensions[2];
    return true;
  } else if (dimensions.size() == 2) {
    n = 1;
    height = 1;
    width = dimensions[0];
    channels = dimensions[1];
    return true;
  } else if (dimensions.size() == 1) {
    n = 1;
    height = 1;
    width = 1;
    channels = dimensions[0];
    return true;
  } else {
    DLOG(ERROR) << "dimension " << dimensions.size() << " is not supported";
    return false;
  }
}

MPSImageDescriptor* API_AVAILABLE(macosx(10.13))
    CreateMPSImageDescriptor(const OperandMac& operand) {
  int32_t type = operand.type;
  MPSImageDescriptor* mpsimage_desc = nullptr;
  if (type != mojom::TENSOR_FLOAT32) {
    LOG(ERROR) << "type " << type << " is not supported";
    return mpsimage_desc;
  }
  uint32_t n, width, height, channels;
  if (!GetMPSImageInfo(operand, n, width, height, channels)) {
    return mpsimage_desc;
  }
  if (n != 1) {
    LOG(ERROR) << "number of images " << n << " is not supported";
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

ExecutionImplMac::ExecutionImplMac(CompilationImplMac* compilation,
                                   mojo::ScopedSharedBufferHandle memory) {
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
    uint32_t length = operand.requiredSize();
    if (@available(macOS 10.13, *)) {
      if (compilation_->is_bnns_ == false) {
        MPSImageDescriptor* descriptor = CreateMPSImageDescriptor(operand);
        if (!descriptor) {
          return;
        }
        MPSImage* mps_img =
            [[MPSImage alloc] initWithDevice:GetMPSCNNContext().device
                             imageDescriptor:descriptor];
        input_mpsimages_[i].reset(mps_img);
        input_mtlbuffers_[i] = [GetMPSCNNContext().device
            newBufferWithLength:length
                        options:MTLResourceOptionCPUCacheModeWriteCombined];
      }
    }
    uint32_t offset = total_length;
    mojo::ScopedSharedBufferMapping mapping =
        memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(
        new OperandInfo(offset, length, std::move(mapping)));
    inputs_info_.push_back(std::move(info));
    total_length += length;
  }
  uint32_t outputs_size = compilation_->outputs_.size();
  if (@available(macOS 10.13, *)) {
    if (compilation_->is_bnns_ == false) {
      output_mpsimages_.resize(outputs_size);
      output_mtlbuffers_.resize(outputs_size);
    }
  }
  for (size_t i = 0; i < outputs_size; ++i) {
    OperandMac& operand = compilation_->operands_[compilation_->outputs_[i]];
    uint32_t length = operand.requiredSize();
    if (@available(macOS 10.13, *)) {
      if (compilation_->is_bnns_ == false) {
        MPSImageDescriptor* descriptor = CreateMPSImageDescriptor(operand);
        if (!descriptor) {
          return;
        }
        MPSImage* mps_img =
            [[MPSImage alloc] initWithDevice:GetMPSCNNContext().device
                             imageDescriptor:descriptor];
        output_mpsimages_[i].reset(mps_img);
        output_mtlbuffers_[i] = [GetMPSCNNContext().device
            newBufferWithLength:length
                        options:MTLResourceOptionCPUCacheModeWriteCombined];
      }
    }
    uint32_t offset = total_length;
    mojo::ScopedSharedBufferMapping mapping =
        memory_->MapAtOffset(length, offset);
    std::unique_ptr<OperandInfo> info(
        new OperandInfo(offset, length, std::move(mapping)));
    outputs_info_.push_back(std::move(info));
    total_length += length;
  }
  // prepare memory for other operands
  if (compilation_->is_bnns_) {
    PrepareBnnsOperandsMemory();
  }
}

ExecutionImplMac::~ExecutionImplMac() {
  if (compilation_->is_bnns_) {
    for (auto iter = bnns_operands_memory_map_.begin();
         iter != bnns_operands_memory_map_.end(); iter++) {
      float* ptr = iter->second;
      if (ptr != nullptr) {
        free(iter->second);
      }
    }
    for (size_t i = 0; i < compilation_->operations_.size(); i++) {
      const OperationMac& operation = compilation_->operations_[i];
      if (operation.local_operation == KBNNSFilter) {
        if (@available(macOS 10.12, *)) {
          BNNSFilterDestroy(operation.filter);
        }
      }
    }
  }
}

void ExecutionImplMac::PrepareBnnsOperandsMemory() {
  // std::map<size_t,float*> _bnns_operands_memory_map
  for (size_t i = 0; i < compilation_->operands_.size(); i++) {
    bool is_input = false;
    for (size_t j = 0; j < compilation_->inputs_.size(); j++) {
      if (compilation_->inputs_[j] == i) {
        is_input = true;
      }
    }
    bool is_output = false;
    for (size_t j = 0; j < compilation_->outputs_.size(); j++) {
      if (compilation_->inputs_[j] == i) {
        is_output = true;
      }
    }
    if (is_input == true || is_output == true) {
      continue;
    }
    OperandMac& operand = compilation_->operands_[i];
    int length = operand.requiredSize();
    bnns_operands_memory_map_[i] = (float*)malloc(length);
  }
}

void ExecutionImplMac::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMac::StartCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        if (compilation_->inputs_.size() > 1) {
          success = false;
          break;
        }

        if (compilation_->is_bnns_) {
          for (size_t i = 0; i < compilation_->operations_.size(); i++) {
            float* src = nullptr;
            float* des = nullptr;
            const OperationMac& operation = compilation_->operations_[i];
            const uint32_t operation_input_idx = operation.inputs[0];
            const uint32_t operation_output_idx = operation.outputs[0];
            const OperandMac& operation_input =
                compilation_->operands_[operation_input_idx];
            const OperandMac& operation_output =
                compilation_->operands_[operation_output_idx];

            bool is_outer_input = false;
            for (size_t j = 0; j < compilation_->inputs_.size(); j++) {
              if (operation_input_idx == compilation_->inputs_[j]) {
                is_outer_input = true;
                break;
              }
            }

            bool is_outer_output = false;
            for (size_t j = 0; j < compilation_->outputs_.size(); j++) {
              if (operation_output_idx == compilation_->outputs_[j]) {
                is_outer_output = true;
                break;
              }
            }

            // get src
            bool tmp_src_malloc = false;
            bool need_offset_transform = false;
            if (operation.offset_x > 0 || operation.offset_y > 0) {
              need_offset_transform = true;
            }
            // For bnns backend, the dimension must satisfy:
            // in_width + 2 * x_padding = x_stride * (out_width - 1) + k_width;
            // in_height + 2 * y_padding = y_stride * (out_height - 1) + k_height;
            // But webml doesn't hava these restricts, so I need to add some 0 values
            // for input image to adapt to bnns
            if (is_outer_input || need_offset_transform) {
              float* raw_input = (float*)inputs_info_[0]->mapping.get();
              if (is_outer_input == false) {
                raw_input = bnns_operands_memory_map_[operation_input_idx];
              }
              if (operation.local_operation == KBNNSFilter &&
                  operation_input.dimensions.size() == 4) {
                int32_t input_batch = operation_input.dimensions[0];
                int32_t ori_input_height = operation_input.dimensions[1];
                int32_t input_height =
                    operation_input.dimensions[1] + operation.offset_y;
                int32_t ori_input_width = operation_input.dimensions[2];
                int32_t input_width =
                    operation_input.dimensions[2] + operation.offset_x;
                int32_t ori_input_depth = operation_input.dimensions[3];
                int32_t input_depth = operation_input.dimensions[3];
                int32_t ori_input_row_stride = ori_input_width;
                int32_t input_row_stride = input_width;
                int32_t ori_input_image_stride =
                    ori_input_width * ori_input_height;
                int32_t input_image_stride = input_width * input_height;

                float* bnns_input = (float*)malloc(sizeof(float) * input_batch *
                    input_image_stride * input_depth);
                tmp_src_malloc = true;

                for (int b = 0; b < input_batch; b++) {
                  for (int h = 0; h < input_height; h++) {
                    for (int w = 0; w < input_width; w++) {
                      for (int d = 0; d < input_depth; d++) {
                        int new_batch_offset =
                            b * input_height * input_width * input_depth;
                        int new_index = new_batch_offset + w +
                            h * input_row_stride + d * input_image_stride;
                        if (h >= ori_input_height or w >= ori_input_width) {
                          *(bnns_input + new_index) = 0;
                        } else {
                          int ori_batch_offset = b * ori_input_height *
                              ori_input_width * ori_input_depth;
                          int ori_index = ori_batch_offset + w +
                              h * ori_input_row_stride +
                              d * ori_input_image_stride;
                          if (is_outer_input) {
                            ori_index = ori_batch_offset +
                                h * ori_input_width * ori_input_depth +
                                w * ori_input_depth + d;
                          }
                          *(bnns_input + new_index) = *(raw_input + ori_index);
                        }
                      } /* end for */
                    }   /* end for */
                  }     /* end for */
                }       /* end for */
                src = bnns_input;
              } else {
                src = raw_input;
              }
            } else {
              src = bnns_operands_memory_map_[operation_input_idx];
            }

            // get des
            if (is_outer_output) {
              des = (float*)outputs_info_[0]->mapping.get();
            } else {
              des = bnns_operands_memory_map_[operation_output_idx];
            }

            if (operation.local_operation == KBNNSFilter) {
              int32_t input_batch_size = operation_input.dimensions[0];
              int result;
              if (input_batch_size == 1) {
                result = BNNSFilterApply(operation.filter, src, des);
              } else {
                int in_stride = 1, out_stride = 1;
                for (size_t i = 1; i < operation_input.dimensions.size(); i++) {
                  in_stride = in_stride * operation_input.dimensions[i];
                }
                for (size_t i = 1; i < operation_output.dimensions.size();
                     i++) {
                  out_stride = out_stride * operation_output.dimensions[i];
                }
                result =
                    BNNSFilterApplyBatch(operation.filter, input_batch_size,
                                         src, in_stride, des, out_stride);
              }
              if (result == -1) {
                success = false;
                DLOG(ERROR) << "Fail to apply a filter";
              }
            } else if (operation.local_operation == KReshape) {
              size_t input_size = operation_input.requiredSize() / 4;
              for (size_t j = 0; j < input_size; j++) {
                des[j] = src[j];
              }
            } else if (operation.local_operation == KConcatenation) {
              for (size_t i = 0; i < operation.inputs.size() - 1; ++i) {
                uint32_t concat_input_idx = operation.inputs[i];
                OperandMac& operand = compilation_->operands_[concat_input_idx];
                float* src = bnns_operands_memory_map_[concat_input_idx];
                int32_t width = operand.dimensions[1];
                int32_t height = operand.dimensions[2];
                int32_t channels = operand.dimensions[3];
                int32_t channel_offset = width * height;
                for (int32_t c = 0; c < channels; c++) {
                  float* temp_des = des + c * channel_offset + i * channel_offset * channels;
                  float* temp_src = src + c * channel_offset;
                  memcpy(temp_des, temp_src, channel_offset * sizeof(float));
                }
              }
            }

            if (is_outer_input && src != nullptr && tmp_src_malloc) {
              free(src);
            }
          }
        } else {
          id<MTLCommandBuffer> command_buffer =
              [GetMPSCNNContext().command_queue commandBuffer];

          for (size_t i = 0; i < compilation_->inputs_.size(); ++i) {
            std::unique_ptr<OperandInfo>& input_data = inputs_info_[i];
            MPSImage* input_img = input_mpsimages_[i].get();
            id<MTLBuffer> input_buffer = input_mtlbuffers_[i];
            memcpy([input_buffer contents], input_data->mapping.get(),
                   input_data->length);
            id<MTLComputeCommandEncoder> encoder =
                [command_buffer computeCommandEncoder];
            id<MTLComputePipelineState> state =
                GetMPSCNNContext().GetSpecializedPipelineState(
                    KernelFor(input_img, @"copy_nhwc_to_metal",
                              @"copy_nhwc_to_metal_nonarray"),
                    {{ushort(input_img.height), ushort(input_img.width),
                      ushort(input_img.featureChannels)}});
            [encoder setComputePipelineState:state];
            [encoder setBuffer:input_buffer offset:0 atIndex:0];
            [encoder setTexture:[input_img texture] atIndex:0];
            const auto& inputLaunchParams =
                SpatialPointwiseKernelLaunchParams(state, input_img);
            [encoder
                 dispatchThreadgroups:inputLaunchParams.threadgroupsPerGrid
                threadsPerThreadgroup:inputLaunchParams.threadsPerThreadgroup];
            [encoder endEncoding];
          }

          for (size_t i = 0; i < compilation_->operations_.size(); i++) {
            const OperationMac& operation = compilation_->operations_[i];
            MPSCNNKernel* kernel = operation.mpscnn_kernel.get();
            MPSCNNBinaryKernel* binary_kernel = operation.mpscnn_binary_kernel.get();
            bool fuse_relu = operation.fuse_code == mojom::FUSED_RELU1
                || operation.fuse_code == mojom::FUSED_RELU6;
            if (!kernel && !binary_kernel) {
              DLOG(INFO) << "No kernel compiled for operation " << i << " type "
                        << operation.type;
              continue;
            }

            MPSImage* src_img = FindInputMPSImageByIndex(operation.inputs[0]);
            if (!src_img) {
              src_img = FindOrCreateMPSTemporaryImageByIndex(operation.inputs[0], command_buffer);
              if (!src_img) {
                success = false;
                break;
              }
            }

            MPSImage* secondary_src_img = nullptr;
            if (binary_kernel) {
              secondary_src_img = FindInputMPSImageByIndex(operation.inputs[1]);
              if (!secondary_src_img) {
                secondary_src_img = FindOrCreateMPSTemporaryImageByIndex(operation.inputs[1], command_buffer);
                if (!secondary_src_img) {
                  success = false;
                  break;
                }
              }
            }

            const uint32_t operation_output_idx = operation.outputs[0];
            const OperandMac& operation_output =
                compilation_->operands_[operation_output_idx];
            MPSImage* dst_img = FindOutputMPSImageByIndex(operation_output_idx);
            if (!dst_img) {
              dst_img = FindOrCreateMPSTemporaryImageByIndex(operation_output_idx, command_buffer);
              if (!dst_img) {
                success = false;
                break;
              }
            }
            if (fuse_relu) {
              // Insert relu layer
              MPSTemporaryImage* relu_input = [MPSTemporaryImage
                  temporaryImageWithCommandBuffer:command_buffer
                                  imageDescriptor:CreateMPSImageDescriptor(
                                                      operation_output)];
              [kernel encodeToCommandBuffer:command_buffer
                                sourceImage:src_img
                           destinationImage:relu_input];
              short threshold = 6;
              if (operation.fuse_code == mojom::FUSED_RELU1) {
                threshold = 1;
              }
              {
                id<MTLComputeCommandEncoder> encoder =
                    [command_buffer computeCommandEncoder];
                id<MTLComputePipelineState> state =
                    GetMPSCNNContext().GetSpecializedPipelineState(
                        KernelFor(dst_img, @"relu", @"relu_nonarray"),
                        {ushort(threshold)});
                [encoder setComputePipelineState:state];
                [encoder setTexture:[relu_input texture] atIndex:0];
                [encoder setTexture:[dst_img texture] atIndex:1];
                const auto& inputLaunchParams =
                    SpatialPointwiseKernelLaunchParams(state, relu_input);
                [encoder
                     dispatchThreadgroups:inputLaunchParams.threadgroupsPerGrid
                    threadsPerThreadgroup:inputLaunchParams
                                              .threadsPerThreadgroup];
                [encoder endEncoding];
              }
            } else if (binary_kernel) {
              [binary_kernel encodeToCommandBuffer:command_buffer
                                      primaryImage:src_img
                                    secondaryImage:secondary_src_img
                                  destinationImage:dst_img];
            } else if (kernel) {
              if (src_img.featureChannels == 3 && dst_img.featureChannels == 4) {
                DLOG(ERROR) << @"Number of source feature channels needed by "
                                "convolution 4 are not available in image with"
                                " 3 feature channels";
                success = false;
                break;
              }
              [kernel encodeToCommandBuffer:command_buffer
                                sourceImage:src_img
                           destinationImage:dst_img];
            }
          }

          if (!success)
            break;

          for (size_t i = 0; i < compilation_->outputs_.size(); ++i) {
            MPSImage* output_img = output_mpsimages_[i];
            id<MTLBuffer> output_buffer = output_mtlbuffers_[i];

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
            id<MTLBuffer> output_buffer = output_mtlbuffers_[i];
            // LOG(INFO) << "Copy memory back from output buffer with length "
            // << output_buffer.length;
            memcpy(output_data->mapping.get(), [output_buffer contents],
                   output_data->length);
            // OperandMac& operand =
            // compilation_->operands_[compilation_->outputs_[i]];
            // PrintOperand(operand, output_data);
          }

          tmp_mpsimage_cache_.clear();
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

MPSImage* API_AVAILABLE(macosx(10.13)) FindMPSImageByIndex(uint32_t index, std::vector<uint32_t>& index_array, std::vector<base::scoped_nsobject<MPSImage> >& image_array) {
  MPSImage* image = nullptr;
  if (@available(macOS 10.13, *)) {
    for (size_t i = 0; i < index_array.size(); ++i) {
      const uint32_t index_in_array = index_array[i];
      if (index == index_in_array) {
        image = image_array[i];
        break;
      }
    }
  }
  return image;
}

MPSImage* ExecutionImplMac::FindInputMPSImageByIndex(uint32_t index) {
  return FindMPSImageByIndex(index, compilation_->inputs_, input_mpsimages_);
}

MPSImage* ExecutionImplMac::FindOutputMPSImageByIndex(uint32_t index) {
  return FindMPSImageByIndex(index, compilation_->outputs_, output_mpsimages_);
}

MPSTemporaryImage* ExecutionImplMac::FindOrCreateMPSTemporaryImageByIndex(uint32_t index, id<MTLCommandBuffer>& command_buffer) {
  MPSTemporaryImage* temp_image = nullptr;
  if (@available(macOS 10.13, *)) {
    const OperandMac& operand = compilation_->operands_[index];
    if (tmp_mpsimage_cache_.find(index) == tmp_mpsimage_cache_.end()) {
      MPSImageDescriptor* descriptor = CreateMPSImageDescriptor(operand);
      if (!descriptor) {
        return nullptr;
      }
      temp_image = [MPSTemporaryImage
          temporaryImageWithCommandBuffer:command_buffer
                          imageDescriptor:descriptor];
      LOG(INFO) << "Set readCount as " << operand.read_count;
      temp_image.readCount = operand.read_count;
      tmp_mpsimage_cache_[index] = temp_image;
    }
    temp_image = tmp_mpsimage_cache_[index];
  }
  return temp_image;
}

}  // namespace ml

