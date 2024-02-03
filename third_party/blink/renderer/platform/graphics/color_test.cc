/*
 * Copyright (c) 2022, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/color.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

Color CreateSRGBColor(float r, float g, float b, float a) {
  return Color::FromColorSpace(Color::ColorSpace::kSRGB, r, g, b, a);
}

// Helper struct for testing purposes.
struct ColorMixTest {
  Color::ColorSpace mix_space;
  std::optional<Color::HueInterpolationMethod> hue_method;
  Color color_left;
  Color color_right;
  float percentage_right;
  float alpha_multiplier;
  Color color_expected;
};

// Helper struct for testing purposes.
struct ColorTest {
  Color color;
  Color color_expected;
};
}  // namespace

TEST(BlinkColor, ColorMixSameColorSpace) {
  ColorMixTest color_mix_tests[] = {
      {Color::ColorSpace::kSRGB, std::nullopt,
       CreateSRGBColor(1.0f, 0.0f, 0.0f, 1.0f),
       CreateSRGBColor(0.0f, 1.0f, 0.0f, 1.0f),
       /*percentage =*/0.5f, /*alpha_multiplier=*/1.0f,
       CreateSRGBColor(0.5f, 0.5f, 0.0f, 1.0f)},
      {Color::ColorSpace::kSRGB, std::nullopt,
       Color::FromColorSpace(Color::ColorSpace::kRec2020, 0.7919771358198009f,
                             0.23097568481079767f, 0.07376147493817597f, 1.0f),
       Color::FromColorSpace(Color::ColorSpace::kLab, 87.81853633115202f,
                             -79.27108223854806f, 80.99459785152247f, 1.0f),
       /*percentage =*/0.5f, /*alpha_multiplier=*/1.0f,
       CreateSRGBColor(0.5f, 0.5f, 0.0f, 1.0f)},
      {Color::ColorSpace::kSRGB, std::nullopt,
       CreateSRGBColor(1.0f, 0.0f, 0.0f, 1.0f),
       CreateSRGBColor(0.0f, 1.0f, 0.0f, 1.0f),
       /*percentage =*/0.25f, /*alpha_multiplier=*/0.5f,
       CreateSRGBColor(0.75f, 0.25f, 0.0f, 0.5f)},
      // Value obtained form the spec https://www.w3.org/TR/css-color-5/.
      {Color::ColorSpace::kSRGB, std::nullopt,
       CreateSRGBColor(1.0f, 0.0f, 0.0f, 0.7f),
       CreateSRGBColor(0.0f, 1.0f, 0.0f, 0.2f),
       /*percentage =*/0.75f, /*alpha_multiplier=*/1.0f,
       CreateSRGBColor(0.53846f, 0.46154f, 0.0f, 0.325f)}};
  for (auto& color_mix_test : color_mix_tests) {
    Color result = Color::FromColorMix(
        color_mix_test.mix_space, color_mix_test.hue_method,
        color_mix_test.color_left, color_mix_test.color_right,
        color_mix_test.percentage_right, color_mix_test.alpha_multiplier);
    EXPECT_EQ(result.GetColorSpace(), color_mix_test.mix_space);
    SkColor4f resultSkColor = result.toSkColor4f();
    SkColor4f expectedSkColor = color_mix_test.color_expected.toSkColor4f();
    EXPECT_NEAR(resultSkColor.fR, expectedSkColor.fR, 0.001f)
        << "Mixing " << color_mix_test.color_left.toSkColor4f().fR << " "
        << color_mix_test.color_left.toSkColor4f().fG << " "
        << color_mix_test.color_left.toSkColor4f().fB << " "
        << color_mix_test.color_left.toSkColor4f().fA << " and "
        << color_mix_test.color_right.toSkColor4f().fR << " "
        << color_mix_test.color_right.toSkColor4f().fG << " "
        << color_mix_test.color_right.toSkColor4f().fB << " "
        << color_mix_test.color_right.toSkColor4f().fA << " produced "
        << resultSkColor.fR << " " << resultSkColor.fG << " "
        << resultSkColor.fB << " " << resultSkColor.fA
        << " and it was expecting " << expectedSkColor.fR << " "
        << expectedSkColor.fG << " " << expectedSkColor.fB << " "
        << expectedSkColor.fA;
    EXPECT_NEAR(resultSkColor.fG, expectedSkColor.fG, 0.001f)
        << "Mixing " << color_mix_test.color_left.toSkColor4f().fR << " "
        << color_mix_test.color_left.toSkColor4f().fG << " "
        << color_mix_test.color_left.toSkColor4f().fB << " "
        << color_mix_test.color_left.toSkColor4f().fA << " and "
        << color_mix_test.color_right.toSkColor4f().fR << " "
        << color_mix_test.color_right.toSkColor4f().fG << " "
        << color_mix_test.color_right.toSkColor4f().fB << " "
        << color_mix_test.color_right.toSkColor4f().fA << " produced "
        << resultSkColor.fR << " " << resultSkColor.fG << " "
        << resultSkColor.fB << " " << resultSkColor.fA
        << " and it was expecting " << expectedSkColor.fR << " "
        << expectedSkColor.fG << " " << expectedSkColor.fB << " "
        << expectedSkColor.fA;
    EXPECT_NEAR(resultSkColor.fB, expectedSkColor.fB, 0.001f)
        << "Mixing " << color_mix_test.color_left.toSkColor4f().fR << " "
        << color_mix_test.color_left.toSkColor4f().fG << " "
        << color_mix_test.color_left.toSkColor4f().fB << " "
        << color_mix_test.color_left.toSkColor4f().fA << " and "
        << color_mix_test.color_right.toSkColor4f().fR << " "
        << color_mix_test.color_right.toSkColor4f().fG << " "
        << color_mix_test.color_right.toSkColor4f().fB << " "
        << color_mix_test.color_right.toSkColor4f().fA << " produced "
        << resultSkColor.fR << " " << resultSkColor.fG << " "
        << resultSkColor.fB << " " << resultSkColor.fA
        << " and it was expecting " << expectedSkColor.fR << " "
        << expectedSkColor.fG << " " << expectedSkColor.fB << " "
        << expectedSkColor.fA;
    EXPECT_NEAR(resultSkColor.fA, expectedSkColor.fA, 0.001f)
        << "Mixing " << color_mix_test.color_left.toSkColor4f().fR << " "
        << color_mix_test.color_left.toSkColor4f().fG << " "
        << color_mix_test.color_left.toSkColor4f().fB << " "
        << color_mix_test.color_left.toSkColor4f().fA << " and "
        << color_mix_test.color_right.toSkColor4f().fR << " "
        << color_mix_test.color_right.toSkColor4f().fG << " "
        << color_mix_test.color_right.toSkColor4f().fB << " "
        << color_mix_test.color_right.toSkColor4f().fA << " produced "
        << resultSkColor.fR << " " << resultSkColor.fG << " "
        << resultSkColor.fB << " " << resultSkColor.fA
        << " and it was expecting " << expectedSkColor.fR << " "
        << expectedSkColor.fG << " " << expectedSkColor.fB << " "
        << expectedSkColor.fA;
  }
}

