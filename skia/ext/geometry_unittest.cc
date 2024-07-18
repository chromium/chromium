// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/geometry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace skia {

namespace {

TEST(Geometry, ScaleRectProportional) {
  SkRect output_rect;

  output_rect = ScaleSkRectProportional(SkRect::MakeXYWH(0, 0, 20, 20),
                                        SkRect::MakeXYWH(0, 0, 40, 40),
                                        SkRect::MakeXYWH(10, 10, 20, 20));
  EXPECT_EQ(output_rect, SkRect::MakeXYWH(5, 5, 10, 10));

  output_rect = ScaleSkRectProportional(SkRect::MakeXYWH(0, 10, 20, 80),
                                        SkRect::MakeXYWH(10, 20, 40, 80),
                                        SkRect::MakeXYWH(0, 30, 60, 20));
  EXPECT_EQ(output_rect, SkRect::MakeXYWH(-5, 20, 30, 20));
}

TEST(Geometry, TileSimple) {
  SkBitmap bm0;
  SkBitmap bm1;

  // Simple tiling that divides the region into two tiles.
  bm0.allocPixels(SkImageInfo::MakeN32Premul(20, 40));
  bm1.allocPixels(SkImageInfo::MakeN32Premul(10, 20));
  SkRect dest_rect = SkRect::MakeXYWH(10, 10, 20, 40);
  std::vector<SkRect> source_rects = {
      SkRect::MakeXYWH(0, 0, 20, 40),
      SkRect::MakeXYWH(0, 0, 10, 20),
  };
  std::vector<sk_sp<SkImage>> source_images = {
      SkImages::RasterFromBitmap(bm0),
      SkImages::RasterFromBitmap(bm1),
  };
  int32_t source_max_size = 22;

  Tiling t(dest_rect, source_rects, source_images, source_max_size);
  EXPECT_EQ(1, t.GetTileCountX());
  EXPECT_EQ(2, t.GetTileCountY());

  SkRect tile_dest_rect;
  std::vector<SkRect> tile_source_rects;
  std::vector<std::optional<SkIRect>> tile_subset_rects;

  t.GetTileRect(0, 0, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(10, 10, 20, 20));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 20, 20));
  EXPECT_EQ(tile_source_rects[1], SkRect::MakeXYWH(0, 0, 10, 10));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(0, 0, 20, 20));
  EXPECT_EQ(tile_subset_rects[1].value(), SkIRect::MakeXYWH(0, 0, 10, 10));

  t.GetTileRect(0, 1, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(10, 30, 20, 20));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 20, 20));
  EXPECT_EQ(tile_source_rects[1], SkRect::MakeXYWH(0, 0, 10, 10));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(0, 20, 20, 20));
  EXPECT_EQ(tile_subset_rects[1].value(), SkIRect::MakeXYWH(0, 10, 10, 10));
}

TEST(Geometry, TileSmallX) {
  SkBitmap bm0;

  // A tiling that has very different width and height.
  bm0.allocPixels(SkImageInfo::MakeN32Premul(5, 50));
  SkRect dest_rect = SkRect::MakeXYWH(0, 0, 5, 50);
  std::vector<SkRect> source_rects = {
      SkRect::MakeXYWH(0, 0, 5, 50),
  };
  std::vector<sk_sp<SkImage>> source_images = {
      SkImages::RasterFromBitmap(bm0),
  };
  int32_t source_max_size = 21;

  Tiling t(dest_rect, source_rects, source_images, source_max_size);
  EXPECT_EQ(1, t.GetTileCountX());
  EXPECT_EQ(3, t.GetTileCountY());

  SkRect tile_dest_rect;
  std::vector<SkRect> tile_source_rects;
  std::vector<std::optional<SkIRect>> tile_subset_rects;

  t.GetTileRect(0, 0, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(0, 0, 5, 20));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 5, 20));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(0, 0, 5, 20));

  t.GetTileRect(0, 1, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(0, 20, 5, 20));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 5, 20));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(0, 20, 5, 20));

  t.GetTileRect(0, 2, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(0, 40, 5, 10));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 5, 10));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(0, 40, 5, 10));
}

TEST(Geometry, TileSmallY) {
  SkBitmap bm0;

  // A tiling that has very different width and height.
  bm0.allocPixels(SkImageInfo::MakeN32Premul(50, 5));
  SkRect dest_rect = SkRect::MakeXYWH(0, 0, 50, 5);
  std::vector<SkRect> source_rects = {
      SkRect::MakeXYWH(0, 0, 50, 5),
  };
  std::vector<sk_sp<SkImage>> source_images = {
      SkImages::RasterFromBitmap(bm0),
  };
  int32_t source_max_size = 21;

  Tiling t(dest_rect, source_rects, source_images, source_max_size);
  EXPECT_EQ(3, t.GetTileCountX());
  EXPECT_EQ(1, t.GetTileCountY());

  SkRect tile_dest_rect;
  std::vector<SkRect> tile_source_rects;
  std::vector<std::optional<SkIRect>> tile_subset_rects;

  t.GetTileRect(0, 0, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(0, 0, 20, 5));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 20, 5));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(0, 0, 20, 5));

  t.GetTileRect(1, 0, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(20, 0, 20, 5));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 20, 5));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(20, 0, 20, 5));

  t.GetTileRect(2, 0, tile_dest_rect, tile_source_rects, tile_subset_rects);
  EXPECT_EQ(tile_dest_rect, SkRect::MakeXYWH(40, 0, 10, 5));
  EXPECT_EQ(tile_source_rects[0], SkRect::MakeXYWH(0, 0, 10, 5));
  EXPECT_EQ(tile_subset_rects[0].value(), SkIRect::MakeXYWH(40, 0, 10, 5));
}

