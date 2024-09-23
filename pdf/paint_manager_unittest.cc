// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
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

base::FilePath GetTestDataFilePath(std::string_view filename) {
  return base::FilePath(FILE_PATH_LITERAL("paint_manager"))
      .AppendASCII(filename);
}

class FakeClient : public PaintManager::Client {
 public:
  MOCK_METHOD(void, InvalidatePluginContainer, (), (override));
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
  void WaitForOnPaint() {
    base::RunLoop run_loop;
    EXPECT_CALL(client_, OnPaint).WillOnce([&run_loop] { run_loop.Quit(); });
    run_loop.Run();
  }

  sk_sp<SkImage> WaitForFlush(
      const std::vector<gfx::Rect>& expected_paint_rects,
      std::vector<PaintReadyRect> fake_ready,
      std::vector<gfx::Rect> fake_pending) {
    EXPECT_CALL(client_, OnPaint(expected_paint_rects, _, _))
        .WillOnce([&fake_ready, &fake_pending](
                      const std::vector<gfx::Rect>& paint_rects,
                      std::vector<PaintReadyRect>& ready,
                      std::vector<gfx::Rect>& pending) {
          ready = std::move(fake_ready);
          pending = std::move(fake_pending);
        });

    sk_sp<SkImage> saved_snapshot;
    base::RunLoop run_loop;
    EXPECT_CALL(client_, UpdateSnapshot)
        .WillOnce([&saved_snapshot, &run_loop](sk_sp<SkImage> snapshot) {
          saved_snapshot = std::move(snapshot);
          run_loop.Quit();
        });
    run_loop.Run();

    return saved_snapshot;
  }

  void TestPaintImage(const gfx::Size& plugin_size,
                      const gfx::Size& source_size,
                      const gfx::Rect& paint_rect,
                      const gfx::Rect& overlapped_rect) {
    // Paint `paint_rect` from `source_size` image over a magenta background.
    paint_manager_.SetSize(plugin_size, 1.0f);
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

    // Check if snapshot has `overlapped_rect` painted red.
    snapshot = snapshot->makeSubset(
        static_cast<GrDirectContext*>(nullptr),
        SkIRect::MakeWH(plugin_size.width(), plugin_size.height()));
    ASSERT_TRUE(snapshot);

    SkBitmap snapshot_bitmap;
    ASSERT_TRUE(snapshot->asLegacyBitmap(&snapshot_bitmap));

    sk_sp<SkSurface> expected_surface =
        CreateSkiaSurfaceForTesting(plugin_size, SK_ColorMAGENTA);
    expected_surface->getCanvas()->clipIRect(
        gfx::RectToSkIRect(overlapped_rect));
    expected_surface->getCanvas()->clear(SK_ColorRED);

    SkBitmap expected_bitmap;
    ASSERT_TRUE(expected_surface->makeImageSnapshot()->asLegacyBitmap(
        &expected_bitmap));

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
        static_cast<GrDirectContext*>(nullptr),
        SkIRect::MakeWH(plugin_size.width(), plugin_size.height()));
    ASSERT_TRUE(snapshot);

    EXPECT_TRUE(
        MatchesPngFile(snapshot.get(), GetTestDataFilePath(expected_png)));
  }

  NiceMock<FakeClient> client_;
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
  paint_manager_.SetSize({400, 300}, 2.0f);

  EXPECT_EQ(gfx::Size(400, 300), paint_manager_.GetEffectiveSize());
  EXPECT_EQ(2.0f, paint_manager_.GetEffectiveDeviceScale());
}

TEST_F(PaintManagerTest, SetSizeWithPaint) {
  paint_manager_.SetSize({400, 300}, 2.0f);

  EXPECT_CALL(client_, InvalidatePluginContainer);
  EXPECT_CALL(client_, UpdateScale(0.5f));
  WaitForOnPaint();
}

TEST_F(PaintManagerTest, SetTransformWithoutSurface) {
  EXPECT_CALL(client_, UpdateLayerTransform).Times(0);
  paint_manager_.SetTransform(0.25f, {150, 50}, {-4, 8},
                              /*schedule_flush=*/true);
}

TEST_F(PaintManagerTest, SetTransformWithSurface) {
  paint_manager_.SetSize({400, 300}, 2.0f);
  WaitForOnPaint();

  EXPECT_CALL(client_,
              UpdateLayerTransform(0.25f, gfx::Vector2dF(116.5f, 29.5f)));
  paint_manager_.SetTransform(0.25f, {150, 50}, {-4, 8},
                              /*schedule_flush=*/true);
  WaitForOnPaint();
}

TEST_F(PaintManagerTest, ClearTransform) {
  paint_manager_.SetSize({400, 300}, 2.0f);
  WaitForOnPaint();

  EXPECT_CALL(client_, UpdateLayerTransform(1.0f, gfx::Vector2dF()));
  paint_manager_.ClearTransform();
}

TEST_F(PaintManagerTest, DoPaintFirst) {
  paint_manager_.SetSize({400, 300}, 2.0f);

  sk_sp<SkImage> snapshot =
      WaitForFlush(/*expected_paint_rects=*/{{0, 0, 400, 300}},
                   /*fake_ready=*/
                   {{{25, 50, 200, 100},
                     CreateSkiaImageForTesting({200, 100}, SK_ColorGRAY)}},
                   /*fake_pending=*/{});

  EXPECT_TRUE(MatchesPngFile(snapshot.get(),
                             GetTestDataFilePath("do_paint_first.png")));
}

TEST_F(PaintManagerTest, PaintImage) {
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
}

TEST_F(PaintManagerTest, Scroll) {
  paint_manager_.SetSize({4, 5}, 1.0f);

  TestScroll(/*scroll_amount=*/{1, 0}, /*expected_paint_rect=*/{0, 0, 1, 5},
             "scroll_right.png");
  TestScroll(/*scroll_amount=*/{-2, 0}, /*expected_paint_rect=*/{2, 0, 2, 5},
             "scroll_left.png");
  TestScroll(/*scroll_amount=*/{0, 3}, /*expected_paint_rect=*/{0, 0, 4, 3},
             "scroll_down.png");
  TestScroll(/*scroll_amount=*/{0, -3}, /*expected_paint_rect=*/{0, 2, 4, 3},
             "scroll_up.png");
}

TEST_F(PaintManagerTest, ScrollIgnored) {
  paint_manager_.SetSize({4, 5}, 1.0f);

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
}

}  // namespace

}  // namespace chrome_pdf