TEST(BlinkColor, ColorMixNone) {
  Color color1 = Color::FromColorSpace(Color::ColorSpace::kXYZD50, std::nullopt,
                                       0.5f, std::nullopt, 1.0f);
  Color color2 = Color::FromColorSpace(Color::ColorSpace::kXYZD50, std::nullopt,
                                       std::nullopt, 0.7f, 1.0f);

  Color result = Color::FromColorMix(
      Color::ColorSpace::kXYZD50, /*hue_method=*/std::nullopt, color1, color2,
      /*percentage=*/0.5f, /*alpha_multiplier=*/1.0f);

  EXPECT_EQ(result.param0_is_none_, true);
  EXPECT_EQ(result.param1_is_none_, false);
  EXPECT_EQ(result.param1_, color1.param1_);
  EXPECT_EQ(result.param2_is_none_, false);
  EXPECT_EQ(result.param2_, color2.param2_);
}

TEST(BlinkColor, ColorInterpolation) {
  struct ColorsTest {
    Color color1;
    Color color2;
    Color::ColorSpace space;
    std::optional<Color::HueInterpolationMethod> hue_method;
    float percentage;
    Color expected;
  };

  // Tests extracted from the CSS Color 4 spec, among others.
  // https://csswg.sesse.net/css-color-4/#interpolation-alpha
  ColorsTest colors_test[] = {
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, std::nullopt, 0.12f,
                             0.98f, 1.0f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.62f, 0.26f, 0.64f,
                             1.0f),
       Color::ColorSpace::kSRGB, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.62f, 0.19f, 0.81f,
                             1.0f)},

      {Color::FromColorSpace(Color::ColorSpace::kHSL, std::nullopt, 0.5f, 0.5f,
                             1.0f),
       Color::FromColorSpace(Color::ColorSpace::kHSL, 180.0f, 0.1f, 0.1f, 1.0f),
       Color::ColorSpace::kHSL, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kHSL, 180.0f, 0.3f, 0.3f,
                             1.0f)},

      {Color::FromColorSpace(Color::ColorSpace::kHWB, std::nullopt, 0.5f, 0.5f,
                             1.0f),
       Color::FromColorSpace(Color::ColorSpace::kHWB, 180.0f, 0.1f, 0.1f, 1.0f),
       Color::ColorSpace::kHWB, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kHWB, 180.0f, 0.3f, 0.3f,
                             1.0f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.5f, std::nullopt, 1.0f,
                             1.0f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 1.0f, 0.5f, 0.0f, 1.0f),
       Color::ColorSpace::kSRGB, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.75f, 0.5f, 0.5f,
                             1.0f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, .5f, 0.0f, 0.0f,
                             std::nullopt),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 1.f, 0.5f, 1.0f, 1.0f),
       Color::ColorSpace::kSRGB, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.75f, 0.25f, 0.5f,
                             1.0f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.24f, 0.12f, 0.98f,
                             0.4f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.62f, 0.26f, 0.64f,
                             0.6f),
       Color::ColorSpace::kSRGB, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.468f, 0.204f, 0.776f,
                             0.5f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.76f, 0.62f, 0.03f,
                             0.4f),
       Color::FromColorSpace(Color::ColorSpace::kDisplayP3, 0.84f, 0.19f, 0.72f,
                             0.6f),
       Color::ColorSpace::kLab, std::nullopt, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kLab, 58.873f, 51.552f, 7.108f,
                             0.5f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.76f, 0.62f, 0.03f,
                             0.4f),
       Color::FromColorSpace(Color::ColorSpace::kDisplayP3, 0.84f, 0.19f, 0.72f,
                             0.6f),
       Color::ColorSpace::kLch, Color::HueInterpolationMethod::kShorter, 0.5f,
       Color::FromColorSpace(Color::ColorSpace::kLch, 58.873f, 81.126f, 31.82f,
                             0.5f)}};

  for (auto& color_test : colors_test) {
    Color result = Color::InterpolateColors(
        color_test.space, color_test.hue_method, color_test.color1,
        color_test.color2, color_test.percentage);
    EXPECT_NEAR(result.param0_, color_test.expected.param0_, 0.01f)
        << "Mixing \n"
        << color_test.color1.param0_is_none_ << ' ' << color_test.color1.param0_
        << " " << color_test.color1.param1_is_none_ << ' '
        << color_test.color1.param1_ << " " << color_test.color1.param2_is_none_
        << ' ' << color_test.color1.param2_ << " "
        << color_test.color1.alpha_is_none_ << ' ' << color_test.color1.alpha_
        << " and \n"
        << color_test.color2.param0_is_none_ << ' ' << color_test.color2.param0_
        << " " << color_test.color2.param1_is_none_ << ' '
        << color_test.color2.param1_ << " " << color_test.color2.param2_is_none_
        << ' ' << color_test.color2.param2_ << " "
        << color_test.color2.alpha_is_none_ << ' ' << color_test.color2.alpha_
        << " produced\n"
        << result.param0_is_none_ << ' ' << result.param0_ << " "
        << result.param1_is_none_ << ' ' << result.param1_ << " "
        << result.param2_is_none_ << ' ' << result.param2_ << " "
        << result.alpha_is_none_ << ' ' << result.alpha_
        << " and it was expecting \n"
        << color_test.expected.param0_is_none_ << ' '
        << color_test.expected.param0_ << " "
        << color_test.expected.param1_is_none_ << ' '
        << color_test.expected.param1_ << " "
        << color_test.expected.param2_is_none_ << ' '
        << color_test.expected.param2_ << " "
        << color_test.expected.alpha_is_none_ << ' '
        << color_test.expected.alpha_;
    EXPECT_NEAR(result.param1_, color_test.expected.param1_, 0.01f)
        << "Mixing \n"
        << color_test.color1.param0_is_none_ << ' ' << color_test.color1.param0_
        << " " << color_test.color1.param1_is_none_ << ' '
        << color_test.color1.param1_ << " " << color_test.color1.param2_is_none_
        << ' ' << color_test.color1.param2_ << " "
        << color_test.color1.alpha_is_none_ << ' ' << color_test.color1.alpha_
        << " \n"
        << color_test.color2.param0_is_none_ << ' ' << color_test.color2.param0_
        << " " << color_test.color2.param1_is_none_ << ' '
        << color_test.color2.param1_ << " " << color_test.color2.param2_is_none_
        << ' ' << color_test.color2.param2_ << " "
        << color_test.color2.alpha_is_none_ << ' ' << color_test.color2.alpha_
        << " produced \n"
        << result.param0_is_none_ << ' ' << result.param0_ << " "
        << result.param1_is_none_ << ' ' << result.param1_ << " "
        << result.param2_is_none_ << ' ' << result.param2_ << " "
        << result.alpha_is_none_ << ' ' << result.alpha_
        << " and it was expecting \n"
        << color_test.expected.param0_is_none_ << ' '
        << color_test.expected.param0_ << " "
        << color_test.expected.param1_is_none_ << ' '
        << color_test.expected.param1_ << " "
        << color_test.expected.param2_is_none_ << ' '
        << color_test.expected.param2_ << " "
        << color_test.expected.alpha_is_none_ << ' '
        << color_test.expected.alpha_;
    EXPECT_NEAR(result.param2_, color_test.expected.param2_, 0.01f)
        << "Mixing \n"
        << color_test.color1.param0_is_none_ << ' ' << color_test.color1.param0_
        << " " << color_test.color1.param1_is_none_ << ' '
        << color_test.color1.param1_ << " " << color_test.color1.param2_is_none_
        << ' ' << color_test.color1.param2_ << " "
        << color_test.color1.alpha_is_none_ << ' ' << color_test.color1.alpha_
        << " \n"
        << color_test.color2.param0_is_none_ << ' ' << color_test.color2.param0_
        << " " << color_test.color2.param1_is_none_ << ' '
        << color_test.color2.param1_ << " " << color_test.color2.param2_is_none_
        << ' ' << color_test.color2.param2_ << " "
        << color_test.color2.alpha_is_none_ << ' ' << color_test.color2.alpha_
        << " produced \n"
        << result.param0_is_none_ << ' ' << result.param0_ << " "
        << result.param1_is_none_ << ' ' << result.param1_ << " "
        << result.param2_is_none_ << ' ' << result.param2_ << " "
        << result.alpha_is_none_ << ' ' << result.alpha_
        << " and it was expecting \n"
        << color_test.expected.param0_is_none_ << ' '
        << color_test.expected.param0_ << " "
        << color_test.expected.param1_is_none_ << ' '
        << color_test.expected.param1_ << " "
        << color_test.expected.param2_is_none_ << ' '
        << color_test.expected.param2_ << " "
        << color_test.expected.alpha_is_none_ << ' '
        << color_test.expected.alpha_;
    EXPECT_NEAR(result.alpha_, color_test.expected.alpha_, 0.01f)
        << "Mixing \n"
        << color_test.color1.param0_is_none_ << ' ' << color_test.color1.param0_
        << " " << color_test.color1.param1_is_none_ << ' '
        << color_test.color1.param1_ << " " << color_test.color1.param2_is_none_
        << ' ' << color_test.color1.param2_ << " "
        << color_test.color1.alpha_is_none_ << ' ' << color_test.color1.alpha_
        << " \n"
        << color_test.color2.param0_is_none_ << ' ' << color_test.color2.param0_
        << " " << color_test.color2.param1_is_none_ << ' '
        << color_test.color2.param1_ << " " << color_test.color2.param2_is_none_
        << ' ' << color_test.color2.param2_ << " "
        << color_test.color2.alpha_is_none_ << ' ' << color_test.color2.alpha_
        << " produced \n"
        << result.param0_is_none_ << ' ' << result.param0_ << " "
        << result.param1_is_none_ << ' ' << result.param1_ << " "
        << result.param2_is_none_ << ' ' << result.param2_ << " "
        << result.alpha_is_none_ << ' ' << result.alpha_
        << " and it was expecting \n"
        << color_test.expected.param0_is_none_ << ' '
        << color_test.expected.param0_ << " "
        << color_test.expected.param1_is_none_ << ' '
        << color_test.expected.param1_ << " "
        << color_test.expected.param2_is_none_ << ' '
        << color_test.expected.param2_ << " "
        << color_test.expected.alpha_is_none_ << ' '
        << color_test.expected.alpha_;
  }
}

