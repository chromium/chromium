// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_mac_bnns.h"

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/public/interfaces/constants.mojom.h"

namespace ml {

namespace {

void TransposeForInput(const ml::OperationMac& operation,
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
         outputs_info_.size() == compilation_->outputs_.size();
}

void ExecutionImplMacBNNS::PrepareBnnsOperandsMemory() {
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
    bnns_operands_memory_map_[i] = (float*)malloc(operand.requiredSize());
  }
}

void ExecutionImplMacBNNS::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplMacBNNS::StartCompute";
  bool success = true;
  if (@available(macOS 10.13, *)) {
    do {
      @autoreleasepool {
        if (compilation_ != nullptr) {
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
            // in_height + 2 * y_padding = y_stride * (out_height - 1) +
            // k_height; But webml doesn't hava these restricts, so I need to
            // add some 0 values for input image to adapt to bnns
            if (is_outer_input || need_offset_transform) {
              float* raw_input = (float*)inputs_info_[0]->mapping.get();
              if (is_outer_input == false) {
                raw_input = bnns_operands_memory_map_[operation_input_idx];
              }
              if (operation.local_operation == KBNNSFilter &&
                  operation_input.dimensions.size() == 4) {
                TransposeForInput(operation, operation_input, src, raw_input,
                                  is_outer_input);
                tmp_src_malloc = true;
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
              const int32_t input_batch_size = operation.input_batch_size;
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
                if (i == 0) {
                  float* input;
                  if (is_outer_input) {
                    input = (float*)inputs_info_[0]->mapping.get();
                    is_outer_input = false;
                  } else {
                    input = operation.concatenations[index - 1];
                  }
                  TransposeForInput(operation, operand, src, input, true);
                  tmp_src_malloc = true;
                } else {
                  src = bnns_operands_memory_map_[concat_input_idx];
                }
                for (int b = 0; b < batch; b++) {
                  for (int c = 0; c < channels; c++) {
                    float* temp_des = des + c * channel_offset +
                                      b * batch_offset * batch +
                                      index * batch_offset;
                    float* temp_src =
                        src + c * channel_offset + b * batch_offset;
                    memcpy(temp_des, temp_src, channel_offset * sizeof(float));
                  }
                }
              }
            }

            if (is_outer_input && src != nullptr && tmp_src_malloc) {
              free(src);
            }
            if (is_outer_output && operation_output.dimensions.size() == 4) {
              const int32_t batch = operation_output.dimensions[0];
              const int32_t width = operation_output.dimensions[1];
              const int32_t height = operation_output.dimensions[2];
              const int32_t channels = operation_output.dimensions[3];
              float* output = (float*)malloc(sizeof(float) * batch * width *
                                             height * channels);
              memcpy(output, des,
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
                      *(des + ori_index) = *(output + bnns_index);
                    }
                  }
                }
              }
              free(output);
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