TEST(Geometry, Overflow) {
  SkBitmap bm0;
  SkBitmap bm1;

  // The tiling will overflow the bounds of bm1. Ensure that its subsets
  // are set to empty instead of overflowing values.
  bm0.allocPixels(SkImageInfo::MakeN32Premul(40, 80));
  bm1.allocPixels(SkImageInfo::MakeN32Premul(10, 20));
  SkRect dest_rect = SkRect::MakeXYWH(10, 10, 20, 40);
  std::vector<SkRect> source_rects = {
      SkRect::MakeXYWH(10, 10, 20, 40),
      SkRect::MakeXYWH(5, 5, 10, 20),
  };
  std::vector<sk_sp<SkImage>> source_images = {
      SkImages::RasterFromBitmap(bm0),
      SkImages::RasterFromBitmap(bm1),
  };
  int32_t source_max_size = 15;

  Tiling t(dest_rect, source_rects, source_images, source_max_size);

  constexpr int kTilesX = 2;
  constexpr int kTilesY = 3;
  EXPECT_EQ(kTilesX, t.GetTileCountX());
  EXPECT_EQ(kTilesY, t.GetTileCountY());

  // Save the output from all tiles for comparison.
  SkRect t_dest[kTilesX][kTilesY];
  SkRect t_base[kTilesX][kTilesY];
  SkRect t_gain[kTilesX][kTilesY];
  SkIRect s_base[kTilesX][kTilesY];
  SkIRect s_gain[kTilesX][kTilesY];

  for (int x = 0; x < kTilesX; ++x) {
    for (int y = 0; y < kTilesY; ++y) {
      SkRect tile_dest_rect;
      std::vector<SkRect> tile_source_rects;
      std::vector<std::optional<SkIRect>> tile_subset_rects;
      t.GetTileRect(x, y, tile_dest_rect, tile_source_rects, tile_subset_rects);
      t_dest[x][y] = tile_dest_rect;
      t_base[x][y] = tile_source_rects[0];
      t_gain[x][y] = tile_source_rects[1];
      s_base[x][y] = tile_subset_rects[0].value();
      s_gain[x][y] = tile_subset_rects[1].value();
    }
  }

  EXPECT_EQ(t_dest[0][0], SkRect::MakeLTRB(10, 10, 23, 24));
  EXPECT_EQ(t_dest[0][1], SkRect::MakeLTRB(10, 24, 23, 38));
  EXPECT_EQ(t_dest[0][2], SkRect::MakeLTRB(10, 38, 23, 50));
  EXPECT_EQ(t_dest[1][0], SkRect::MakeLTRB(23, 10, 30, 24));
  EXPECT_EQ(t_dest[1][1], SkRect::MakeLTRB(23, 24, 30, 38));
  EXPECT_EQ(t_dest[1][2], SkRect::MakeLTRB(23, 38, 30, 50));

  EXPECT_EQ(t_base[0][0], SkRect::MakeLTRB(0, 0, 13, 14));
  EXPECT_EQ(t_base[0][1], SkRect::MakeLTRB(0, 0, 13, 14));
  EXPECT_EQ(t_base[0][2], SkRect::MakeLTRB(0, 0, 13, 12));
  EXPECT_EQ(t_base[1][0], SkRect::MakeLTRB(0, 0, 7, 14));
  EXPECT_EQ(t_base[1][1], SkRect::MakeLTRB(0, 0, 7, 14));
  EXPECT_EQ(t_base[1][2], SkRect::MakeLTRB(0, 0, 7, 12));

  EXPECT_EQ(t_gain[0][0], SkRect::MakeLTRB(0, 0, 6.5, 7));
  EXPECT_EQ(t_gain[0][1], SkRect::MakeLTRB(0, 0, 6.5, 7));
  EXPECT_EQ(t_gain[0][2], SkRect::MakeLTRB(0, 0, 6.5, 6));
  EXPECT_EQ(t_gain[1][0], SkRect::MakeEmpty());
  EXPECT_EQ(t_gain[1][1], SkRect::MakeEmpty());
  EXPECT_EQ(t_gain[1][2], SkRect::MakeEmpty());

  EXPECT_EQ(s_base[0][0], SkIRect::MakeLTRB(10, 10, 23, 24));
  EXPECT_EQ(s_base[0][1], SkIRect::MakeLTRB(10, 24, 23, 38));
  EXPECT_EQ(s_base[0][2], SkIRect::MakeLTRB(10, 38, 23, 50));
  EXPECT_EQ(s_base[1][0], SkIRect::MakeLTRB(23, 10, 30, 24));
  EXPECT_EQ(s_base[1][1], SkIRect::MakeLTRB(23, 24, 30, 38));
  EXPECT_EQ(s_base[1][2], SkIRect::MakeLTRB(23, 38, 30, 50));

  EXPECT_EQ(s_gain[0][0], SkIRect::MakeLTRB(5, 5, 10, 12));
  EXPECT_EQ(s_gain[0][1], SkIRect::MakeLTRB(5, 12, 10, 19));
  EXPECT_EQ(s_gain[0][2], SkIRect::MakeLTRB(5, 19, 10, 20));
  EXPECT_EQ(s_gain[1][0], SkIRect::MakeEmpty());
  EXPECT_EQ(s_gain[1][1], SkIRect::MakeEmpty());
  EXPECT_EQ(s_gain[1][2], SkIRect::MakeEmpty());
}

