// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"

namespace blink {

namespace {

void RunTest(const base::StringPiece input,
             double expected_val,
             blink::AdSize::LengthUnit expected_unit) {
  auto [out_val, out_units] = blink::ParseAdSizeString(input);
  EXPECT_EQ(out_val, expected_val);
  EXPECT_EQ(out_units, expected_unit);
}

}  // namespace

TEST(AdDisplaySizeUtilsTest, ParseSizeStringPixels) {
  RunTest("200px", 200, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringScreenWidth) {
  RunTest("200sw", 200, blink::AdSize::LengthUnit::kScreenWidth);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringWithSpace) {
  RunTest("200 px", 200, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringWithLotsOfSpaces) {
  RunTest("200       px", 200, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNoValue) {
  RunTest("px", 0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringSpacesButNoValue) {
  RunTest("  px", 0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringValueButNoUnits) {
  RunTest("10", 10, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringInvalidUnit) {
  RunTest("10in", 10, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringValueAndUnitSwapped) {
  RunTest("px200", 0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringEmptyString) {
  RunTest("", 0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringGarbageInString) {
  RunTest("123abc456px", 123, blink::AdSize::LengthUnit::kPixels);
}

}  // namespace blink
