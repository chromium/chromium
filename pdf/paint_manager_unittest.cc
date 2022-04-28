// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::NiceMock;

base::FilePath GetTestDataFilePath(base::StringPiece filename) {
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

}  // namespace

}  // namespace chrome_pdf
