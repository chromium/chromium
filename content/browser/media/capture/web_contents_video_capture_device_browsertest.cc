// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <tuple>

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/features.h"
#include "content/browser/media/capture/content_capture_device_browsertest_base.h"
#include "content/browser/media/capture/fake_video_capture_stack.h"
#include "content/browser/media/capture/frame_test_util.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {
namespace {

class WebContentsVideoCaptureDeviceBrowserTest
    : public ContentCaptureDeviceBrowserTestBase,
      public FrameTestUtil {
 public:
  WebContentsVideoCaptureDeviceBrowserTest() = default;
  ~WebContentsVideoCaptureDeviceBrowserTest() override = default;

  // Runs the browser until a frame whose content matches the given |color| is
  // found in the captured frames queue, or until a testing failure has
  // occurred.
  void WaitForFrameWithColor(SkColor color) {
    VLOG(1) << "Waiting for frame content area filled with color: red="
            << SkColorGetR(color) << ", green=" << SkColorGetG(color)
            << ", blue=" << SkColorGetB(color);

    while (!testing::Test::HasFailure()) {
      EXPECT_TRUE(capture_stack()->started());
      EXPECT_FALSE(capture_stack()->error_occurred());
      capture_stack()->ExpectNoLogMessages();

      while (capture_stack()->has_captured_frames() &&
             !testing::Test::HasFailure()) {
        // Pop the next frame from the front of the queue and convert to a RGB
        // bitmap for analysis.
        const SkBitmap rgb_frame = capture_stack()->NextCapturedFrame();
        EXPECT_FALSE(rgb_frame.empty());

        // Three regions of the frame will be analyzed: 1) the upper-left
        // quadrant of the content region where the iframe draws; 2) the
        // remaining three quadrants of the content region where the main frame
        // draws; and 3) the non-content (i.e., letterboxed) region.
        const gfx::Size frame_size(rgb_frame.width(), rgb_frame.height());
        const gfx::Size source_size = GetExpectedSourceSize();
        const gfx::Rect iframe_rect(0, 0, source_size.width() / 2,
                                    source_size.height() / 2);

        // Compute the Rects representing where the three regions would be in
        // the |rgb_frame|.
        const gfx::RectF content_in_frame_rect_f(
            media::ComputeLetterboxRegion(gfx::Rect(frame_size), source_size));
        const gfx::RectF iframe_in_frame_rect_f = TransformSimilarly(
            gfx::Rect(source_size), content_in_frame_rect_f, iframe_rect);

        // viz::SoftwareRenderer does not do color space management. Otherwise
        // (normal case), be strict about color differences.
        // TODO(crbug/795132): SkiaRenderer temporarily uses same code as
        // software compositor. Fix plumbing for SkiaRenderer.
        const int max_color_diff =
            (IsSoftwareCompositingTest() || features::IsUsingSkiaRenderer())
                ? kVeryLooseMaxColorDifference
                : kMaxColorDifference;

        // Determine the average RGB color in the three regions-of-interest in
        // the frame.
        const auto average_iframe_rgb = ComputeAverageColor(
            rgb_frame, ToSafeIncludeRect(iframe_in_frame_rect_f), gfx::Rect());
        const auto average_mainframe_rgb = ComputeAverageColor(
            rgb_frame, ToSafeIncludeRect(content_in_frame_rect_f),
            ToSafeExcludeRect(iframe_in_frame_rect_f));
        const auto average_letterbox_rgb =
            ComputeAverageColor(rgb_frame, gfx::Rect(frame_size),
                                ToSafeExcludeRect(content_in_frame_rect_f));

        VLOG(1)
            << "Video frame analysis: size=" << frame_size.ToString()
            << ", captured upper-left quadrant of content should be bound by "
               "approx. "
            << ToSafeIncludeRect(iframe_in_frame_rect_f).ToString()
            << " and has average color " << average_iframe_rgb
            << ", captured remaining quadrants of content should be bound by "
               "approx. "
            << ToSafeIncludeRect(content_in_frame_rect_f).ToString()
            << " and has average color " << average_mainframe_rgb
            << ", letterbox region has average color " << average_letterbox_rgb;

        // The letterboxed region should always be black.
        if (IsFixedAspectRatioTest()) {
          EXPECT_TRUE(IsApproximatelySameColor(
              SK_ColorBLACK, average_letterbox_rgb, max_color_diff));
        }

        if (testing::Test::HasFailure()) {
          ADD_FAILURE() << "Test failure occurred at this frame; PNG dump: "
                        << cc::GetPNGDataUrl(rgb_frame);
          return;
        }

        // Return if the content region(s) now has/have the expected color(s).
        if (IsCrossSiteCaptureTest() &&
            IsApproximatelySameColor(color, average_iframe_rgb,
                                     max_color_diff) &&
            IsApproximatelySameColor(SK_ColorWHITE, average_mainframe_rgb,
                                     max_color_diff)) {
          VLOG(1) << "Observed desired frame.";
          return;
        } else if (!IsCrossSiteCaptureTest() &&
                   IsApproximatelySameColor(color, average_iframe_rgb,
                                            max_color_diff) &&
                   IsApproximatelySameColor(color, average_mainframe_rgb,
                                            max_color_diff)) {
          VLOG(1) << "Observed desired frame.";
          return;
        } else {
          VLOG(3) << "PNG dump of undesired frame: "
                  << cc::GetPNGDataUrl(rgb_frame);
        }
      }

      // Wait for at least the minimum capture period before checking for more
      // captured frames.
      base::RunLoop run_loop;
      GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), GetMinCapturePeriod());
      run_loop.Run();
    }
  }

  // Used by certain tests to determine whether the capturer has been
  // re-targetted.
  viz::FrameSinkId GetCurrentFrameSinkId() {
    auto* const view = static_cast<RenderWidgetHostViewBase*>(
        shell()->web_contents()->GetRenderWidgetHostView());
    return view ? view->GetFrameSinkId() : viz::FrameSinkId();
  }

 protected:
  // Don't call this. Call <BaseClass>::GetExpectedSourceSize() instead.
  gfx::Size GetCapturedSourceSize() const final {
    return shell()
        ->web_contents()
        ->GetMainFrame()
        ->GetView()
        ->GetViewBounds()
        .size();
  }

  std::unique_ptr<FrameSinkVideoCaptureDevice> CreateDevice() final {
    auto* const main_frame = shell()->web_contents()->GetMainFrame();
    return std::make_unique<WebContentsVideoCaptureDevice>(
        main_frame->GetProcess()->GetID(), main_frame->GetRoutingID());
  }

  void WaitForFirstFrame() final { WaitForFrameWithColor(SK_ColorBLACK); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDeviceBrowserTest);
};

