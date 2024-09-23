/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/decimal.h"

#include <algorithm>
#include <cfloat>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

constexpr int kExponentMax = 1023;
constexpr int kExponentMin = -1023;
constexpr int kPrecision = 18;

constexpr uint64_t kMaxCoefficient =
    UINT64_C(0xDE0B6B3A763FFFF);  // 999999999999999999 == 18 9's

// This class handles Decimal special values.
class SpecialValueHandler {
  STACK_ALLOCATED();

 public:
  enum HandleResult {
    kBothFinite,
    kBothInfinity,
    kEitherNaN,
    kLHSIsInfinity,
    kRHSIsInfinity,
  };

  SpecialValueHandler(const Decimal& lhs, const Decimal& rhs);
  SpecialValueHandler(const SpecialValueHandler&) = delete;
  SpecialValueHandler& operator=(const SpecialValueHandler&) = delete;

  HandleResult Handle();
  Decimal Value() const;

 private:
  enum Result {
    kResultIsLHS,
    kResultIsRHS,
    kResultIsUnknown,
  };

  const Decimal& lhs_;
  const Decimal& rhs_;
  Result result_ = kResultIsUnknown;
};

SpecialValueHandler::SpecialValueHandler(const Decimal& lhs, const Decimal& rhs)
    : lhs_(lhs), rhs_(rhs) {}

SpecialValueHandler::HandleResult SpecialValueHandler::Handle() {
  if (lhs_.IsFinite() && rhs_.IsFinite())
    return kBothFinite;

  if (lhs_.IsNaN()) {
    result_ = kResultIsLHS;
    return kEitherNaN;
  }

  if (rhs_.IsNaN()) {
    result_ = kResultIsRHS;
    return kEitherNaN;
  }

  if (lhs_.IsInfinity())
    return rhs_.IsInfinity() ? kBothInfinity : kLHSIsInfinity;

  DCHECK(rhs_.IsInfinity());
  return kRHSIsInfinity;
}

Decimal SpecialValueHandler::Value() const {
  DCHECK(result_ == kResultIsLHS || result_ == kResultIsRHS);
  return (result_ == kResultIsLHS) ? lhs_ : rhs_;
}

// This class is used for 128 bit unsigned integer arithmetic.
class UInt128 {
  STACK_ALLOCATED();

 public:
  UInt128(uint64_t low, uint64_t high) : high_(high), low_(low) {}

  UInt128& operator/=(uint32_t);

  uint64_t High() const { return high_; }
  uint64_t Low() const { return low_; }

  static UInt128 Multiply(uint64_t u, uint64_t v) {
    return UInt128(u * v, MultiplyHigh(u, v));
  }

 private:
  static uint32_t HighUInt32(uint64_t x) {
    return static_cast<uint32_t>(x >> 32);
  }
  static uint32_t LowUInt32(uint64_t x) {
    return static_cast<uint32_t>(x & ((static_cast<uint64_t>(1) << 32) - 1));
  }
  static uint64_t MakeUInt64(uint32_t low, uint32_t high) {
    return low | (static_cast<uint64_t>(high) << 32);
  }

  static uint64_t MultiplyHigh(uint64_t, uint64_t);

  uint64_t high_;
  uint64_t low_;
};

UInt128& UInt128::operator/=(const uint32_t divisor) {
  DCHECK(divisor);

  if (!high_) {
    low_ /= divisor;
    return *this;
  }

  uint32_t dividend[4];
  dividend[0] = LowUInt32(low_);
  dividend[1] = HighUInt32(low_);
  dividend[2] = LowUInt32(high_);
  dividend[3] = HighUInt32(high_);

  uint32_t quotient[4];
  uint32_t remainder = 0;
  for (int i = 3; i >= 0; --i) {
    const uint64_t work = MakeUInt64(dividend[i], remainder);
    remainder = static_cast<uint32_t>(work % divisor);
    quotient[i] = static_cast<uint32_t>(work / divisor);
  }
  low_ = MakeUInt64(quotient[0], quotient[1]);
  high_ = MakeUInt64(quotient[2], quotient[3]);
  return *this;
}

