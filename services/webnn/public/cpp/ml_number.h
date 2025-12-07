// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_ML_NUMBER_H_
#define SERVICES_WEBNN_PUBLIC_CPP_ML_NUMBER_H_

#include <cstdint>
#include <variant>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn {

class COMPONENT_EXPORT(WEBNN_PUBLIC_CPP) MLNumber {
 public:
  enum class BaseType {
    kFloatingPoint,
    kSignedInteger,
    kUnsignedInteger,
  };

  static MLNumber FromFloat64(double number);
  static MLNumber FromInt64(int64_t number);
  static MLNumber FromUint64(uint64_t number);

  static MLNumber Infinity();
  static MLNumber NegativeInfinity();

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit MLNumber(mojo::DefaultConstruct::Tag);

  // Copyable and movable.
  MLNumber(const MLNumber&);
  MLNumber& operator=(const MLNumber&);
  MLNumber(MLNumber&&) noexcept;
  MLNumber& operator=(MLNumber&&) noexcept;

  ~MLNumber();

  // Returns true if `this` is greater than `other` when each value is
  // interpreted as `cast_data_type`.
  bool IsGreaterThan(const MLNumber& other,
                     OperandDataType casted_data_type) const;

  uint16_t AsFloat16() const;
  float AsFloat32() const;
  double AsFloat64() const;
  int8_t AsInt8() const;
  uint8_t AsUint8() const;
  int32_t AsInt32() const;
  uint32_t AsUint32() const;
  int64_t AsInt64() const;
  uint64_t AsUint64() const;

  BaseType GetBaseType() const;
  bool IsNaN() const;

 private:
  explicit MLNumber(double number);
  explicit MLNumber(int64_t number);
  explicit MLNumber(uint64_t number);

  std::variant<double, int64_t, uint64_t> number_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_ML_NUMBER_H_
