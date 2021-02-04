// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <utility>

#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace chrome_pdf {
namespace {

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

void TestPaintImageResult(const SkISize& graphics_size,
                          const SkISize& src_size,
                          const gfx::Rect& paint_rect,
                          const SkIRect& overlapped_rect) {
  auto graphics = SkiaGraphics::Create(gfx::SkISizeToSize(graphics_size));
  ASSERT_TRUE(graphics);

  // Create snapshots as SkImage and SkBitmap after painting.
  graphics->PaintImage(CreateSourceImage(src_size), paint_rect);
  sk_sp<SkImage> snapshot = graphics->CreateSnapshot();
  SkBitmap snapshot_bitmap;
  ASSERT_TRUE(snapshot->asLegacyBitmap(&snapshot_bitmap));

  // Verify snapshot dimensions.
  EXPECT_EQ(snapshot->dimensions(), graphics_size)
      << snapshot->width() << " x " << snapshot->height()
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

TEST(SkiaGraphicsTest, PaintImage) {
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

}  // namespace
}  // namespace chrome_pdf
