// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/test/task_environment.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/ppapi_migration/image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace chrome_pdf {

namespace {

struct FakeSkiaGraphicsClient : public SkiaGraphics::Client {
  FakeSkiaGraphicsClient() = default;
  ~FakeSkiaGraphicsClient() override = default;

  void UpdateSnapshot(sk_sp<SkImage> new_snapshot) override {
    snapshot = std::move(new_snapshot);
  }

  sk_sp<SkImage> snapshot;
};

Image CreateSourceImage(const SkISize& src_size) {
  SkBitmap bitmap = CreateN32PremulSkBitmap(src_size);
  bitmap.eraseColor(SK_ColorRED);
  return Image(bitmap);
}

SkBitmap GenerateExpectedBitmap(const SkISize& graphics_size,
                                const SkIRect& rect) {
  SkBitmap bitmap = CreateN32PremulSkBitmap(graphics_size);
  bitmap.erase(SK_ColorRED, rect);
  return bitmap;
}

}  // namespace

class SkiaGraphicsTest : public testing::Test {
 protected:
  void TestPaintImageResult(const SkISize& graphics_size,
                            const SkISize& src_size,
                            const gfx::Rect& paint_rect,
                            const SkIRect& overlapped_rect) {
    graphics_ =
        SkiaGraphics::Create(&client_, gfx::SkISizeToSize(graphics_size));
    ASSERT_TRUE(graphics_);

    // Create snapshots as SkImage and SkBitmap after painting.
    graphics_->PaintImage(CreateSourceImage(src_size), paint_rect);
    graphics_->Flush(base::DoNothing());
    SkBitmap snapshot_bitmap;
    ASSERT_TRUE(client_.snapshot->asLegacyBitmap(&snapshot_bitmap));

    // Verify snapshot dimensions.
    EXPECT_EQ(client_.snapshot->dimensions(), graphics_size)
        << client_.snapshot->width() << " x " << client_.snapshot->height()
        << " != " << graphics_size.width() << " x " << graphics_size.height();

    // Verify the snapshot matches the expected result.
    const SkBitmap expected_bitmap =
        GenerateExpectedBitmap(graphics_size, overlapped_rect);
    EXPECT_TRUE(
        cc::MatchesBitmap(snapshot_bitmap, expected_bitmap,
                          cc::ExactPixelComparator(/*discard_alpha=*/false)))
        << "SkBitmap comparison failed for graphics size of "
        << graphics_size.width() << " x " << graphics_size.height();
  }

  FakeSkiaGraphicsClient client_;

  std::unique_ptr<Graphics> graphics_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SkiaGraphicsTest, Flush) {
  graphics_ = SkiaGraphics::Create(&client_, gfx::Size(20, 20));
  ASSERT_TRUE(graphics_);

  // The client's snapshot is nullptr before flushing.
  EXPECT_FALSE(client_.snapshot);

  EXPECT_TRUE(graphics_->Flush(base::DoNothing()));

  // The client's snapshot has changed after flushing.
  EXPECT_TRUE(client_.snapshot);
}

TEST_F(SkiaGraphicsTest, PaintImage) {
  struct PaintImageParams {
    // Size of the graphics to be painted on.
    SkISize graphics_size;

    // Size of the source image.
    SkISize src_size;

    // Painting area.
    gfx::Rect paint_rect;

    // Common area of the graphics, the source image and the painting area.
    SkIRect overlapped_rect;
  };

  static constexpr PaintImageParams kPaintImageTestParams[] = {
      // Paint area is within the graphics and the source image.
      {{20, 20}, {15, 15}, gfx::Rect(0, 0, 10, 10), {0, 0, 10, 10}},
      // Paint area is not completely within the graphics, or the source
      // image.
      {{50, 30}, {30, 50}, gfx::Rect(10, 10, 30, 30), {10, 10, 30, 30}},
      // Paint area is outside the graphics.
      {{10, 10}, {30, 30}, gfx::Rect(10, 10, 10, 10), {0, 0, 0, 0}},
      // Paint area is outside the source image.
      {{15, 15}, {5, 5}, gfx::Rect(10, 10, 5, 5), {0, 0, 0, 0}},
  };

  for (const auto& params : kPaintImageTestParams)
    TestPaintImageResult(params.graphics_size, params.src_size,
                         params.paint_rect, params.overlapped_rect);
}

}  // namespace chrome_pdf