TEST(BlinkColor, HueInterpolation) {
  struct HueTest {
    float value1;
    float value2;
    float percentage;
    Color::HueInterpolationMethod method;
    float expected;
  };

  auto HueMethodToString = [](Color::HueInterpolationMethod method) {
    switch (method) {
      case Color::HueInterpolationMethod::kShorter:
        return "shorter";
      case Color::HueInterpolationMethod::kLonger:
        return "kLonger";
      case Color::HueInterpolationMethod::kIncreasing:
        return "kIncreasing";
      case Color::HueInterpolationMethod::kDecreasing:
        return "kDecreasing";
    }
  };

  HueTest hue_tests[] = {
      {60.0f, 330.0f, 0.0f, Color::HueInterpolationMethod::kShorter, 60.0f},
      {60.0f, 330.0f, 1.0f, Color::HueInterpolationMethod::kShorter, 330.0f},
      {60.0f, 330.0f, 0.7f, Color::HueInterpolationMethod::kShorter, 357.0f},
      {60.0f, 330.0f, 0.0f, Color::HueInterpolationMethod::kLonger, 60.0f},
      {60.0f, 330.0f, 1.0f, Color::HueInterpolationMethod::kLonger, 330.0f},
      {60.0f, 330.0f, 0.7f, Color::HueInterpolationMethod::kLonger, 249.0f},
      {60.0f, 330.0f, 0.0f, Color::HueInterpolationMethod::kIncreasing, 60.0f},
      {60.0f, 330.0f, 1.0f, Color::HueInterpolationMethod::kIncreasing, 330.0f},
      {60.0f, 330.0f, 0.7f, Color::HueInterpolationMethod::kIncreasing, 249.0f},
      {60.0f, 330.0f, 0.0f, Color::HueInterpolationMethod::kDecreasing, 60.0f},
      {60.0f, 330.0f, 1.0f, Color::HueInterpolationMethod::kDecreasing, 330.0f},
      {60.0f, 330.0f, 0.7f, Color::HueInterpolationMethod::kDecreasing, 357.0f},
      {60.0f, 90.0f, 0.0f, Color::HueInterpolationMethod::kShorter, 60.0f},
      {60.0f, 90.0f, 1.0f, Color::HueInterpolationMethod::kShorter, 90.0f},
      {60.0f, 90.0f, 0.7f, Color::HueInterpolationMethod::kShorter, 81.0f},
      {60.0f, 90.0f, 0.0f, Color::HueInterpolationMethod::kLonger, 60.0f},
      {60.0f, 90.0f, 1.0f, Color::HueInterpolationMethod::kLonger, 90.0f},
      {60.0f, 90.0f, 0.7f, Color::HueInterpolationMethod::kLonger, 189.0f},
      {60.0f, 90.0f, 0.0f, Color::HueInterpolationMethod::kIncreasing, 60.0f},
      {60.0f, 90.0f, 1.0f, Color::HueInterpolationMethod::kIncreasing, 90.0f},
      {60.0f, 90.0f, 0.7f, Color::HueInterpolationMethod::kIncreasing, 81.0f},
      {60.0f, 90.0f, 0.0f, Color::HueInterpolationMethod::kDecreasing, 60.0f},
      {60.0f, 90.0f, 1.0f, Color::HueInterpolationMethod::kDecreasing, 90.0f},
      {60.0f, 90.0f, 0.7f, Color::HueInterpolationMethod::kDecreasing, 189.0f},
  };

  for (auto& hue_test : hue_tests) {
    float result = Color::HueInterpolation(
        hue_test.value1, hue_test.value2, hue_test.percentage, hue_test.method);

    EXPECT_NEAR(result, hue_test.expected, 0.01f)
        << hue_test.value1 << ' ' << hue_test.value2 << ' '
        << hue_test.percentage << ' ' << HueMethodToString(hue_test.method)
        << " produced " << result << " but was expecting " << hue_test.expected;
  }
}

