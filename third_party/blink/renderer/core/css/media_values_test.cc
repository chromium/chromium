// Use of this source code is governed by a BSD-style license that can be
// Copyright 2014 The Chromium Authors. All rights reserved.
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct MediaValuesTestCase {
  double value;
  CSSPrimitiveValue::UnitType type;
  unsigned font_size;
  unsigned viewport_width;
  unsigned viewport_height;
  bool success;
  double output;
};

TEST(MediaValuesTest, Basic) {
  MediaValuesTestCase test_cases[] = {
      {40.0, CSSPrimitiveValue::UnitType::kPixels, 16, 300, 300, true, 40},
      {40.0, CSSPrimitiveValue::UnitType::kEms, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kRems, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kExs, 16, 300, 300, true, 320},
      {40.0, CSSPrimitiveValue::UnitType::kChs, 16, 300, 300, true, 320},
      {43.0, CSSPrimitiveValue::UnitType::kViewportWidth, 16, 848, 976, true,
       364.64},
      {100.0, CSSPrimitiveValue::UnitType::kViewportWidth, 16, 821, 976, true,
       821},
      {43.0, CSSPrimitiveValue::UnitType::kViewportHeight, 16, 848, 976, true,
       419.68},
      {43.0, CSSPrimitiveValue::UnitType::kViewportMin, 16, 848, 976, true,
       364.64},
      {43.0, CSSPrimitiveValue::UnitType::kViewportMax, 16, 848, 976, true,
       419.68},
      {1.3, CSSPrimitiveValue::UnitType::kCentimeters, 16, 300, 300, true,
       49.133858},
      {1.3, CSSPrimitiveValue::UnitType::kMillimeters, 16, 300, 300, true,
       4.913386},
      {1.3, CSSPrimitiveValue::UnitType::kQuarterMillimeters, 16, 300, 300,
       true, 1.2283465},
      {1.3, CSSPrimitiveValue::UnitType::kInches, 16, 300, 300, true, 124.8},
      {13, CSSPrimitiveValue::UnitType::kPoints, 16, 300, 300, true, 17.333333},
      {1.3, CSSPrimitiveValue::UnitType::kPicas, 16, 300, 300, true, 20.8},
      {40.0, CSSPrimitiveValue::UnitType::kUserUnits, 16, 300, 300, true, 40},
      {1.3, CSSPrimitiveValue::UnitType::kUnknown, 16, 300, 300, false, 20},
      {0.0, CSSPrimitiveValue::UnitType::kUnknown, 0, 0, 0, false,
       0.0}  // Do not remove the terminating line.
  };

  for (unsigned i = 0; test_cases[i].viewport_width; ++i) {
    MediaValuesCached::MediaValuesCachedData data;
    data.em_size = test_cases[i].font_size;
    data.viewport_width = test_cases[i].viewport_width;
    data.viewport_height = test_cases[i].viewport_height;
    MediaValuesCached media_values(data);

    double output = 0;
    bool success = media_values.ComputeLength(test_cases[i].value,
                                              test_cases[i].type, output);
    EXPECT_EQ(test_cases[i].success, success);
    if (success)
      EXPECT_FLOAT_EQ(test_cases[i].output, output);
  }
}

}  // namespace blink
