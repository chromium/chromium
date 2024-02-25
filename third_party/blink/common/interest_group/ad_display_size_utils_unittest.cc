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

void RunTest(const std::string& input,
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
  EXPECT_EQ("sh", ConvertAdSizeUnitToString(
                      blink::AdSize::LengthUnit::kScreenHeight));
  EXPECT_TRUE(
      ConvertAdSizeUnitToString(blink::AdSize::LengthUnit::kInvalid).empty());
}

TEST(AdDisplaySizeUtilsTest, ConvertAdSizeToString) {
  // clang-format off
  const AdSize kAdSize1(10, AdSize::LengthUnit::kPixels,
                        15, AdSize::LengthUnit::kPixels);
  EXPECT_EQ(ConvertAdSizeToString(kAdSize1), "10px,15px");

  const AdSize kAdSize2(0.5, AdSize::LengthUnit::kScreenWidth,
                        0.2, AdSize::LengthUnit::kScreenHeight);
  EXPECT_EQ(ConvertAdSizeToString(kAdSize2), "0.5sw,0.2sh");

  const AdSize kAdSize3(0.2, AdSize::LengthUnit::kScreenHeight,
                        0.5, AdSize::LengthUnit::kScreenWidth);
  EXPECT_EQ(ConvertAdSizeToString(kAdSize3), "0.2sh,0.5sw");

  const AdSize kAdSize4(10.5, AdSize::LengthUnit::kPixels,
                        11, AdSize::LengthUnit::kScreenWidth);
  EXPECT_EQ(ConvertAdSizeToString(kAdSize4), "10.5px,11sw");
  // clang-format on
}

TEST(AdDisplaySizeUtilsTest, ConvertAdDimensionToString) {
  EXPECT_EQ(ConvertAdDimensionToString(10, AdSize::LengthUnit::kPixels),
            "10px");
  EXPECT_EQ(ConvertAdDimensionToString(0.5, AdSize::LengthUnit::kScreenWidth),
            "0.5sw");
  EXPECT_EQ(ConvertAdDimensionToString(0.5, AdSize::LengthUnit::kScreenHeight),
            "0.5sh");
}

// Positive test cases.
TEST(AdDisplaySizeUtilsTest, ParseSizeStringWithUnits) {
  RunTest("100px", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("100sw", 100.0, blink::AdSize::LengthUnit::kScreenWidth);
  RunTest("100sh", 100.0, blink::AdSize::LengthUnit::kScreenHeight);
  RunTest("100.0px", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("100.0sw", 100.0, blink::AdSize::LengthUnit::kScreenWidth);
  RunTest("100.0sh", 100.0, blink::AdSize::LengthUnit::kScreenHeight);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringOnlyNumbers) {
  RunTest("100", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("100.0", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersTrailingSpaces) {
  RunTest("100 ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("100   ", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersLeadingSpaces) {
  RunTest(" 100", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("   100", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersTrailingLeadingSpaces) {
  RunTest(" 100 ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("   100 ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest(" 100   ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("   100   ", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringPixelsTrailingSpace) {
  RunTest("100px ", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringPixelsLeadingSpace) {
  RunTest(" 100px", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("   100px", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringPixelsTrailingLeadingSpaces) {
  RunTest(" 100px ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("   100px ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest(" 100px   ", 100.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("   100px   ", 100.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeZeroPixel) {
  RunTest("0", 0.0, blink::AdSize::LengthUnit::kPixels);
  RunTest(" 0 ", 0.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("0.0", 0.0, blink::AdSize::LengthUnit::kPixels);
  RunTest(" 0.0 ", 0.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("0px", 0.0, blink::AdSize::LengthUnit::kPixels);
  RunTest("0.0px", 0.0, blink::AdSize::LengthUnit::kPixels);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersWithDecimal) {
  RunTest("0.1px", 0.1, blink::AdSize::LengthUnit::kPixels);
  RunTest("0.100px", 0.1, blink::AdSize::LengthUnit::kPixels);
}

// Negative test cases.
TEST(AdDisplaySizeUtilsTest, ParseSizeStringNoValue) {
  RunTest("px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest(" px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest(" px ", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNegativeValue) {
  RunTest("-100px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest(" -100px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest(" - 100px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("-0", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("-0px", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersLeadingZero) {
  RunTest("01px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("00px", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringSingleDot) {
  RunTest(".", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringSingleDotWithUnit) {
  RunTest(".px", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringPixelsMiddleSpaces) {
  RunTest("100 px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100   px", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersTrailingDot) {
  RunTest("100.px", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringNumbersLeadingDot) {
  RunTest(".1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringInvalidUnit) {
  RunTest("10in", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringValueAndUnitSwapped) {
  RunTest("px100", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringEmptyString) {
  RunTest("", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringSingleSpace) {
  RunTest(" ", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringMultipleSpaces) {
  RunTest("   ", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringSpacesInNumbers) {
  RunTest("100 1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100. 1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100 1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100 1 px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100 1 . 1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("0 0px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("0 0", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringSpacesInUnit) {
  RunTest("100p x", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100s w", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ParseSizeStringWrongFormat) {
  RunTest("123abc456px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100%px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100pixels", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("varpx", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("var px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100/2px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("100..1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("10e3px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("2-1px", 0.0, blink::AdSize::LengthUnit::kInvalid);
  RunTest("2-1", 0.0, blink::AdSize::LengthUnit::kInvalid);
}

TEST(AdDisplaySizeUtilsTest, ValidAdSize) {
  AdSize ad_size(10.0, AdSize::LengthUnit::kPixels, 5.0,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_TRUE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeInvalidUnits) {
  AdSize ad_size(10.0, AdSize::LengthUnit::kInvalid, 5.0,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeZeroValue) {
  AdSize ad_size(0.0, AdSize::LengthUnit::kPixels, 5.0,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeNegativeValue) {
  AdSize ad_size(-1.0, AdSize::LengthUnit::kPixels, 5.0,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

TEST(AdDisplaySizeUtilsTest, AdSizeInfiniteValue) {
  AdSize ad_size(std::numeric_limits<double>::infinity(),
                 AdSize::LengthUnit::kPixels, 5.0,
                 AdSize::LengthUnit::kScreenWidth);
  EXPECT_FALSE(IsValidAdSize(ad_size));
}

}  // namespace blink
