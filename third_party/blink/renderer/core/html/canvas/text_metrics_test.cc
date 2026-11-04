// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/core/html/canvas/shaped_text.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {
class FontsHolder : public GarbageCollected<FontsHolder> {
 public:
  void Trace(Visitor* visitor) const { visitor->Trace(fonts); }

  HeapVector<Member<Font>> fonts;
};
}  // namespace

class TextMetricsTest : public testing::Test {
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

  const Font* GetFont(FontType type) const { return fonts_holder->fonts[type]; }

  test::TaskEnvironment task_environment_;
  Persistent<FontsHolder> fonts_holder;
};

// getIndexFromOffset() is forwarded to ShapedText::advanceToPosition(), which
// is covered in shaped_text_test.cc.
TEST_F(TextMetricsTest, GetIndexFromOffsetMatchesShapedText) {
  ScopedNoFontAntialiasingForTest disable_no_font_antialiasing_for_test(false);

  struct {
    const UChar* string;
    TextDirection direction;
    FontType font;
  } cases[] = {
      {u"0fi1fi23fif456fifi", TextDirection::kLtr, kLatinFont},
      {u"\u0645\u064E1\u0645\u064E\u0645\u064E23", TextDirection::kRtl,
       kArabicFont},
      {u"123\u697D\u3057\u3044\u30010123", TextDirection::kLtr, kCJKFont},
  };
  for (const auto& c : cases) {
    const String text(c.string);
    ShapedText* shaped_text = MakeGarbageCollected<ShapedText>(
        GetFont(c.font), c.direction, V8CanvasTextBaseline::Enum::kAlphabetic,
        V8CanvasTextAlign::Enum::kLeft, text,
        *MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas));
    TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(*shaped_text);
    for (double x = -5; x <= text_metrics->width() + 5; x += 1) {
      EXPECT_EQ(shaped_text->advanceToPosition(x),
                text_metrics->getIndexFromOffset(x))
          << text << " at " << x;
    }
  }
}

// getTextClusters() with no range covers the whole text, so an empty text
// yields no clusters.
TEST_F(TextMetricsTest, GetTextClustersEmptyText) {
  // measureText(''), and an empty ShapedText as used in frame-less documents.
  ShapedText* shaped_texts[] = {
      MakeGarbageCollected<ShapedText>(
          GetFont(kLatinFont), TextDirection::kLtr,
          V8CanvasTextBaseline::Enum::kAlphabetic,
          V8CanvasTextAlign::Enum::kLeft, g_empty_string,
          *MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas)),
      MakeGarbageCollected<ShapedText>(),
  };
  for (ShapedText* shaped_text : shaped_texts) {
    TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(*shaped_text);
    EXPECT_TRUE(text_metrics->getTextClusters(/*options=*/nullptr).empty());
    EXPECT_TRUE(shaped_text->glyphClusters().empty());
  }
}

}  // namespace blink
