// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_math_function_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {
// |float| has roughly 7 digits of precision.
const double epsilon = 1e-6;
}  // namespace

struct SizesCalcTestCase {
  const char* input;
  const float output;
  const bool valid;
  const bool dont_run_in_css_calc;
};

#define EXPECT_APPROX_EQ(expected, actual)            \
  {                                                   \
    double actual_error = actual - expected;          \
    double allowed_error = expected * epsilon;        \
    EXPECT_LE(abs(actual_error), abs(allowed_error)); \
  }

static void VerifyCSSCalc(String text,
                          double value,
                          bool valid,
                          unsigned font_size,
                          unsigned viewport_width,
                          unsigned viewport_height) {
  const CSSValue* css_value = CSSParser::ParseSingleValue(
      CSSPropertyID::kLeft, text,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  const auto* math_value = DynamicTo<CSSMathFunctionValue>(css_value);
  if (!math_value) {
    EXPECT_FALSE(valid) << text;
    return;
  }

  ASSERT_TRUE(valid) << text;

  Font font;
  CSSToLengthConversionData::FontSizes font_sizes(font_size, font_size, &font,
                                                  1);
  CSSToLengthConversionData::ViewportSize viewport_size(viewport_width,
                                                        viewport_height);
  CSSToLengthConversionData conversion_data(nullptr, font_sizes, viewport_size,
                                            1.0);
  EXPECT_APPROX_EQ(value, math_value->ComputeLength<float>(conversion_data));
}

TEST(SizesMathFunctionParserTest, Basic) {
  ScopedCSSComparisonFunctionsForTest scope(true);

  SizesCalcTestCase test_cases[] = {
      {"calc(500px + 10em)", 660, true, false},
      {"calc(500px / 8)", 62.5, true, false},
      {"calc(500px + 2 * 10em)", 820, true, false},
      {"calc(500px + 2*10em)", 820, true, false},
      {"calc(500px + 0.5*10em)", 580, true, false},
      {"calc(500px + (0.5*10em + 13px))", 593, true, false},
      {"calc(100vw + (0.5*10em + 13px))", 593, true, false},
      {"calc(100vh + (0.5*10em + 13px))", 736, true, false},
      {"calc(100vh + calc(0.5*10em + 13px))", 736, true,
       true},  // CSSCalculationValue does not parse internal "calc(".
      {"calc(100vh + (50%*10em + 13px))", 0, false, false},
      {"calc(50em+13px)", 0, false, false},
      {"calc(50em-13px)", 0, false, false},
      {"calc(500px + 10)", 0, false, false},
      {"calc(500 + 10)", 0, false, false},
      {"calc(500px + 10s)", 0, false,
       true},  // This test ASSERTs in CSSCalculationValue.
      {"calc(500px + 1cm)", 537.795276, true, false},
      {"calc(500px - 10s)", 0, false,
       true},  // This test ASSERTs in CSSCalculationValue.
      {"calc(500px - 1cm)", 462.204724, true, false},
      {"calc(500px - 1vw)", 495, true, false},
      {"calc(50px*10)", 500, true, false},
      {"calc(50px*10px)", 0, false, false},
      {"calc(50px/10px)", 0, false, false},
      {"calc(500px/10)", 50, true, false},
      {"calc(500/10)", 0, false, false},
      {"calc(500px/0.5)", 1000, true, false},
      {"calc(500px/.5)", 1000, true, false},
      {"calc(500/0)", 0, false, false},
      {"calc(500px/0)", 0, false, false},
      {"calc(-500px/10)", 0, true,
       true},  // CSSCalculationValue does not clamp negative values to 0.
      {"calc(((4) * ((10px))))", 40, true, false},
      {"calc(50px / 0)", 0, false, false},
      {"calc(50px / (10 + 10))", 2.5, true, false},
      {"calc(50px / (10 - 10))", 0, false, false},
      {"calc(50px / (10 * 10))", 0.5, true, false},
      {"calc(50px / (10 / 10))", 50, true, false},
      {"calc(200px*)", 0, false, false},
      {"calc(+ +200px)", 0, false, false},
      {"calc()", 0, false, false},
      {"calc(100px + + +100px)", 0, false, false},
      {"calc(200px 200px)", 0, false, false},
      {"calc(100px * * 2)", 0, false, false},
      {"calc(100px @ 2)", 0, false, false},
      {"calc(1 flim 2)", 0, false, false},
      {"calc(100px @ 2)", 0, false, false},
      {"calc(1 flim 2)", 0, false, false},
      {"calc(1 flim (2))", 0, false, false},
      {"calc((100vw - 2 * 40px - 2 * 30px) / 3)", 120, true, false},
      {"calc((100vw - 40px - 60px - 40px) / 3)", 120, true, false},
      {"calc((50vw + 40px + 30px + 40px) / 3)", 120, true, false},
      {"calc((100vw - 2 / 2 * 40px - 2 * 30px) / 4)", 100, true, false},
      {"calc((100vw - 2 * 2 / 2 * 40px - 2 * 30px) / 3)", 120, true, false},
      {"calc((100vw - 2 * 2 / 2 * 40px - 2 * 30px) / 3)", 120, true, false},
      {"calc((100vw - 2 * 2 * 20px - 2 * 30px) / 3)", 120, true, false},
      {"calc((100vw - 320px / 2 / 2 - 2 * 30px) / 3)", 120, true, false},
      // Following test cases contain comparison functions.
      {"min()", 0, false, false},
      {"min(100px)", 100, true, false},
      {"min(200px, 100px, 300px, 40px, 1000px)", 40, true, false},
      {"min( 100px , 200px )", 100, true, false},
      {"min(100, 200, 300)", 0, false, false},
      {"min(100, 200px, 300px)", 0, false, false},
      {"min(100px 200px)", 0, false, false},
      {"min(100px, , 200px)", 0, false, false},
      {"min(100px, 200px,)", 0, false, false},
      {"min(, 100px, 200px)", 0, false, false},
      {"max()", 0, false, false},
      {"max(100px)", 100, true, false},
      {"max(200px, 100px, 300px, 40px, 1000px)", 1000, true, false},
      {"max( 100px , 200px )", 200, true, false},
      {"max(100, 200, 300)", 0, false, false},
      {"max(100, 200px, 300px)", 0, false, false},
      {"max(100px 200px)", 0, false, false},
      {"max(100px, , 200px)", 0, false, false},
      {"max(100px, 200px,)", 0, false, false},
      {"max(, 100px, 200px)", 0, false, false},
      {"calc(min(100px, 200px) + max(300px, 400px))", 500, true, false},
      {"calc(max(300px, 400px) - min(100px, 200px))", 300, true, false},
      {"calc(min(100px, 200px) * max(3, 4, 5))", 500, true, false},
      {"calc(min(100px, 200px) / max(3, 4, 5))", 20, true, false},
      {"max(10px, min(20px, 1em))", 16, true, false},
      {"min(20px, max(10px, 1em))", 16, true, false},
      {"clamp(10px, 20px, 30px)", 20, true, false},
      {"clamp(10px, 5px, 30px)", 10, true, false},
      {"clamp(10px, 35px, 30px)", 30, true, false},
      {"clamp(30px, 20px, 10px)", 30, true, false},
      {"clamp(10px, 20px, clamp(20px, 30px, 40px))", 20, true, false},
      {"clamp()", 0, false, false},
      {"clamp( )", 0, false, false},
      {"clamp(,)", 0, false, false},
      {"clamp(1px, )", 0, false, false},
      {"clamp(, 1px)", 0, false, false},
      {"clamp(1px, 1px)", 0, false, false},
      {"clamp(1px, , 1px)", 0, false, false},
      {"clamp(, 1px, 1px)", 0, false, false},
      {"clamp(1px, 1px, )", 0, false, false},
      {"clamp(1px, 1px, 1px, )", 0, false, false},
      {"clamp(1px 1px 1px)", 0, false, false},
      {nullptr, 0, true, false}  // Do not remove the terminator line.
  };

  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 500;
  data.viewport_height = 643;
  data.device_width = 500;
  data.device_height = 643;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = kPointerTypeFine;
  data.default_font_size = 16;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;
  auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  for (unsigned i = 0; test_cases[i].input; ++i) {
    SizesMathFunctionParser calc_parser(
        CSSParserTokenRange(CSSTokenizer(test_cases[i].input).TokenizeToEOF()),
        media_values);
    ASSERT_EQ(test_cases[i].valid, calc_parser.IsValid());
    if (calc_parser.IsValid())
      EXPECT_APPROX_EQ(test_cases[i].output, calc_parser.Result());
  }

  for (unsigned i = 0; test_cases[i].input; ++i) {
    if (test_cases[i].dont_run_in_css_calc)
      continue;
    VerifyCSSCalc(test_cases[i].input, test_cases[i].output,
                  test_cases[i].valid, data.default_font_size,
                  data.viewport_width, data.viewport_height);
  }
}

}  // namespace blink