// Tests that the device refuses to start if the WebContents target was
// destroyed before the device could start.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       ErrorsOutIfWebContentsHasGoneBeforeDeviceStart) {
  NavigateToInitialDocument();

  auto* const main_frame = shell()->web_contents()->GetMainFrame();
  const auto render_process_id = main_frame->GetProcess()->GetID();
  const auto render_frame_id = main_frame->GetRoutingID();
  const auto capture_params = SnapshotCaptureParams();

  // Delete the WebContents instance and the Shell. This makes the
  // render_frame_id invalid.
  shell()->web_contents()->Close();
  ASSERT_FALSE(RenderFrameHost::FromID(render_process_id, render_frame_id));

  // Create the device.
  auto device = std::make_unique<WebContentsVideoCaptureDevice>(
      render_process_id, render_frame_id);
  // Running the pending UI tasks should cause the device to realize the
  // WebContents is gone.
  RunUntilIdle();

  // Attempt to start the device, and expect the video capture stack to have
  // been notified of the error.
  device->AllocateAndStartWithReceiver(capture_params,
                                       capture_stack()->CreateFrameReceiver());
  EXPECT_FALSE(capture_stack()->started());
  EXPECT_TRUE(capture_stack()->error_occurred());
  capture_stack()->ExpectHasLogMessages();

  device->StopAndDeAllocate();
  device.reset();
  RunUntilIdle();
}

