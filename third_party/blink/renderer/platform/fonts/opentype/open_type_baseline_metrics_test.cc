// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_baseline_metrics.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_types.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace {
const char kCanvasTestFontName[] = "CanvasTest.ttf";
const char kAhemFontName[] = "Ahem.ttf";
}  // namespace

namespace blink {

class OpenTypeBaselineMetricsTest : public FontTestBase {
 protected:
  Font CreateCanvasTestFont(float size) {
    FontDescription::VariantLigatures ligatures;
    return blink::test::CreateTestFont(
        AtomicString("CanvasTest"),
        blink::test::BlinkWebTestsFontsTestDataPath(kCanvasTestFontName), size,
        &ligatures);
  }

  Font CreateAhemFont(float size) {
    FontDescription::VariantLigatures ligatures;
    return blink::test::CreateTestFont(
        AtomicString("Ahem"),
        blink::test::BlinkWebTestsFontsTestDataPath(kAhemFontName), size,
        &ligatures);
  }
};

TEST_F(OpenTypeBaselineMetricsTest, AlphabeticBaseline) {
  Font baseline_test_font = CreateCanvasTestFont(24);
  OpenTypeBaselineMetrics baseline_metrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeAlphabeticBaseline(), 0);

  baseline_test_font = CreateCanvasTestFont(200);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeAlphabeticBaseline(), 0);

  baseline_test_font = CreateCanvasTestFont(0);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeAlphabeticBaseline(), 0);

  baseline_test_font = CreateAhemFont(50);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_FALSE(baseline_metrics.OpenTypeAlphabeticBaseline());
}

TEST_F(OpenTypeBaselineMetricsTest, HangingBaseline) {
  Font baseline_test_font = CreateCanvasTestFont(24);
  OpenTypeBaselineMetrics baseline_metrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeHangingBaseline(), 12);

  baseline_test_font = CreateCanvasTestFont(55);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeHangingBaseline(), 27.5);

  baseline_test_font = CreateCanvasTestFont(0);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeHangingBaseline(), 0);

  baseline_test_font = CreateCanvasTestFont(300);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeHangingBaseline(), 150);

  baseline_test_font = CreateAhemFont(50);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_FALSE(baseline_metrics.OpenTypeHangingBaseline());
}

TEST_F(OpenTypeBaselineMetricsTest, IdeographicBaseline) {
  Font baseline_test_font = CreateCanvasTestFont(24);
  OpenTypeBaselineMetrics baseline_metrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeIdeographicBaseline(), 3);

  baseline_test_font = CreateCanvasTestFont(50);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeIdeographicBaseline(), 6.25);

  baseline_test_font = CreateCanvasTestFont(800);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeIdeographicBaseline(), 100);

  baseline_test_font = CreateCanvasTestFont(0);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_EQ(baseline_metrics.OpenTypeIdeographicBaseline(), 0);

  baseline_test_font = CreateAhemFont(50);
  baseline_metrics = OpenTypeBaselineMetrics(
      baseline_test_font.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
      FontOrientation::kHorizontal);
  EXPECT_FALSE(baseline_metrics.OpenTypeIdeographicBaseline());
}

}  // namespace blink
