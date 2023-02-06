// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_utils.h"

#include <string>

namespace blink {

namespace {

blink::InterestGroup::Size::LengthUnit ConvertUnitStringToUnitEnum(
    const base::StringPiece input) {
  if (input == "sw") {
    return blink::InterestGroup::Size::LengthUnit::kScreenWidth;
  }

  if (input == "px") {
    return blink::InterestGroup::Size::LengthUnit::kPixels;
  }

  return blink::InterestGroup::Size::LengthUnit::kInvalid;
}

}  // namespace

std::tuple<double, blink::InterestGroup::Size::LengthUnit>
ParseInterestGroupSizeString(const base::StringPiece input) {
  size_t split_pos = input.find_last_of("0123456789. ");

  if (split_pos == std::string::npos) {
    // This return value will fail the interest group size validator.
    return {0, blink::InterestGroup::Size::LengthUnit::kInvalid};
  }

  double length_val = 0;
  // It does not matter if the number saturates. So, we don't need to check the
  // return value here.
  base::StringToDouble(input.substr(0, split_pos + 1), &length_val);
  base::StringPiece length_units_str = input.substr(split_pos + 1);
  blink::InterestGroup::Size::LengthUnit length_units =
      ConvertUnitStringToUnitEnum(length_units_str);

  return {length_val, length_units};
}

}  // namespace blink
