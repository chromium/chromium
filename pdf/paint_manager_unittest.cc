// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::NiceMock;

constexpr char kRenderAndPaintTimeMetric[] = "PDF.RenderAndPaintTime";
constexpr char kRenderPaintAndFlushTimeMetric[] = "PDF.RenderPaintAndFlushTime";

base::FilePath GetPaintManagerTestDataFilePath(std::string_view filename) {
  return base::FilePath(FILE_PATH_LITERAL("paint_manager"))
      .AppendASCII(filename);
}

class MockPaintManagerClient : public PaintManager::Client {
 public:
  MOCK_METHOD(void, InvalidatePluginContainer, (), (override));
  MOCK_METHOD(SkBitmap*,
              InstallBuffer,
              (SkImageInfo image_info, base::span<uint8_t> data),
              (override));
  MOCK_METHOD(void,
              OnPaint,
              (const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending),
              (override));
  MOCK_METHOD(void, UpdateSnapshot, (sk_sp<SkImage> snapshot), (override));
  MOCK_METHOD(void, UpdateScale, (float scale), (override));
  MOCK_METHOD(void,
              UpdateLayerTransform,
              (float scale, const gfx::Vector2dF& translate),
              (override));
};

class PaintManagerTest : public testing::Test {
 protected:
  struct FakeReadyRect {
    gfx::Rect rect;
    sk_sp<SkImage> image;
  };

  void WaitForOnPaint() {
    base::RunLoop run_loop;
    EXPECT_CALL(client_, OnPaint).WillOnce([&run_loop] { run_loop.Quit(); });
    run_loop.Run();
  }

  void SetSizeAndInstall(const gfx::Size& new_size, float device_scale) {
    // This size check matches the one in PaintManager::SetSize() to replicate
    // its behavior.
    gfx::Size old_image_size =
        gfx::SkISizeToSize(client_bitmap_.info().dimensions());
    gfx::Size padded_new_size =
        PaintManager::GetNewContextSize(old_image_size, new_size);
    if (old_image_size != padded_new_size || client_bitmap_.empty()) {
      EXPECT_CALL(client_, InstallBuffer)
          .WillOnce([this](SkImageInfo image_info, base::span<uint8_t> data) {
            this->client_bitmap_.installPixels(image_info, data.data(),
                                               image_info.minRowBytes());
            return &client_bitmap_;
          });
    }
    paint_manager_.SetSize(new_size, device_scale, kPremul_SkAlphaType);
  }

  sk_sp<SkImage> WaitForFlush(
      const std::vector<gfx::Rect>& expected_paint_rects,
      std::vector<FakeReadyRect> fake_ready,
      std::vector<gfx::Rect> fake_pending) {
    EXPECT_CALL(client_, OnPaint(expected_paint_rects, _, _))
        .WillOnce([&fake_ready, &fake_pending, this](
                      const std::vector<gfx::Rect>& paint_rects,
                      std::vector<PaintReadyRect>& ready,
                      std::vector<gfx::Rect>& pending) {
          sk_sp<SkSurface> draw_surface =
              SkSurfaces::Raster(client_bitmap_.info());

          for (const auto& ready_rect : fake_ready) {
            auto skrect = gfx::RectToSkRect(ready_rect.rect);
            SkPaint paint;
            paint.setColor(SK_ColorWHITE);
            draw_surface->getCanvas()->drawRect(skrect, paint);
            draw_surface->getCanvas()->drawImageRect(
                ready_rect.image, skrect, skrect, SkSamplingOptions(), nullptr,
                SkCanvas::kStrict_SrcRectConstraint);
            ready.emplace_back(ready_rect.rect);
          }

          pending = std::move(fake_pending);

          SkPixmap px;
          draw_surface->peekPixels(&px);
          client_bitmap_.writePixels(px);
        });

    sk_sp<SkImage> saved_snapshot;
    base::RunLoop run_loop;
    EXPECT_CALL(client_, UpdateSnapshot)
        .WillOnce([&saved_snapshot, &run_loop](sk_sp<SkImage> snapshot) {
          saved_snapshot = std::move(snapshot);
          run_loop.QuitWhenIdle();
        });
    run_loop.Run();

    return saved_snapshot;
  }