TEST(BlinkColor, toSkColor4fValidation) {
  struct ColorFunctionValues {
    Color::ColorSpace color_space;
    float param0;
    float param1;
    float param2;
  };

  ColorFunctionValues color_function_values[] = {
      {Color::ColorSpace::kSRGB, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kSRGBLinear, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kDisplayP3, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kA98RGB, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kProPhotoRGB, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kRec2020, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kXYZD50, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kXYZD65, 1.0f, 0.7f, 0.2f},
      {Color::ColorSpace::kLab, 87.82f, -79.3f, 80.99f},
      {Color::ColorSpace::kOklab, 0.421f, 0.165f, -0.1f},
      {Color::ColorSpace::kLch, 29.69f, 56.11f, 327.1f},
      {Color::ColorSpace::kOklch, 0.628f, 0.225f, 0.126f},
      {Color::ColorSpace::kSRGBLegacy, 0.7f, 0.5f, 0.0f},
      {Color::ColorSpace::kHSL, 4.0f, 0.5f, 0.0f},
      {Color::ColorSpace::kHWB, 4.0f, 0.5f, 0.0f}};

  Color::ColorSpace color_interpolation_space[] = {
      Color::ColorSpace::kXYZD65,     Color::ColorSpace::kXYZD50,
      Color::ColorSpace::kSRGBLinear, Color::ColorSpace::kLab,
      Color::ColorSpace::kOklab,      Color::ColorSpace::kLch,
      Color::ColorSpace::kOklch,      Color::ColorSpace::kSRGB,
      Color::ColorSpace::kSRGBLegacy, Color::ColorSpace::kHSL,
      Color::ColorSpace::kHWB,        Color::ColorSpace::kNone};

  for (auto& space : color_interpolation_space) {
    for (auto& color_function_value : color_function_values) {
      // To validate if the color conversions are done correctly, we will
      // convert all input to SkColor4f and then convert the input to the
      // ColorInterpolationSpace, and then that one to SkColor4f. Those two
      // values should be the same, if the transformations are correct.
      // ToSkColor4f is validate in color_conversions_test.cc.
      Color input;
      input = Color::FromColorSpace(
          color_function_value.color_space, color_function_value.param0,
          color_function_value.param1, color_function_value.param2, 1.0f);

      SkColor4f expected_output = input.toSkColor4f();
      input.ConvertToColorSpace(space);
      SkColor4f output = input.toSkColor4f();

      EXPECT_NEAR(expected_output.fR, output.fR, 0.01f)
          << "Converting from "
          << Color::ColorSpaceToString(color_function_value.color_space)
          << " to " << Color::ColorSpaceToString(space);
      EXPECT_NEAR(expected_output.fG, output.fG, 0.01f)
          << "Converting from "
          << Color::ColorSpaceToString(color_function_value.color_space)
          << " to " << Color::ColorSpaceToString(space);
      EXPECT_NEAR(expected_output.fB, output.fB, 0.01f)
          << "Converting from "
          << Color::ColorSpaceToString(color_function_value.color_space)
          << " to " << Color::ColorSpaceToString(space);
    }
  }
}

