// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <optional>
#include <tuple>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_test_utils.h"
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
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#endif

namespace content {
namespace {

class WebContentsVideoCaptureDeviceBrowserTest
    : public ContentCaptureDeviceBrowserTestBase,
      public FrameTestUtil {
 public:
  WebContentsVideoCaptureDeviceBrowserTest() {
    // TODO(https://crbug.com/1324757): tests should work with HiDPI enabled.
    scoped_feature_list_.InitAndDisableFeature(media::kWebContentsCaptureHiDpi);
  }

  WebContentsVideoCaptureDeviceBrowserTest(
      const WebContentsVideoCaptureDeviceBrowserTest&) = delete;
  WebContentsVideoCaptureDeviceBrowserTest& operator=(
      const WebContentsVideoCaptureDeviceBrowserTest&) = delete;

  ~WebContentsVideoCaptureDeviceBrowserTest() override = default;

  // Runs the browser until a frame whose content matches the given |color| is
  // found in the captured frames queue, or until a testing failure has
  // occurred. When |tolerate_color| is non-nullopt, encountering a frame that
  // does not match both |color| and |tolerate_color| will cause a testing
  // failure. This allows the callers to tighten the tolerance on the frames
  // they are willing to accept (since specifying `tolerate_color` causes the
  // test to fail in case we encounter something else).
  void WaitForFrameWithColor(
      SkColor color,
      std::optional<SkColor> tolerate_color = std::nullopt) {
    const std::string color_string =
        base::StringPrintf("red=%d, green=%d, blue=%d", SkColorGetR(color),
                           SkColorGetG(color), SkColorGetB(color));
    const std::string tolerated_color_string =
        tolerate_color
            ? base::StringPrintf(
                  "red=%d, green=%d, blue=%d", SkColorGetR(*tolerate_color),
                  SkColorGetG(*tolerate_color), SkColorGetB(*tolerate_color))
            : std::string("<none>");
    VLOG(1) << "Waiting for frame content area filled with color: "
            << color_string << ", tolerated color: " << tolerated_color_string;

    while (!testing::Test::HasFailure()) {
      EXPECT_TRUE(capture_stack()->Started());
      EXPECT_FALSE(capture_stack()->ErrorOccurred());
      capture_stack()->ExpectNoLogMessages();

      while (capture_stack()->HasCapturedFrames() &&
             !testing::Test::HasFailure()) {
        // Pop the next frame from the front of the queue and convert to a RGB
        // bitmap for analysis.
        const SkBitmap rgb_frame = capture_stack()->NextCapturedFrame();
        EXPECT_FALSE(rgb_frame.empty());

        // Three regions of the frame will be analyzed:
        // 1. The upper-left quadrant of the content region where the iframe
        // draws. If the iframe is not present (the non-cross-frame test
        // variant), this region will come from the main frame.
        // 2. The remaining three quadrants of the content region where the main
        // frame draws.
        // 3. The non-content (i.e., letterboxed) region.
        //
        // In both cross-site and non-cross-site variants, region #1 should be
        // of |color|. Region #2 should be of |color| in non-cross-site tests,
        // and white in cross-site tests. And region #3 must always be black,
        // but won't be present at all if the tests don't request fixed aspect
        // ratio frames.

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
        const int max_color_diff = (IsSoftwareCompositingTest())
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
            << ", letterbox region has average color " << average_letterbox_rgb
            << ", max_color_diff is " << max_color_diff;

        if (IsFixedAspectRatioTest() &&
            !IsApproximatelySameColor(
                rgb_frame, gfx::Rect(frame_size),
                ToSafeExcludeRect(content_in_frame_rect_f), SK_ColorBLACK,
                max_color_diff)) {
          // The letterboxed region should always be black for fixed aspect
          // ratio tests, and not present otherwise.
          ADD_FAILURE() << "Letterbox region is not black; PNG dump:\n"
                        << cc::GetPNGDataUrl(rgb_frame);
          return;
        }

        const SkColor expected_mainframe_color =
            IsCrossSiteCaptureTest() ? SK_ColorWHITE : color;
        const SkColor tolerated_mainframe_color =
            IsCrossSiteCaptureTest() ? SK_ColorWHITE
                                     : tolerate_color.value_or(SK_ColorWHITE);

        if (IsApproximatelySameColor(rgb_frame,
                                     ToSafeIncludeRect(iframe_in_frame_rect_f),
                                     gfx::Rect(), color, max_color_diff) &&
            IsApproximatelySameColor(
                rgb_frame, ToSafeIncludeRect(content_in_frame_rect_f),
                ToSafeExcludeRect(iframe_in_frame_rect_f),
                expected_mainframe_color, max_color_diff)) {
          // If we have a frame that matches all expectations, we can stop
          // waiting.
          VLOG(1) << "Observed desired frame.";
          return;
        }

        if (tolerate_color &&
            IsApproximatelySameColor(
                rgb_frame, ToSafeIncludeRect(iframe_in_frame_rect_f),
                gfx::Rect(), *tolerate_color, max_color_diff) &&
            IsApproximatelySameColor(
                rgb_frame, ToSafeIncludeRect(content_in_frame_rect_f),
                ToSafeExcludeRect(iframe_in_frame_rect_f),
                tolerated_mainframe_color, max_color_diff)) {
          // Otherwise, if the frame matches a color that the caller told us to
          // tolearate, we'll keep waiting for the frame.
          VLOG(1) << "Observed frame with tolerated color. This is fine, keep "
                     "waiting.";
          continue;  // Skip requesting refreshed frame - the damage signal
                     // should propagate eventually and we should get a new
                     // frame.
        }

        if (tolerate_color) {
          // Otherwise, if the tolerated color was set and we reached this
          // point, it means the frame we got did not match both expected and
          // tolerated colors.
          ADD_FAILURE() << "Observed frame that did not match both expected "
                           "and tolerated colors. color="
                        << color_string
                        << ", tolerated_color=" << tolerated_color_string
                        << ", PNG dump:\n"
                        << cc::GetPNGDataUrl(rgb_frame);
          return;
        }

        // Otherwise, we weren't told to tolerate colors other than the expected
        // one, and the frame did not match. Keep waiting.
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
        ->GetPrimaryMainFrame()
        ->GetView()
        ->GetViewBounds()
        .size();
  }

  std::unique_ptr<FrameSinkVideoCaptureDevice> CreateDevice() final {
    auto* const main_frame = shell()->web_contents()->GetPrimaryMainFrame();
    const GlobalRenderFrameHostId id(main_frame->GetProcess()->GetID(),
                                     main_frame->GetRoutingID());
    return std::make_unique<WebContentsVideoCaptureDevice>(id);
  }

  void WaitForFirstFrame() final { WaitForFrameWithColor(SK_ColorBLACK); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the device refuses to start if the WebContents target was
// destroyed before the device could start.
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
#if defined(MEMORY_SANITIZER)
#define MAYBE_ErrorsOutIfWebContentsHasGoneBeforeDeviceStart \
  DISABLED_ErrorsOutIfWebContentsHasGoneBeforeDeviceStart
#else
#define MAYBE_ErrorsOutIfWebContentsHasGoneBeforeDeviceStart \
  ErrorsOutIfWebContentsHasGoneBeforeDeviceStart
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       MAYBE_ErrorsOutIfWebContentsHasGoneBeforeDeviceStart) {
  NavigateToInitialDocument();

  auto* const main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  const auto capture_params = SnapshotCaptureParams();

  const GlobalRenderFrameHostId id(main_frame->GetProcess()->GetID(),
                                   main_frame->GetRoutingID());
  // Delete the WebContents instance and the Shell. This makes the
  // render_frame_id invalid.
  shell()->web_contents()->Close();
  ASSERT_FALSE(RenderFrameHost::FromID(id));

  // Create the device.
  auto device = std::make_unique<WebContentsVideoCaptureDevice>(id);
  // Running the pending UI tasks should cause the device to realize the
  // WebContents is gone.
  RunUntilIdle();

  // Attempt to start the device, and expect the video capture stack to have
  // been notified of the error.
  device->AllocateAndStartWithReceiver(capture_params,
                                       capture_stack()->CreateFrameReceiver());
  RunUntilIdle();

  EXPECT_FALSE(capture_stack()->Started());
  EXPECT_TRUE(capture_stack()->ErrorOccurred());
  capture_stack()->ExpectHasLogMessages();

  device->StopAndDeAllocate();
  device.reset();
  RunUntilIdle();
}

// Tests that the device starts, captures a frame, and then gracefully
// errors-out because the WebContents is destroyed before the device is stopped.
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
// TODO(crbug.com/328658521): It is also flaky on macOS.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_ErrorsOutWhenWebContentsIsDestroyed \
  DISABLED_ErrorsOutWhenWebContentsIsDestroyed
#else
#define MAYBE_ErrorsOutWhenWebContentsIsDestroyed \
  ErrorsOutWhenWebContentsIsDestroyed
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       MAYBE_ErrorsOutWhenWebContentsIsDestroyed) {
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
  EXPECT_TRUE(capture_stack()->ErrorOccurred());
  capture_stack()->ExpectHasLogMessages();

  StopAndDeAllocate();
}

// Tests that capture is re-targetted when the render view of a WebContents
// changes.
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
// TODO(crbug.com/328658521): It is also flaky on macOS.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_ChangesTargettedRenderView DISABLED_ChangesTargettedRenderView
#else
#define MAYBE_ChangesTargettedRenderView ChangesTargettedRenderView
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       MAYBE_ChangesTargettedRenderView) {
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

#if BUILDFLAG(IS_WIN)
class WebContentsVideoCaptureDeviceBrowserTestAura
    : public WebContentsVideoCaptureDeviceBrowserTest {
 public:
  // WebContentsVideoCaptureDeviceBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kApplyNativeOcclusionToCompositor,
        {{features::kApplyNativeOcclusionToCompositorType.name,
          features::kApplyNativeOcclusionToCompositorTypeRelease}});

    WebContentsVideoCaptureDeviceBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies capture still works if the WindowTreeHost is occluded.
// TODO(crbug.com/372481179): Failing on win-asan.
#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_WIN)
#define MAYBE_CapturesWhenOccluded DISABLED_CapturesWhenOccluded
#else
#define MAYBE_CapturesWhenOccluded CapturesWhenOccluded
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTestAura,
                       MAYBE_CapturesWhenOccluded) {
  aura::WindowTreeHost* window_tree_host = shell()->window()->GetHost();
  aura::test::DisableNativeWindowOcclusionTracking(window_tree_host);
  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Make a content change in the first page and wait for capture to reflect
  // that.
  ChangePageContentColor(SK_ColorRED);
  WaitForFrameWithColor(SK_ColorRED);

  // Simulate the WindowTreeHost being occluded.
  window_tree_host->SetNativeWindowOcclusionState(
      aura::Window::OcclusionState::OCCLUDED, {});

  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // Make a change and ensure it was captured.
  ChangePageContentColor(SK_ColorGREEN);
  WaitForFrameWithColor(SK_ColorGREEN);
}
#endif

// Tests that capture is re-targetted when a renderer crash is followed by a
// reload. Regression test for http://crbug.com/916332.
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
// TODO(crbug.com/328658521): It is also flaky on macOS.
// TODO(crbug.com/372481179): Failing on win-asan.
#if defined(MEMORY_SANITIZER) || \
    (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)) || BUILDFLAG(IS_MAC)
#define MAYBE_RecoversAfterRendererCrash DISABLED_RecoversAfterRendererCrash
#else
#define MAYBE_RecoversAfterRendererCrash RecoversAfterRendererCrash
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       MAYBE_RecoversAfterRendererCrash) {
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
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
// TODO(crbug/328419809): Also flaky on Mac.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_SuspendsAndResumes DISABLED_SuspendsAndResumes
#else
#define MAYBE_SuspendsAndResumes SuspendsAndResumes
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       MAYBE_SuspendsAndResumes) {
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
                                             base::Seconds(5));
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
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
// TODO(crbug.com/328658521): It is also flaky on macOS.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_DeliversRefreshFramesUponRequest \
  DISABLED_DeliversRefreshFramesUponRequest
#else
#define MAYBE_DeliversRefreshFramesUponRequest DeliversRefreshFramesUponRequest
#endif
IN_PROC_BROWSER_TEST_F(WebContentsVideoCaptureDeviceBrowserTest,
                       MAYBE_DeliversRefreshFramesUponRequest) {
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
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, media::VideoPixelFormat>> {
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
  media::VideoPixelFormat GetVideoPixelFormat() const override {
    return std::get<3>(GetParam());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebContentsVideoCaptureDeviceBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_WIN)
    if (!IsSoftwareCompositingTest()) {
      // In order to test the NV12 code-path, we need to use hardware GPU in the
      // tests as the product code checks whether hardware when deciding whether
      // NV12 is used.
      // NOTE: Pre-existing comment in `ContentCaptureDeviceBrowserTestBase`
      // suggested that this can cause the tests to take 12+ seconds just to
      // spin up a render process on debug builds. It can also cause test
      // failures in MSAN builds, or exacerbate OOM situations on highly-loaded
      // machines.
      command_line->AppendSwitch(switches::kUseGpuInTests);
    }
#endif

#if BUILDFLAG(IS_ANDROID)
    // Disable RenderDocument temporarily while we figure out why the test
    // "CapturesContentChange" is flaky when we change RenderFrameHosts.
    scoped_feature_list_.InitWithFeatures({}, {features::kRenderDocument});
#endif
  }

  // Returns human-readable description of the test based on test parameters.
  // Currently unused due to CQ treating the tests as new and applying higher
  // flakiness bar for them, which makes it impossible to land them (they
  // flake ~1 in 20 times).
  static std::string GetDescription(
      const testing::TestParamInfo<
          WebContentsVideoCaptureDeviceBrowserTestP::ParamType>& info) {
    std::string name = base::StrCat(
        {std::get<0>(info.param) ? "Software_" : "GPU_",
         std::get<1>(info.param) ? "Fixed_" : "Variable_",
         std::get<2>(info.param) ? "CrossSite_" : "Main_",
         std::get<3>(info.param) == media::VideoPixelFormat::PIXEL_FORMAT_I420
             ? "I420"
             : "Detect"});
    return name;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(
    All,
    WebContentsVideoCaptureDeviceBrowserTestP,
    testing::Combine(
        // Note: On ChromeOS and Android, software compositing is not an option.
        testing::Values(false /* GPU-accelerated compositing */),
        testing::Values(false /* variable aspect ratio */,
                        true /* fixed aspect ratio */),
        testing::Values(false /* page has only a main frame */,
                        true /* page contains a cross-site iframe */),
        testing::Values(media::VideoPixelFormat::PIXEL_FORMAT_I420)),
    &WebContentsVideoCaptureDeviceBrowserTestP::GetDescription);
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// On MacOS, there is a newly added support for NV12-in-GMB. It relies on GPU
// acceleration, but has a feature detection built-in if the format is
// specified as media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN.
INSTANTIATE_TEST_SUITE_P(
    All,
    WebContentsVideoCaptureDeviceBrowserTestP,
    testing::Combine(
        testing::Values(false /* GPU-accelerated compositing */,
                        true /* software compositing */),
        testing::Values(false /* variable aspect ratio */,
                        true /* fixed aspect ratio */),
        testing::Values(false /* page has only a main frame */,
                        true /* page contains a cross-site iframe */),
        testing::Values(media::VideoPixelFormat::PIXEL_FORMAT_I420,
                        media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN)),
    &WebContentsVideoCaptureDeviceBrowserTestP::GetDescription);
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
                        true /* page contains a cross-site iframe */),
        testing::Values(media::VideoPixelFormat::PIXEL_FORMAT_I420)),
    &WebContentsVideoCaptureDeviceBrowserTestP::GetDescription);
