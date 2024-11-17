// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {
class FontsHolder : public GarbageCollected<FontsHolder> {
 public:
  void Trace(Visitor* visitor) const {
    for (const Font& font : fonts) {
      font.Trace(visitor);
    }
  }

  std::vector<Font> fonts;
};
}  // namespace

class TextMetricsTest : public FontTestBase {
 public:
  enum FontType {
    kLatinFont = 0,
    kArabicFont = 1,
    kCJKFont = 2,
  };

 protected:
  void SetUp() override {
    FontDescription::VariantLigatures ligatures;
    fonts_holder = MakeGarbageCollected<FontsHolder>();
    fonts_holder->fonts.push_back(blink::test::CreateTestFont(
        AtomicString("Roboto"),
        blink::test::PlatformTestDataPath(
            "third_party/Roboto/roboto-regular.woff2"),
        12.0, &ligatures));

    fonts_holder->fonts.push_back(blink::test::CreateTestFont(
        AtomicString("Noto"),
        blink::test::PlatformTestDataPath(
            "third_party/Noto/NotoNaskhArabic-regular.woff2"),
        12.0, &ligatures));

    fonts_holder->fonts.push_back(blink::test::CreateTestFont(
        AtomicString("M PLUS 1p"),
        blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"),
        12.0, &ligatures));
  }

  void TearDown() override {}

  const Font& GetFont(FontType type) const { return fonts_holder->fonts[type]; }

  FontCachePurgePreventer font_cache_purge_preventer;
  Persistent<FontsHolder> fonts_holder;
};

// Tests for CaretPositionForOffset with mixed bidi text
struct CaretPositionForOffsetBidiTestData {
  // The string that should be processed.
  const UChar* string;
  // Text direction to test
  TextDirection direction;
  // The expected positions to test.
  std::vector<unsigned> positions;
  // Points to test.
  std::vector<double> points;
  // The font to use
  TextMetricsTest::FontType font;
} caret_position_for_offset_test_data[] = {
    // Values are carefully chosen to verify that the bidi correction rules
    // behave as expected.
    // 0
    {u"0123456789",
     TextDirection::kLtr,
     {0, 0, 0, 1, 5, 5, 9, 10, 10, 10},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 32, 36, 62, 66, 67.38, 70},
#else
     {-5, 0, 2, 5, 33, 37, 65, 68, 70, 75},
#endif
     TextMetricsTest::kLatinFont},

    // 1
    {u"0123456789",
     TextDirection::kRtl,
     {0, 0, 0, 1, 5, 5, 9, 10, 10, 10},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 32, 36, 62, 66, 67.38, 70},
#else
     {-5, 0, 2, 5, 33, 37, 65, 68, 70, 75},
#endif
     TextMetricsTest::kLatinFont},

    // 2
    {u"0fi1fi23fif456fifi",
     TextDirection::kLtr,
     {0, 0, 0, 1, 10, 11, 16, 17, 18, 18},
#if BUILDFLAG(IS_WIN)
     {-5, 0, 2, 5, 49, 53, 81, 85, 88, 90},
#else
     {-5, 0, 2, 5, 46, 50, 77, 80, 83, 85},
#endif
     TextMetricsTest::kLatinFont},

    // 3
    {u"0fi1fi23fif456fifi",
     TextDirection::kRtl,
     {0, 0, 0, 1, 10, 11, 16, 17, 18, 18},
#if BUILDFLAG(IS_WIN)
     {-5, 0, 2, 5, 49, 53, 81, 85, 88, 90},
#else
     {-5, 0, 2, 5, 46, 50, 77, 80, 83, 85},
#endif
     TextMetricsTest::kLatinFont},

    // 4
    {u"مَ1مَمَ23مَمَمَ345مَمَمَمَ",
     TextDirection::kLtr,
     {26, 26, 26, 15, 15, 18, 18, 7, 7, 9, 9, 2, 2, 3, 3, 0, 0, 0},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 3, 20, 23, 40, 45, 57, 61, 71, 74, 82, 86, 90, 93, 96, 97.306,
      105},
#elif BUILDFLAG(IS_WIN)
     {-5, 0, 3, 20, 22, 40, 44, 56, 60, 70, 74, 81, 85, 89, 91, 94, 96, 105},
#else
     {-5, 0, 3, 21, 25, 41, 47, 60, 64, 74, 78, 87, 91, 94, 96, 100, 102, 105},
