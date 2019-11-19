// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

using blink::test::CreateTestFont;

namespace blink {

static const int kTimeLimitMillis = 3000;
static const int kWarmupRuns = 10000;
static const int kTimeCheckInterval = 1000000;

class ShapeResultPerfTest {
  USING_FAST_MALLOC(ShapeResultPerfTest);

 public:
  enum FontName {
    ahem,
    amiri,
    megalopolis,
    roboto,
  };

  ShapeResultPerfTest()
      : timer(kWarmupRuns,
              base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
              kTimeCheckInterval) {}

 protected:
  TextRun SetupFont(FontName font_name, const String& text, bool ltr) {
    FontDescription::VariantLigatures ligatures(
        FontDescription::kEnabledLigaturesState);
    font = CreateTestFont(
        "TestFont",
        test::PlatformTestDataPath(font_path.find(font_name)->value), 100,
        &ligatures);

    return TextRun(
        text, /* xpos */ 0, /* expansion */ 0,
        TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
        ltr ? TextDirection::kLtr : TextDirection::kRtl, false);
  }

  Font font;

  HashMap<FontName, String, WTF::IntHash<FontName>> font_path = {
      {ahem, "Ahem.woff"},
      {amiri, "third_party/Amiri/amiri_arabic.woff2"},
      {megalopolis, "third_party/MEgalopolis/MEgalopolisExtra.woff"},
      {roboto, "third_party/Roboto/roboto-regular.woff2"},
  };

  base::LapTimer timer;
};

class OffsetForPositionPerfTest : public ShapeResultPerfTest,
                                  public testing::TestWithParam<float> {
 public:
  void OffsetForPosition(TextRun& run,
                         IncludePartialGlyphsOption partial,
                         BreakGlyphsOption breakopt) {
    timer.Reset();
    float position = GetParam();
    do {
      font.OffsetForPosition(run, position, partial, breakopt);
      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());
  }
};

class CharacterRangePerfTest : public ShapeResultPerfTest,
                               public testing::TestWithParam<int> {
 public:
  void GetCharacter(TextRun& run) {
    timer.Reset();
    int endpos = GetParam();
    do {
      font.SelectionRectForText(run, FloatPoint(), 100, 0, endpos);
      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());
  }
};

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionFullBreak) {
  TextRun run = SetupFont(ahem, "FURACOLO", true);
  OffsetForPosition(run, OnlyFullGlyphs, BreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " LTR full break", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionFullDontBreak) {
  TextRun run = SetupFont(ahem, "FURACOLO", true);
  OffsetForPosition(run, OnlyFullGlyphs, DontBreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " LTR full", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionIncludePartialBreak) {
  TextRun run = SetupFont(ahem, "FURACOLO", true);
  OffsetForPosition(run, IncludePartialGlyphs, BreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " LTR partial break", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionIncludePartialDontBreak) {
  TextRun run = SetupFont(ahem, "FURACOLO", true);
  OffsetForPosition(run, IncludePartialGlyphs, DontBreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " LTR partial", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionFullBreak) {
  TextRun run = SetupFont(ahem, "OLOCARUF", false);
  OffsetForPosition(run, OnlyFullGlyphs, BreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " RTL full break", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionFullDontBreak) {
  TextRun run = SetupFont(ahem, "OLOCARUF", false);
  OffsetForPosition(run, OnlyFullGlyphs, DontBreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " RTL full", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionIncludePartialBreak) {
  TextRun run = SetupFont(ahem, "OLOCARUF", false);
  OffsetForPosition(run, IncludePartialGlyphs, BreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " RTL partial break", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionIncludePartialDontBreak) {
  TextRun run = SetupFont(ahem, "OLOCARUF", false);
  OffsetForPosition(run, IncludePartialGlyphs, DontBreakGlyphs);
  perf_test::PrintResult("OffsetForPositionPerfTest", " RTL partial", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

INSTANTIATE_TEST_SUITE_P(OffsetForPosition,
                         OffsetForPositionPerfTest,
                         testing::Values(0, 10, 60, 100, 200, 350));

TEST_P(CharacterRangePerfTest, LTRCharacterForPosition) {
  TextRun run = SetupFont(ahem, "FURACOLO", true);
  GetCharacter(run);
  perf_test::PrintResult("CharacterRangePerfTest", " LTR", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

TEST_P(CharacterRangePerfTest, RTLCharacterForPosition) {
  TextRun run = SetupFont(ahem, "OLOCARUF", false);
  GetCharacter(run);
  perf_test::PrintResult("CharacterRangePerfTest", " RTL", "",
                         timer.LapsPerSecond(), "runs/s", true);
}

INSTANTIATE_TEST_SUITE_P(CharacterRange,
                         CharacterRangePerfTest,
                         testing::Values(0, 1, 2, 4, 8));

}  // namespace blink
