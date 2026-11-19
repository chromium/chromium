// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skia_paint_canvas.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace blink {

class PlainTextPainterTest : public FontTestBase {
 public:
  void SetUp() override {
    FontTestBase::SetUp();
    MemoryPressureListenerRegistry::Initialize();
  }

  size_t CacheMapSizeOf(const PlainTextPainter& painter) const {
    return painter.cache_map_.size();
  }
};

TEST_F(PlainTextPainterTest, MemoryPressure) {
  // We don't cache on low-end devices.
  if (MemoryPressureListenerRegistry::IsLowEndDevice()) {
    return;
  }
  base::TestMemoryConsumerRegistry test_registry;

  auto* painter =
      MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
  EXPECT_EQ(0u, CacheMapSizeOf(*painter));
  Font* font = test::CreateAhemFont(40);
  painter->SegmentAndShape(TextRun("text"), *font);
  EXPECT_EQ(1u, CacheMapSizeOf(*painter));

  // Update limit to critical (0) and release memory.
  {
    base::RunLoop run_loop;
    test_registry.NotifyUpdateMemoryLimitAndReleaseAsync(
        base::MemoryLimit::FromPercent(0), run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_EQ(0u, CacheMapSizeOf(*painter));
}

// Sub-ranges spanning multiple PlainTextItems, and starting in the middle of
// the first item.
TEST_F(PlainTextPainterTest, ComputeSubInlineSizeMultipleItems) {
  constexpr float kFontSize = 10;
  auto* painter =
      MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
  Font* font = test::CreateAhemFont(kFontSize);
  const TextRun run("AB CD EF");
  ASSERT_GT(painter->SegmentAndShape(run, *font).ItemList().size(), 1u);

  // All Ahem glyphs advance by kFontSize.
  EXPECT_FLOAT_EQ(4 * kFontSize,
                  painter->ComputeSubInlineSize(run, 1, 5, *font));
  EXPECT_FLOAT_EQ(6 * kFontSize,
                  painter->ComputeSubInlineSize(run, 1, 7, *font));
  EXPECT_FLOAT_EQ(7 * kFontSize,
                  painter->ComputeSubInlineSize(run, 0, 7, *font));
}

TEST_F(PlainTextPainterTest, DrawWithBidiReorderSubRunMultipleItems) {
  constexpr float kFontSize = 10;
  constexpr wtf_size_t kFrom = 1;
  constexpr wtf_size_t kTo = 7;
  const gfx::PointF location(100, 50);
  auto* painter =
      MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
  Font* font = test::CreateAhemFont(kFontSize);
  const TextRun run("AB CD EF");
  ASSERT_GT(painter->SegmentAndShape(run, *font).ItemList().size(), 1u);

  // Contiguous same-direction items overlapping [kFrom, kTo) are coalesced
  // into a single text blob starting at location
  cc::PaintRecorder recorder;
  EXPECT_TRUE(painter->DrawWithBidiReorder(
      run, kFrom, kTo, *font, Font::kUseFallbackIfFontNotReady,
      *recorder.beginRecording(), location, cc::PaintFlags()));
  const cc::PaintRecord record = recorder.finishRecordingAsPicture();
  ASSERT_FALSE(record.empty());

  Vector<float> actual_lefts;
  Vector<float> actual_widths;
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawTextBlob) {
      const auto& text_op = static_cast<const cc::DrawTextBlobOp&>(op);
      actual_lefts.push_back(text_op.x + text_op.blob->bounds().left());
      actual_widths.push_back(text_op.blob->bounds().width());
    }
  }

  ASSERT_EQ(1u, actual_lefts.size());
  EXPECT_NEAR(location.x(), actual_lefts[0], 0.01f);
  const float expected_width =
      painter->ComputeSubInlineSize(run, kFrom, kTo, *font);
  EXPECT_NEAR(expected_width, actual_widths[0], 0.01f);

  // Mixed-BiDi text splits only at direction boundaries, coalescing multi-word
  // runs within each direction
  const String bidi_text(u"AB \u05D0\u05D1 \u05D2\u05D3 EF");
  const TextRun bidi_run(bidi_text, TextDirection::kLtr);
  cc::PaintRecorder bidi_recorder;
  EXPECT_TRUE(painter->DrawWithBidiReorder(
      bidi_run, 1, 10, *font, Font::kUseFallbackIfFontNotReady,
      *bidi_recorder.beginRecording(), location, cc::PaintFlags()));
  const cc::PaintRecord bidi_record = bidi_recorder.finishRecordingAsPicture();
  ASSERT_FALSE(bidi_record.empty());

  Vector<float> bidi_xs;
  for (const cc::PaintOp& op : bidi_record) {
    if (op.GetType() == cc::PaintOpType::kDrawTextBlob) {
      const auto& text_op = static_cast<const cc::DrawTextBlobOp&>(op);
      bidi_xs.push_back(text_op.x);
    }
  }

  const float ltr_prefix_width =
      painter->ComputeSubInlineSize(bidi_run, 1, 3, *font);
  const float rtl_run_width =
      painter->ComputeSubInlineSize(bidi_run, 3, 8, *font);
  ASSERT_EQ(3u, bidi_xs.size());
  EXPECT_NEAR(location.x(), bidi_xs[0], 0.01f);
  EXPECT_NEAR(location.x() + ltr_prefix_width, bidi_xs[1], 0.01f);
  EXPECT_NEAR(location.x() + ltr_prefix_width + rtl_run_width, bidi_xs[2],
              0.01f);
}

