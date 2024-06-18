// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/operand_descriptor.h"

#include <numeric>

#include "base/numerics/checked_math.h"

namespace webnn {

// static
base::expected<OperandDescriptor, std::string> OperandDescriptor::Create(
    OperandDataType data_type,
    base::span<const uint32_t> shape) {
  base::CheckedNumeric<size_t> checked_number_of_bytes = std::accumulate(
      shape.begin(), shape.end(),
      base::CheckedNumeric<size_t>(GetBytesPerElement(data_type)),
      std::multiplies());

  // TODO(crbug.com/345271830): Consider performing backend-specific checks
  // here, such as the requirement that Core ML dimension values must be no
  // larger than INT_MAX.
  //
  // TODO(crbug.com/329482489): Define the max rank of an operand.

  size_t number_of_bytes;
  if (!checked_number_of_bytes.AssignIfValid(&number_of_bytes)) {
    return base::unexpected(
        "Invalid descriptor: The byte length is too large.");
  }

  if (number_of_bytes == 0) {
    // TODO(crbug.com/329471677): Consider supporting size 0 dimensions.
    return base::unexpected(
        "Invalid descriptor: All dimensions should be positive.");
  }

  return OperandDescriptor(data_type,
                           std::vector<uint32_t>(shape.begin(), shape.end()));
}

// static
OperandDescriptor OperandDescriptor::UnsafeCreateForTesting(
    OperandDataType data_type,
    base::span<const uint32_t> shape) {
  return OperandDescriptor(data_type,
                           std::vector<uint32_t>(shape.begin(), shape.end()));
}

// static
size_t OperandDescriptor::GetBytesPerElement(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return sizeof(float);
    case OperandDataType::kFloat16:
      return sizeof(uint16_t);
    case OperandDataType::kInt32:
      return sizeof(int32_t);
    case OperandDataType::kUint32:
      return sizeof(uint32_t);
    case OperandDataType::kInt64:
      return sizeof(int64_t);
    case OperandDataType::kUint64:
      return sizeof(uint64_t);
    case OperandDataType::kInt8:
      return sizeof(int8_t);
    case OperandDataType::kUint8:
      return sizeof(uint8_t);
  }
}

OperandDescriptor::OperandDescriptor(mojo::DefaultConstruct::Tag) {}

OperandDescriptor::OperandDescriptor(OperandDataType data_type,
                                     std::vector<uint32_t> shape)
    : data_type_(data_type), shape_(std::move(shape)) {}

OperandDescriptor::OperandDescriptor(const OperandDescriptor&) = default;
OperandDescriptor& OperandDescriptor::operator=(const OperandDescriptor&) =
    default;
OperandDescriptor::OperandDescriptor(OperandDescriptor&&) noexcept = default;
OperandDescriptor& OperandDescriptor::operator=(OperandDescriptor&&) noexcept =
    default;

OperandDescriptor::~OperandDescriptor() = default;

size_t OperandDescriptor::PackedByteLength() const {
  // Overflow checks are not needed here because this same calculation is
  // performed with overflow checking in `Create()`. `this` would not exist if
  // those checks failed.
  return std::accumulate(shape_.begin(), shape_.end(),
                         GetBytesPerElement(data_type_), std::multiplies());
}

size_t OperandDescriptor::NumberOfElements() const {
  // See `PackedByteLength()` for why overflow checks are not needed here.
  // Note that NumberOfElements() <= PackedByteLength().
  return std::accumulate(shape_.begin(), shape_.end(), 1, std::multiplies());
}

}  // namespace webnn
