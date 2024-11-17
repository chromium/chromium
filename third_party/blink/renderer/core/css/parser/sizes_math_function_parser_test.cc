// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_math_function_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

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
  CSSToLengthConversionData::LineHeightSize line_height_size;
  CSSToLengthConversionData::ViewportSize viewport_size(viewport_width,
                                                        viewport_height);
  CSSToLengthConversionData::ContainerSizes container_sizes;
  CSSToLengthConversionData::AnchorData anchor_data;
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData conversion_data(
      WritingMode::kHorizontalTb, font_sizes, line_height_size, viewport_size,
      container_sizes, anchor_data, 1.0, ignored_flags);
  EXPECT_APPROX_EQ(value, math_value->ComputeLength<float>(conversion_data));
}

TEST(SizesMathFunctionParserTest, Basic) {
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
      {"calc(-500px/10)", 0, true,
       true},  // CSSCalculationValue does not clamp negative values to 0.
      {"calc(((4) * ((10px))))", 40, true, false},
      // TODO(crbug.com/1133390): These test cases failed with Infinity and NaN
      // parsing implementation. Below tests will be reactivated when the
      // sizes_math function supports the infinity and NaN.
      //{"calc(500px/0)", 0, false, false},
      //{"calc(50px / 0)", 0, false, false},
      //{"calc(50px / (10 - 10))", 0, false, false},
      {"calc(50px / (10 + 10))", 2.5, true, false},
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
      // Unbalanced )-token.
      {"calc(1px + 2px) )", 0, false, false},
  };

  MediaValuesCached::MediaValuesCachedData data;
  data.viewport_width = 500;
  data.viewport_height = 643;
  data.device_width = 500;
  data.device_height = 643;
  data.device_pixel_ratio = 2.0;
  data.color_bits_per_component = 24;
  data.monochrome_bits_per_component = 0;
  data.primary_pointer_type = mojom::blink::PointerType::kPointerFineType;
  data.three_d_enabled = true;
  data.media_type = media_type_names::kScreen;
  data.strict_mode = true;
  data.display_mode = blink::mojom::DisplayMode::kBrowser;
  auto* media_values = MakeGarbageCollected<MediaValuesCached>(data);

  for (const SizesCalcTestCase& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input);
    SizesMathFunctionParser calc_parser(stream, media_values);
    bool is_valid = calc_parser.IsValid() && stream.AtEnd();
    SCOPED_TRACE(test_case.input);
    ASSERT_EQ(test_case.valid, is_valid);
    if (is_valid) {
      EXPECT_APPROX_EQ(test_case.output, calc_parser.Result());
    }
  }

  for (const SizesCalcTestCase& test_case : test_cases) {
    if (test_case.dont_run_in_css_calc) {
      continue;
    }
    VerifyCSSCalc(test_case.input, test_case.output, test_case.valid,
                  data.em_size, data.viewport_width, data.viewport_height);
  }
}

TEST(SizesMathFunctionParserTest, CleansUpWhitespace) {
  CSSParserTokenStream stream("calc(1px)    ");
  SizesMathFunctionParser calc_parser(
      stream, MakeGarbageCollected<MediaValuesCached>());
  EXPECT_TRUE(calc_parser.IsValid());
  EXPECT_EQ(stream.RemainingText(), "");
}

TEST(SizesMathFunctionParserTest, RestoresOnFailure) {
  CSSParserTokenStream stream("calc(1px @)");
  SizesMathFunctionParser calc_parser(
      stream, MakeGarbageCollected<MediaValuesCached>());
  EXPECT_FALSE(calc_parser.IsValid());
  EXPECT_EQ(stream.RemainingText(), "calc(1px @)");
}

TEST(SizesMathFunctionParserTest, LeavesTrailingComma) {
  CSSParserTokenStream stream("calc(1px) , more stuff");
  SizesMathFunctionParser calc_parser(
      stream, MakeGarbageCollected<MediaValuesCached>());
  EXPECT_TRUE(calc_parser.IsValid());
  EXPECT_EQ(stream.RemainingText(), ", more stuff");
}

TEST(SizesMathFunctionParserTest, LeavesTrailingTokens) {
  CSSParserTokenStream stream("calc(1px) ! trailing tokens");
  SizesMathFunctionParser calc_parser(
      stream, MakeGarbageCollected<MediaValuesCached>());
  EXPECT_TRUE(calc_parser.IsValid());
  EXPECT_EQ(stream.RemainingText(), "! trailing tokens");
}

}  // namespace blink