// Returns high 64bit of 128bit product.
uint64_t UInt128::MultiplyHigh(uint64_t u, uint64_t v) {
  const uint64_t u_low = LowUInt32(u);
  const uint64_t u_high = HighUInt32(u);
  const uint64_t v_low = LowUInt32(v);
  const uint64_t v_high = HighUInt32(v);
  const uint64_t partial_product = u_high * v_low + HighUInt32(u_low * v_low);
  return u_high * v_high + HighUInt32(partial_product) +
         HighUInt32(u_low * v_high + LowUInt32(partial_product));
}

static int CountDigits(uint64_t x) {
  int number_of_digits = 0;
  for (uint64_t power_of_ten = 1; x >= power_of_ten; power_of_ten *= 10) {
    ++number_of_digits;
    if (power_of_ten >= std::numeric_limits<uint64_t>::max() / 10)
      break;
  }
  return number_of_digits;
}

static uint64_t ScaleDown(uint64_t x, int n) {
  DCHECK_GE(n, 0);
  while (n > 0 && x) {
    x /= 10;
    --n;
  }
  return x;
}

static uint64_t ScaleUp(uint64_t x, int n) {
  DCHECK_GE(n, 0);
  DCHECK_LE(n, kPrecision);

  uint64_t y = 1;
  uint64_t z = 10;
  for (;;) {
    if (n & 1)
      y = y * z;

    n >>= 1;
    if (!n)
      return x * y;

    z = z * z;
  }
}

}  // namespace

Decimal::EncodedData::EncodedData(Sign sign, FormatClass format_class)
    : coefficient_(0), exponent_(0), format_class_(format_class), sign_(sign) {}

Decimal::EncodedData::EncodedData(Sign sign, int exponent, uint64_t coefficient)
    : format_class_(coefficient ? kClassNormal : kClassZero), sign_(sign) {
  if (exponent >= kExponentMin && exponent <= kExponentMax) {
    while (coefficient > kMaxCoefficient) {
      coefficient /= 10;
      ++exponent;
    }
  }

  if (exponent > kExponentMax) {
    coefficient_ = 0;
    exponent_ = 0;
    format_class_ = kClassInfinity;
    return;
  }

  if (exponent < kExponentMin) {
    coefficient_ = 0;
    exponent_ = 0;
    format_class_ = kClassZero;
    return;
  }

  coefficient_ = coefficient;
  exponent_ = static_cast<int16_t>(exponent);
}

bool Decimal::EncodedData::operator==(const EncodedData& another) const {
  return sign_ == another.sign_ && format_class_ == another.format_class_ &&
         exponent_ == another.exponent_ && coefficient_ == another.coefficient_;
}

Decimal::Decimal(int32_t i32)
    : data_(i32 < 0 ? kNegative : kPositive,
            0,
            i32 < 0 ? static_cast<uint64_t>(-static_cast<int64_t>(i32))
                    : static_cast<uint64_t>(i32)) {}

Decimal::Decimal(Sign sign, int exponent, uint64_t coefficient)
    : data_(sign, exponent, coefficient) {}

Decimal::Decimal(const EncodedData& data) : data_(data) {}

Decimal::Decimal(const Decimal& other) = default;

Decimal& Decimal::operator=(const Decimal& other) = default;

Decimal& Decimal::operator+=(const Decimal& other) {
  data_ = (*this + other).data_;
  return *this;
}

Decimal& Decimal::operator-=(const Decimal& other) {
  data_ = (*this - other).data_;
  return *this;
}

Decimal& Decimal::operator*=(const Decimal& other) {
  data_ = (*this * other).data_;
  return *this;
}

Decimal& Decimal::operator/=(const Decimal& other) {
  data_ = (*this / other).data_;
  return *this;
}

Decimal Decimal::operator-() const {
  if (IsNaN())
    return *this;

  Decimal result(*this);
  result.data_.SetSign(InvertSign(data_.GetSign()));
  return result;
}

