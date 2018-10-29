// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/sizes_calc_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/media_type_names.h"

namespace blink {

struct SizesCalcTestCase {
  const char* input;
  const float output;
  const bool valid;
  const bool dont_run_in_css_calc;
};

static void VerifyCSSCalc(String text,
                          double value,
                          bool valid,
                          unsigned font_size,
                          unsigned viewport_width,
                          unsigned viewport_height) {
  CSSLengthArray length_array;
  const CSSValue* css_value = CSSParser::ParseSingleValue(
      CSSPropertyLeft, text,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  const CSSPrimitiveValue* primitive_value = ToCSSPrimitiveValue(css_value);
  if (primitive_value)
    primitive_value->AccumulateLengthArray(length_array);
  else
    ASSERT_EQ(valid, false);
  float length = length_array.values.at(CSSPrimitiveValue::kUnitTypePixels);
  length +=
      length_array.values.at(CSSPrimitiveValue::kUnitTypeFontSize) * font_size;
  length += length_array.values.at(CSSPrimitiveValue::kUnitTypeViewportWidth) *
            viewport_width / 100.0;
  length += length_array.values.at(CSSPrimitiveValue::kUnitTypeViewportHeight) *
            viewport_height / 100.0;
  ASSERT_EQ(value, length);
}

TEST(SizesCalcParserTest, Basic) {
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
  data.display_mode = kWebDisplayModeBrowser;
  MediaValues* media_values = MediaValuesCached::Create(data);

  for (unsigned i = 0; test_cases[i].input; ++i) {
    SizesCalcParser calc_parser(
        CSSParserTokenRange(CSSTokenizer(test_cases[i].input).TokenizeToEOF()),
        media_values);
    ASSERT_EQ(test_cases[i].valid, calc_parser.IsValid());
    if (calc_parser.IsValid())
      ASSERT_EQ(test_cases[i].output, calc_parser.Result());
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
