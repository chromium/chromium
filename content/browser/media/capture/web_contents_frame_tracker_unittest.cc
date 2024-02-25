// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_switches.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_feedback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen_info.h"

namespace content {
namespace {

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::StrictMock;

constexpr viz::FrameSinkId kInitSinkId(123, 456);

// Standardized screen resolutions to test common scenarios.
constexpr gfx::Size kSizeZero{0, 0};
constexpr gfx::Size kSize720p{1280, 720};
constexpr gfx::Size kSize1080p{1920, 1080};
constexpr gfx::Size kSizeWsxgaPlus{1680, 1050};

class SimpleContext : public WebContentsFrameTracker::Context {
 public:
  ~SimpleContext() override = default;

  // WebContentsFrameTracker::Context overrides.
  std::optional<gfx::Rect> GetScreenBounds() override { return screen_bounds_; }

  WebContentsImpl::CaptureTarget GetCaptureTarget() override {
    return WebContentsImpl::CaptureTarget{frame_sink_id_, gfx::NativeView{}};
  }
  void IncrementCapturerCount(const gfx::Size& capture_size) override {
    ++capturer_count_;
    last_capture_size_ = capture_size;
  }
  void DecrementCapturerCount() override { --capturer_count_; }

  void SetScaleOverrideForCapture(float scale) override {
    scale_override_ = scale;
  }
  float GetScaleOverrideForCapture() const override { return scale_override_; }
  int capturer_count() const { return capturer_count_; }
  const gfx::Size& last_capture_size() const { return last_capture_size_; }

  void set_frame_sink_id(viz::FrameSinkId frame_sink_id) {
    frame_sink_id_ = frame_sink_id;
  }
  void set_screen_bounds(std::optional<gfx::Rect> screen_bounds) {
    screen_bounds_ = std::move(screen_bounds);
  }
  float scale_override() const { return scale_override_; }

 private:
  int capturer_count_ = 0;
  viz::FrameSinkId frame_sink_id_ = kInitSinkId;
  gfx::Size last_capture_size_;
  std::optional<gfx::Rect> screen_bounds_;
  float scale_override_ = 1.0f;
};

// The capture device is mostly for interacting with the frame tracker. We do
// care about the frame tracker pushing back target updates, however.
class MockCaptureDevice : public WebContentsVideoCaptureDevice {
 public:
  MOCK_METHOD2(OnTargetChanged,
               void(const std::optional<viz::VideoCaptureTarget>&, uint32_t));
  MOCK_METHOD0(OnTargetPermanentlyLost, void());

  base::WeakPtr<MockCaptureDevice> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockCaptureDevice> weak_ptr_factory_{this};
};

// This test class is intentionally quite similar to
// |WebContentsVideoCaptureDevice|, and provides convenience methods for calling
// into the |WebContentsFrameTracker|, which interacts with UI thread objects
// and needs to be called carefully on the UI thread.
class WebContentsFrameTrackerTest : public RenderViewHostTestHarness {
 protected:
  WebContentsFrameTrackerTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    // The tests assume that they are running on the main thread (which is
    // equivalent to the browser's UI thread) so that they can make calls on the
    // tracker object synchronously.
    ASSERT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    // Views in the web context are incredibly fragile and prone to
    // non-deterministic test failures, so we use TestWebContents here.
    web_contents_ = TestWebContents::Create(browser_context(), nullptr);
    device_ = std::make_unique<StrictMock<MockCaptureDevice>>();

    // All tests should call target changed as part of initialization.
    EXPECT_CALL(*device_, OnTargetChanged(_, _)).Times(1);