TEST(BlinkColor, ExportAsXYZD50Floats) {
  Color::ColorSpace color_spaces[] = {
      Color::ColorSpace::kXYZD65,     Color::ColorSpace::kXYZD50,
      Color::ColorSpace::kSRGBLinear, Color::ColorSpace::kLab,
      Color::ColorSpace::kOklab,      Color::ColorSpace::kLch,
      Color::ColorSpace::kOklch,      Color::ColorSpace::kSRGB,
      Color::ColorSpace::kHSL,        Color::ColorSpace::kHWB,
      Color::ColorSpace::kDisplayP3,  Color::ColorSpace::kProPhotoRGB,
      Color::ColorSpace::kRec2020,    Color::ColorSpace::kA98RGB};

  struct FloatValues {
    float x;
    float y;
    float z;
  };
  FloatValues input_parameters[] = {
      {0.5f, 0.0f, 1.0f},
      {0.6f, 0.2f, 0.2f},
      {0.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 1.0f},
  };

  for (auto& input_parameter : input_parameters) {
    Color expected =
        Color::FromColorSpace(Color::ColorSpace::kXYZD50, input_parameter.x,
                                 input_parameter.y, input_parameter.z, 1.0f);
    for (auto& space : color_spaces) {
      Color input = Color::FromColorSpace(
          Color::ColorSpace::kXYZD50, input_parameter.x, input_parameter.y,
          input_parameter.z, 1.0f);
      input.ConvertToColorSpace(space);
      auto [x, y, z] = input.ExportAsXYZD50Floats();

      EXPECT_NEAR(x, expected.param0_, 0.01f)
          << "Converting through " << Color::ColorSpaceToString(space);
      EXPECT_NEAR(y, expected.param1_, 0.01f)
          << "Converting through " << Color::ColorSpaceToString(space);
      EXPECT_NEAR(z, expected.param2_, 0.01f)
          << "Converting through " << Color::ColorSpaceToString(space);
    }
  }
}