TEST(Geometry, Fractional) {
  SkBitmap bm0;
  SkBitmap bm1;

  // Test where the source rects are not pixel-aligned.
  bm0.allocPixels(SkImageInfo::MakeN32Premul(30, 50));
  SkRect dest_rect = SkRect::MakeXYWH(0, 0, 20, 40);
  std::vector<SkRect> source_rects = {
      SkRect::MakeXYWH(5.5, 5.5, 20, 40),
  };
  std::vector<sk_sp<SkImage>> source_images = {
      SkImages::RasterFromBitmap(bm0),
  };
  int32_t source_max_size = 20;

  Tiling t(dest_rect, source_rects, source_images, source_max_size);

  constexpr int kTilesX = 2;
  constexpr int kTilesY = 3;
  EXPECT_EQ(kTilesX, t.GetTileCountX());
  EXPECT_EQ(kTilesY, t.GetTileCountY());

  // Save the output from all tiles for comparison.
  SkRect out_dest[kTilesX][kTilesY];
  SkRect out_source[kTilesX][kTilesY];
  SkIRect out_subset[kTilesX][kTilesY];

  for (int x = 0; x < kTilesX; ++x) {
    for (int y = 0; y < kTilesY; ++y) {
      SkRect tile_dest_rect;
      std::vector<SkRect> tile_source_rects;
      std::vector<std::optional<SkIRect>> tile_subset_rects;
      t.GetTileRect(x, y, tile_dest_rect, tile_source_rects, tile_subset_rects);
      out_dest[x][y] = tile_dest_rect;
      out_source[x][y] = tile_source_rects[0];
      out_subset[x][y] = tile_subset_rects[0].value();
    }
  }

  // The subsets for the image will be overlapping (e.g, the
  // first entry ends at Y=25, then the next one starts at
  // Y=24).
  EXPECT_EQ(out_subset[0][0], SkIRect::MakeLTRB(5, 5, 24, 25));
  EXPECT_EQ(out_subset[0][1], SkIRect::MakeLTRB(5, 24, 24, 44));
  EXPECT_EQ(out_subset[0][2], SkIRect::MakeLTRB(5, 43, 24, 46));
  EXPECT_EQ(out_subset[1][0], SkIRect::MakeLTRB(23, 5, 26, 25));
  EXPECT_EQ(out_subset[1][1], SkIRect::MakeLTRB(23, 24, 26, 44));
  EXPECT_EQ(out_subset[1][2], SkIRect::MakeLTRB(23, 43, 26, 46));

  // All of the sources are aligned to a half-pixel.
  EXPECT_EQ(out_source[0][0], SkRect::MakeLTRB(0.5, 0.5, 18.5, 19.5));
  EXPECT_EQ(out_source[0][1], SkRect::MakeLTRB(0.5, 0.5, 18.5, 19.5));
  EXPECT_EQ(out_source[0][2], SkRect::MakeLTRB(0.5, 0.5, 18.5, 2.5));
  EXPECT_EQ(out_source[1][0], SkRect::MakeLTRB(0.5, 0.5, 2.5, 19.5));
  EXPECT_EQ(out_source[1][1], SkRect::MakeLTRB(0.5, 0.5, 2.5, 19.5));
  EXPECT_EQ(out_source[1][2], SkRect::MakeLTRB(0.5, 0.5, 2.5, 2.5));

  // The destination tiles cover `dest_rect`.
  EXPECT_EQ(out_dest[0][0], SkRect::MakeLTRB(0, 0, 18, 19));
  EXPECT_EQ(out_dest[0][1], SkRect::MakeLTRB(0, 19, 18, 38));
  EXPECT_EQ(out_dest[0][2], SkRect::MakeLTRB(0, 38, 18, 40));
  EXPECT_EQ(out_dest[1][0], SkRect::MakeLTRB(18, 0, 20, 19));
  EXPECT_EQ(out_dest[1][1], SkRect::MakeLTRB(18, 19, 20, 38));
  EXPECT_EQ(out_dest[1][2], SkRect::MakeLTRB(18, 38, 20, 40));
}

}  // namespace

}  // namespace skia