    // This PostTask technically isn't necessary since we're already on the main
    // thread which is equivalent to the browser's UI thread, but it's a bit
    // cleaner to do so in case we want to switch to a different threading model
    // for the tests in the future.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTrackerTest::SetUpOnUIThread,
                       base::Unretained(this),
                       base::SingleThreadTaskRunner::GetCurrentDefault()));
    RunAllTasksUntilIdle();
  }

  void SetUpOnUIThread(
      const scoped_refptr<base::SequencedTaskRunner> device_task_runner) {
    auto context = std::make_unique<SimpleContext>();
    raw_context_ = context.get();
    SetScreenSize(kSize1080p);
    tracker_ = std::make_unique<WebContentsFrameTracker>(
        device_task_runner, device_->AsWeakPtr(), controller());
    tracker_->SetWebContentsAndContextForTesting(web_contents_.get(),
                                                 std::move(context));
  }

  void TearDown() override {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTrackerTest::TearDownOnUIThread,
                       base::Unretained(this)));
    RunAllTasksUntilIdle();
    RenderViewHostTestHarness::TearDown();
  }

  void TearDownOnUIThread() {
    tracker_.reset();
    device_.reset();
    web_contents_.reset();
  }

  void SetScreenSize(const gfx::Size& size) {
    raw_context_->set_screen_bounds(gfx::Rect{size});
  }

  void SetFrameSinkId(viz::FrameSinkId id) {
    raw_context_->set_frame_sink_id(id);
  }

  void StartTrackerOnUIThread(const gfx::Size& capture_size) {
    // Using base::Unretained for the tracker is presumed safe due to using
    // RunAllTasksUntilIdle in TearDown.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTracker::WillStartCapturingWebContents,
                       base::Unretained(tracker_.get()), capture_size,
                       true /* is_high_dpi_enabled */));
  }

  void StopTrackerOnUIThread() {
    // Using base::Unretained for the tracker is presumed safe due to using
    // RunAllTasksUntilIdle in TearDown.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTracker::DidStopCapturingWebContents,
                       base::Unretained(tracker_.get())));
  }

  // The controller is ignored on Android, and must be initialized on all
  // other platforms.
  MouseCursorOverlayController* controller() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return nullptr;
#else
    return &controller_;
#endif
  }
  WebContentsFrameTracker* tracker() { return tracker_.get(); }
  SimpleContext* context() { return raw_context_; }
  StrictMock<MockCaptureDevice>* device() { return device_.get(); }

 private:
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MouseCursorOverlayController controller_;
#endif

  std::unique_ptr<TestWebContents> web_contents_;
  std::unique_ptr<StrictMock<MockCaptureDevice>> device_;
  std::unique_ptr<WebContentsFrameTracker> tracker_;

  // Save because the pointed-to location should not change during testing.
  raw_ptr<SimpleContext, AcrossTasksDanglingUntriaged> raw_context_;
};