// Tests that the device starts, captures a frame, and then gracefully
// errors-out because the WebContents is destroyed before the device is stopped.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       ErrorsOutWhenWebContentsIsDestroyed) {
  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Initially, the device captures any content changes normally.
  ChangePageContentColor(SK_ColorRED);
  WaitForFrameWithColor(SK_ColorRED);

  // Delete the WebContents instance and the Shell, and allow the the "target
  // permanently lost" error to propagate to the video capture stack.
  shell()->web_contents()->Close();
  RunUntilIdle();
  EXPECT_TRUE(capture_stack()->error_occurred());
  capture_stack()->ExpectHasLogMessages();

  StopAndDeAllocate();
}

// Tests that capture is re-targetted when the render view of a WebContents
// changes.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       ChangesTargettedRenderView) {
  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Make a content change in the first page and wait for capture to reflect
  // that.
  ChangePageContentColor(SK_ColorRED);
  WaitForFrameWithColor(SK_ColorRED);

  // Navigate to an alternate site, checking that the FrameSinkIds before/after
  // the navigation are different.
  const viz::FrameSinkId frame_sink_id_before = GetCurrentFrameSinkId();
  EXPECT_TRUE(frame_sink_id_before.is_valid());
  NavigateToAlternateSite();
  const viz::FrameSinkId frame_sink_id_after = GetCurrentFrameSinkId();
  EXPECT_TRUE(frame_sink_id_after.is_valid());
  EXPECT_NE(frame_sink_id_before, frame_sink_id_after);

  // Make a content change in the second page and wait for capture to reflect
  // that. This proves that the capturer was successfully re-targetted to the
  // second page.
  ChangePageContentColor(SK_ColorGREEN);
  WaitForFrameWithColor(SK_ColorGREEN);
}

// Tests that capture is re-targetted when a renderer crash is followed by a
// reload. Regression test for http://crbug.com/916332.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       RecoversAfterRendererCrash) {
  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Make a content change in the first page and wait for capture to reflect
  // that.
  ChangePageContentColor(SK_ColorRED);
  WaitForFrameWithColor(SK_ColorRED);

  // Crash the renderer.
  EXPECT_TRUE(GetCurrentFrameSinkId().is_valid());
  CrashTheRenderer();
  EXPECT_FALSE(GetCurrentFrameSinkId().is_valid());

  // Now, reload the page.
  ReloadAfterCrash();
  EXPECT_TRUE(GetCurrentFrameSinkId().is_valid());

  // Make a content change in the reloaded page and wait for capture to reflect
  // that. This proves that the capturer successfully re-targetted to the
  // reloaded page.
  ChangePageContentColor(SK_ColorGREEN);
  WaitForFrameWithColor(SK_ColorGREEN);
}

// Tests that the device stops delivering frames while suspended. When resumed,
// any content changes that occurred during the suspend should cause a new frame
// to be delivered, to ensure the client is up-to-date.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       SuspendsAndResumes) {
  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Initially, the device captures any content changes normally.
  ChangePageContentColor(SK_ColorRED);
  WaitForFrameWithColor(SK_ColorRED);

  // Suspend the device.
  device()->MaybeSuspend();
  RunUntilIdle();
  ClearCapturedFramesQueue();

  // Change the page content and run the browser for five seconds. Expect no
  // frames were queued because the device should be suspended.
  ChangePageContentColor(SK_ColorGREEN);
  base::RunLoop run_loop;
  GetUIThreadTaskRunner({})->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                             base::TimeDelta::FromSeconds(5));
  run_loop.Run();
  EXPECT_FALSE(HasCapturedFramesInQueue());

  // Resume the device and wait for an automatic refresh frame containing the
  // content that was updated while the device was suspended.
  device()->Resume();
  WaitForFrameWithColor(SK_ColorGREEN);

  StopAndDeAllocate();
  EXPECT_FALSE(shell()->web_contents()->IsBeingCaptured());
}

// Tests that the device delivers refresh frames when asked, while the source
// content is not changing.
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       DeliversRefreshFramesUponRequest) {
  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Set the page content to a known color.
  ChangePageContentColor(SK_ColorRED);
  WaitForFrameWithColor(SK_ColorRED);

  // Without making any further changes to the source (which would trigger
  // frames to be captured), request and wait for ten refresh frames.
  for (int i = 0; i < 10; ++i) {
    ClearCapturedFramesQueue();
    device()->RequestRefreshFrame();
    WaitForFrameWithColor(SK_ColorRED);
  }

  StopAndDeAllocate();
  EXPECT_FALSE(shell()->web_contents()->IsBeingCaptured());
}

