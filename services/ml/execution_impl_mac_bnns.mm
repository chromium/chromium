// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mac_bnns.h"

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

namespace {
vImage_Error GetResamplingFilter(const float scale,
                                 const float kernel_width,
                                 const ml::OperationMac& operation,
                                 ResamplingFilter& resampling_filter) {
  resampling_filter = malloc(vImageGetResamplingFilterSize(
      scale, operation.kernelFunc, kernel_width, kvImageEdgeExtend));
  vImage_Error error = vImageNewResamplingFilterForFunctionUsingBuffer(
      resampling_filter, scale, operation.kernelFunc, kernel_width, nullptr,
      kvImageEdgeExtend);
  return error;
}

bool TransposeForInput(const ml::OperationMac& operation,
                       const ml::OperandMac& operation_input,
                       float*& src,
                       float*& raw_input,
                       bool is_outer_input) {
  const int32_t input_batch = operation_input.dimensions[0];
  const int32_t ori_input_height = operation_input.dimensions[1];
  const int32_t input_height =
      operation_input.dimensions[1] + operation.offset_y;
  const int32_t ori_input_width = operation_input.dimensions[2];
  const int32_t input_width =
      operation_input.dimensions[2] + operation.offset_x;
  const int32_t ori_input_depth = operation_input.dimensions[3];
  const int32_t input_depth = operation_input.dimensions[3];
  const int32_t ori_input_row_stride = ori_input_width;
  const int32_t input_row_stride = input_width;
  const int32_t ori_input_image_stride = ori_input_width * ori_input_height;
  const int32_t input_image_stride = input_width * input_height;
  float* bnns_input = (float*)malloc(sizeof(float) * input_batch *
                                     input_image_stride * input_depth);
  if (bnns_input == nullptr) {
    DLOG(ERROR) << "Fail to alloc memory!";
    return false;
  }

  for (int b = 0; b < input_batch; b++) {
    for (int h = 0; h < input_height; h++) {
      for (int w = 0; w < input_width; w++) {
        for (int d = 0; d < input_depth; d++) {
          int new_batch_offset = b * input_height * input_width * input_depth;
          int new_index = new_batch_offset + w + h * input_row_stride +
                          d * input_image_stride;
          if (h >= ori_input_height || w >= ori_input_width) {
            *(bnns_input + new_index) = 0;
          } else {
            int ori_batch_offset =
                b * ori_input_height * ori_input_width * ori_input_depth;
            int ori_index = ori_batch_offset + w + h * ori_input_row_stride +
                            d * ori_input_image_stride;
            if (is_outer_input) {
              ori_index = ori_batch_offset +
                          h * ori_input_width * ori_input_depth +
                          w * ori_input_depth + d;
            }
            *(bnns_input + new_index) = *(raw_input + ori_index);
          }
        }
      }
    }
  }
  src = bnns_input;
  return true;
}
}

ExecutionImplMacBNNS::ExecutionImplMacBNNS(
    base::WeakPtr<CompilationImplMac> compilation,
    mojo::ScopedSharedBufferHandle memory) {
  compilation_ = compilation;
  uint32_t mapped_length = 0;
  SetupOperandInfoForOperands(inputs_info_, compilation_->operands_,
                              compilation_->inputs_, memory, mapped_length);
  SetupOperandInfoForOperands(outputs_info_, compilation_->operands_,
                              compilation_->outputs_, memory, mapped_length);
  PrepareBnnsOperandsMemory();
}

ExecutionImplMacBNNS::~ExecutionImplMacBNNS() {
  for (auto iter = bnns_operands_memory_map_.begin();
       iter != bnns_operands_memory_map_.end(); iter++) {
    float* ptr = iter->second;
    if (ptr != nullptr) {
      free(iter->second);
    }
  }
}

bool ExecutionImplMacBNNS::IsValid() const {
  return compilation_ != nil &&
         inputs_info_.size() == compilation_->inputs_.size() &&
         outputs_info_.size() == compilation_->outputs_.size() &&
         bnns_operands_memory_map_.size() == compilation_->operands_.size();
}

bool ExecutionImplMacBNNS::PrepareBnnsOperandsMemory() {
  // std::map<size_t,float*> _bnns_operands_memory_map
  for (size_t i = 0; i < compilation_->operands_.size(); i++) {
    OperandMac& operand = compilation_->operands_[i];
    bnns_operands_memory_map_[i] = (float*)malloc(operand.requiredSize());
    if (bnns_operands_memory_map_[i] == nullptr) {
      return false;
    }
  }
  return true;
}

