// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/color_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

void RunPassCssTest(const std::string& css_string, SkColor expected_result) {
  SkColor color = 0;
  EXPECT_TRUE(ParseCssColorString(css_string, &color));
  EXPECT_EQ(color, expected_result);
}

void RunFailCssTest(const std::string& css_string) {
  SkColor color = 0;
  EXPECT_FALSE(ParseCssColorString(css_string, &color));
}

void RunPassHexTest(const std::string& css_string, SkColor expected_result) {
  SkColor color = 0;
  EXPECT_TRUE(ParseHexColorString(css_string, &color));
  EXPECT_EQ(color, expected_result);
}

void RunFailHexTest(const std::string& css_string) {
  SkColor color = 0;
  EXPECT_FALSE(ParseHexColorString(css_string, &color));
}

void RunPassRgbTest(const std::string& rgb_string, SkColor expected) {
  SkColor color = 0;
  EXPECT_TRUE(ParseRgbColorString(rgb_string, &color));
  EXPECT_EQ(color, expected);
}

void RunFailRgbTest(const std::string& rgb_string) {
  SkColor color = 0;
  EXPECT_FALSE(ParseRgbColorString(rgb_string, &color));
}

TEST(ColorParser, HexNormalCSS) {
  RunPassHexTest("#34006A", SkColorSetARGB(0xFF, 0x34, 0, 0x6A));
}

TEST(ColorParser, HexShorCSS) {
  RunPassHexTest("#A1E", SkColorSetARGB(0xFF, 0xAA, 0x11, 0xEE));
}

TEST(ColorParser, HexWithAlphaCSS) {
  RunPassHexTest("#340061CC", SkColorSetARGB(0xCC, 0x34, 0, 0x61));
}

TEST(ColorParser, HexWithAlphaShortCSS) {
  RunPassHexTest("#A1E9", SkColorSetARGB(0x99, 0xAA, 0x11, 0xEE));
}

TEST(ColorParser, FailCSSNoHash) {
  RunFailHexTest("11FF22");
}

TEST(ColorParser, FailCSSTooShort) {
  RunFailHexTest("#FF22C");
}

TEST(ColorParser, FailCSSTooLong) {
  RunFailHexTest("#FF22128");
}

TEST(ColorParser, FailCSSInvalid) {
  RunFailHexTest("#-22128");
}

TEST(ColorParser, FailHexWithPlus) {
  RunFailHexTest("#+22128");
}

TEST(ColorParser, AcceptHsl) {
  // Run basic color tests.
  RunPassCssTest("hsl(0, 100%, 50%)", SK_ColorRED);
  RunPassCssTest("hsl(120, 100%, 50%)", SK_ColorGREEN);
  RunPassCssTest("hsl(240, 100%, 50%)", SK_ColorBLUE);
  RunPassCssTest("hsl(180, 100%, 50%)", SK_ColorCYAN);

  // Passing in >100% saturation should be equivalent to 100%.
  RunPassCssTest("hsl(120, 200%, 50%)", SK_ColorGREEN);

  // Passing in the same degree +/- full rotations should be equivalent.
  RunPassCssTest("hsl(480, 100%, 50%)", SK_ColorGREEN);
  RunPassCssTest("hsl(-240, 100%, 50%)", SK_ColorGREEN);

  // We should be able to parse doubles
  RunPassCssTest("hsl(120.0, 100.0%, 50.0%)", SK_ColorGREEN);
}

TEST(ColorParser, InvalidHsl) {
  RunFailCssTest("(0,100%,50%)");
  RunFailCssTest("[0, 100, 50]");
  RunFailCssTest("hs l(0,100%,50%)");
  RunFailCssTest("rgb(0,100%,50%)");
  RunFailCssTest("hsl(0,100%)");
  RunFailCssTest("hsl(100%,50%)");
  RunFailCssTest("hsl(120, 100, 50)");
  RunFailCssTest("hsl[120, 100%, 50%]");
  RunFailCssTest("hsl(120, 100%, 50%, 1.0)");
  RunFailCssTest("hsla(120, 100%, 50%)");
}

TEST(ColorParser, AcceptHsla) {
  // Run basic color tests.
  RunPassCssTest("hsla(0, 100%, 50%, 1.0)", SK_ColorRED);
  RunPassCssTest("hsla(0, 100%, 50%, 0.0)",
                 SkColorSetARGB(0x00, 0xFF, 0x00, 0x00));
  RunPassCssTest("hsla(0, 100%, 50%, 0.5)",
                 SkColorSetARGB(0x7F, 0xFF, 0x00, 0x00));
  RunPassCssTest("hsla(0, 100%, 50%, 0.25)",
                 SkColorSetARGB(0x3F, 0xFF, 0x00, 0x00));
  RunPassCssTest("hsla(0, 100%, 50%, 0.75)",
                 SkColorSetARGB(0xBF, 0xFF, 0x00, 0x00));

  // We should able to parse integer alpha value.
  RunPassCssTest("hsla(0, 100%, 50%, 1)", SK_ColorRED);
}

TEST(ColorParser, AcceptRgb) {
  // Run basic color tests.
  RunPassRgbTest("rgb(255,0,0)", SK_ColorRED);
  RunPassRgbTest("rgb(0,    255, 0)", SK_ColorGREEN);
  RunPassRgbTest("rgb(0, 0, 255)", SK_ColorBLUE);
}

TEST(ColorParser, InvalidRgb) {
  RunFailRgbTest("(0,100,50)");
  RunFailRgbTest("[0, 100, 50]");
  RunFailRgbTest("rg b(0,100,50)");
  RunFailRgbTest("rgb(0,-100, 10)");
  RunFailRgbTest("rgb(100,50)");
  RunFailRgbTest("rgb(120.0, 100.6, 50.3)");
  RunFailRgbTest("rgb[120, 100, 50]");
  RunFailRgbTest("rgb(120, 100, 50, 1.0)");
  RunFailRgbTest("rgba(120, 100, 50)");
  RunFailRgbTest("rgb(0, 300, 0)");
  // This is valid RGB but we don't support percentages yet.
  RunFailRgbTest("rgb(100%, 0%, 100%)");
}

TEST(ColorParser, AcceptRgba) {
  // Run basic color tests.
  RunPassRgbTest("rgba(255, 0, 0, 1.0)", SK_ColorRED);
  RunPassRgbTest("rgba(255, 0, 0, 0.0)",
                 SkColorSetARGB(0x00, 0xFF, 0x00, 0x00));
  RunPassRgbTest("rgba(255, 0, 0, 0.5)",
                 SkColorSetARGB(0x7F, 0xFF, 0x00, 0x00));
  RunPassRgbTest("rgba(255, 0, 0, 0.25)",
                 SkColorSetARGB(0x3F, 0xFF, 0x00, 0x00));
  RunPassRgbTest("rgba(255, 0, 0, 0.75)",
                 SkColorSetARGB(0xBF, 0xFF, 0x00, 0x00));

  // We should able to parse an integer alpha value.
  RunPassRgbTest("rgba(255, 0, 0, 1)", SK_ColorRED);
}

TEST(ColorParser, CssBasicKeyword) {
  RunPassCssTest("red", SK_ColorRED);
  RunPassCssTest("blue", SK_ColorBLUE);

  RunFailCssTest("my_red");
}

}  // namespace content
