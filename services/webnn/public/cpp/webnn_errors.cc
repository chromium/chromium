// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_errors.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"

namespace webnn {

namespace {

static constexpr char kInputParam[] = "input";

std::string SupportedDataTypesString(SupportedDataTypes supported_types) {
  std::vector<std::string> type_strings;
  std::ranges::transform(
      supported_types, std::back_inserter(type_strings),
      [](OperandDataType type) { return DataTypeToString(type); });
  return base::StrCat(
      {", must be one of [", base::JoinString(type_strings, ", "), "]."});
}

std::string SupportedRanksString(SupportedRanks supported_ranks) {
  return base::StrCat({", must be in range [",
                       base::NumberToString(supported_ranks.min), ", ",
                       base::NumberToString(supported_ranks.max), "]."});
}

std::string NotSupportedTensorsError(std::string_view operand_kind,
                                     std::string_view operand_name,
                                     const OperandDescriptor& descriptor,
                                     SupportedTensors supported_tensors) {
  std::string operand_str = base::StrCat({" for ", operand_kind});
  if (!operand_name.empty()) {
    base::StrAppend(&operand_str, {" named '", operand_name, "'"});
  }

  if (!supported_tensors.data_types.Has(descriptor.data_type())) {
    return base::StrCat(
        {"Unsupported data type ", DataTypeToString(descriptor.data_type()),
         operand_str, SupportedDataTypesString(supported_tensors.data_types)});
  }

  if (!supported_tensors.ranks.Supports(descriptor.Rank())) {
    return base::StrCat({"Unsupported rank ",
                         base::NumberToString(descriptor.Rank()), operand_str,
                         SupportedRanksString(supported_tensors.ranks)});
  }

  NOTREACHED();
}

}  // namespace
std::string DataTypeToString(OperandDataType type) {
  switch (type) {
    case OperandDataType::kFloat32:
      return "float32";
    case OperandDataType::kFloat16:
      return "float16";
    case OperandDataType::kInt32:
      return "int32";
    case OperandDataType::kUint32:
      return "uint32";
    case OperandDataType::kInt8:
      return "int8";
    case OperandDataType::kUint8:
      return "uint8";
    case OperandDataType::kInt64:
      return "int64";
    case OperandDataType::kUint64:
      return "uint64";
    case OperandDataType::kUint4:
      return "uint4";
    case OperandDataType::kInt4:
      return "int4";
  }
}

std::string NotSupportedArgumentTypeError(std::string_view argument_name,
                                          OperandDataType type,
                                          SupportedDataTypes supported_types) {
  return base::StrCat({"Unsupported data type ", DataTypeToString(type),
                       " for argument ", argument_name,
                       SupportedDataTypesString(supported_types)});
}

std::string NotSupportedArgumentError(std::string_view argument_name,
                                      const OperandDescriptor& descriptor,
                                      SupportedTensors supported_tensors) {
  if (!supported_tensors.data_types.Has(descriptor.data_type())) {
    return NotSupportedArgumentTypeError(argument_name, descriptor.data_type(),
                                         supported_tensors.data_types);
  }

  if (!supported_tensors.ranks.Supports(descriptor.Rank())) {
    return base::StrCat({"Unsupported rank ",
                         base::NumberToString(descriptor.Rank()),
                         " for argument ", argument_name,
                         SupportedRanksString(supported_tensors.ranks)});
  }

  NOTREACHED();
}

std::string NotSupportedConstantError(const OperandDescriptor& descriptor,
                                      SupportedTensors supported_tensors) {
  return NotSupportedTensorsError("constant", /*operand_name=*/"", descriptor,
                                  supported_tensors);
}

std::string NotSupportedInputArgumentTypeError(
    OperandDataType type,
    SupportedDataTypes supported_types) {
  return NotSupportedArgumentTypeError(kInputParam, type, supported_types);
}

std::string NotSupportedInputArgumentError(const OperandDescriptor& descriptor,
                                           SupportedTensors supported_tensors) {
  return NotSupportedArgumentError(kInputParam, descriptor, supported_tensors);
}

std::string NotSupportedInputError(std::string_view input_name,
                                   const OperandDescriptor& descriptor,
                                   SupportedTensors supported_tensors) {
  return NotSupportedTensorsError("input operand", input_name, descriptor,
                                  supported_tensors);
}

std::string NotSupportedOpOutputRankError(uint32_t rank,
                                          SupportedRanks supported_ranks) {
  CHECK(!supported_ranks.Supports(rank));
  return base::StrCat({"Unsupported rank ", base::NumberToString(rank),
                       " for output", SupportedRanksString(supported_ranks)});
}

std::string NotSupportedOpOutputTypeError(OperandDataType type,
                                          SupportedDataTypes supported_types) {
  CHECK(!supported_types.Has(type));
  return base::StrCat({"Unsupported data type ", DataTypeToString(type),
                       " for output",
                       SupportedDataTypesString(supported_types)});
}

std::string NotSupportedOutputError(std::string_view output_name,
                                    const OperandDescriptor& descriptor,
                                    SupportedTensors supported_tensors) {
  return NotSupportedTensorsError("output operand", output_name, descriptor,
                                  supported_tensors);
}

std::string NotSupportedMLTensorTypeError(OperandDataType type,
                                          SupportedDataTypes supported_types) {
  return base::StrCat({"Unsupported data type ", DataTypeToString(type),
                       " for MLTensor",
                       SupportedDataTypesString(supported_types)});
}

std::string NotSupportedTensorSizeError(uint64_t byte_length,
                                        uint64_t size_limit) {
  return base::StrCat({"The tensor size ", base::NumberToString(byte_length),
                       " exceeds the size limit ",
                       base::NumberToString(size_limit)});
}

std::string GetErrorLabelPrefix(std::string_view label) {
  if (label.empty()) {
    return "";
  }
  return base::StrCat({"[", label, "] "});
}

std::string ErrorWithLabel(std::string_view label,
                           std::string_view error_message) {
  return base::StrCat({GetErrorLabelPrefix(label), error_message});
}

}  // namespace webnn
