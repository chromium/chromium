// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/lame_capture_overlay_chromeos.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace content {
namespace {

constexpr gfx::Size kFrameSize = gfx::Size(320, 200);
constexpr gfx::Rect kContentRegion = gfx::Rect(kFrameSize);
constexpr gfx::RectF kSpanOfEntireFrame = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);

class LameCaptureOverlayChromeOSTest : public testing::Test {
 public:
  void RunUntilIdle() { env_.RunUntilIdle(); }

  // Returns a 32x32 bitmap filled with red.
  static SkBitmap CreateTestBitmap() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(32, 32);
    CHECK(!bitmap.isNull());
    bitmap.eraseColor(SK_ColorRED);
    return bitmap;
  }

  // Returns true if there are any non-zero pixels in the region |rect| within
  // |frame|.
  static bool NonZeroPixelsInRegion(const media::VideoFrame* frame,
                                    const gfx::Rect& rect) {
    bool y_found = false, u_found = false, v_found = false;
    for (int y = rect.y(); y < rect.bottom(); ++y) {
      auto* const yplane = frame->visible_data(media::VideoFrame::kYPlane) +
                           y * frame->stride(media::VideoFrame::kYPlane);
      auto* const uplane = frame->visible_data(media::VideoFrame::kUPlane) +
                           (y / 2) * frame->stride(media::VideoFrame::kUPlane);
      auto* const vplane = frame->visible_data(media::VideoFrame::kVPlane) +
                           (y / 2) * frame->stride(media::VideoFrame::kVPlane);
      for (int x = rect.x(); x < rect.right(); ++x) {
        if (yplane[x] != 0)
          y_found = true;
        if (uplane[x / 2])
          u_found = true;
        if (vplane[x / 2])
          v_found = true;
      }
    }
    return (y_found && u_found && v_found);
  }

 private:
  base::test::TaskEnvironment env_;
};

TEST_F(LameCaptureOverlayChromeOSTest, UnsetImageNotRenderedOnFrame) {
  mojo::Remote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  LameCaptureOverlayChromeOS overlay(
      nullptr, overlay_remote.BindNewPipeAndPassReceiver());

  // Bounds set, but no image. → Should not render anything.
  overlay.SetBounds(kSpanOfEntireFrame);
  EXPECT_FALSE(overlay.MakeRenderer(kContentRegion));

  // Both image and bounds set. → Should render something.
  overlay.SetImageAndBounds(CreateTestBitmap(), kSpanOfEntireFrame);
  EXPECT_TRUE(overlay.MakeRenderer(kContentRegion));
}

TEST_F(LameCaptureOverlayChromeOSTest, HiddenImageNotRenderedOnFrame) {
  mojo::Remote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  LameCaptureOverlayChromeOS overlay(
      nullptr, overlay_remote.BindNewPipeAndPassReceiver());

  // Both image and bounds set. → Should render something.
  overlay.SetImageAndBounds(CreateTestBitmap(), kSpanOfEntireFrame);
  EXPECT_TRUE(overlay.MakeRenderer(kContentRegion));

  // Bounds set to empty. → Should render nothing.
  overlay.SetBounds(gfx::RectF());
  EXPECT_FALSE(overlay.MakeRenderer(kContentRegion));
}

TEST_F(LameCaptureOverlayChromeOSTest, OutOfBoundsOverlayNotRenderedOnFrame) {
  mojo::Remote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  LameCaptureOverlayChromeOS overlay(
      nullptr, overlay_remote.BindNewPipeAndPassReceiver());

  // Both image and bounds set. → Should render something.
  overlay.SetImageAndBounds(CreateTestBitmap(), kSpanOfEntireFrame);
  EXPECT_TRUE(overlay.MakeRenderer(kContentRegion));

  // Move overlay to above and left of content area. → Should render nothing.
  overlay.SetBounds(gfx::RectF(-0.5f, -0.5f, 0.25f, 0.25f));
  EXPECT_FALSE(overlay.MakeRenderer(kContentRegion));
}

TEST_F(LameCaptureOverlayChromeOSTest, ImageRenderedOnFrame) {
  mojo::Remote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  LameCaptureOverlayChromeOS overlay(
      nullptr, overlay_remote.BindNewPipeAndPassReceiver());

  // Create blank black frame. No non-zero pixels should be present.
  const auto frame = media::VideoFrame::CreateZeroInitializedFrame(
      media::PIXEL_FORMAT_I420, kFrameSize, gfx::Rect(kFrameSize), kFrameSize,
      base::TimeDelta());
  ASSERT_FALSE(NonZeroPixelsInRegion(frame.get(), frame->visible_rect()));

  // Set image and bounds to a location in the lower-right quadrant of the
  // frame.
  const gfx::RectF bounds(0.8f, 0.8f, 0.1f, 0.1f);
  overlay.SetImageAndBounds(CreateTestBitmap(), bounds);

  // Render the overlay in the VideoFrame.
  auto renderer = overlay.MakeRenderer(kContentRegion);
  ASSERT_TRUE(renderer);
  std::move(renderer).Run(frame.get());

  // Check that there are non-zero pixels present in the modified region.
  const gfx::Rect mutated_region = gfx::ToEnclosingRect(
      gfx::ScaleRect(bounds, kContentRegion.width(), kContentRegion.height()));
  EXPECT_TRUE(NonZeroPixelsInRegion(frame.get(), mutated_region));

  // Check that there are no non-zero pixels present in the rest of the frame.
  const struct BlankRegion {
    const char* const description;
    gfx::Rect rect;
  } regions_around_mutation[] = {
      {"above", gfx::Rect(0, 0, kContentRegion.width(), mutated_region.y())},
      {"left", gfx::Rect(0, mutated_region.y(), mutated_region.x(),
                         mutated_region.height())},
      {"right", gfx::Rect(mutated_region.right(), mutated_region.y(),
                          kContentRegion.width() - mutated_region.right(),
                          mutated_region.height())},
      {"below", gfx::Rect(0, mutated_region.bottom(), kContentRegion.width(),
                          kContentRegion.height() - mutated_region.bottom())},
  };
  for (const BlankRegion& region : regions_around_mutation) {
    SCOPED_TRACE(testing::Message() << region.description);
    EXPECT_FALSE(NonZeroPixelsInRegion(frame.get(), region.rect));
  }
}

TEST_F(LameCaptureOverlayChromeOSTest, ReportsLostMojoConnection) {
  class MockOwner : public LameCaptureOverlayChromeOS::Owner {
   public:
    ~MockOwner() final = default;
    MOCK_METHOD1(OnOverlayConnectionLost,
                 void(LameCaptureOverlayChromeOS* overlay));
  } mock_owner;

  mojo::Remote<viz::mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  LameCaptureOverlayChromeOS overlay(
      &mock_owner, overlay_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(overlay_remote);
  RunUntilIdle();  // Propagate mojo tasks.

  EXPECT_CALL(mock_owner, OnOverlayConnectionLost(&overlay));
  overlay_remote.reset();
  RunUntilIdle();  // Propagate mojo tasks.
}

}  // namespace
}  // namespace content