Decimal Decimal::operator+(const Decimal& rhs) const {
  const Decimal& lhs = *this;
  const Sign lhs_sign = lhs.GetSign();
  const Sign rhs_sign = rhs.GetSign();

  SpecialValueHandler handler(lhs, rhs);
  switch (handler.Handle()) {
    case SpecialValueHandler::kBothFinite:
      break;

    case SpecialValueHandler::kBothInfinity:
      return lhs_sign == rhs_sign ? lhs : Nan();

    case SpecialValueHandler::kEitherNaN:
      return handler.Value();

    case SpecialValueHandler::kLHSIsInfinity:
      return lhs;

    case SpecialValueHandler::kRHSIsInfinity:
      return rhs;
  }

  const AlignedOperands aligned_operands = AlignOperands(lhs, rhs);

  const uint64_t result =
      lhs_sign == rhs_sign
          ? aligned_operands.lhs_coefficient + aligned_operands.rhs_coefficient
          : aligned_operands.lhs_coefficient - aligned_operands.rhs_coefficient;

  if (lhs_sign == kNegative && rhs_sign == kPositive && !result)
    return Decimal(kPositive, aligned_operands.exponent, 0);

  return static_cast<int64_t>(result) >= 0
             ? Decimal(lhs_sign, aligned_operands.exponent, result)
             : Decimal(InvertSign(lhs_sign), aligned_operands.exponent,
                       -static_cast<int64_t>(result));
}

Decimal Decimal::operator-(const Decimal& rhs) const {
  const Decimal& lhs = *this;
  const Sign lhs_sign = lhs.GetSign();
  const Sign rhs_sign = rhs.GetSign();

  SpecialValueHandler handler(lhs, rhs);
  switch (handler.Handle()) {
    case SpecialValueHandler::kBothFinite:
      break;

    case SpecialValueHandler::kBothInfinity:
      return lhs_sign == rhs_sign ? Nan() : lhs;

    case SpecialValueHandler::kEitherNaN:
      return handler.Value();

    case SpecialValueHandler::kLHSIsInfinity:
      return lhs;

    case SpecialValueHandler::kRHSIsInfinity:
      return Infinity(InvertSign(rhs_sign));
  }

  const AlignedOperands aligned_operands = AlignOperands(lhs, rhs);

  const uint64_t result =
      lhs_sign == rhs_sign
          ? aligned_operands.lhs_coefficient - aligned_operands.rhs_coefficient
          : aligned_operands.lhs_coefficient + aligned_operands.rhs_coefficient;

  if (lhs_sign == kNegative && rhs_sign == kNegative && !result)
    return Decimal(kPositive, aligned_operands.exponent, 0);

  return static_cast<int64_t>(result) >= 0
             ? Decimal(lhs_sign, aligned_operands.exponent, result)
             : Decimal(InvertSign(lhs_sign), aligned_operands.exponent,
                       -static_cast<int64_t>(result));
}

Decimal Decimal::operator*(const Decimal& rhs) const {
  const Decimal& lhs = *this;
  const Sign lhs_sign = lhs.GetSign();
  const Sign rhs_sign = rhs.GetSign();
  const Sign result_sign = lhs_sign == rhs_sign ? kPositive : kNegative;

  SpecialValueHandler handler(lhs, rhs);
  switch (handler.Handle()) {
    case SpecialValueHandler::kBothFinite: {
      const uint64_t lhs_coefficient = lhs.data_.Coefficient();
      const uint64_t rhs_coefficient = rhs.data_.Coefficient();
      int result_exponent = lhs.Exponent() + rhs.Exponent();
      UInt128 work(UInt128::Multiply(lhs_coefficient, rhs_coefficient));
      while (work.High()) {
        work /= 10;
        ++result_exponent;
      }
      return Decimal(result_sign, result_exponent, work.Low());
    }

    case SpecialValueHandler::kBothInfinity:
      return Infinity(result_sign);

    case SpecialValueHandler::kEitherNaN:
      return handler.Value();

    case SpecialValueHandler::kLHSIsInfinity:
      return rhs.IsZero() ? Nan() : Infinity(result_sign);

    case SpecialValueHandler::kRHSIsInfinity:
      return lhs.IsZero() ? Nan() : Infinity(result_sign);
  }

  NOTREACHED_IN_MIGRATION();
  return Nan();
}

