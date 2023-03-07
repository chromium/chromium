// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"

#include <limits>
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

TEST(AdDisplaySizeUtilsTest, ConvertAdSizeUnitToString) {
  EXPECT_EQ("px",
            ConvertAdSizeUnitToString(blink::AdSize::LengthUnit::kPixels));
  EXPECT_EQ("sw",
            ConvertAdSizeUnitToString(blink::AdSize::LengthUnit::kScreenWidth));
  EXPECT_TRUE(
      ConvertAdSizeUnitToString(blink::AdSize::LengthUnit::kInvalid).empty());
}

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

TEST(AdDisplaySizeUtilsTest, ValidAdSize) {
  AdSize ad_size(10, AdSize::LengthUnit::kPixels, 5,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_TRUE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeInvalidUnits) {
  AdSize ad_size(10, AdSize::LengthUnit::kInvalid, 5,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeZeroValue) {
  AdSize ad_size(0, AdSize::LengthUnit::kPixels, 5,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeNegativeValue) {
  AdSize ad_size(-1, AdSize::LengthUnit::kPixels, 5,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeInfiniteValue) {
  AdSize ad_size(std::numeric_limits<double>::infinity(),
                 AdSize::LengthUnit::kPixels, 5,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

}  // namespace blink
