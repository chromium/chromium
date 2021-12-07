// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/types.h"

#include <cmath>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "media/formats/hls/parse_context.h"
#include "third_party/re2/src/re2/re2.h"

namespace media {
namespace hls {
namespace types {

ParseStatus::Or<DecimalInteger> ParseDecimalInteger(SourceString source_str) {
  static const base::NoDestructor<re2::RE2> decimal_integer_regex("\\d{1,20}");

  const auto str = source_str.Str();

  // Check that the set of characters is allowed: 0-9
  // NOTE: It may be useful to split this into a separate function which
  // extracts the range containing valid characters from a given
  // base::StringPiece. For now that's the caller's responsibility.
  if (!RE2::FullMatch(re2::StringPiece(str.data(), str.size()),
                      *decimal_integer_regex)) {
    return ParseStatusCode::kFailedToParseDecimalInteger;
  }

  DecimalInteger result;
  if (!base::StringToUint64(str, &result)) {
    return ParseStatusCode::kFailedToParseDecimalInteger;
  }

  return result;
}

ParseStatus::Or<DecimalFloatingPoint> ParseDecimalFloatingPoint(
    SourceString source_str) {
  // Utilize signed parsing function
  auto result = ParseSignedDecimalFloatingPoint(source_str);
  if (result.has_error()) {
    return ParseStatusCode::kFailedToParseDecimalFloatingPoint;
  }

  // Decimal-floating-point values may not be negative (including -0.0)
  SignedDecimalFloatingPoint value = std::move(result).value();
  if (std::signbit(value)) {
    return ParseStatusCode::kFailedToParseDecimalFloatingPoint;
  }

  return value;
}

ParseStatus::Or<SignedDecimalFloatingPoint> ParseSignedDecimalFloatingPoint(
    SourceString source_str) {
  // Accept no decimal point, decimal point with leading digits, trailing
  // digits, or both
  static const base::NoDestructor<re2::RE2> decimal_floating_point_regex(
      "-?(\\d+|\\d+\\.|\\.\\d+|\\d+\\.\\d+)");

  const auto str = source_str.Str();

  // Check that the set of characters is allowed: - . 0-9
  // `base::StringToDouble` is not as strict as the HLS spec
  if (!re2::RE2::FullMatch(re2::StringPiece(str.data(), str.size()),
                           *decimal_floating_point_regex)) {
    return ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint;
  }

  DecimalFloatingPoint result;
  const bool success = base::StringToDouble(str, &result);
  if (!success || !std::isfinite(result)) {
    return ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint;
  }

  return result;
}

}  // namespace types
}  // namespace hls
}  // namespace media