#endif
     TextMetricsTest::kArabicFont},

    // 5
    {u"مَ1مَمَ23مَمَمَ345مَمَمَمَ",
     TextDirection::kRtl,
     {26, 26, 26, 18, 18, 15, 15, 9, 9, 7, 7, 3, 3, 2, 2, 0, 0, 0},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 3, 20, 23, 40, 45, 57, 61, 71, 74, 82, 86, 90, 93, 96, 97.306,
      105},
#elif BUILDFLAG(IS_WIN)
     {-5, 0, 3, 20, 22, 40, 44, 56, 60, 70, 74, 81, 85, 89, 91, 94, 96, 105},
#else
     {-5, 0, 3, 21, 25, 41, 47, 60, 64, 74, 78, 87, 91, 94, 96, 100, 102, 105},
#endif
     TextMetricsTest::kArabicFont},

    // 6
    {u"あ1あمَ23あمَあ345",
     TextDirection::kLtr,
#if BUILDFLAG(IS_FUCHSIA)  // Very very narrrow glyph
     {0, 0, 0, 3, 5, 7, 7, 7, 8, 8, 8, 10, 10, 14, 14, 14},
#else
     {0, 0, 0, 3, 5, 7, 7, 7, 7, 8, 8, 10, 10, 14, 14, 14},
#endif
#if BUILDFLAG(IS_FUCHSIA)
     {-5, 0, 4, 21, 25, 35, 39, 46, 49, 50, 53, 55, 59, 84, 86, 95},
#else
     {-5, 0, 4, 29, 33, 43, 47, 50, 53, 61, 65, 67, 71, 100, 102, 110},
#endif
     TextMetricsTest::kArabicFont},

    // 7
    {u"あ1あمَ23あمَあ345",
     TextDirection::kRtl,
     {10, 10, 10, 10, 10, 8, 8, 5, 5, 3, 3, 3, 3, 3},
#if BUILDFLAG(IS_FUCHSIA)
     {-5, 0, 4, 27, 31, 33, 37, 55, 59, 61, 65, 84, 86, 95},
#else
     {-5, 0, 3, 31, 35, 37, 41, 63, 67, 69, 73, 100, 102, 110},
#endif
     TextMetricsTest::kArabicFont},

    // 8
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kLtr,
     {0, 0, 0, 1, 20, 20, 21, 22, 22},
     {-5, 0, 1, 10, 210, 215, 228, 234, 250},
     TextMetricsTest::kCJKFont},

    // 9
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kRtl,
     {22, 22, 22, 21, 21, 21, 1, 21, 21, 21},
     {-5, 0, 1, 11, 12, 14, 20, 234, 237, 250},
     TextMetricsTest::kCJKFont},

    // 10
    {u"123楽しいドライブ、0123",
     TextDirection::kLtr,
     {0, 0, 0, 1, 7, 8, 14, 15, 15, 15},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 72, 78, 142, 145, 148, 152},
#else
     {-5, 0, 2, 5, 72, 78, 140, 143, 145, 150},
#endif
     TextMetricsTest::kCJKFont},

    // 11
    {u"123楽しいドライブ、0123",
     TextDirection::kRtl,
     {0, 0, 0, 1, 7, 8, 14, 15, 15, 15},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 72, 78, 142, 145, 148, 152},
#else
     {-5, 0, 2, 5, 72, 78, 140, 143, 145, 150},
#endif
     TextMetricsTest::kCJKFont},
};
class CaretPositionForOffsetBidiTest
    : public TextMetricsTest,
      public testing::WithParamInterface<CaretPositionForOffsetBidiTestData> {};
INSTANTIATE_TEST_SUITE_P(
    TextMetrics,
    CaretPositionForOffsetBidiTest,
    testing::ValuesIn(caret_position_for_offset_test_data));

TEST_P(CaretPositionForOffsetBidiTest, CaretPositionForOffsetsBidi) {
  const auto& test_data = GetParam();
  String text_string(test_data.string);
  TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(
      GetFont(test_data.font), test_data.direction, kAlphabeticTextBaseline,
      kLeftTextAlign, text_string);

  for (wtf_size_t i = 0; i < test_data.points.size(); ++i) {
    EXPECT_EQ(test_data.positions[i],
              text_metrics->caretPositionFromPoint(test_data.points[i]))
        << "at index " << i;
  }
}

}  // namespace blink