  // Generates the expectations for use with TestPaintImage().
  //
  // - `plugin_size`: The expected image size.
  // - `paint_rect`: Expected to be painted white.
  // - `overlapped_rect`: Expected to be painted red.
  //                      May paint over `paint_rect`.
  SkBitmap GeneratePaintImageExpectation(const gfx::Size& plugin_size,
                                         const gfx::Rect& paint_rect,
                                         const gfx::Rect& overlapped_rect) {
    sk_sp<SkSurface> surface =
        CreateSkiaSurfaceForTesting(plugin_size, SK_ColorMAGENTA);
    SkCanvas* canvas = surface->getCanvas();
    canvas->clipIRect(gfx::RectToSkIRect(paint_rect));
    canvas->clear(SK_ColorWHITE);
    canvas->clipIRect(gfx::RectToSkIRect(overlapped_rect));
    canvas->clear(SK_ColorRED);

    SkBitmap bitmap;
    EXPECT_TRUE(surface->makeImageSnapshot()->asLegacyBitmap(&bitmap));
    return bitmap;
  }

  void TestPaintImage(const gfx::Size& plugin_size,
                      const gfx::Size& source_size,
                      const gfx::Rect& paint_rect,
                      const gfx::Rect& overlapped_rect) {
    // Paint `paint_rect` from `source_size` image over a magenta background.
    SetSizeAndInstall(plugin_size, 1.0f);
    sk_sp<SkImage> snapshot = WaitForFlush(
        /*expected_paint_rects=*/{{gfx::Rect(plugin_size)}},
        /*fake_ready=*/
        {
            {gfx::Rect(plugin_size),
             CreateSkiaImageForTesting(plugin_size, SK_ColorMAGENTA)},
            {paint_rect, CreateSkiaImageForTesting(source_size, SK_ColorRED)},
        },
        /*fake_pending=*/{});
    ASSERT_TRUE(snapshot);

    // Check if `snapshot` matches `expected_bitmap`.
    snapshot = snapshot->makeSubset(
        SkRecorder::TODO(),
        SkIRect::MakeWH(plugin_size.width(), plugin_size.height()), {});
    ASSERT_TRUE(snapshot);

    SkBitmap snapshot_bitmap;
    ASSERT_TRUE(snapshot->asLegacyBitmap(&snapshot_bitmap));

    SkBitmap expected_bitmap =
        GeneratePaintImageExpectation(plugin_size, paint_rect, overlapped_rect);
    EXPECT_TRUE(cc::MatchesBitmap(snapshot_bitmap, expected_bitmap,
                                  cc::ExactPixelComparator()));
  }

  void TestScroll(const gfx::Vector2d& scroll_amount,
                  const gfx::Rect& expected_paint_rect,
                  std::string_view expected_png) {
    // Paint non-uniform initial image.
    gfx::Size plugin_size = paint_manager_.GetEffectiveSize();
    ASSERT_GE(plugin_size.width(), 4);
    ASSERT_GE(plugin_size.height(), 4);

    sk_sp<SkSurface> initial_surface =
        CreateSkiaSurfaceForTesting(plugin_size, SK_ColorRED);
    initial_surface->getCanvas()->clipIRect(SkIRect::MakeLTRB(
        1, 1, plugin_size.width() - 1, plugin_size.height() - 2));
    initial_surface->getCanvas()->clear(SK_ColorGREEN);

    paint_manager_.Invalidate();
    ASSERT_TRUE(WaitForFlush(
        /*expected_paint_rects=*/{gfx::Rect(plugin_size)},
        /*fake_ready=*/
        {{gfx::Rect(plugin_size), initial_surface->makeImageSnapshot()}},
        /*fake_pending=*/{}));

    // Scroll by `scroll_amount`, painting `expected_paint_rect` magenta.
    paint_manager_.ScrollRect(gfx::Rect(plugin_size), scroll_amount);
    sk_sp<SkImage> snapshot = WaitForFlush(
        /*expected_paint_rects=*/{expected_paint_rect},
        /*fake_ready=*/
        {{expected_paint_rect,
          CreateSkiaImageForTesting(plugin_size, SK_ColorMAGENTA)}},
        /*fake_pending=*/{});
    ASSERT_TRUE(snapshot);

    // Compare snapshot to `expected_png`.
    snapshot = snapshot->makeSubset(
        SkRecorder::TODO(),
        SkIRect::MakeWH(plugin_size.width(), plugin_size.height()), {});
    ASSERT_TRUE(snapshot);
    EXPECT_TRUE(MatchesPngFile(*snapshot,
                               GetPaintManagerTestDataFilePath(expected_png)));
  }

  NiceMock<MockPaintManagerClient> client_;
  SkBitmap client_bitmap_;
  PaintManager paint_manager_{&client_};
};

TEST_F(PaintManagerTest, GetNewContextSizeWhenGrowingBelowMaximum) {
  EXPECT_EQ(gfx::Size(450, 350),
            PaintManager::GetNewContextSize({450, 350}, {450, 349}));
  EXPECT_EQ(gfx::Size(450, 350),
            PaintManager::GetNewContextSize({450, 350}, {449, 350}));
}