TEST_F(WebContentsFrameTrackerTest, CalculatesPreferredSizeClampsToView) {
  SetScreenSize(kSize720p);
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize1080p));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeNoLargerThanCaptureSize) {
  SetScreenSize(kSize1080p);
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));
  EXPECT_EQ(kSize1080p, tracker()->CalculatePreferredSize(kSize1080p));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeWithCorrectAspectRatio) {
  SetScreenSize(kSizeWsxgaPlus);

  // 720P is strictly less than WSXGA+, so should be unchanged.
  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));

  // 1080P is larger, so should be scaled appropriately.
  EXPECT_EQ((gfx::Size{1680, 945}),
            tracker()->CalculatePreferredSize(kSize1080p));

  // Wider capture size.
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1050}));

  // Taller capture size.
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2100}));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeAspectRatioWithNoOffByOneErrors) {
  SetScreenSize(kSizeWsxgaPlus);

  // Wider capture size.
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1050}));
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1051}));
  EXPECT_EQ((gfx::Size{1680, 526}),
            tracker()->CalculatePreferredSize(gfx::Size{3360, 1052}));
  EXPECT_EQ((gfx::Size{1680, 525}),
            tracker()->CalculatePreferredSize(gfx::Size{3361, 1052}));
  EXPECT_EQ((gfx::Size{1680, 666}),
            tracker()->CalculatePreferredSize(gfx::Size{5897, 2339}));

  // Taller capture size.
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2100}));
  EXPECT_EQ((gfx::Size{499, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2101}));
  EXPECT_EQ((gfx::Size{499, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1000, 2102}));
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1001, 2102}));
  EXPECT_EQ((gfx::Size{500, 1050}),
            tracker()->CalculatePreferredSize(gfx::Size{1002, 2102}));

  // Some larger and prime factor cases to sanity check.
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21841, 7351}));
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21841, 7349}));
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21839, 7351}));
  EXPECT_EQ((gfx::Size{1680, 565}),
            tracker()->CalculatePreferredSize(gfx::Size{21839, 7349}));

  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139441, 55651}));
  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139439, 55651}));
  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139441, 55649}));
  EXPECT_EQ((gfx::Size{1680, 670}),
            tracker()->CalculatePreferredSize(gfx::Size{139439, 55649}));

  // Finally, just check for roundoff errors.
  SetScreenSize(gfx::Size{1000, 1000});
  EXPECT_EQ((gfx::Size{1000, 333}),
            tracker()->CalculatePreferredSize(gfx::Size{3000, 1000}));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeLeavesCaptureSizeIfSmallerThanScreen) {
  // Smaller in both directions, but different aspect ratio, should be
  // unchanged.
  SetScreenSize(kSize1080p);
  EXPECT_EQ(kSizeWsxgaPlus, tracker()->CalculatePreferredSize(kSizeWsxgaPlus));
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeWithZeroValueProperly) {
  // If a capture dimension is zero, the preferred size should be (0, 0).
  EXPECT_EQ((kSizeZero), tracker()->CalculatePreferredSize(gfx::Size{0, 1000}));
  EXPECT_EQ((kSizeZero), tracker()->CalculatePreferredSize(kSizeZero));
  EXPECT_EQ((kSizeZero), tracker()->CalculatePreferredSize(gfx::Size{1000, 0}));

  // If a screen dimension is zero, the preferred size should be (0, 0). This
  // probably means the tab isn't being drawn anyway.
  SetScreenSize(gfx::Size{1920, 0});
  EXPECT_EQ(kSizeZero, tracker()->CalculatePreferredSize(kSize720p));
  SetScreenSize(gfx::Size{0, 1080});
  EXPECT_EQ(kSizeZero, tracker()->CalculatePreferredSize(kSize720p));
  SetScreenSize(kSizeZero);
  EXPECT_EQ(kSizeZero, tracker()->CalculatePreferredSize(kSize720p));
}

TEST_F(WebContentsFrameTrackerTest, UpdatesPreferredSizeOnWebContents) {
  StartTrackerOnUIThread(kSize720p);
  RunAllTasksUntilIdle();

  // In this case, the capture size requested is smaller than the screen size,
  // so it should be used.
  EXPECT_EQ(kSize720p, context()->last_capture_size());
  EXPECT_EQ(context()->capturer_count(), 1);

  // When we stop the tracker, the web contents issues a preferred size change
  // of the "old" size--so it shouldn't change.
  StopTrackerOnUIThread();
  RunAllTasksUntilIdle();
  EXPECT_EQ(kSize720p, context()->last_capture_size());
  EXPECT_EQ(context()->capturer_count(), 0);
}

TEST_F(WebContentsFrameTrackerTest, NotifiesOfLostTargets) {
  EXPECT_CALL(*device(), OnTargetPermanentlyLost()).Times(1);
  tracker()->WebContentsDestroyed();
  RunAllTasksUntilIdle();
}