TEST(BlinkColor, Premultiply) {
  ColorTest color_tests[] = {
      // Testing rectangular-color-space premultiplication.
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.24f, 0.12f, 0.98f,
                             0.4f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.24f * 0.4f,
                             0.12f * 0.4f, 0.98f * 0.4f, 1.0f)},
      // Testing none value in each component premultiplication.
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, std::nullopt, 0.26f,
                             0.64f, 0.6f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, std::nullopt,
                             0.26f * 0.6f, 0.64f * 0.6f, 1.0f)},
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.26f, std::nullopt,
                             0.64f, 0.6f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.26f * 0.6f,
                             std::nullopt, 0.64f * 0.6f, 1.0f)},
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.26f, 0.64f,
                             std::nullopt, 0.6f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.26f * 0.6f,
                             0.64f * 0.6f, std::nullopt, 1.0f)},
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 1.0f, 0.8f, 0.0f,
                             std::nullopt),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 1.0f, 0.8f, 0.0f,
                             std::nullopt)},
      // Testing polar-color-space premultiplication. Hue component should not
      // be premultiplied.
      {Color::FromColorSpace(Color::ColorSpace::kLch, 0.24f, 0.12f, 0.98f,
                             0.4f),
       Color::FromColorSpace(Color::ColorSpace::kLch, 0.24f * 0.4f,
                             0.12f * 0.4f, 0.98f, 1.0f)},
      {Color::FromColorSpace(Color::ColorSpace::kOklch, 0.24f, 0.12f, 0.98f,
                             0.4f),
       Color::FromColorSpace(Color::ColorSpace::kOklch, 0.24f * 0.4f,
                             0.12f * 0.4f, 0.98f, 1.0f)}};

  for (auto& color_test : color_tests) {
    color_test.color.PremultiplyColor();

    if (color_test.color.param0_is_none_) {
      EXPECT_EQ(color_test.color.param0_is_none_,
                color_test.color_expected.param0_is_none_);
    } else {
      EXPECT_NEAR(color_test.color.param0_, color_test.color_expected.param0_,
                  0.001f)
          << "Premultiplying generated " << color_test.color.param0_ << " "
          << color_test.color.param1_ << " " << color_test.color.param2_ << " "
          << color_test.color.alpha_ << " and it was expecting "
          << color_test.color_expected.param0_ << " "
          << color_test.color_expected.param1_ << " "
          << color_test.color_expected.param2_ << " "
          << color_test.color_expected.alpha_;
    }
    if (color_test.color_expected.param1_is_none_) {
      EXPECT_EQ(color_test.color.param1_is_none_,
                color_test.color_expected.param1_is_none_);
    } else {
      EXPECT_NEAR(color_test.color.param1_, color_test.color_expected.param1_,
                  0.001f)
          << "Premultiplying generated " << color_test.color.param0_ << " "
          << color_test.color.param1_ << " " << color_test.color.param2_ << " "
          << color_test.color.alpha_ << " and it was expecting "
          << color_test.color_expected.param0_ << " "
          << color_test.color_expected.param1_ << " "
          << color_test.color_expected.param2_ << " "
          << color_test.color_expected.alpha_;
    }
    if (color_test.color_expected.param2_is_none_) {
      EXPECT_EQ(color_test.color.param2_is_none_,
                color_test.color_expected.param2_is_none_);
    } else {
      EXPECT_NEAR(color_test.color.param2_, color_test.color_expected.param2_,
                  0.001f)
          << "Premultiplying generated " << color_test.color.param0_ << " "
          << color_test.color.param1_ << " " << color_test.color.param2_ << " "
          << color_test.color.alpha_ << " and it was expecting "
          << color_test.color_expected.param0_ << " "
          << color_test.color_expected.param1_ << " "
          << color_test.color_expected.param2_ << " "
          << color_test.color_expected.alpha_;
    }
    if (color_test.color_expected.alpha_is_none_) {
      EXPECT_EQ(color_test.color.alpha_is_none_,
                color_test.color_expected.alpha_is_none_);
    } else {
      EXPECT_NEAR(color_test.color.alpha_, color_test.color_expected.alpha_,
                  0.001f)
          << "Premultiplying generated " << color_test.color.param0_ << " "
          << color_test.color.param1_ << " " << color_test.color.param2_ << " "
          << color_test.color.alpha_ << " and it was expecting "
          << color_test.color_expected.param0_ << " "
          << color_test.color_expected.param1_ << " "
          << color_test.color_expected.param2_ << " "
          << color_test.color_expected.alpha_;
    }
  }
}