TEST_F(PaintManagerTest, GetNewContextSizeWhenGrowingAboveMaximum) {
  EXPECT_EQ(gfx::Size(501, 400),
            PaintManager::GetNewContextSize({450, 350}, {451, 350}));
  EXPECT_EQ(gfx::Size(500, 401),
            PaintManager::GetNewContextSize({450, 350}, {450, 351}));
}

TEST_F(PaintManagerTest, GetNewContextSizeWhenShrinkingAboveMinimum) {
  EXPECT_EQ(gfx::Size(450, 350),
            PaintManager::GetNewContextSize({450, 350}, {350, 251}));
  EXPECT_EQ(gfx::Size(450, 350),
            PaintManager::GetNewContextSize({450, 350}, {351, 250}));
}

TEST_F(PaintManagerTest, GetNewContextSizeWhenShrinkingBelowMinimum) {
  EXPECT_EQ(gfx::Size(399, 300),
            PaintManager::GetNewContextSize({450, 350}, {349, 250}));
  EXPECT_EQ(gfx::Size(400, 299),
            PaintManager::GetNewContextSize({450, 350}, {350, 249}));
}

TEST_F(PaintManagerTest, Create) {
  EXPECT_EQ(gfx::Size(0, 0), paint_manager_.GetEffectiveSize());
  EXPECT_EQ(1.0f, paint_manager_.GetEffectiveDeviceScale());
}

TEST_F(PaintManagerTest, SetSizeWithoutPaint) {
  EXPECT_CALL(client_, InvalidatePluginContainer).Times(0);
  SetSizeAndInstall({400, 300}, 2.0f);

  EXPECT_EQ(gfx::Size(400, 300), paint_manager_.GetEffectiveSize());
  EXPECT_EQ(2.0f, paint_manager_.GetEffectiveDeviceScale());
}

TEST_F(PaintManagerTest, SetSizeWithPaint) {
  EXPECT_CALL(client_, UpdateScale(0.5));
  EXPECT_CALL(client_, InvalidatePluginContainer);
  SetSizeAndInstall({400, 300}, 2.0f);

  WaitForOnPaint();
}

TEST_F(PaintManagerTest, SetTransformWithSurface) {
  SetSizeAndInstall({400, 300}, 2.0f);
  WaitForOnPaint();

  EXPECT_CALL(client_,
              UpdateLayerTransform(0.25f, gfx::Vector2dF(116.5f, 29.5f)));
  paint_manager_.SetTransform(0.25f, {150, 50}, {-4, 8});
  paint_manager_.Invalidate();
  WaitForOnPaint();
}

TEST_F(PaintManagerTest, ClearTransform) {
  SetSizeAndInstall({400, 300}, 2.0f);
  WaitForOnPaint();

  EXPECT_CALL(client_, UpdateLayerTransform(1.0f, gfx::Vector2dF()));
  paint_manager_.ClearTransform();
}

TEST_F(PaintManagerTest, DoPaintFirst) {
  base::HistogramTester histograms;

  SetSizeAndInstall({400, 300}, 2.0f);

  // Clear the background, to replicate a realistic scenario.
  // (`PdfViewWebClient`'s `first_paint` logic clears the background before
  // every first draw).
  sk_sp<SkImage> snapshot = WaitForFlush(
      /*expected_paint_rects=*/{{0, 0, 400, 300}},
      /*fake_ready=*/
      {{{0, 0, 400, 300}, CreateSkiaImageForTesting({400, 300}, SK_ColorBLACK)},
       {{25, 50, 200, 100},
        CreateSkiaImageForTesting({200, 100}, SK_ColorGRAY)}},
      /*fake_pending=*/{});
  ASSERT_TRUE(snapshot);
  sk_sp<SkImage> subset =
      snapshot->makeSubset(SkRecorder::TODO(), {0, 0, 400, 300}, {});
  ASSERT_TRUE(subset);
  EXPECT_TRUE(MatchesPngFile(
      *subset, GetPaintManagerTestDataFilePath("do_paint_first.png")));

  histograms.ExpectTotalCount(kRenderAndPaintTimeMetric, 1);
  histograms.ExpectTotalCount(kRenderPaintAndFlushTimeMetric, 1);
}

