// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"

#include "base/functional/callback_helpers.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/run_loop.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
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
    test_registry.NotifyUpdateMemoryLimitAsync(
        base::MemoryLimit::FromPercent(0), base::DoNothing());
    test_registry.NotifyReleaseMemoryAsync(run_loop.QuitClosure());
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

  // Expected horizontal position of the first drawn glyph for each item
  // overlapping [kFrom, kTo): drawn glyphs are contiguous, starting at
  // `location`, and all Ahem glyphs advance by kFontSize.
  Vector<float> expected_lefts;
  for (const PlainTextItem& item :
       painter->SegmentAndShape(run, *font).ItemList()) {
    if (item.EndOffset() <= kFrom || kTo <= item.StartOffset()) {
      continue;
    }
    expected_lefts.push_back(location.x() +
                             kFontSize *
                                 (std::max(item.StartOffset(), kFrom) - kFrom));
  }
  ASSERT_GT(expected_lefts.size(), 1u);

  // Record the horizontal position of the first glyph in each drawn blob.
  cc::PaintRecorder recorder;
  EXPECT_TRUE(painter->DrawWithBidiReorder(
      run, kFrom, kTo, *font, Font::kUseFallbackIfFontNotReady,
      *recorder.beginRecording(), location, cc::PaintFlags()));
  const cc::PaintRecord record = recorder.finishRecordingAsPicture();

  Vector<float> actual_lefts;
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawTextBlob) {
      const auto& text_op = static_cast<const cc::DrawTextBlobOp&>(op);
      actual_lefts.push_back(text_op.x + text_op.blob->bounds().left());
    }
  }

  ASSERT_EQ(expected_lefts.size(), actual_lefts.size());
  for (wtf_size_t i = 0; i < expected_lefts.size(); ++i) {
    EXPECT_NEAR(expected_lefts[i], actual_lefts[i], 0.01f) << "item " << i;
  }
}

}  // namespace blink
