// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

#include "cc/paint/display_item_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace blink {
namespace {

using testing::_;

class DrawingDisplayItemTest : public testing::Test {
 protected:
  FakeDisplayItemClient client_;
};

static sk_sp<PaintRecord> CreateRectRecord(const FloatRect& record_bounds) {
  PaintRecorder recorder;
  cc::PaintCanvas* canvas =
      recorder.beginRecording(record_bounds.Width(), record_bounds.Height());
  canvas->drawRect(record_bounds, PaintFlags());
  return recorder.finishRecordingAsPicture();
}

static sk_sp<PaintRecord> CreateRectRecordWithTranslate(
    const FloatRect& record_bounds,
    float dx,
    float dy) {
  PaintRecorder recorder;
  cc::PaintCanvas* canvas =
      recorder.beginRecording(record_bounds.Width(), record_bounds.Height());
  canvas->save();
  canvas->translate(dx, dy);
  canvas->drawRect(record_bounds, PaintFlags());
  canvas->restore();
  return recorder.finishRecordingAsPicture();
}

TEST_F(DrawingDisplayItemTest, DrawsContent) {
  FloatRect record_bounds(5.5, 6.6, 7.7, 8.8);
  DrawingDisplayItem item(client_.Id(), DisplayItem::Type::kDocumentBackground,
                          EnclosingIntRect(record_bounds),
                          CreateRectRecord(record_bounds),
                          client_.VisualRectOutsetForRasterEffects());
  EXPECT_EQ(EnclosingIntRect(record_bounds), item.VisualRect());
  EXPECT_TRUE(item.DrawsContent());
}

TEST_F(DrawingDisplayItemTest, NullPaintRecord) {
  DrawingDisplayItem item(client_.Id(), DisplayItem::Type::kDocumentBackground,
                          IntRect(), nullptr,
                          client_.VisualRectOutsetForRasterEffects());
  EXPECT_FALSE(item.DrawsContent());
}

TEST_F(DrawingDisplayItemTest, EmptyPaintRecord) {
  DrawingDisplayItem item(client_.Id(), DisplayItem::Type::kDocumentBackground,
                          IntRect(), sk_make_sp<PaintRecord>(),
                          RasterEffectOutset::kNone);
  EXPECT_FALSE(item.DrawsContent());
}

TEST_F(DrawingDisplayItemTest, EqualsForUnderInvalidation) {
  ScopedPaintUnderInvalidationCheckingForTest under_invalidation_checking(true);

  FloatRect bounds1(100.1, 100.2, 100.3, 100.4);
  DrawingDisplayItem item1(client_.Id(), DisplayItem::kDocumentBackground,
                           EnclosingIntRect(bounds1), CreateRectRecord(bounds1),
                           client_.VisualRectOutsetForRasterEffects());
  DrawingDisplayItem translated(client_.Id(), DisplayItem::kDocumentBackground,
                                EnclosingIntRect(bounds1),
                                CreateRectRecordWithTranslate(bounds1, 10, 20),
                                client_.VisualRectOutsetForRasterEffects());
  // This item contains a DrawingRecord that is different from but visually
  // equivalent to item1's.
  DrawingDisplayItem zero_translated(
      client_.Id(), DisplayItem::kDocumentBackground, EnclosingIntRect(bounds1),
      CreateRectRecordWithTranslate(bounds1, 0, 0),
      client_.VisualRectOutsetForRasterEffects());

  FloatRect bounds2(100.5, 100.6, 100.7, 100.8);
  DrawingDisplayItem item2(client_.Id(), DisplayItem::kDocumentBackground,
                           EnclosingIntRect(bounds1), CreateRectRecord(bounds2),
                           client_.VisualRectOutsetForRasterEffects());

  DrawingDisplayItem empty_item(client_.Id(), DisplayItem::kDocumentBackground,
                                IntRect(), nullptr,
                                client_.VisualRectOutsetForRasterEffects());

  EXPECT_TRUE(item1.EqualsForUnderInvalidation(item1));
  EXPECT_FALSE(item1.EqualsForUnderInvalidation(item2));
  EXPECT_FALSE(item1.EqualsForUnderInvalidation(translated));
  EXPECT_TRUE(item1.EqualsForUnderInvalidation(zero_translated));
  EXPECT_FALSE(item1.EqualsForUnderInvalidation(empty_item));

  EXPECT_FALSE(item2.EqualsForUnderInvalidation(item1));
  EXPECT_TRUE(item2.EqualsForUnderInvalidation(item2));
  EXPECT_FALSE(item2.EqualsForUnderInvalidation(translated));
  EXPECT_FALSE(item2.EqualsForUnderInvalidation(zero_translated));
  EXPECT_FALSE(item2.EqualsForUnderInvalidation(empty_item));

  EXPECT_FALSE(translated.EqualsForUnderInvalidation(item1));
  EXPECT_FALSE(translated.EqualsForUnderInvalidation(item2));
  EXPECT_TRUE(translated.EqualsForUnderInvalidation(translated));
  EXPECT_FALSE(translated.EqualsForUnderInvalidation(zero_translated));
  EXPECT_FALSE(translated.EqualsForUnderInvalidation(empty_item));

  EXPECT_TRUE(zero_translated.EqualsForUnderInvalidation(item1));
  EXPECT_FALSE(zero_translated.EqualsForUnderInvalidation(item2));
  EXPECT_FALSE(zero_translated.EqualsForUnderInvalidation(translated));
  EXPECT_TRUE(zero_translated.EqualsForUnderInvalidation(zero_translated));
  EXPECT_FALSE(zero_translated.EqualsForUnderInvalidation(empty_item));

  EXPECT_FALSE(empty_item.EqualsForUnderInvalidation(item1));
  EXPECT_FALSE(empty_item.EqualsForUnderInvalidation(item2));
  EXPECT_FALSE(empty_item.EqualsForUnderInvalidation(translated));
  EXPECT_FALSE(empty_item.EqualsForUnderInvalidation(zero_translated));
  EXPECT_TRUE(empty_item.EqualsForUnderInvalidation(empty_item));
}

TEST_F(DrawingDisplayItemTest, SolidColorRect) {
  FloatRect record_bounds(5, 6, 10, 10);
  DrawingDisplayItem item(client_.Id(), DisplayItem::Type::kDocumentBackground,
                          EnclosingIntRect(record_bounds),
                          CreateRectRecord(record_bounds),
                          client_.VisualRectOutsetForRasterEffects());
  EXPECT_EQ(IntRect(5, 6, 10, 10), item.VisualRect());
  EXPECT_TRUE(item.IsSolidColor());
}

TEST_F(DrawingDisplayItemTest, NonSolidColorSnappedRect) {
  FloatRect record_bounds(5.1, 6.9, 10, 10);
  DrawingDisplayItem item(client_.Id(), DisplayItem::Type::kDocumentBackground,
                          EnclosingIntRect(record_bounds),
                          CreateRectRecord(record_bounds),
                          client_.VisualRectOutsetForRasterEffects());
  EXPECT_EQ(IntRect(5, 6, 11, 11), item.VisualRect());
  // Not solid color if the drawing does not fully cover the visual rect.
  EXPECT_FALSE(item.IsSolidColor());
}

TEST_F(DrawingDisplayItemTest, NonSolidColorOval) {
  FloatRect record_bounds(5, 6, 10, 10);

  PaintRecorder recorder;
  cc::PaintCanvas* canvas =
      recorder.beginRecording(record_bounds.Width(), record_bounds.Height());
  canvas->drawOval(record_bounds, PaintFlags());

  DrawingDisplayItem item(client_.Id(), DisplayItem::Type::kDocumentBackground,
                          EnclosingIntRect(record_bounds),
                          recorder.finishRecordingAsPicture(),
                          client_.VisualRectOutsetForRasterEffects());
  EXPECT_EQ(IntRect(5, 6, 10, 10), item.VisualRect());
  // Not solid color if the drawing does not fully cover the visual rect.
  EXPECT_FALSE(item.IsSolidColor());
}

// This test ensures that DrawingDisplayItem::RectKnownToBeOpaque() doesn't
// cover any antialiased pixels around the corners.
TEST_F(DrawingDisplayItemTest, OpaqueRectForDrawRRect) {
  constexpr float kRadiusStep = 0.1;
  constexpr int kSize = 100;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kSize, kSize);
  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorWHITE);
  for (float r = kRadiusStep; r < kSize / 2; r += kRadiusStep) {
    PaintRecorder recorder;
    recorder.beginRecording(kSize, kSize)
        ->drawRRect(SkRRect::MakeRectXY(SkRect::MakeWH(kSize, kSize), r, r),
                    flags);
    DrawingDisplayItem item(
        client_.Id(), DisplayItem::Type::kDocumentBackground,
        IntRect(0, 0, kSize, kSize), recorder.finishRecordingAsPicture(),
        RasterEffectOutset::kNone);

    auto rect = item.RectKnownToBeOpaque();
    bitmap.eraseColor(SK_ColorBLACK);
    SkiaPaintCanvas(bitmap).drawPicture(item.GetPaintRecord());
    for (int y = rect.Y(); y < rect.MaxY(); ++y) {
      for (int x = rect.X(); x < rect.MaxX(); ++x) {
        SkColor pixel = bitmap.getColor(x, y);
        EXPECT_EQ(SK_ColorWHITE, pixel)
            << " radius=" << r << " x=" << x << " y=" << y
            << " non-white pixel=" << std::hex << pixel;
      }
    }
  }
}

}  // namespace
}  // namespace blink
