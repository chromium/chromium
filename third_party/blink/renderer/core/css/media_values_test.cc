// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
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

class MediaValuesTest : public PageTestBase {};

TEST_F(MediaValuesTest, Basic) {
  MediaValuesTestCase test_cases[] = {
      {40.0, CSSPrimitiveValue::UnitType::kPixels, 16, 300, 300, true, 40},
      {40.0, CSSPrimitiveValue::UnitType::kEms, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kRems, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kCaps, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kRcaps, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kExs, 16, 300, 300, true, 320},
      {40.0, CSSPrimitiveValue::UnitType::kRexs, 16, 300, 300, true, 320},
      {40.0, CSSPrimitiveValue::UnitType::kChs, 16, 300, 300, true, 320},
      {40.0, CSSPrimitiveValue::UnitType::kRchs, 16, 300, 300, true, 320},
      {40.0, CSSPrimitiveValue::UnitType::kIcs, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kRics, 16, 300, 300, true, 640},
      {40.0, CSSPrimitiveValue::UnitType::kLhs, 16, 300, 300, true, 800},
      {40.0, CSSPrimitiveValue::UnitType::kRlhs, 16, 300, 300, true, 800},
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
  };

  for (MediaValuesTestCase test_case : test_cases) {
    MediaValuesCached::MediaValuesCachedData data;
    data.em_size = test_case.font_size;
    data.viewport_width = test_case.viewport_width;
    data.viewport_height = test_case.viewport_height;
    data.line_height = 20;
    MediaValuesCached* media_values =
        MakeGarbageCollected<MediaValuesCached>(data);

    double output = 0;
    bool success =
        media_values->ComputeLength(test_case.value, test_case.type, output);
    EXPECT_EQ(test_case.success, success);
    if (success) {
      EXPECT_FLOAT_EQ(test_case.output, output);
    }
  }
}

TEST_F(MediaValuesTest, ZoomedFontUnits) {
  LoadAhem();
  GetFrame().SetLayoutZoomFactor(2.0f);

  // Set 'font:Ahem 10px' as the default font.
  Settings* settings = GetDocument().GetSettings();
  ASSERT_TRUE(settings);
  settings->GetGenericFontFamilySettings().UpdateStandard(AtomicString("Ahem"));
  settings->SetDefaultFontSize(10.0f);

  UpdateAllLifecyclePhasesForTest();

  auto* media_values = MakeGarbageCollected<MediaValuesDynamic>(&GetFrame());

  double em = 0;
  double rem = 0;
  double ex = 0;
  double rex = 0;
  double ch = 0;
  double rch = 0;
  double ic = 0;
  double ric = 0;
  double lh = 0;
  double rlh = 0;
  double cap = 0;
  double rcap = 0;

  using UnitType = CSSPrimitiveValue::UnitType;

  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kEms, em));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kRems, rem));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kExs, ex));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kRexs, rex));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kChs, ch));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kRchs, rch));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kIcs, ic));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kRics, ric));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kLhs, lh));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kRlhs, rlh));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kCaps, cap));
  EXPECT_TRUE(media_values->ComputeLength(1.0, UnitType::kRcaps, rcap));

  EXPECT_DOUBLE_EQ(10.0, em);
  EXPECT_DOUBLE_EQ(10.0, rem);
  EXPECT_DOUBLE_EQ(8.0, ex);
  EXPECT_DOUBLE_EQ(8.0, rex);
  EXPECT_DOUBLE_EQ(10.0, ch);
  EXPECT_DOUBLE_EQ(10.0, rch);
  EXPECT_DOUBLE_EQ(10.0, ic);
  EXPECT_DOUBLE_EQ(10.0, ric);
  EXPECT_DOUBLE_EQ(10.0, lh);
  EXPECT_DOUBLE_EQ(10.0, rlh);
  EXPECT_DOUBLE_EQ(8.0, cap);
  EXPECT_DOUBLE_EQ(8.0, rcap);
}

}  // namespace blink