class WebContentsVideoCaptureDeviceBrowserTestP
    : public WebContentsVideoCaptureDeviceBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  bool IsSoftwareCompositingTest() const override {
    return std::get<0>(GetParam());
  }
  bool IsFixedAspectRatioTest() const override {
    return std::get<1>(GetParam());
  }
  bool IsCrossSiteCaptureTest() const override {
    return std::get<2>(GetParam());
  }
};

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(
    All,
    WebContentsVideoCaptureDeviceBrowserTestP,
    testing::Combine(
        // Note: On ChromeOS, software compositing is not an option.
        testing::Values(false /* GPU-accelerated compositing */),
        testing::Values(false /* variable aspect ratio */,
                        true /* fixed aspect ratio */),
        testing::Values(false /* page has only a main frame */,
                        true /* page contains a cross-site iframe */)));
#else
INSTANTIATE_TEST_SUITE_P(
    All,
    WebContentsVideoCaptureDeviceBrowserTestP,
    testing::Combine(
        testing::Values(false /* GPU-accelerated compositing */,
                        true /* software compositing */),
        testing::Values(false /* variable aspect ratio */,
                        true /* fixed aspect ratio */),
        testing::Values(false /* page has only a main frame */,
                        true /* page contains a cross-site iframe */)));
#endif  // defined(OS_CHROMEOS)

// Tests that the device successfully captures a series of content changes,
// whether the browser is running with software compositing or GPU-accelerated
// compositing, whether the WebContents is visible/hidden or occluded/unoccluded
// and whether the main document contains a cross-site iframe.

// Fails on LACROS and linux. http://crbug.com/1108205
#if BUILDFLAG(IS_LACROS) || defined(OS_LINUX)
#define MAYBE_CapturesContentChanges DISABLED_CapturesContentChanges
#else
#define MAYBE_CapturesContentChanges CapturesContentChanges
#endif
IN_PROC_BROWSER_TEST_P(WebContentsVideoCaptureDeviceBrowserTestP,
                       MAYBE_CapturesContentChanges) {
  SCOPED_TRACE(testing::Message()
               << "Test parameters: "
               << (IsSoftwareCompositingTest() ? "Software Compositing"
                                               : "GPU Compositing")
               << " with "
               << (IsFixedAspectRatioTest() ? "Fixed Video Aspect Ratio"
                                            : "Variable Video Aspect Ratio"));

  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  for (int visilibilty_case = 0; visilibilty_case < 3; ++visilibilty_case) {
    switch (visilibilty_case) {
      case 0: {
        SCOPED_TRACE(testing::Message()
                     << "Visibility case: WebContents is showing.");
        shell()->web_contents()->WasShown();
        base::RunLoop().RunUntilIdle();
        ASSERT_EQ(shell()->web_contents()->GetVisibility(),
                  content::Visibility::VISIBLE);
        break;
      }

      case 1: {
        SCOPED_TRACE(testing::Message()
                     << "Visibility case: WebContents is hidden.");
        shell()->web_contents()->WasHidden();
        base::RunLoop().RunUntilIdle();
        ASSERT_EQ(shell()->web_contents()->GetVisibility(),
                  content::Visibility::HIDDEN);
        break;
      }

      case 2: {
        SCOPED_TRACE(
            testing::Message()
            << "Visibility case: WebContents is showing, but occluded.");
        shell()->web_contents()->WasShown();
        shell()->web_contents()->WasOccluded();
        base::RunLoop().RunUntilIdle();
        ASSERT_EQ(shell()->web_contents()->GetVisibility(),
                  content::Visibility::OCCLUDED);
        break;
      }
    }

    static constexpr SkColor kColorsToCycleThrough[] = {
        SK_ColorRED,  SK_ColorGREEN,   SK_ColorBLUE,  SK_ColorYELLOW,
        SK_ColorCYAN, SK_ColorMAGENTA, SK_ColorWHITE,
    };
    for (SkColor color : kColorsToCycleThrough) {
      ChangePageContentColor(color);
      WaitForFrameWithColor(color);
    }
  }

  StopAndDeAllocate();
  EXPECT_FALSE(shell()->web_contents()->IsBeingCaptured());
}

}  // namespace
}  // namespace content