Decimal Decimal::operator/(const Decimal& rhs) const {
  const Decimal& lhs = *this;
  const Sign lhs_sign = lhs.GetSign();
  const Sign rhs_sign = rhs.GetSign();
  const Sign result_sign = lhs_sign == rhs_sign ? kPositive : kNegative;

  SpecialValueHandler handler(lhs, rhs);
  switch (handler.Handle()) {
    case SpecialValueHandler::kBothFinite:
      break;

    case SpecialValueHandler::kBothInfinity:
      return Nan();

    case SpecialValueHandler::kEitherNaN:
      return handler.Value();

    case SpecialValueHandler::kLHSIsInfinity:
      return Infinity(result_sign);

    case SpecialValueHandler::kRHSIsInfinity:
      return Zero(result_sign);
  }

  DCHECK(lhs.IsFinite());
  DCHECK(rhs.IsFinite());

  if (rhs.IsZero())
    return lhs.IsZero() ? Nan() : Infinity(result_sign);

  int result_exponent = lhs.Exponent() - rhs.Exponent();

  if (lhs.IsZero())
    return Decimal(result_sign, result_exponent, 0);

  uint64_t remainder = lhs.data_.Coefficient();
  const uint64_t divisor = rhs.data_.Coefficient();
  uint64_t result = 0;
  for (;;) {
    while (remainder < divisor && result < kMaxCoefficient / 10) {
      remainder *= 10;
      result *= 10;
      --result_exponent;
    }
    if (remainder < divisor)
      break;
    uint64_t quotient = remainder / divisor;
    if (result > kMaxCoefficient - quotient)
      break;
    result += quotient;
    remainder %= divisor;
    if (!remainder)
      break;
  }

  if (remainder > divisor / 2)
    ++result;

  return Decimal(result_sign, result_exponent, result);
}

bool Decimal::operator==(const Decimal& rhs) const {
  return data_ == rhs.data_ || CompareTo(rhs).IsZero();
}

bool Decimal::operator!=(const Decimal& rhs) const {
  if (data_ == rhs.data_)
    return false;
  const Decimal result = CompareTo(rhs);
  if (result.IsNaN())
    return false;
  return !result.IsZero();
}

bool Decimal::operator<(const Decimal& rhs) const {
  const Decimal result = CompareTo(rhs);
  if (result.IsNaN())
    return false;
  return !result.IsZero() && result.IsNegative();
}

bool Decimal::operator<=(const Decimal& rhs) const {
  if (data_ == rhs.data_)
    return true;
  const Decimal result = CompareTo(rhs);
  if (result.IsNaN())
    return false;
  return result.IsZero() || result.IsNegative();
}

bool Decimal::operator>(const Decimal& rhs) const {
  const Decimal result = CompareTo(rhs);
  if (result.IsNaN())
    return false;
  return !result.IsZero() && result.IsPositive();
}

bool Decimal::operator>=(const Decimal& rhs) const {
  if (data_ == rhs.data_)
    return true;
  const Decimal result = CompareTo(rhs);
  if (result.IsNaN())
    return false;
  return result.IsZero() || !result.IsNegative();
}

Decimal Decimal::Abs() const {
  Decimal result(*this);
  result.data_.SetSign(kPositive);
  return result;
}

Decimal::AlignedOperands Decimal::AlignOperands(const Decimal& lhs,
                                                const Decimal& rhs) {
  DCHECK(lhs.IsFinite());
  DCHECK(rhs.IsFinite());

  const int lhs_exponent = lhs.Exponent();
  const int rhs_exponent = rhs.Exponent();
  int exponent = std::min(lhs_exponent, rhs_exponent);
  uint64_t lhs_coefficient = lhs.data_.Coefficient();
  uint64_t rhs_coefficient = rhs.data_.Coefficient();

  if (lhs_exponent > rhs_exponent) {
    const int number_of_lhs_digits = CountDigits(lhs_coefficient);
    if (number_of_lhs_digits) {
      const int lhs_shift_amount = lhs_exponent - rhs_exponent;
      const int overflow = number_of_lhs_digits + lhs_shift_amount - kPrecision;
      if (overflow <= 0) {
        lhs_coefficient = ScaleUp(lhs_coefficient, lhs_shift_amount);
      } else {
        lhs_coefficient = ScaleUp(lhs_coefficient, lhs_shift_amount - overflow);
        rhs_coefficient = ScaleDown(rhs_coefficient, overflow);
        exponent += overflow;
      }
    }

  } else if (lhs_exponent < rhs_exponent) {
    const int number_of_rhs_digits = CountDigits(rhs_coefficient);
    if (number_of_rhs_digits) {
      const int rhs_shift_amount = rhs_exponent - lhs_exponent;
      const int overflow = number_of_rhs_digits + rhs_shift_amount - kPrecision;
      if (overflow <= 0) {
        rhs_coefficient = ScaleUp(rhs_coefficient, rhs_shift_amount);
      } else {
        rhs_coefficient = ScaleUp(rhs_coefficient, rhs_shift_amount - overflow);
        lhs_coefficient = ScaleDown(lhs_coefficient, overflow);
        exponent += overflow;
      }
    }
  }

  AlignedOperands aligned_operands;
  aligned_operands.exponent = exponent;
  aligned_operands.lhs_coefficient = lhs_coefficient;
  aligned_operands.rhs_coefficient = rhs_coefficient;
  return aligned_operands;
}