// We test target changing for all other tests as part of set up, but also
// test the observer callbacks here.
TEST_F(WebContentsFrameTrackerTest, NotifiesOfTargetChanges) {
  const viz::FrameSinkId kNewId(42, 1337);
  SetFrameSinkId(kNewId);
  EXPECT_CALL(
      *device(),
      OnTargetChanged(std::make_optional<viz::VideoCaptureTarget>(kNewId),
                      /*sub_capture_target_version=*/0))
      .Times(1);

  // The tracker doesn't actually use the frame host information, just
  // posts a possible target change.
  tracker()->RenderFrameHostChanged(nullptr, nullptr);
  RunAllTasksUntilIdle();
}

TEST_F(WebContentsFrameTrackerTest,
       CroppingChangesTargetParametersAndInvokesCallback) {
  const base::Token kCropId(19831230, 19840730);

  // Expect the callback handed to Crop() to be invoke with kSuccess.
  bool success = false;
  base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)> callback =
      base::BindOnce(
          [](bool* success, media::mojom::ApplySubCaptureTargetResult result) {
            *success =
                (result == media::mojom::ApplySubCaptureTargetResult::kSuccess);
          },
          &success);

  // Expect OnTargetChanged() to be invoked once with the crop-ID.
  EXPECT_CALL(*device(),
              OnTargetChanged(std::make_optional<viz::VideoCaptureTarget>(
                                  kInitSinkId, kCropId),
                              /*sub_capture_target_version=*/1))
      .Times(1);

  tracker()->ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType::kCropTarget, kCropId,
      /*sub_capture_target_version=*/1, std::move(callback));

  RunAllTasksUntilIdle();
  EXPECT_TRUE(success);
}

TEST_F(WebContentsFrameTrackerTest, SetsScaleOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Capture starts at 1080p size, there should be no scale override.
  EXPECT_EQ(kSize1080p, context()->last_capture_size());
  EXPECT_EQ(context()->capturer_count(), 1);
  EXPECT_EQ(context()->scale_override(), 1.0f);

  // Calling SetCapturedContentSize with that size is a no-op.
  tracker()->SetCapturedContentSize(kSize1080p);
  EXPECT_EQ(context()->scale_override(), 1.0f);

  // Adjust the captured content size to a smaller size. This should activate a
  // scale override correlative to the difference between the two resolutions.
  tracker()->SetCapturedContentSize(kSize720p);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5);

  // Scaling should go up to a maximum of 2.0.
  tracker()->SetCapturedContentSize(gfx::Size(960, 540));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 2.0f);

  // The tracker should assume that we are now already scaled by the override
  // value, and so shouldn't change the override if we start getting frames that
  // are large enough.
  tracker()->SetCapturedContentSize(gfx::Size(1920, 1080));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 2.0f);

  // If a frame ends up being larger than the capture_size, the scale
  // should get adjusted downwards so that the post-scaling size matches
  // the capture size. This assumes a current scale override of 2.0f.
  tracker()->SetCapturedContentSize(gfx::Size(2560, 1440));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);

  // The scaled size should now match the capture size with a scale
  // override of 1.5.
  tracker()->SetCapturedContentSize(gfx::Size(1920, 1080));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);

  // The scaling calculation is based on fitting a scaled copy of the
  // source rectangle within the capture region, preserving aspect ratio.
  // If the content size changes in a way that doesn't affect the scale
  // factor (i.e. letterboxing or pillarboxing), the scale override remains
  // unchanged.
  tracker()->SetCapturedContentSize(gfx::Size(1080, 1080));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);
  tracker()->SetCapturedContentSize(gfx::Size(1920, 540));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);

  // When we stop the tracker, the web contents issues a preferred size change
  // of the "old" size--so it shouldn't change.
  StopTrackerOnUIThread();
  RunAllTasksUntilIdle();
  EXPECT_EQ(kSize1080p, context()->last_capture_size());
  EXPECT_EQ(context()->capturer_count(), 0);
}

