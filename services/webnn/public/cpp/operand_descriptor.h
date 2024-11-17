// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_OPERAND_DESCRIPTOR_H_
#define SERVICES_WEBNN_PUBLIC_CPP_OPERAND_DESCRIPTOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
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

class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) OperandDescriptor {
 public:
  // Creates a valid `OperandDescriptor` or returns an error message which may
  // be returned to script if the inputs are not valid.
  static base::expected<OperandDescriptor, std::string> Create(
      OperandDataType data_type,
      base::span<const uint32_t> shape);

  // Same as above, but skip validation checks. This may be used to create an
  // invalid descriptor to test that its deserialization fails.
  static OperandDescriptor UnsafeCreateForTesting(
      OperandDataType data_type,
      base::span<const uint32_t> shape);

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

  uint32_t Rank() const { return static_cast<uint32_t>(shape_.size()); }
  // Total byte length assuming perfect packing. Some tensors described by this
  // `OperandDescriptor` may be stored with more bytes.
  size_t PackedByteLength() const;
  size_t NumberOfElements() const;

  friend constexpr auto operator<=>(const OperandDescriptor& lhs,
                                    const OperandDescriptor& rhs) = default;

 private:
  OperandDescriptor(OperandDataType data_type, std::vector<uint32_t> shape);

  OperandDataType data_type_;
  std::vector<uint32_t> shape_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_OPERAND_DESCRIPTOR_H_
