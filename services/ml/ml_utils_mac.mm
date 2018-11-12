// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/ml_utils_mac.h"

namespace ml {

OperandMac::OperandMac() = default;
OperandMac::OperandMac(const OperandMac& operand) = default;
OperandMac::OperandMac(const Operand& operand)
    : Operand(operand), read_count(0) {}
OperandMac::~OperandMac() = default;

OperationMac::OperationMac() = default;
OperationMac::OperationMac(const OperationMac& operation) = default;
OperationMac::OperationMac(const Operation& operation)
    : Operation(operation), local_operation(KBNNSFilter) {}
OperationMac::~OperationMac() = default;

bool ParameterExtracterForConv(const OperationMac& operation,
                               const std::vector<uint32_t>& inputs,
                               const std::vector<uint32_t>& outputs,
                               std::map<uint32_t, ValueInfo>& values,
                               std::unique_ptr<int8_t[]>& memory,
                               std::vector<OperandMac>& operands,
                               int32_t& input_width,
                               int32_t& input_height,
                               int32_t& output_width,
                               int32_t& output_height,
                               bool& implicit_padding,
                               int32_t& padding_left,
                               int32_t& padding_right,
                               int32_t& padding_top,
                               int32_t& padding_bottom,
                               int32_t& stride_width,
                               int32_t& stride_height,
                               int32_t& padding_code,
                               int32_t& fuse_code,
                               int32_t& depth_out,
                               int32_t& filter_height,
                               int32_t& filter_width,
                               int32_t& depth_in,
                               int32_t& index,
                               int32_t& depthwise_multiplier,
                               bool depthwise) {
  uint32_t output_idx = outputs[0];
  OperandMac& output = operands[output_idx];
  output_height = output.dimensions[1];
  output_width = output.dimensions[2];
  int32_t input_idx = inputs[index++];
  OperandMac& input = operands[input_idx];
  input_height = input.dimensions[1];
  input_width = input.dimensions[2];

  OperandMac& filter = operands[inputs[index++]];
  if (depthwise) {
    depth_out = filter.dimensions[3];
  } else {
    depth_out = filter.dimensions[0];
    depth_in = filter.dimensions[3];
  }
  filter_height = filter.dimensions[1];
  filter_width = filter.dimensions[2];

  OperandMac& bias = operands[inputs[index++]];
  DLOG(INFO) << "  bias length: " << bias.dimensions[0];

  if ((!depthwise && inputs.size() == 10) ||
      (depthwise && inputs.size() == 11)) {
    implicit_padding = false;
    padding_left = getScalarInt32(values[inputs[index++]], memory.get());
    padding_right = getScalarInt32(values[inputs[index++]], memory.get());
    padding_top = getScalarInt32(values[inputs[index++]], memory.get());
    padding_bottom = getScalarInt32(values[inputs[index++]], memory.get());
  } else if ((!depthwise && inputs.size() == 7) ||
             (depthwise && inputs.size() == 8)) {
    implicit_padding = true;
    padding_code = getScalarInt32(values[inputs[index++]], memory.get());
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return false;
  }
  stride_width = getScalarInt32(values[inputs[index++]], memory.get());
  stride_height = getScalarInt32(values[inputs[index++]], memory.get());
  if (depthwise == true) {
    depthwise_multiplier =
        getScalarInt32(values[inputs[index++]], memory.get());
    if (depthwise_multiplier != 1) {
      DLOG(ERROR) << "  depthwise_multiplier " << depthwise_multiplier
                  << " is not supported.";
      return false;
    }
    depth_in = depth_out / depthwise_multiplier;
  }
  fuse_code = getScalarInt32(values[inputs[index++]], memory.get());
  return true;
}

void SetupOperandInfoForOperands(
    std::vector<std::unique_ptr<OperandInfo>>& opearnd_info_array,
    std::vector<OperandMac>& operands,
    const std::vector<uint32_t>& operands_index_array,
    mojo::ScopedSharedBufferHandle& memory,
    uint32_t& mapped_length) {
  for (size_t i = 0; i < operands_index_array.size(); ++i) {
    const uint32_t length = operands[operands_index_array[i]].requiredSize();
    mojo::ScopedSharedBufferMapping mapping =
        memory->MapAtOffset(length, mapped_length);
    std::unique_ptr<OperandInfo> info(
        new OperandInfo(mapped_length, length, std::move(mapping)));
    opearnd_info_array.push_back(std::move(info));
    mapped_length += length;
  }
}
}
