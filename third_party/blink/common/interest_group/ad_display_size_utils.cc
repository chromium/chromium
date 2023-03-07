// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"

namespace blink {

namespace {

blink::AdSize::LengthUnit ConvertUnitStringToUnitEnum(
    const base::StringPiece input) {
  if (input == "sw") {
    return blink::AdSize::LengthUnit::kScreenWidth;
  }

  if (input == "px") {
    return blink::AdSize::LengthUnit::kPixels;
  }

  return blink::AdSize::LengthUnit::kInvalid;
}

}  // namespace

std::string ConvertAdSizeUnitToString(const blink::AdSize::LengthUnit& unit) {
  switch (unit) {
    case blink::AdSize::LengthUnit::kPixels:
      return "px";
    case blink::AdSize::LengthUnit::kScreenWidth:
      return "sw";
    case blink::AdSize::LengthUnit::kInvalid:
      return "";
  }
}

std::tuple<double, blink::AdSize::LengthUnit> ParseAdSizeString(
    const base::StringPiece input) {
  size_t split_pos = input.find_last_of("0123456789. ");

  if (split_pos == std::string::npos) {
    // This return value will fail the interest group size validator.
    return {0, blink::AdSize::LengthUnit::kInvalid};
  }

  double length_val = 0;
  // It does not matter if the number saturates. So, we don't need to check the
  // return value here.
  base::StringToDouble(input.substr(0, split_pos + 1), &length_val);
  base::StringPiece length_units_str = input.substr(split_pos + 1);
  blink::AdSize::LengthUnit length_units =
      ConvertUnitStringToUnitEnum(length_units_str);

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
