// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/widget/screen_info.h"

namespace content {
namespace {

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::StrictMock;

constexpr gfx::Size kSize720p = gfx::Size(1280, 720);
constexpr gfx::Size kSize1080p = gfx::Size(1920, 1080);

class SimpleContext : public WebContentsFrameTracker::Context {
  ~SimpleContext() override = default;

  // WebContentsFrameTracker::Context overrides.
  base::Optional<gfx::Rect> GetScreenBounds() override {
    return screen_bounds_;
  }
  viz::FrameSinkId GetFrameSinkIdForCapture() override {
    return frame_sink_id_;
  }
  void IncrementCapturerCount(const gfx::Size& capture_size) override {
    ++capturer_count_;
  }
  void DecrementCapturerCount() override { --capturer_count_; }

  // Setters.
  int capturer_count() const { return capturer_count_; }
  void set_frame_sink_id(viz::FrameSinkId frame_sink_id) {
    frame_sink_id_ = frame_sink_id;
  }
  void set_screen_bounds(base::Optional<gfx::Rect> screen_bounds) {
    screen_bounds_ = std::move(screen_bounds);
  }

 private:
  int capturer_count_ = 0;
  viz::FrameSinkId frame_sink_id_;
  base::Optional<gfx::Rect> screen_bounds_;
};

class SimpleWebContentsDelegate : public WebContentsDelegate {
 public:
  void UpdatePreferredSize(WebContents* contents,
                           const gfx::Size& size) override {
    // We generally get a zero-value preferred size update when the contents are
    // torn down--this can be safely ignored for the purpose of these tests.
    if (size == gfx::Size{}) {
      return;
    }
    current_size = size;
  }

  gfx::Size current_size;
};

// The capture device is mostly for interacting with the frame tracker. We do
// care about the frame tracker pushing back target updates, however.
class MockCaptureDevice : public WebContentsVideoCaptureDevice,
                          public base::SupportsWeakPtr<MockCaptureDevice> {
 public:
  using WebContentsVideoCaptureDevice::AsWeakPtr;
  MOCK_METHOD1(OnTargetChanged, void(const viz::FrameSinkId&));
  MOCK_METHOD0(OnTargetPermanentlyLost, void());
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

    // Views in the web context are incredibly fragile and prone to
    // non-deterministic test failures, so we use TestWebContents here.
    web_contents_ = TestWebContents::Create(browser_context(), nullptr);
    device_ = std::make_unique<StrictMock<MockCaptureDevice>>();

    // All tests should call target changed as part of initialization.
    EXPECT_CALL(*device_, OnTargetChanged(_)).Times(1);

    tracker_ = std::make_unique<WebContentsFrameTracker>(device_->AsWeakPtr(),
                                                         controller());

    // It's fine to use the UI thread task runner for callbacks here, since the
    // mock delegate doesn't care what thread it executes on.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&WebContentsFrameTrackerTest::SetUpOnUIThread,
                                  base::Unretained(this)));
    RunAllTasksUntilIdle();
  }

  void SetUpOnUIThread() {
    web_contents_->SetDelegate(&delegate_);
    auto context = std::make_unique<SimpleContext>();
    raw_context_ = context.get();
    tracker_->SetWebContentsAndContextForTesting(web_contents_.get(),
                                                 std::move(context));
    SetFrameSinkId(viz::FrameSinkId(123, 456));
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

  void SetFrameSinkId(const viz::FrameSinkId id) {
    raw_context_->set_frame_sink_id(id);
  }

  void StartTrackerOnUIThread(const gfx::Size& capture_size) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTracker::WillStartCapturingWebContents,
                       tracker_->AsWeakPtr(), capture_size));
  }

  void StopTrackerOnUIThread() {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsFrameTracker::DidStopCapturingWebContents,
                       tracker_->AsWeakPtr()));
  }

  // The controller is ignored on Android, and must be initialized on all
  // other platforms.
  MouseCursorOverlayController* controller() {
#if defined(OS_ANDROID)
    return nullptr;
#else
    return &controller_;
#endif
  }
  const SimpleWebContentsDelegate& delegate() const { return delegate_; }
  WebContentsFrameTracker* tracker() { return tracker_.get(); }
  SimpleContext* context() { return raw_context_; }

 private:
#if !defined(OS_ANDROID)
  MouseCursorOverlayController controller_;
#endif

  std::unique_ptr<TestWebContents> web_contents_;
  SimpleWebContentsDelegate delegate_;
  std::unique_ptr<StrictMock<MockCaptureDevice>> device_;
  std::unique_ptr<WebContentsFrameTracker> tracker_;

  // Save because the pointed-to location should not change during testing.
  SimpleContext* raw_context_;
};

TEST_F(WebContentsFrameTrackerTest, CalculatesPreferredSizeClampsToView) {
  SetScreenSize(kSize720p);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WebContentsFrameTracker* tracker) {
            EXPECT_EQ(kSize720p, tracker->CalculatePreferredSize(kSize720p));
            EXPECT_EQ(kSize720p, tracker->CalculatePreferredSize(kSize1080p));
          },
          base::Unretained(tracker())));
  RunAllTasksUntilIdle();
}

TEST_F(WebContentsFrameTrackerTest,
       CalculatesPreferredSizeNoLargerThanCaptureSize) {
  SetScreenSize(kSize1080p);
  RunAllTasksUntilIdle();

  EXPECT_EQ(kSize720p, tracker()->CalculatePreferredSize(kSize720p));
  EXPECT_EQ(kSize1080p, tracker()->CalculatePreferredSize(kSize1080p));
}

TEST_F(WebContentsFrameTrackerTest, UpdatesPreferredSizeOnWebContents) {
  SetScreenSize(kSize1080p);
  StartTrackerOnUIThread(kSize720p);
  RunAllTasksUntilIdle();

  // In this case, the capture size requested is smaller than the screen size,
  // so it should be used.
  EXPECT_EQ(kSize720p, delegate().current_size);
  EXPECT_EQ(context()->capturer_count(), 1);
  // When we stop the tracker, the web contents issues a preferred size change
  // of the "old" size--so it shouldn't change.
  StopTrackerOnUIThread();
  RunAllTasksUntilIdle();
  EXPECT_EQ(kSize720p, delegate().current_size);
  EXPECT_EQ(context()->capturer_count(), 0);
}

TEST_F(WebContentsFrameTrackerTest, NotifiesOfLostTargets) {
  EXPECT_CALL(*device_, OnTargetPermanentlyLost()).Times(1);
  tracker()->WebContentsDestroyed();
  RunAllTasksUntilIdle();
}

// We test target changing for all other tests as part of set up, but also
// test the observer callbacks here.
TEST_F(WebContentsFrameTrackerTest, NotifiesOfTargetChanges) {
  const viz::FrameSinkId kNewId(42, 1337);
  EXPECT_CALL(*device_, OnTargetChanged(kNewId)).Times(1);
  SetFrameSinkId(kNewId);
  // The tracker doesn't actually use the frame host information, just
  // posts a possible target change.
  tracker()->RenderFrameHostChanged(nullptr, nullptr);
  RunAllTasksUntilIdle();
}

}  // namespace
}  // namespace content
