// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/operand_descriptor.h"

#include <algorithm>
#include <numeric>

#include "base/containers/to_vector.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/webnn_errors.h"

namespace webnn {

namespace {

#define ASSIGN_OR_RETURN_ERROR_WITH_LABEL_IF_ERROR(lhs, rexpr, label) \
  ASSIGN_OR_RETURN(lhs, rexpr, [&label](std::string error) {          \
    return ErrorWithLabel(label, error);                              \
  });

base::expected<void, std::string> IsValidPermutation(
    base::span<const uint32_t> permutation,
    OperandDataType data_type,
    base::span<const uint32_t> shape) {
  // TODO(crbug.com/428232161): Support sub-byte transposes.
  if (OperandDescriptor::GetBitsPerElement(data_type) < 8u) {
    return base::unexpected(
        "Invalid descriptor: Permutation is not supported for sub-byte data "
        "types.");
  }
  if (permutation.size() != shape.size()) {
    return base::unexpected(
        "Invalid descriptor: Permutation size doesn't match with shape.");
  }
  std::vector<uint32_t> sorted_permutation = base::ToVector(permutation);
  std::ranges::sort(sorted_permutation);
  for (size_t i = 0; i < sorted_permutation.size(); ++i) {
    if (sorted_permutation[i] != i) {
      return base::unexpected(
          "Invalid descriptor: Permutation contains invalid dimension.");
    }
  }
  return base::ok();
}

}  // namespace

// static
base::expected<OperandDescriptor, std::string> OperandDescriptor::Create(
    const ContextProperties& context_properties,
    OperandDataType data_type,
    base::span<const uint32_t> shape,
    std::string_view label) {
  ASSIGN_OR_RETURN_ERROR_WITH_LABEL_IF_ERROR(
      uint64_t byte_length,
      ValidateAndGetByteLength(OperandDescriptor::GetBitsPerElement(data_type),
                               shape),
      label);

  if (byte_length > context_properties.tensor_byte_length_limit) {
    return base::unexpected(ErrorWithLabel(
        label, NotSupportedTensorSizeError(
                   byte_length, context_properties.tensor_byte_length_limit)));
  }
  return OperandDescriptor(data_type, base::ToVector(shape));
}

// static
base::expected<OperandDescriptor, std::string>
OperandDescriptor::CreateForDeserialization(
    OperandDataType data_type,
    base::span<const uint32_t> shape,
    base::span<const uint32_t> pending_permutation) {
  RETURN_IF_ERROR(ValidateAndGetByteLength(
      OperandDescriptor::GetBitsPerElement(data_type), shape));
  if (!pending_permutation.empty()) {
    RETURN_IF_ERROR(IsValidPermutation(pending_permutation, data_type, shape));
  }
  return OperandDescriptor(data_type, base::ToVector(shape),
                           base::ToVector(pending_permutation));
}

// static
OperandDescriptor OperandDescriptor::UnsafeCreateForTesting(
    OperandDataType data_type,
    base::span<const uint32_t> shape,
    base::span<const uint32_t> pending_permutation) {
  return OperandDescriptor(data_type, base::ToVector(shape),
                           base::ToVector(pending_permutation));
}

// static
size_t OperandDescriptor::GetBitsPerElement(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return sizeof(float) * 8;
    case OperandDataType::kFloat16:
      return sizeof(uint16_t) * 8;
    case OperandDataType::kInt32:
      return sizeof(int32_t) * 8;
    case OperandDataType::kUint32:
      return sizeof(uint32_t) * 8;
    case OperandDataType::kInt64:
      return sizeof(int64_t) * 8;
    case OperandDataType::kUint64:
      return sizeof(uint64_t) * 8;
    case OperandDataType::kInt8:
      return sizeof(int8_t) * 8;
    case OperandDataType::kUint8:
      return sizeof(uint8_t) * 8;
    case OperandDataType::kInt4:
    case OperandDataType::kUint4:
      return 4;
  }
}

OperandDescriptor::OperandDescriptor(mojo::DefaultConstruct::Tag) {}

OperandDescriptor::OperandDescriptor(OperandDataType data_type,
                                     std::vector<uint32_t> shape)
    : data_type_(data_type), shape_(std::move(shape)) {}

OperandDescriptor::OperandDescriptor(OperandDataType data_type,
                                     std::vector<uint32_t> shape,
                                     std::vector<uint32_t> pending_permutation)
    : data_type_(data_type),
      shape_(std::move(shape)),
      pending_permutation_(std::move(pending_permutation)) {}

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
  base::CheckedNumeric<uint64_t> checked_number_of_bytes =
      (base::CheckedNumeric<uint64_t>(GetBitsPerElement(data_type_)) *
           NumberOfElements() +
       7) /
      8;
  return checked_number_of_bytes.ValueOrDie<size_t>();
}

size_t OperandDescriptor::NumberOfElements() const {
  // See `PackedByteLength()` for why overflow checks are not needed here.
  return std::accumulate(shape_.begin(), shape_.end(), static_cast<size_t>(1),
                         std::multiplies());
}

void OperandDescriptor::SetPendingPermutation(
    base::span<const uint32_t> permutation) {
  CHECK(IsValidPermutation(permutation, data_type_, shape_).has_value());
  pending_permutation_.assign(permutation.begin(), permutation.end());
}
}  // namespace webnn