TEST_F(PaintManagerTest, PaintImage) {
  base::HistogramTester histograms;

  // Painted area is within the plugin area and the source image.
  TestPaintImage(/*plugin_size=*/{20, 20}, /*source_size=*/{15, 15},
                 /*paint_rect=*/{0, 0, 10, 10},
                 /*overlapped_rect=*/{0, 0, 10, 10});

  // Painted area straddles the plugin area and the source image.
  TestPaintImage(/*plugin_size=*/{50, 30}, /*source_size=*/{30, 50},
                 /*paint_rect=*/{10, 10, 30, 30},
                 /*overlapped_rect=*/{10, 10, 20, 20});

  // Painted area is outside the plugin area.
  TestPaintImage(/*plugin_size=*/{10, 10}, /*source_size=*/{30, 30},
                 /*paint_rect=*/{10, 10, 10, 10},
                 /*overlapped_rect=*/{0, 0, 0, 0});

  // Painted area is outside the source image.
  TestPaintImage(/*plugin_size=*/{15, 15}, /*source_size=*/{5, 5},
                 /*paint_rect=*/{10, 10, 5, 5},
                 /*overlapped_rect=*/{0, 0, 0, 0});

  histograms.ExpectTotalCount(kRenderAndPaintTimeMetric, 4);
  histograms.ExpectTotalCount(kRenderPaintAndFlushTimeMetric, 4);
}

TEST_F(PaintManagerTest, Scroll) {
  base::HistogramTester histograms;

  SetSizeAndInstall({4, 5}, 1.0f);

  TestScroll(/*scroll_amount=*/{1, 0}, /*expected_paint_rect=*/{0, 0, 1, 5},
             "scroll_right.png");
  TestScroll(/*scroll_amount=*/{-2, 0}, /*expected_paint_rect=*/{2, 0, 2, 5},
             "scroll_left.png");
  TestScroll(/*scroll_amount=*/{0, 3}, /*expected_paint_rect=*/{0, 0, 4, 3},
             "scroll_down.png");
  TestScroll(/*scroll_amount=*/{0, -3}, /*expected_paint_rect=*/{0, 2, 4, 3},
             "scroll_up.png");

  histograms.ExpectTotalCount(kRenderAndPaintTimeMetric, 8);
  histograms.ExpectTotalCount(kRenderPaintAndFlushTimeMetric, 8);
}

TEST_F(PaintManagerTest, ScrollIgnored) {
  base::HistogramTester histograms;

  SetSizeAndInstall({4, 5}, 1.0f);

  // Scroll to the edge of the plugin area.
  TestScroll(/*scroll_amount=*/{4, 0}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");
  TestScroll(/*scroll_amount=*/{-4, 0}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");
  TestScroll(/*scroll_amount=*/{0, 5}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");
  TestScroll(/*scroll_amount=*/{0, -5}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");

  // Scroll outside of the plugin area.
  TestScroll(/*scroll_amount=*/{5, 0}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");
  TestScroll(/*scroll_amount=*/{-7, 0}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");
  TestScroll(/*scroll_amount=*/{0, 8}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");
  TestScroll(/*scroll_amount=*/{0, -9}, /*expected_paint_rect=*/{0, 0, 4, 5},
             "scroll_ignored.png");

  histograms.ExpectTotalCount(kRenderAndPaintTimeMetric, 16);
  histograms.ExpectTotalCount(kRenderPaintAndFlushTimeMetric, 16);
}

TEST_F(PaintManagerTest, ResizeRecyclesDrawBuffer) {
  // 1. Set size to 100x100 and trigger a paint that returns early (no flush).
  // If `draw_buffer_` isn't cleared on SetSize(), this leaves draw_buffer_
  // allocated but not released.
  SetSizeAndInstall(gfx::Size(100, 100), 1.0f);
  paint_manager_.Invalidate();
  WaitForOnPaint();

  // 2. Resize to 200x200.
  SetSizeAndInstall(gfx::Size(200, 200), 1.0f);
  paint_manager_.Invalidate();
  SkBitmap larger_bitmap;
  larger_bitmap.allocN32Pixels(200, 200);
  larger_bitmap.eraseColor(SK_ColorBLUE);

  // 3. Paint at 200x200, if `draw_buffer_` is still 150x150, the copy will clip
  // at 150x150 and fail the pixel comparison, however, since it is cleared,
  // this works.
  auto snapshot =
      WaitForFlush({gfx::Rect(0, 0, 200, 200)},
                   {{gfx::Rect(0, 0, 200, 200), larger_bitmap.asImage()}}, {});
  ASSERT_TRUE(snapshot);

  snapshot = snapshot->makeSubset(nullptr, SkIRect::MakeWH(200, 200), {});
  ASSERT_TRUE(snapshot);

  SkBitmap snapshot_bitmap;
  ASSERT_TRUE(snapshot->asLegacyBitmap(&snapshot_bitmap));

  EXPECT_TRUE(cc::MatchesBitmap(snapshot_bitmap, larger_bitmap,
                                cc::ExactPixelComparator()));
}

}  // namespace

}  // namespace chrome_pdf