static bool IsMultiplePowersOfTen(uint64_t coefficient, int n) {
  return !coefficient || !(coefficient % ScaleUp(1, n));
}

// Round toward positive infinity.
Decimal Decimal::Ceil() const {
  if (IsSpecial())
    return *this;

  if (Exponent() >= 0)
    return *this;

  uint64_t result = data_.Coefficient();
  const int number_of_digits = CountDigits(result);
  const int number_of_drop_digits = -Exponent();
  if (number_of_digits <= number_of_drop_digits)
    return IsPositive() ? Decimal(1) : Zero(kPositive);

  result = ScaleDown(result, number_of_drop_digits);
  if (IsPositive() &&
      !IsMultiplePowersOfTen(data_.Coefficient(), number_of_drop_digits))
    ++result;
  return Decimal(GetSign(), 0, result);
}

Decimal Decimal::CompareTo(const Decimal& rhs) const {
  const Decimal result(*this - rhs);
  switch (result.data_.GetFormatClass()) {
    case EncodedData::kClassInfinity:
      return result.IsNegative() ? Decimal(-1) : Decimal(1);

    case EncodedData::kClassNaN:
    case EncodedData::kClassNormal:
      return result;

    case EncodedData::kClassZero:
      return Zero(kPositive);

    default:
      NOTREACHED_IN_MIGRATION();
      return Nan();
  }
}

// Round toward negative infinity.
Decimal Decimal::Floor() const {
  if (IsSpecial())
    return *this;

  if (Exponent() >= 0)
    return *this;

  uint64_t result = data_.Coefficient();
  const int number_of_digits = CountDigits(result);
  const int number_of_drop_digits = -Exponent();
  if (number_of_digits < number_of_drop_digits)
    return IsPositive() ? Zero(kPositive) : Decimal(-1);

  result = ScaleDown(result, number_of_drop_digits);
  if (IsNegative() &&
      !IsMultiplePowersOfTen(data_.Coefficient(), number_of_drop_digits))
    ++result;
  return Decimal(GetSign(), 0, result);
}

Decimal Decimal::FromDouble(double double_value) {
  if (std::isfinite(double_value))
    return FromString(String::NumberToStringECMAScript(double_value));

  if (std::isinf(double_value))
    return Infinity(double_value < 0 ? kNegative : kPositive);

  return Nan();
}