// Verifies that drawing contiguous visual sub-ranges via DrawNode produces
// the exact same pixels as drawing the full PlainTextNode in one call
// (crbug.com/578524618)
TEST_F(PlainTextPainterTest, DrawNodeSubRangesMatchFullDrawPixels) {
  // Fits the longest 11-character run in 10px Ahem with a 4px margin
  constexpr int kWidth = 118;
  constexpr int kHeight = 16;
  constexpr float kFontSize = 10;
  const gfx::PointF origin(4, 12);
  auto* painter =
      MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
  Font* font = test::CreateAhemFont(kFontSize);
  cc::PaintFlags flags;
  flags.setColor(SK_ColorBLACK);

  struct RangePair {
    wtf_size_t from;
    wtf_size_t to;
  };
  auto check_sub_ranges = [&](const char* label, const TextRun& run,
                              base::span<const RangePair> visual_ranges,
                              Font::DrawType draw_type) {
    SCOPED_TRACE(label);
    ASSERT_FALSE(visual_ranges.empty());
    const PlainTextNode& node = painter->SegmentAndShape(run, *font);

    SkBitmap full_bitmap;
    full_bitmap.allocN32Pixels(kWidth, kHeight);
    full_bitmap.eraseColor(SK_ColorTRANSPARENT);
    cc::SkiaPaintCanvas full_canvas(full_bitmap);
    painter->DrawNode(node, 0, node.TextContent().length(), *font, full_canvas,
                      origin, flags, draw_type);

    SkBitmap sub_bitmap;
    sub_bitmap.allocN32Pixels(kWidth, kHeight);
    sub_bitmap.eraseColor(SK_ColorTRANSPARENT);
    cc::SkiaPaintCanvas sub_canvas(sub_bitmap);
    gfx::PointF curr = origin;
    for (const RangePair& range : visual_ranges) {
      painter->DrawNode(node, range.from, range.to, *font, sub_canvas, curr,
                        flags, draw_type);
      curr.Offset(
          painter->ComputeSubInlineSize(run, range.from, range.to, *font), 0);
    }

    // Report only the first differing pixel instead of one failure per pixel
    for (int y = 0; y < kHeight; ++y) {
      for (int x = 0; x < kWidth; ++x) {
        ASSERT_EQ(full_bitmap.getColor(x, y), sub_bitmap.getColor(x, y))
            << "pixel (" << x << ", " << y << ")";
      }
    }
  };

  const RangePair kLtrRanges[] = {{0, 1}, {1, 4}, {4, 7}, {7, 8}};
  check_sub_ranges("LTR", TextRun("AB CD EF"), kLtrRanges,
                   Font::DrawType::kGlyphsOnly);

  const String rtl_text(u"\u05D0\u05D1 \u05D2\u05D3 \u05D4\u05D5");
  const RangePair kRtlVisualRanges[] = {{7, 8}, {4, 7}, {1, 4}, {0, 1}};
  check_sub_ranges("RTL", TextRun(rtl_text, TextDirection::kRtl),
                   kRtlVisualRanges, Font::DrawType::kGlyphsOnly);

  const String bidi_text(u"AB \u05D0\u05D1 \u05D2\u05D3 EF");
  const RangePair kBidiVisualRanges[] = {{0, 1}, {1, 3},  {6, 8},
                                         {3, 6}, {8, 10}, {10, 11}};
  check_sub_ranges("BiDi glyphs only", TextRun(bidi_text, TextDirection::kLtr),
                   kBidiVisualRanges, Font::DrawType::kGlyphsOnly);
  check_sub_ranges("BiDi glyphs and clusters",
                   TextRun(bidi_text, TextDirection::kLtr), kBidiVisualRanges,
                   Font::DrawType::kGlyphsAndClusters);
}

}  // namespace blink
