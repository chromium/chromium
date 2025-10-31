// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_OPERAND_DESCRIPTOR_H_
#define SERVICES_WEBNN_PUBLIC_CPP_OPERAND_DESCRIPTOR_H_

#include <numeric>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace webnn {

enum class OperandDataType {
  kFloat32,
  kFloat16,
  kInt32,
  kUint32,
  kInt64,
  kUint64,
  kInt8,
  kUint8,
  kInt4,
  kUint4,

  kMinValue = kFloat32,
  kMaxValue = kUint4,
};

struct ContextProperties;

class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) OperandDescriptor {
 public:
  // Validates the tensor size limit and returns the byte length of the tensor.
  // The `T` must be an integral type. The `bits_per_element` should be the bit
  // size of the data type, such as from `GetBitsPerElement`.
  template <typename T>
  static base::expected<uint64_t, std::string> ValidateAndGetByteLength(
      size_t bits_per_element,
      base::span<const T> shape) {
    static_assert(std::is_integral_v<T>,
                  "Shape type must be an integral type.");
    // TODO(crbug.com/329482489): Specify the max rank of an operand. Consider
    // exposing different ranks for different backends (e.g. Core ML supports
    // only up to rank 5).
    if (shape.size() > 8) {
      return base::unexpected(
          "Invalid descriptor: The maximum rank of an operand is 8.");
    }

    // Enforce dimension range according to
    // https://www.w3.org/TR/webnn/#valid-dimension.
    if (std::ranges::any_of(shape, [](T dimension) {
          return !base::CheckedNumeric<int32_t>(dimension).IsValid();
        })) {
      return base::unexpected(
          "Invalid descriptor: All dimensions must be in the range of "
          "int32_t.");
    }

    base::CheckedNumeric<size_t> checked_number_of_elements =
        std::accumulate(shape.begin(), shape.end(),
                        base::CheckedNumeric<size_t>(1), std::multiplies());
    if (!checked_number_of_elements.IsValid()) {
      return base::unexpected(
          "Invalid descriptor: The number of elements is too large.");
    }

    // Since the data stored in memory are in 8-bits bytes, here we need to make
    // up an integer multiple of 8 to calculate the `checked_number_of_bytes`.
    base::CheckedNumeric<uint64_t> checked_number_of_bytes =
        (checked_number_of_elements.Cast<uint64_t>() * bits_per_element + 7) /
        8;

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

    return number_of_bytes;
  }

  // Creates a valid `OperandDescriptor` or returns an error message which may
  // be returned to script if the inputs are not valid.
  static base::expected<OperandDescriptor, std::string> Create(
      const ContextProperties& context_properties,
      OperandDataType data_type,
      base::span<const uint32_t> shape,
      std::string_view label);

  // This function is called by `OperandDescriptor` mojom traits that need to be
  // validated tensor size limit later.
  static base::expected<OperandDescriptor, std::string>
  CreateForDeserialization(OperandDataType data_type,
                           base::span<const uint32_t> shape,
                           base::span<const uint32_t> pending_permutation);

  // Same as above, but skip validation checks. This may be used to create an
  // invalid descriptor to test that its deserialization fails.
  static OperandDescriptor UnsafeCreateForTesting(
      OperandDataType data_type,
      base::span<const uint32_t> shape,
      base::span<const uint32_t> pending_permutation = {});

  static size_t GetBitsPerElement(OperandDataType data_type);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit OperandDescriptor(mojo::DefaultConstruct::Tag);

  // Copyable and movable.
  OperandDescriptor(const OperandDescriptor&);
  OperandDescriptor& operator=(const OperandDescriptor&);
  OperandDescriptor(OperandDescriptor&&) noexcept;
  OperandDescriptor& operator=(OperandDescriptor&&) noexcept;

  ~OperandDescriptor();

  OperandDataType data_type() const { return data_type_; }
  const std::vector<uint32_t>& shape() const { return shape_; }
  const std::vector<uint32_t>& pending_permutation() const {
    return pending_permutation_;
  }

  uint32_t Rank() const { return static_cast<uint32_t>(shape_.size()); }
  // Total byte length assuming perfect packing. Some tensors described by this
  // `OperandDescriptor` may be stored with more bytes.
  size_t PackedByteLength() const;
  size_t NumberOfElements() const;

  void SetPendingPermutation(base::span<const uint32_t> permutation);

  friend constexpr auto operator<=>(const OperandDescriptor& lhs,
                                    const OperandDescriptor& rhs) {
    if (auto cmp = lhs.data_type_ <=> rhs.data_type_; cmp != 0) {
      return cmp;
    }
    return lhs.shape_ <=> rhs.shape_;
  }
  friend constexpr bool operator==(const OperandDescriptor& lhs,
                                   const OperandDescriptor& rhs) {
    return lhs.data_type_ == rhs.data_type_ && lhs.shape_ == rhs.shape_;
  }

 private:
  OperandDescriptor(OperandDataType data_type, std::vector<uint32_t> shape);
  OperandDescriptor(OperandDataType data_type,
                    std::vector<uint32_t> shape,
                    std::vector<uint32_t> permutation);

  OperandDataType data_type_;
  std::vector<uint32_t> shape_;
  std::vector<uint32_t> pending_permutation_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_OPERAND_DESCRIPTOR_H_