TEST_F(WebContentsFrameTrackerTest, SettingScaleFactorMaintainsStableCapture) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Adjust the captured content size to a smaller size. This should activate a
  // scale override correlative to the difference between the two resolutions.
  tracker()->SetCapturedContentSize(kSize720p);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5);

  // It should now be scaled to the capture_size, meaning 1080P. The
  // scale override factor should be unaffected.
  tracker()->SetCapturedContentSize(kSize1080p);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiIsRoundedIfBetweenBounds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Both factors should be 1.4f, which should be between bounds and rounded
  // up.
  tracker()->SetCapturedContentSize(gfx::Size{1370, 771});
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiIsRoundedIfBetweenDifferentBounds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Both factors should be 1.6f, which should be between bounds and rounded
  // down.
  tracker()->SetCapturedContentSize(gfx::Size{1200, 675});
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiIsRoundedToMinimum) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Both factors should be 1.3f, which should be between bounds and rounded
  // down.
  tracker()->SetCapturedContentSize(gfx::Size{1477, 831});
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiIsRoundedToMaximum) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Both factors should be well over the maximum of 2.0f.
  tracker()->SetCapturedContentSize(gfx::Size{320, 240});
  EXPECT_DOUBLE_EQ(context()->scale_override(), 2.0f);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiScalingIsStable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Both factors should be 1.25f, which should be exactly a scaling factor.
  static constexpr gfx::Size kContentSize(1536, 864);
  tracker()->SetCapturedContentSize(kContentSize);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);

  // Now that its applied, it should stay the same.
  static const gfx::Size kScaledContentSize =
      gfx::ScaleToRoundedSize(kContentSize, 1.25f);
  tracker()->SetCapturedContentSize(kScaledContentSize);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);

  // If it varies slightly that shouldn't result in any changes.
  static const gfx::Size kScaledLargerContentSize =
      gfx::ScaleToRoundedSize(kContentSize, 1.27f);
  tracker()->SetCapturedContentSize(kScaledLargerContentSize);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);

  static const gfx::Size kScaledSmallerContentSize =
      gfx::ScaleToRoundedSize(kContentSize, 1.23f);
  tracker()->SetCapturedContentSize(kScaledSmallerContentSize);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiAdjustsForResourceUtilization) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  // Both factors should be 2.0f, which should be exactly a scaling factor.
  tracker()->SetCapturedContentSize(gfx::Size(960, 540));
  EXPECT_DOUBLE_EQ(context()->scale_override(), 2.0f);

  // Start with default feedback, which should be ignored.
  media::VideoCaptureFeedback feedback;
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 2.0f);

  // As the feedback continues to be poor, the scale override should lower.
  feedback.resource_utilization = 0.9f;
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.75f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.0f);

  // If things get significantly better, it should go back up.
  feedback.resource_utilization = 0.49f;
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.75f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 2.0f);
}

TEST_F(WebContentsFrameTrackerTest, HighDpiAdjustsForMaxPixelRate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kWebContentsCaptureHiDpi);

  StartTrackerOnUIThread(kSize1080p);
  RunAllTasksUntilIdle();

  tracker()->SetCapturedContentSize(kSize720p);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);

  // Test using too many pixels.
  media::VideoCaptureFeedback feedback;
  feedback.max_pixels = kSize720p.width() * kSize720p.height() - 1;

  // We should lower the maximum, which should eventually lower the override.
  // First, max is now 1.75f.
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);

  // Now max is 1.5f.
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);

  // Now max is 1.25f.
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);

  // Now max is 1.0f.
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.0f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.0f);

  // Things should only change if it gets significantly better.
  feedback.max_pixels = kSize720p.width() * kSize720p.height() + 1;
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.0f);

  feedback.max_pixels = kSize720p.width() * kSize720p.height() * 1.33f;
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.25f);
  tracker()->OnUtilizationReport(feedback);
  EXPECT_DOUBLE_EQ(context()->scale_override(), 1.5f);
}

}  // namespace
}  // namespace content
