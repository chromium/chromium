// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/variable_axes_names.h"

#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

TEST(VariableAxesNamesTest, TestVariableAxes) {
  String file_path = blink::test::BlinkWebTestsDir() +
                     "/third_party/Homecomputer/Sixtyfour.ttf";
  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> typeface = mgr->makeFromFile(file_path.Utf8().c_str(), 0);
  Vector<VariationAxis> axes = VariableAxesNames::GetVariationAxes(typeface);
  EXPECT_EQ(axes.size(), (unsigned)2);
  VariationAxis axis1 = axes.at(0);
  EXPECT_EQ(axis1.name, "Weight");
  EXPECT_EQ(axis1.tag, "wght");
  EXPECT_EQ(axis1.minValue, 200);
  EXPECT_EQ(axis1.maxValue, 900);
  EXPECT_EQ(axis1.defaultValue, 200);
  VariationAxis axis2 = axes.at(1);
  EXPECT_EQ(axis2.name, "Width");
  EXPECT_EQ(axis2.tag, "wdth");
  EXPECT_EQ(axis2.minValue, 100);
  EXPECT_EQ(axis2.maxValue, 200);
  EXPECT_EQ(axis2.defaultValue, 100);
}

}  // namespace blink