Decimal Decimal::FromString(const String& str) {
  int exponent = 0;
  Sign exponent_sign = kPositive;
  int number_of_digits = 0;
  int number_of_digits_after_dot = 0;
  int number_of_extra_digits = 0;
  Sign sign = kPositive;

  enum {
    kStateDigit,
    kStateDot,
    kStateDotDigit,
    kStateE,
    kStateEDigit,
    kStateESign,
    kStateSign,
    kStateStart,
    kStateZero,
  } state = kStateStart;

#define HandleCharAndBreak(expected, nextState) \
  if (ch == expected) {                         \
    state = nextState;                          \
    break;                                      \
  }

#define HandleTwoCharsAndBreak(expected1, expected2, nextState) \
  if (ch == expected1 || ch == expected2) {                     \
    state = nextState;                                          \
    break;                                                      \
  }

  uint64_t accumulator = 0;
  for (unsigned index = 0; index < str.length(); ++index) {
    const int ch = str[index];
    switch (state) {
      case kStateDigit:
        if (ch >= '0' && ch <= '9') {
          if (number_of_digits < kPrecision) {
            ++number_of_digits;
            accumulator *= 10;
            accumulator += ch - '0';
          } else {
            ++number_of_extra_digits;
          }
          break;
        }

        HandleCharAndBreak('.', kStateDot);
        HandleTwoCharsAndBreak('E', 'e', kStateE);
        return Nan();

      case kStateDot:
      case kStateDotDigit:
        if (ch >= '0' && ch <= '9') {
          if (number_of_digits < kPrecision) {
            ++number_of_digits;
            ++number_of_digits_after_dot;
            accumulator *= 10;
            accumulator += ch - '0';
          }
          state = kStateDotDigit;
          break;
        }

        HandleTwoCharsAndBreak('E', 'e', kStateE);
        return Nan();

      case kStateE:
        if (ch == '+') {
          exponent_sign = kPositive;
          state = kStateESign;
          break;
        }

        if (ch == '-') {
          exponent_sign = kNegative;
          state = kStateESign;
          break;
        }

        if (ch >= '0' && ch <= '9') {
          exponent = ch - '0';
          state = kStateEDigit;
          break;
        }

        return Nan();

      case kStateEDigit:
        if (ch >= '0' && ch <= '9') {
          exponent *= 10;
          exponent += ch - '0';
          if (exponent > kExponentMax + kPrecision) {
            if (accumulator)
              return exponent_sign == kNegative ? Zero(kPositive)
                                                : Infinity(sign);
            return Zero(sign);
          }
          state = kStateEDigit;
          break;
        }

        return Nan();

      case kStateESign:
        if (ch >= '0' && ch <= '9') {
          exponent = ch - '0';
          state = kStateEDigit;
          break;
        }

        return Nan();

      case kStateSign:
        if (ch >= '1' && ch <= '9') {
          accumulator = ch - '0';
          number_of_digits = 1;
          state = kStateDigit;
          break;
        }

        HandleCharAndBreak('0', kStateZero);
        HandleCharAndBreak('.', kStateDot);
        return Nan();

      case kStateStart:
        if (ch >= '1' && ch <= '9') {
          accumulator = ch - '0';
          number_of_digits = 1;
          state = kStateDigit;
          break;
        }

        if (ch == '-') {
          sign = kNegative;
          state = kStateSign;
          break;
        }

        if (ch == '+') {
          sign = kPositive;
          state = kStateSign;
          break;
        }

        HandleCharAndBreak('0', kStateZero);
        HandleCharAndBreak('.', kStateDot);
        return Nan();

      case kStateZero:
        if (ch == '0')
          break;

        if (ch >= '1' && ch <= '9') {
          accumulator = ch - '0';
          number_of_digits = 1;
          state = kStateDigit;
          break;
        }

        HandleCharAndBreak('.', kStateDot);
        HandleTwoCharsAndBreak('E', 'e', kStateE);
        return Nan();

      default:
        NOTREACHED_IN_MIGRATION();
        return Nan();
    }
  }

  if (state == kStateZero)
    return Zero(sign);

  if (state == kStateDigit || state == kStateEDigit ||
      state == kStateDotDigit) {
    int result_exponent = exponent * (exponent_sign == kNegative ? -1 : 1) -
                          number_of_digits_after_dot + number_of_extra_digits;
    if (result_exponent < kExponentMin)
      return Zero(kPositive);

    const int overflow = result_exponent - kExponentMax + 1;
    if (overflow > 0) {
      if (overflow + number_of_digits - number_of_digits_after_dot > kPrecision)
        return Infinity(sign);
      accumulator = ScaleUp(accumulator, overflow);
      result_exponent -= overflow;
    }

    return Decimal(sign, result_exponent, accumulator);
  }

  return Nan();
}

Decimal Decimal::Infinity(const Sign sign) {
  return Decimal(EncodedData(sign, EncodedData::kClassInfinity));
}

Decimal Decimal::Nan() {
  return Decimal(EncodedData(kPositive, EncodedData::kClassNaN));
}

