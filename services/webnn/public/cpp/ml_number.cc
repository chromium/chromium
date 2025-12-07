// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/ml_number.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn {

namespace {

template <typename T>
constexpr T CastTo(const std::variant<double, int64_t, uint64_t>& number) {
  return std::visit(
      absl::Overload{
          [](const double& d) { return base::saturated_cast<T>(d); },
          [](const int64_t& i) { return base::saturated_cast<T>(i); },
          [](const uint64_t& u) { return base::saturated_cast<T>(u); }},
      number);
}

}  // namespace

// static
MLNumber MLNumber::FromFloat64(double number) {
  return MLNumber(number);
}

// static
MLNumber MLNumber::FromInt64(int64_t number) {
  return MLNumber(number);
}

// static
MLNumber MLNumber::FromUint64(uint64_t number) {
  return MLNumber(number);
}

// static
MLNumber MLNumber::Infinity() {
  return MLNumber(std::numeric_limits<double>::infinity());
}

// static
MLNumber MLNumber::NegativeInfinity() {
  return MLNumber(-std::numeric_limits<double>::infinity());
}

MLNumber::MLNumber(mojo::DefaultConstruct::Tag) : number_(0.0) {}

MLNumber::MLNumber(double number) : number_(number) {}

MLNumber::MLNumber(int64_t number) : number_(number) {}

MLNumber::MLNumber(uint64_t number) : number_(number) {}

MLNumber::MLNumber(const MLNumber&) = default;
MLNumber& MLNumber::operator=(const MLNumber&) = default;
MLNumber::MLNumber(MLNumber&&) noexcept = default;
MLNumber& MLNumber::operator=(MLNumber&&) noexcept = default;

MLNumber::~MLNumber() = default;

bool MLNumber::IsGreaterThan(const MLNumber& other,
                             OperandDataType casted_data_type) const {
  switch (casted_data_type) {
    case webnn::OperandDataType::kFloat16:
      return fp16_ieee_to_fp32_value(AsFloat16()) >
             fp16_ieee_to_fp32_value(other.AsFloat16());
    case webnn::OperandDataType::kFloat32:
      return AsFloat32() > other.AsFloat32();
    case webnn::OperandDataType::kInt8:
      return AsInt8() > other.AsInt8();
    case webnn::OperandDataType::kUint8:
      return AsUint8() > other.AsUint8();
    case webnn::OperandDataType::kInt32:
      return AsInt32() > other.AsInt32();
    case webnn::OperandDataType::kUint32:
      return AsUint32() > other.AsUint32();
    case webnn::OperandDataType::kInt64:
      return AsInt64() > other.AsInt64();
    case webnn::OperandDataType::kUint64:
      return AsUint64() > other.AsUint64();
    case webnn::OperandDataType::kInt4:
    case webnn::OperandDataType::kUint4:
      NOTREACHED();
  }
}

uint16_t MLNumber::AsFloat16() const {
  return std::visit(
      absl::Overload{
          [](const double& d) { return fp16_ieee_from_fp32_value(d); },
          [](const int64_t& i) {
            return fp16_ieee_from_fp32_value(base::saturated_cast<float>(i));
          },
          [](const uint64_t& u) {
            return fp16_ieee_from_fp32_value(base::saturated_cast<float>(u));
          }},
      number_);
}

float MLNumber::AsFloat32() const {
  return CastTo<float>(number_);
}

double MLNumber::AsFloat64() const {
  return CastTo<double>(number_);
}

int8_t MLNumber::AsInt8() const {
  return CastTo<int8_t>(number_);
}

uint8_t MLNumber::AsUint8() const {
  return CastTo<uint8_t>(number_);
}

int32_t MLNumber::AsInt32() const {
  return CastTo<int32_t>(number_);
}

uint32_t MLNumber::AsUint32() const {
  return CastTo<uint32_t>(number_);
}

int64_t MLNumber::AsInt64() const {
  return CastTo<int64_t>(number_);
}

uint64_t MLNumber::AsUint64() const {
  return CastTo<uint64_t>(number_);
}

MLNumber::BaseType MLNumber::GetBaseType() const {
  return std::visit(
      absl::Overload{
          [](const double& _) { return BaseType::kFloatingPoint; },
          [](const int64_t& _) { return BaseType::kSignedInteger; },
          [](const uint64_t& _) { return BaseType::kUnsignedInteger; }},
      number_);
}

bool MLNumber::IsNaN() const {
  return std::isnan(AsFloat64());
}

}  // namespace webnn
