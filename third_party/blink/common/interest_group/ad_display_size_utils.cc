// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace blink {

namespace {

blink::AdSize::LengthUnit ConvertUnitStringToUnitEnum(std::string_view input) {
  if (input == "px") {
    return blink::AdSize::LengthUnit::kPixels;
  }

  if (input == "sw") {
    return blink::AdSize::LengthUnit::kScreenWidth;
  }

  if (input == "sh") {
    return blink::AdSize::LengthUnit::kScreenHeight;
  }

  return blink::AdSize::LengthUnit::kInvalid;
}

}  // namespace

std::string ConvertAdDimensionToString(double value, AdSize::LengthUnit units) {
  return base::NumberToString(value) + ConvertAdSizeUnitToString(units);
}

std::string ConvertAdSizeUnitToString(const blink::AdSize::LengthUnit& unit) {
  switch (unit) {
    case blink::AdSize::LengthUnit::kPixels:
      return "px";
    case blink::AdSize::LengthUnit::kScreenWidth:
      return "sw";
    case blink::AdSize::LengthUnit::kScreenHeight:
      return "sh";
    case blink::AdSize::LengthUnit::kInvalid:
      return "";
  }
}

std::string ConvertAdSizeToString(const blink::AdSize& ad_size) {
  DCHECK(IsValidAdSize(ad_size));
  return base::StrCat(
      {ConvertAdDimensionToString(ad_size.width, ad_size.width_units), ",",
       ConvertAdDimensionToString(ad_size.height, ad_size.height_units)});
}

std::tuple<double, blink::AdSize::LengthUnit> ParseAdSizeString(
    std::string_view input) {
  std::string value;
  std::string unit;
  // This regular expression is used to parse the ad size specified in
  // `generateBid()` and `joinAdInterestGroup()`. The input has the format of
  // numbers followed by an optional unit, for example: "100px". Note:
  // 1. We allow leading and trailing spaces, for example: " 100px ".
  // 2. We allow the unit to be ignored, for example: "100" will be parsed as
  // 100 pixels.
  // 3. We allow decimal numbers, for example: "100.123px".
  // 4. We disallow spaces between numbers and the unit, for example: "100 px"
  // is not allowed.
  if (!re2::RE2::FullMatch(
          std::string_view(input),
          R"(^\s*((?:0|(?:[1-9][0-9]*))(?:\.[0-9]+)?)(px|sw|sh)?\s*$)", &value,
          &unit)) {
    // This return value will fail the interest group size validator.
    return {0.0, blink::AdSize::LengthUnit::kInvalid};
  }

  double length_val = 0.0;
  if (!base::StringToDouble(value, &length_val)) {
    return {0.0, blink::AdSize::LengthUnit::kInvalid};
  }

  // If the input consists of pure numbers without an unit, it will be parsed as
  // pixels.
  blink::AdSize::LengthUnit length_units =
      unit.empty() ? blink::AdSize::LengthUnit::kPixels
                   : ConvertUnitStringToUnitEnum(unit);

  return {length_val, length_units};
}

bool IsValidAdSize(const blink::AdSize& size) {
  // Disallow non-positive and non-finite values.
  if (size.width <= 0 || size.height <= 0 || !std::isfinite(size.width) ||
      !std::isfinite(size.height)) {
    return false;
  }

  // Disallow invalid units.
  if (size.width_units == blink::AdSize::LengthUnit::kInvalid ||
      size.height_units == blink::AdSize::LengthUnit::kInvalid) {
    return false;
  }

  return true;
}

}  // namespace blink