#endif

// Tests that the device successfully captures a series of content changes,
// whether the browser is running with software compositing or GPU-accelerated
// compositing, whether the WebContents is visible/hidden or occluded/unoccluded
// and whether the main document contains a cross-site iframe.
// TODO(crbug.com/40947039): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not
// TODO(crbug/328419809): Also flaky on Mac.
// TODO(crbug/329654821): Also flaky for ChromeOS ASAN LSAN and debug.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)) ||                \
    (BUILDFLAG(IS_CHROMEOS_ASH) && !defined(NDEBUG))
#define MAYBE_CapturesContentChanges DISABLED_CapturesContentChanges
#else
#define MAYBE_CapturesContentChanges CapturesContentChanges
#endif
IN_PROC_BROWSER_TEST_P(WebContentsVideoCaptureDeviceBrowserTestP,
                       MAYBE_CapturesContentChanges) {
  media::VideoPixelFormat specified_format = GetVideoPixelFormat();
  media::VideoPixelFormat expected_format = specified_format;

  if (specified_format == media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    if (IsSoftwareCompositingTest()) {
      expected_format = media::VideoPixelFormat::PIXEL_FORMAT_I420;
    } else {
      expected_format = media::VideoPixelFormat::PIXEL_FORMAT_NV12;
    }
  }

  SCOPED_TRACE(testing::Message()
               << "Test parameters: "
               << (IsSoftwareCompositingTest() ? "Software Compositing"
                                               : "GPU Compositing")
               << " with "
               << (IsFixedAspectRatioTest() ? "Fixed Video Aspect Ratio"
                                            : "Variable Video Aspect Ratio")
               << ", specified format is "
               << media::VideoPixelFormatToString(specified_format));

  capture_stack()->SetFrameReceivedCallback(base::BindRepeating(
      [](media::VideoPixelFormat expected_format, media::VideoFrame* frame) {
        EXPECT_EQ(frame->format(), expected_format);
      },
      expected_format));

  NavigateToInitialDocument();
  AllocateAndStartAndWaitForFirstFrame();
  EXPECT_TRUE(shell()->web_contents()->IsBeingCaptured());

  // First frame is supposed to be black, store this as a previous color:
  std::optional<SkColor> previous_color = SK_ColorBLACK;

  for (int visibility_case = 0; visibility_case < 3; ++visibility_case) {
    switch (visibility_case) {
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
      WaitForFrameWithColor(color, previous_color);
      previous_color = color;
    }
  }

  StopAndDeAllocate();
  EXPECT_FALSE(shell()->web_contents()->IsBeingCaptured());
}

}  // namespace
}  // namespace content