void ExecutionImplMacBNNS::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMacBNNS::StartCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        if (compilation_ != nullptr) {
          for (size_t i = 0; i < compilation_->operations_.size(); i++) {
            const OperationMac& operation = compilation_->operations_[i];
            bool is_outer_input = false;
            uint32_t operation_input_idx = operation.inputs[0];
            OperandMac& operation_input =
                compilation_->operands_[operation_input_idx];

            std::vector<float*> src(operation.inputs.size(), nullptr);
            std::vector<float*> temp_src(operation.inputs.size(), nullptr);
            float* des = nullptr;

            // get src
            bool tmp_src_malloc = false;
            bool need_offset_transform = false;
            if (operation.offset_x > 0 || operation.offset_y > 0) {
              need_offset_transform = true;
            }

            for (size_t i = 0; i < operation.inputs.size(); ++i) {
              size_t input_flag = 0;
              const uint32_t input_idx = operation.inputs[i];
              for (size_t j = 0; j < compilation_->inputs_.size(); j++) {
                if (input_idx == compilation_->inputs_[j]) {
                  is_outer_input = true;
                  operation_input_idx = input_idx;
                  operation_input = compilation_->operands_[input_idx];
                  input_flag = j;
                  break;
                }
              }

              // For bnns backend, the dimension must satisfy:
              // in_width + 2 * x_padding = x_stride * (out_width - 1) +
              // k_width; in_height + 2 * y_padding = y_stride * (out_height -
              // 1) + k_height; But webml doesn't hava these restricts, so I
              // need to add some 0 values for input image to adapt to bnns
              if (is_outer_input == true || need_offset_transform) {
                float* raw_input = nullptr;
                if (is_outer_input == false) {
                  raw_input = bnns_operands_memory_map_[operation_input_idx];
                } else {
                  raw_input = (float*)inputs_info_[input_flag]->mapping.get();
                }

                if (operation_input.dimensions.size() == 4) {
                  if (!TransposeForInput(operation, operation_input,
                                         temp_src[i], raw_input,
                                         is_outer_input)) {
                    success = false;
                  }
                  src[i] = temp_src[i];
                  tmp_src_malloc = true;
                  need_offset_transform = false;
                } else {
                  src[i] = raw_input;
                }
                is_outer_input = false;
              } else {
                if (operation.local_operation == KAdd ||
                    operation.local_operation == KMul ||
                    operation.local_operation == KConcatenation || i == 0) {
                  src[i] = bnns_operands_memory_map_[operation_input_idx];
                }
              }
            }

            bool is_outer_output = false;
            size_t output_flag = 0;
            uint32_t operation_output_idx = operation.outputs[0];
            OperandMac& operation_output =
                compilation_->operands_[operation_output_idx];
            for (size_t i = 0; i < operation.outputs.size(); ++i) {
              const uint32_t output_idx = operation.outputs[0];
              for (size_t j = 0; j < compilation_->outputs_.size(); j++) {
                if (operation_output_idx == compilation_->outputs_[j]) {
                  is_outer_output = true;
                  operation_output_idx = output_idx;
                  operation_output = compilation_->operands_[output_idx];
                  output_flag = j;
                  break;
                }
              }

              if (is_outer_output == true) {
                break;
              }
            }

            // get des
            if (is_outer_output) {
              des = (float*)outputs_info_[output_flag]->mapping.get();
            } else {
              des = bnns_operands_memory_map_[operation_output_idx];
            }

            if (operation.local_operation == KBNNSFilter) {
              const int32_t input_batch_size = operation.input_batch_size;
              int result;
              if (input_batch_size == 1) {
                result = BNNSFilterApply(operation.filter, src[0], des);
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
                                         src[0], in_stride, des, out_stride);
              }
              if (result == -1) {
                success = false;
                DLOG(ERROR) << "Fail to apply a filter";
              }
            } else if (operation.local_operation == KReshape) {
              size_t input_size = operation_input.requiredSize() / 4;
              for (size_t j = 0; j < input_size; j++) {
                des[j] = src[0][j];
              }
            } else if (operation.local_operation == KConcatenation) {
              int32_t batch_offset_sum = 0;
              for (size_t index = 0; index < operation.inputs.size() - 1;
                   ++index) {
                uint32_t concat_input_idx = operation.inputs[index];
                OperandMac& operand = compilation_->operands_[concat_input_idx];
                const int32_t batch = operand.dimensions[0];
                const int32_t width = operand.dimensions[1];
                const int32_t height = operand.dimensions[2];
                const int32_t channels = operand.dimensions[3];
                const int32_t channel_offset = width * height;
                const int32_t batch_offset = width * height * channels;

                for (int b = 0; b < batch; b++) {
                  for (int c = 0; c < channels; c++) {
                    float* temp_des = des + c * channel_offset +
                                      b * batch_offset * batch +
                                      batch_offset_sum;
                    float* temp_src =
                        src[index] + c * channel_offset + b * batch_offset;
                    memcpy(temp_des, temp_src, channel_offset * sizeof(float));
                  }
                }
                batch_offset_sum += batch_offset;
              }
            } else if (operation.local_operation == KAdd ||
                       operation.local_operation == KMul) {
              float* input_a_values = src[0];
              float* input_b_values = src[1];
              const OperandMac& output =
                  compilation_->operands_[operation.outputs[0]];
              int32_t output_length = product(output.dimensions);
              float* output_vector =
                  (float*)malloc(output_length * sizeof(float));
              if (output_vector == nullptr) {
                DLOG(ERROR) << "Fail to alloc memory!";
                success = false;
              }
              if (operation.local_operation == KAdd) {
                vDSP_vadd(input_a_values, 1, input_b_values, 1, output_vector,
                          1, output_length);
              } else {
                vDSP_vmul(input_a_values, 1, input_b_values, 1, output_vector,
                          1, output_length);
              }

              if (operation.filter != nullptr) {
                int result =
                    BNNSFilterApply(operation.filter, output_vector, des);
                if (result == -1) {
                  success = false;
                  DLOG(ERROR) << "Fail to apply a activity function after add!";
                }
              } else {
                memcpy(des, output_vector, output_length);
              }
              free(output_vector);
            } else if (operation.local_operation == KResize) {
              const OperandMac& input =
                  compilation_->operands_[operation.inputs[0]];
              const int32_t input_batch = input.dimensions[0];
              const int32_t input_width = input.dimensions[1];
              const int32_t input_height = input.dimensions[2];
              const int32_t input_depth = input.dimensions[3];
              vImage_Buffer source_buffer;
              source_buffer.data = src[0];
              source_buffer.height = input_height;
              source_buffer.width = input_width;
              source_buffer.rowBytes = input_width * sizeof(float);

              const OperandMac& output =
                  compilation_->operands_[operation.outputs[0]];
              const int32_t output_batch = output.dimensions[0];
              const int32_t output_width = output.dimensions[1];
              const int32_t output_height = output.dimensions[2];
              const int32_t output_depth = output.dimensions[3];

              float kernel_width = 1;
              float scale = float(output_height * output_depth * output_batch) /
                            (input_height * input_depth * input_batch);
              ResamplingFilter resampling_filter_verticle;
              vImage_Error error;
              error = GetResamplingFilter(scale, kernel_width, operation,
                                          resampling_filter_verticle);
              if (error != kvImageNoError) {
                success = false;
                DLOG(ERROR) << "Fail to new resampling filter for function!";
              }
              vImage_Buffer intermediate_buffer;
              intermediate_buffer.data =
                  (float*)malloc(output_height * output_depth * output_batch *
                                 input_width * sizeof(float));
              intermediate_buffer.height = output_height;
              intermediate_buffer.width = input_width;
              intermediate_buffer.rowBytes = input_width * sizeof(float);

              float yTranslate =
                  -(output_height * output_depth * output_batch) /
                  (scale * scale);
              vImageVerticalShear_PlanarF(
                  &source_buffer, &intermediate_buffer, 0, 0, yTranslate, 0,
                  resampling_filter_verticle, 0, kvImageNoFlags);
              if (error != kvImageNoError) {
                success = false;
                DLOG(ERROR) << "Fail to new vertical shear!";
              }

              vImage_Buffer des_buffer;
              des_buffer.data = des;
              des_buffer.height = output_height;
              des_buffer.width = output_width;
              des_buffer.rowBytes = output_width * sizeof(float);

              scale = (float)output_width / input_width;
              ResamplingFilter resampling_filter_horizontal;
              error = GetResamplingFilter(scale, kernel_width, operation,
                                          resampling_filter_horizontal);
              if (error != kvImageNoError) {
                success = false;
                DLOG(ERROR) << "Fail to new resampling filter for function!";
              }

              float xTranslate = input_width - output_width;
              error = vImageHorizontalShear_PlanarF(
                  &intermediate_buffer, &des_buffer, 0, 0, xTranslate, 0,
                  resampling_filter_horizontal, 0, kvImageNoFlags);
              if (error != kvImageNoError) {
                success = false;
                DLOG(ERROR) << "Fail to new horizontal shear!";
              }

              free(intermediate_buffer.data);
              free(resampling_filter_horizontal);
              free(resampling_filter_verticle);
            }

            if (tmp_src_malloc) {
              for (auto it = temp_src.begin(); it != temp_src.end(); it++) {
                if (*it != nullptr) {
                  delete *it;
                }
              }
              temp_src.clear();
            }

            if (is_outer_output && operation_output.dimensions.size() == 4) {
              const int32_t batch = operation_output.dimensions[0];
              const int32_t width = operation_output.dimensions[1];
              const int32_t height = operation_output.dimensions[2];
              const int32_t channels = operation_output.dimensions[3];
             
              memcpy(bnns_operands_memory_map_[operation_output_idx], des,
                     batch * width * height * channels * sizeof(float));
              for (int b = 0; b < batch; b++) {
                for (int h = 0; h < height; h++) {
                  for (int w = 0; w < width; w++) {
                    for (int c = 0; c < channels; c++) {
                      const int new_batch_offset =
                          b * height * width * channels;
                      const int bnns_index =
                          new_batch_offset + w + h * width + c * width * height;
                      const int ori_index = new_batch_offset +
                                            h * width * channels +
                                            w * channels + c;
                      *(des + ori_index) = *(bnns_operands_memory_map_[operation_output_idx] + bnns_index);
                    }
                  }
                }
              }
            }
          }
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

}  // namespace ml
