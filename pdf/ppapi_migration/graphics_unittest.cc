// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <memory>

#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

namespace {

SkBitmap GenerateExpectedBitmap(const SkISize& graphics_size,
                                const SkIRect& rect) {
  SkBitmap bitmap = CreateN32PremulSkBitmap(graphics_size);
  bitmap.erase(SK_ColorRED, rect);
  return bitmap;
}

// Creates a nonuniform SkBitmap with given `width` and `height`, such
// that scrolling will cause a noticeable change to the bitmap. Returns an
// empty SkBitmap if either `width` or `height` is less than 4.
SkBitmap CreateNonuniformBitmap(int width, int height) {
  if (width < 4 || height < 4)
    return SkBitmap();

  SkBitmap bitmap = CreateN32PremulSkBitmap(SkISize::Make(width, height));
  bitmap.eraseColor(SK_ColorRED);
  bitmap.erase(SK_ColorGREEN, {1, 1, width - 1, height - 2});
  bitmap.erase(SK_ColorBLACK, {2, 3, 1, 2});
  return bitmap;
}

}  // namespace

class SkiaGraphicsTest : public testing::Test {
 protected:
  void TestPaintImageResult(const SkISize& graphics_size,
                            const gfx::Size& src_size,
                            const gfx::Rect& paint_rect,
                            const SkIRect& overlapped_rect) {
    surface_ = SkSurface::MakeRasterN32Premul(graphics_size.width(),
                                              graphics_size.height());
    ASSERT_TRUE(surface_);
    graphics_ = std::make_unique<SkiaGraphics>(surface_.get());

    // Paint and snapshot.
    graphics_->PaintImage(CreateSkiaImageForTesting(src_size, SK_ColorRED),
                          paint_rect);
    SkBitmap snapshot_bitmap = MakeSnapshot();

    // Verify snapshot dimensions.
    EXPECT_EQ(snapshot_bitmap.dimensions(), graphics_size)
        << snapshot_bitmap.width() << " x " << snapshot_bitmap.height()
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

  SkBitmap MakeSnapshot() {
    SkBitmap bitmap;
    surface_->makeImageSnapshot()->asLegacyBitmap(&bitmap);
    return bitmap;
  }

  sk_sp<SkSurface> surface_;

  std::unique_ptr<Graphics> graphics_;
};

class SkiaGraphicsScrollTest : public SkiaGraphicsTest {
 protected:
  static constexpr gfx::Rect kGraphicsRect = gfx::Rect(4, 5);

  // Initializes `initial_bitmap_` and `graphics_` before scrolling tests.
  void SetUp() override {
    surface_ = SkSurface::MakeRasterN32Premul(kGraphicsRect.width(),
                                              kGraphicsRect.height());
    ASSERT_TRUE(surface_);
    graphics_ = std::make_unique<SkiaGraphics>(surface_.get());

    // Paint a nonuniform SkBitmap to graphics.
    initial_bitmap_ =
        CreateNonuniformBitmap(kGraphicsRect.width(), kGraphicsRect.height());
    graphics_->PaintImage(initial_bitmap_, kGraphicsRect);
    SkBitmap initial_snapshot = MakeSnapshot();

    ASSERT_TRUE(cc::MatchesBitmap(initial_snapshot, initial_bitmap_,
                                  cc::ExactPixelComparator(false)));
  }

  // Resets the canvas with `initial_bitmap_`, then scrolls it by
  // `scroll_amount`.
  void ResetAndScroll(const gfx::Vector2d& scroll_amount) {
    if (!graphics_)
      return;

    graphics_->PaintImage(initial_bitmap_, kGraphicsRect);
    graphics_->Scroll(kGraphicsRect, scroll_amount);
  }

  SkBitmap initial_bitmap_;
};

// static
constexpr gfx::Rect SkiaGraphicsScrollTest::kGraphicsRect;

TEST_F(SkiaGraphicsTest, PaintImage) {
  struct PaintImageParams {
    // Size of the graphics to be painted on.
    SkISize graphics_size;

    // Size of the source image.
    gfx::Size src_size;

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

TEST_F(SkiaGraphicsScrollTest, InvalidScroll) {
  static constexpr gfx::Vector2d kNoOpScrollAmounts[] = {
      // Scroll to the edge of the graphics rect.
      {kGraphicsRect.width(), 0},
      {-kGraphicsRect.width(), 0},
      {0, kGraphicsRect.height()},
      {0, -kGraphicsRect.height()},
      // Scroll outside the graphics rect.
      {kGraphicsRect.width() + 1, 0},
      {-(kGraphicsRect.width() + 2), 0},
      {0, kGraphicsRect.height() + 3},
      {0, -(kGraphicsRect.height() + 4)},
  };

  for (const auto& no_op_amount : kNoOpScrollAmounts) {
    ResetAndScroll(no_op_amount);
    SkBitmap snapshot = MakeSnapshot();
    EXPECT_TRUE(cc::MatchesBitmap(snapshot, initial_bitmap_,
                                  cc::ExactPixelComparator(false)))
        << "SkBitmap comparison failed for scroll amount of "
        << no_op_amount.ToString();
  }
}

TEST_F(SkiaGraphicsScrollTest, Scroll) {
  static constexpr gfx::Vector2d kValidScrollAmounts[] = {
      {1, 0},
      {-2, 0},
      {0, 3},
      {0, -3},
  };

  for (const auto& valid_amount : kValidScrollAmounts) {
    ResetAndScroll(valid_amount);
    SkBitmap snapshot = MakeSnapshot();
    EXPECT_FALSE(cc::MatchesBitmap(snapshot, initial_bitmap_,
                                   cc::ExactPixelComparator(false)))
        << "The scroll amount of " << valid_amount.ToString()
        << " failed to change the snapshot of `graphics_`";
  }
}

}  // namespace chrome_pdf