TEST(BlinkColor, Unpremultiply) {
  ColorTest color_tests[] = {
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.096f, 0.048f,
                                0.392f, 1.0f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.24f, 0.12f, 0.98f,
                                0.4f)},
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.372f, 0.156f,
                                0.384f, 1.0f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.62f, 0.26f, 0.64f,
                                0.6f)},
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.5f, 0.4f, 0.0f,
                                1.0f),
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 1.0f, 0.8f, 0.0f,
                                0.5f)}};

  for (auto& color_test : color_tests) {
    color_test.color.alpha_ = color_test.color_expected.alpha_;
    color_test.color.UnpremultiplyColor();

    EXPECT_NEAR(color_test.color.param0_, color_test.color_expected.param0_,
                0.001f)
        << "Unpremultiplying generated " << color_test.color.param0_ << " "
        << color_test.color.param1_ << " " << color_test.color.param2_ << " "
        << color_test.color.alpha_ << " and it was expecting "
        << color_test.color_expected.param0_ << " "
        << color_test.color_expected.param1_ << " "
        << color_test.color_expected.param2_ << " "
        << color_test.color_expected.alpha_;
    EXPECT_NEAR(color_test.color.param1_, color_test.color_expected.param1_,
                0.001f)
        << "Unpremultiplying generated " << color_test.color.param0_ << " "
        << color_test.color.param1_ << " " << color_test.color.param2_ << " "
        << color_test.color.alpha_ << " and it was expecting "
        << color_test.color_expected.param0_ << " "
        << color_test.color_expected.param1_ << " "
        << color_test.color_expected.param2_ << " "
        << color_test.color_expected.alpha_;
    EXPECT_NEAR(color_test.color.param2_, color_test.color_expected.param2_,
                0.001f)
        << "Unpremultiplying generated " << color_test.color.param0_ << " "
        << color_test.color.param1_ << " " << color_test.color.param2_ << " "
        << color_test.color.alpha_ << " and it was expecting "
        << color_test.color_expected.param0_ << " "
        << color_test.color_expected.param1_ << " "
        << color_test.color_expected.param2_ << " "
        << color_test.color_expected.alpha_;
    EXPECT_NEAR(color_test.color.alpha_, color_test.color_expected.alpha_,
                0.001f)
        << "Unpremultiplying generated " << color_test.color.param0_ << " "
        << color_test.color.param1_ << " " << color_test.color.param2_ << " "
        << color_test.color.alpha_ << " and it was expecting "
        << color_test.color_expected.param0_ << " "
        << color_test.color_expected.param1_ << " "
        << color_test.color_expected.param2_ << " "
        << color_test.color_expected.alpha_;
  }
}

TEST(BlinkColor, ConvertToColorSpace) {
  struct ColorConversionTest {
    Color input_color;
    Color::ColorSpace destination_color_space;
    Color expected_color;
  };

  ColorConversionTest conversion_tests[] = {
      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kDisplayP3,
       Color::FromColorSpace(Color::ColorSpace::kDisplayP3, 0.313084f,
                             0.494041f, 0.730118f)},

      {Color::FromColorSpace(Color::ColorSpace::kDisplayP3, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kSRGB,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.123874f, 0.507355f,
                             0.771198f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kA98RGB,
       Color::FromColorSpace(Color::ColorSpace::kA98RGB, 0.346851f, 0.496124f,
                             0.736271f)},

      {Color::FromColorSpace(Color::ColorSpace::kA98RGB, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kSRGB,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, -0.153808f, 0.503925f,
                             0.763874f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kProPhotoRGB,
       Color::FromColorSpace(Color::ColorSpace::kProPhotoRGB, 0.374905f,
                             0.416401f, 0.663692f)},

      {Color::FromColorSpace(Color::ColorSpace::kProPhotoRGB, 0.25f, 0.5f,
                             0.75f),
       Color::ColorSpace::kSRGB,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, -0.510605f, 0.612396f,
                             0.825333f)},

      {Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kRec2020,
       Color::FromColorSpace(Color::ColorSpace::kRec2020, 0.331976f, 0.440887f,
                             0.696358f)},

      {Color::FromColorSpace(Color::ColorSpace::kRec2020, 0.25f, 0.5f, 0.75f),
       Color::ColorSpace::kSRGB,
       Color::FromColorSpace(Color::ColorSpace::kSRGB, -0.280102f, 0.565536f,
                             0.79958f)},
  };

  for (auto& test : conversion_tests) {
    test.input_color.ConvertToColorSpace(test.destination_color_space);

    EXPECT_NEAR(test.input_color.param0_, test.expected_color.param0_, 0.001f)
        << "Converting generated " << test.input_color.param0_ << " "
        << test.input_color.param1_ << " " << test.input_color.param2_ << " "
        << test.input_color.alpha_ << " and it was expecting "
        << test.expected_color.param0_ << " " << test.expected_color.param1_
        << " " << test.expected_color.param2_ << " "
        << test.expected_color.alpha_;
    EXPECT_NEAR(test.input_color.param1_, test.expected_color.param1_, 0.001f)
        << "Converting generated " << test.input_color.param0_ << " "
        << test.input_color.param1_ << " " << test.input_color.param2_ << " "
        << test.input_color.alpha_ << " and it was expecting "
        << test.expected_color.param0_ << " " << test.expected_color.param1_
        << " " << test.expected_color.param2_ << " "
        << test.expected_color.alpha_;
    EXPECT_NEAR(test.input_color.param2_, test.expected_color.param2_, 0.001f)
        << "Converting generated " << test.input_color.param0_ << " "
        << test.input_color.param1_ << " " << test.input_color.param2_ << " "
        << test.input_color.alpha_ << " and it was expecting "
        << test.expected_color.param0_ << " " << test.expected_color.param1_
        << " " << test.expected_color.param2_ << " "
        << test.expected_color.alpha_;
    EXPECT_NEAR(test.input_color.alpha_, test.expected_color.alpha_, 0.001f)
        << "Converting generated " << test.input_color.param0_ << " "
        << test.input_color.param1_ << " " << test.input_color.param2_ << " "
        << test.input_color.alpha_ << " and it was expecting "
        << test.expected_color.param0_ << " " << test.expected_color.param1_
        << " " << test.expected_color.param2_ << " "
        << test.expected_color.alpha_;
  }
}

