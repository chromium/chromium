// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
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

namespace {

constexpr char kMetricPrefixOffsetForPosition[] = "OffsetForPosition.";
constexpr char kMetricPrefixCharacterRange[] = "CharacterRange.";
constexpr char kMetricThroughput[] = "throughput";

}  // namespace

class ShapeResultPerfTest {
  USING_FAST_MALLOC(ShapeResultPerfTest);

 public:
  enum FontName {
    kAhem,
    kAmiri,
    kMegalopolis,
    kRoboto,
  };

  ShapeResultPerfTest()
      : timer(kWarmupRuns,
              base::Milliseconds(kTimeLimitMillis),
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

  void ReportResult(const std::string& metric_prefix,
                    const std::string& story_prefix) {
    std::string story = story_prefix + "_" + param_string;
    perf_test::PerfResultReporter reporter(metric_prefix, story);
    reporter.RegisterImportantMetric(kMetricThroughput, "runs/s");
    reporter.AddResult(kMetricThroughput, timer.LapsPerSecond());
  }

  Font font;

  HashMap<FontName, String, WTF::IntHash<FontName>> font_path = {
      {kAhem, "Ahem.woff"},
      {kAmiri, "third_party/Amiri/amiri_arabic.woff2"},
      {kMegalopolis, "third_party/MEgalopolis/MEgalopolisExtra.woff"},
      {kRoboto, "third_party/Roboto/roboto-regular.woff2"},
  };

  base::LapTimer timer;
  std::string param_string;
};

class OffsetForPositionPerfTest : public ShapeResultPerfTest,
                                  public testing::TestWithParam<float> {
 public:
  void OffsetForPosition(TextRun& run,
                         IncludePartialGlyphsOption partial,
                         BreakGlyphsOption breakopt) {
    timer.Reset();
    float position = GetParam();
    param_string = base::NumberToString(position);
    do {
      font.OffsetForPosition(run, position, partial, breakopt);
      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());
  }

 protected:
  void ReportResult(const std::string& story_prefix) {
    ShapeResultPerfTest::ReportResult(kMetricPrefixOffsetForPosition,
                                      story_prefix);
  }
};

class CharacterRangePerfTest : public ShapeResultPerfTest,
                               public testing::TestWithParam<int> {
 public:
  void GetCharacter(TextRun& run) {
    timer.Reset();
    int endpos = GetParam();
    param_string = base::NumberToString(endpos);
    do {
      font.SelectionRectForText(run, gfx::PointF(), 100, 0, endpos);
      timer.NextLap();
    } while (!timer.HasTimeLimitExpired());
  }

 protected:
  void ReportResult(const std::string& story_prefix) {
    ShapeResultPerfTest::ReportResult(kMetricPrefixCharacterRange,
                                      story_prefix);
  }
};

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionFullBreak) {
  TextRun run = SetupFont(kAhem, "FURACOLO", true);
  OffsetForPosition(run, kOnlyFullGlyphs, BreakGlyphsOption(true));
  ReportResult("LTR_full_break");
}

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionFullDontBreak) {
  TextRun run = SetupFont(kAhem, "FURACOLO", true);
  OffsetForPosition(run, kOnlyFullGlyphs, BreakGlyphsOption(false));
  ReportResult("LTR_full");
}

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionIncludePartialBreak) {
  TextRun run = SetupFont(kAhem, "FURACOLO", true);
  OffsetForPosition(run, kIncludePartialGlyphs, BreakGlyphsOption(true));
  ReportResult("LTR_partial_break");
}

TEST_P(OffsetForPositionPerfTest, LTROffsetForPositionIncludePartialDontBreak) {
  TextRun run = SetupFont(kAhem, "FURACOLO", true);
  OffsetForPosition(run, kIncludePartialGlyphs, BreakGlyphsOption(false));
  ReportResult("LTR_partial");
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionFullBreak) {
  TextRun run = SetupFont(kAhem, "OLOCARUF", false);
  OffsetForPosition(run, kOnlyFullGlyphs, BreakGlyphsOption(true));
  ReportResult("RTL_full_break");
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionFullDontBreak) {
  TextRun run = SetupFont(kAhem, "OLOCARUF", false);
  OffsetForPosition(run, kOnlyFullGlyphs, BreakGlyphsOption(false));
  ReportResult("RTL_full");
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionIncludePartialBreak) {
  TextRun run = SetupFont(kAhem, "OLOCARUF", false);
  OffsetForPosition(run, kIncludePartialGlyphs, BreakGlyphsOption(true));
  ReportResult("RTL_partial_break");
}

TEST_P(OffsetForPositionPerfTest, RTLOffsetForPositionIncludePartialDontBreak) {
  TextRun run = SetupFont(kAhem, "OLOCARUF", false);
  OffsetForPosition(run, kIncludePartialGlyphs, BreakGlyphsOption(false));
  ReportResult("RTL_partial");
}

INSTANTIATE_TEST_SUITE_P(OffsetForPosition,
                         OffsetForPositionPerfTest,
                         testing::Values(0, 10, 60, 100, 200, 350));

TEST_P(CharacterRangePerfTest, LTRCharacterForPosition) {
  TextRun run = SetupFont(kAhem, "FURACOLO", true);
  GetCharacter(run);
  ReportResult("LTR");
}

TEST_P(CharacterRangePerfTest, RTLCharacterForPosition) {
  TextRun run = SetupFont(kAhem, "OLOCARUF", false);
  GetCharacter(run);
  ReportResult("RTL");
}

INSTANTIATE_TEST_SUITE_P(CharacterRange,
                         CharacterRangePerfTest,
                         testing::Values(0, 1, 2, 4, 8));

}  // namespace blink
