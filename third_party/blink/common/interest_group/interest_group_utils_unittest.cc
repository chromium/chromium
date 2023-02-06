// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_utils.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace blink {

namespace {

void RunTest(const base::StringPiece input,
             double expected_val,
             blink::InterestGroup::Size::LengthUnit expected_unit) {
  auto [out_val, out_units] = blink::ParseInterestGroupSizeString(input);
  EXPECT_EQ(out_val, expected_val);
  EXPECT_EQ(out_units, expected_unit);
}

}  // namespace

TEST(InterestGroupUtilsTest, ParseSizeStringPixels) {
  RunTest("200px", 200, blink::InterestGroup::Size::LengthUnit::kPixels);
}

TEST(InterestGroupUtilsTest, ParseSizeStringScreenWidth) {
  RunTest("200sw", 200, blink::InterestGroup::Size::LengthUnit::kScreenWidth);
}

TEST(InterestGroupUtilsTest, ParseSizeStringWithSpace) {
  RunTest("200 px", 200, blink::InterestGroup::Size::LengthUnit::kPixels);
}

TEST(InterestGroupUtilsTest, ParseSizeStringWithLotsOfSpaces) {
  RunTest("200       px", 200, blink::InterestGroup::Size::LengthUnit::kPixels);
}

TEST(InterestGroupUtilsTest, ParseSizeStringNoValue) {
  RunTest("px", 0, blink::InterestGroup::Size::LengthUnit::kInvalid);
}

TEST(InterestGroupUtilsTest, ParseSizeStringSpacesButNoValue) {
  RunTest("  px", 0, blink::InterestGroup::Size::LengthUnit::kPixels);
}

TEST(InterestGroupUtilsTest, ParseSizeStringValueButNoUnits) {
  RunTest("10", 10, blink::InterestGroup::Size::LengthUnit::kInvalid);
}

TEST(InterestGroupUtilsTest, ParseSizeStringInvalidUnit) {
  RunTest("10in", 10, blink::InterestGroup::Size::LengthUnit::kInvalid);
}

TEST(InterestGroupUtilsTest, ParseSizeStringValueAndUnitSwapped) {
  RunTest("px200", 0, blink::InterestGroup::Size::LengthUnit::kInvalid);
}

TEST(InterestGroupUtilsTest, ParseSizeStringEmptyString) {
  RunTest("", 0, blink::InterestGroup::Size::LengthUnit::kInvalid);
}

TEST(InterestGroupUtilsTest, ParseSizeStringGarbageInString) {
  RunTest("123abc456px", 123, blink::InterestGroup::Size::LengthUnit::kPixels);
}

}  // namespace blink