TEST(BlinkColor, ResolveMissingComponents) {
  struct ResolveMissingComponentsTest {
    Color input_color;
    Color expected_color;
  };

  ResolveMissingComponentsTest tests[] = {
      {
          Color::FromColorSpace(Color::ColorSpace::kSRGB, std::nullopt, 0.2f,
                                0.3f, 0.4f),
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.0f, 0.2f, 0.3f,
                                0.4f),
      },
      {
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1f, std::nullopt,
                                0.3f, 0.4f),
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1f, 0.0f, 0.3f,
                                0.4f),
      },
      {
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1f, 0.2f,
                                std::nullopt, 0.4f),
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1f, 0.2f, 0.0f,
                                0.4f),
      },
      {
          // Alpha remains unresolved
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1f, 0.2f, 0.3f,
                                std::nullopt),
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1f, 0.2f, 0.3f,
                                std::nullopt),
      },
      {
          Color::FromColorSpace(Color::ColorSpace::kSRGB, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt),
          Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.0f, 0.0f, 0.0f,
                                std::nullopt),
      },
  };

  for (auto& test : tests) {
    test.input_color.ResolveMissingComponents();
    EXPECT_EQ(test.input_color, test.expected_color);
  }
}

TEST(BlinkColor, SubstituteMissingParameters) {
  Color srgb1 =
      Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.1, 0.2, 0.3, 0.4);
  Color srgb2 =
      Color::FromColorSpace(Color::ColorSpace::kSRGB, 0.5, 0.6, 0.7, 0.8);
  Color oklab =
      Color::FromColorSpace(Color::ColorSpace::kOklab, 0.6, 0.0, 0.1, 0.8);

  // Substitute one param.
  {
    for (int param_index = 0; param_index < 4; param_index++) {
      Color c1 = srgb1;
      Color c2 = srgb2;
      Color expected_c1 = c1;
      switch (param_index) {
        case 0:
          c1.param0_is_none_ = true;
          expected_c1.param0_ = c2.param0_;
          break;
        case 2:
          c1.param1_is_none_ = true;
          expected_c1.param1_ = c2.param1_;
          break;
        case 3:
          c1.param2_is_none_ = true;
          expected_c1.param2_ = c2.param2_;
          break;
        case 4:
          c1.alpha_is_none_ = true;
          expected_c1.alpha_ = c2.alpha_;
      }

      Color c1_copy = c1;
      Color c2_copy = c2;

      EXPECT_TRUE(Color::SubstituteMissingParameters(c1, c2));
      EXPECT_EQ(c1, expected_c1);
      EXPECT_EQ(c2, srgb2);

      // Test with arguments inverted.
      EXPECT_TRUE(Color::SubstituteMissingParameters(c2_copy, c1_copy));
      EXPECT_EQ(c1_copy, expected_c1);
      EXPECT_EQ(c2_copy, srgb2);
    }
  }

  // Nones on both sides remain.
  {
    for (int param_index = 0; param_index < 4; param_index++) {
      Color c1 = srgb1;
      Color c2 = srgb2;
      switch (param_index) {
        case 0:
          c1.param0_is_none_ = true;
          c2.param0_is_none_ = true;
          break;
        case 1:
          c1.param1_is_none_ = true;
          c2.param1_is_none_ = true;
          break;
        case 2:
          c1.param2_is_none_ = true;
          c2.param2_is_none_ = true;
          break;
        case 4:
          c1.alpha_is_none_ = true;
          c2.alpha_is_none_ = true;
          break;
      }

      Color expected_c1 = c1;
      Color expected_c2 = c2;

      EXPECT_TRUE(Color::SubstituteMissingParameters(c1, c2));
      EXPECT_EQ(c1, expected_c1);
      EXPECT_EQ(c2, expected_c2);

      // Test with arguments inverted.
      EXPECT_TRUE(Color::SubstituteMissingParameters(c2, c1));
      EXPECT_EQ(c1, expected_c1);
      EXPECT_EQ(c2, expected_c2);
    }
  }

  // Different colorspaces fail
  {
    Color c1 = srgb1;
    Color c2 = oklab;
    EXPECT_FALSE(Color::SubstituteMissingParameters(c1, c2));
  }
}
}  // namespace blink