Decimal Decimal::Remainder(const Decimal& rhs) const {
  const Decimal quotient = *this / rhs;
  return quotient.IsSpecial()
             ? quotient
             : *this - (quotient.IsNegative() ? quotient.Ceil()
                                              : quotient.Floor()) *
                           rhs;
}

Decimal Decimal::Round() const {
  if (IsSpecial())
    return *this;

  if (Exponent() >= 0)
    return *this;

  uint64_t result = data_.Coefficient();
  const int number_of_digits = CountDigits(result);
  const int number_of_drop_digits = -Exponent();
  if (number_of_digits < number_of_drop_digits)
    return Zero(kPositive);

  result = ScaleDown(result, number_of_drop_digits - 1);
  if (result % 10 >= 5)
    result += 10;
  result /= 10;
  return Decimal(GetSign(), 0, result);
}

double Decimal::ToDouble() const {
  if (IsFinite()) {
    bool valid;
    const double double_value = ToString().ToDouble(&valid);
    return valid ? double_value : std::numeric_limits<double>::quiet_NaN();
  }

  if (IsInfinity())
    return IsNegative() ? -std::numeric_limits<double>::infinity()
                        : std::numeric_limits<double>::infinity();

  return std::numeric_limits<double>::quiet_NaN();
}

String Decimal::ToString() const {
  switch (data_.GetFormatClass()) {
    case EncodedData::kClassInfinity:
      return GetSign() ? "-Infinity" : "Infinity";

    case EncodedData::kClassNaN:
      return "NaN";

    case EncodedData::kClassNormal:
    case EncodedData::kClassZero:
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }

  StringBuilder builder;
  if (GetSign())
    builder.Append('-');

  int original_exponent = Exponent();
  uint64_t coefficient = data_.Coefficient();

  if (original_exponent < 0) {
    const int kMaxDigits = DBL_DIG;
    uint64_t last_digit = 0;
    while (CountDigits(coefficient) > kMaxDigits) {
      last_digit = coefficient % 10;
      coefficient /= 10;
      ++original_exponent;
    }

    if (last_digit >= 5)
      ++coefficient;

    while (original_exponent < 0 && coefficient && !(coefficient % 10)) {
      coefficient /= 10;
      ++original_exponent;
    }
  }

  const String digits = String::Number(coefficient);
  int coefficient_length = static_cast<int>(digits.length());
  const int adjusted_exponent = original_exponent + coefficient_length - 1;
  if (original_exponent <= 0 && adjusted_exponent >= -6) {
    if (!original_exponent) {
      builder.Append(digits);
      return builder.ToString();
    }

    if (adjusted_exponent >= 0) {
      for (int i = 0; i < coefficient_length; ++i) {
        builder.Append(digits[i]);
        if (i == adjusted_exponent)
          builder.Append('.');
      }
      return builder.ToString();
    }

    builder.Append("0.");
    for (int i = adjusted_exponent + 1; i < 0; ++i)
      builder.Append('0');

    builder.Append(digits);

  } else {
    builder.Append(digits[0]);
    while (coefficient_length >= 2 && digits[coefficient_length - 1] == '0')
      --coefficient_length;
    if (coefficient_length >= 2) {
      builder.Append('.');
      for (int i = 1; i < coefficient_length; ++i)
        builder.Append(digits[i]);
    }

    if (adjusted_exponent) {
      builder.Append(adjusted_exponent < 0 ? "e" : "e+");
      builder.AppendNumber(adjusted_exponent);
    }
  }
  return builder.ToString();
}

Decimal Decimal::Zero(Sign sign) {
  return Decimal(EncodedData(sign, EncodedData::kClassZero));
}

std::ostream& operator<<(std::ostream& ostream, const Decimal& decimal) {
  Decimal::EncodedData data = decimal.Value();
  return ostream << "encode(" << String::Number(data.Coefficient()).Ascii()
                 << ", " << String::Number(data.Exponent()).Ascii() << ", "
                 << (data.GetSign() == Decimal::kNegative ? "Negative"
                                                          : "Positive")
                 << ")=" << decimal.ToString().Ascii();
}

}  // namespace blink
