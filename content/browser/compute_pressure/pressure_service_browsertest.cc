// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

namespace content {

using device::mojom::PressureSource;
using device::mojom::PressureState;
using device::mojom::PressureUpdate;

namespace {

bool SupportsSharedWorker() {
#if BUILDFLAG(IS_ANDROID)
  // SharedWorkers are not enabled on Android. https://crbug.com/154571
  return false;
#else
  return true;
#endif
}

class TestVideoOverlayWindow : public VideoOverlayWindow {
 public:
  TestVideoOverlayWindow() = default;
  ~TestVideoOverlayWindow() override = default;

  TestVideoOverlayWindow(const TestVideoOverlayWindow&) = delete;
  TestVideoOverlayWindow& operator=(const TestVideoOverlayWindow&) = delete;

  bool IsActive() const override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() const override { return true; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateNaturalSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetPlayPauseButtonVisibility(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetNextSlideButtonVisibility(bool is_visible) override {}
  void SetPreviousSlideButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}

 private:
  gfx::Size size_;
};

class TestContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  std::unique_ptr<VideoOverlayWindow> CreateWindowForVideoPictureInPicture(
      VideoPictureInPictureWindowController* controller) override {
    return std::make_unique<TestVideoOverlayWindow>();
  }
};

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents) override {
    window_controller_ = PictureInPictureWindowController::
        GetOrCreateVideoPictureInPictureController(web_contents);
    return PictureInPictureResult::kSuccess;
  }

  void ExitPictureInPicture() override {
    window_controller_->Close(false /* should_pause_video */);
    window_controller_ = nullptr;
  }

 private:
  raw_ptr<VideoPictureInPictureWindowController> window_controller_;
};

class ComputePressureBrowserTest : public ContentBrowserTest {
 public:
  ComputePressureBrowserTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeUIForMediaStream);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    test_url_ =
        https_server_.GetURL("/compute_pressure/deliver_update_test.html");
    ASSERT_TRUE(NavigateToURL(shell(), test_url_));

    content_browser_client_ = std::make_unique<TestContentBrowserClient>();
    shell()->web_contents()->SetDelegate(&web_contents_delegate_);
  }

 protected:
  device::ScopedPressureManagerOverrider pressure_manager_overrider_;
  TestWebContentsDelegate web_contents_delegate_;
  std::unique_ptr<TestContentBrowserClient> content_browser_client_;
  net::EmbeddedTestServer https_server_ =
      net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS);
  GURL test_url_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ComputePressureBrowserTest, DeliverUpdate) {
  // Start PressureObserver in frame, dedicated worker and shared worker.
  ASSERT_TRUE(ExecJs(shell(), "observer.observe('cpu');"));
  ASSERT_TRUE(ExecJs(shell(), "startDedicatedWorker();"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "startSharedWorker();"));
  }

  // Deliver update.
  const base::TimeTicks time = base::TimeTicks::Now();
  PressureUpdate update(PressureSource::kCpu, PressureState::kNominal, time);
  pressure_manager_overrider_.UpdateClients(std::move(update));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("nominal", EvalJs(shell(), "datasets.frame.samples[0].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("nominal",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[0].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(1);"));
    ASSERT_EQ(1, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("nominal",
              EvalJs(shell(), "datasets.sharedWorker.samples[0].state;"));
  }
}

IN_PROC_BROWSER_TEST_F(ComputePressureBrowserTest, DeliverUpdateForSameOrigin) {
  // Start PressureObserver in frame, dedicated worker and shared worker.
  ASSERT_TRUE(ExecJs(shell(), "observer.observe('cpu');"));
  ASSERT_TRUE(ExecJs(shell(), "startDedicatedWorker();"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "startSharedWorker();"));
  }

  // Focus on same-origin iframe, observers can still receive updates.
  ASSERT_TRUE(ExecJs(shell(), "same_origin_iframe.focus();"));

  // Deliver update.
  const base::TimeTicks time = base::TimeTicks::Now();
  PressureUpdate update(PressureSource::kCpu, PressureState::kNominal, time);
  pressure_manager_overrider_.UpdateClients(std::move(update));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("nominal", EvalJs(shell(), "datasets.frame.samples[0].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("nominal",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[0].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(1);"));
    ASSERT_EQ(1, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("nominal",
              EvalJs(shell(), "datasets.sharedWorker.samples[0].state;"));
  }
}

IN_PROC_BROWSER_TEST_F(ComputePressureBrowserTest, NoUpdateForCrossOrigin) {
  // Start PressureObserver in frame, dedicated worker and shared worker.
  ASSERT_TRUE(ExecJs(shell(), "observer.observe('cpu');"));
  ASSERT_TRUE(ExecJs(shell(), "startDedicatedWorker();"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "startSharedWorker();"));
  }

  // Focus on cross-origin iframe, observers can not receive updates.
  ASSERT_TRUE(ExecJs(shell(), "cross_origin_iframe.focus();"));

  // Deliver update.
  const base::TimeTicks time1 = base::TimeTicks::Now();
  PressureUpdate update1(PressureSource::kCpu, PressureState::kNominal, time1);
  pressure_manager_overrider_.UpdateClients(std::move(update1));

  // Focus on main frame, observers can receive updates again.
  ASSERT_TRUE(ExecJs(shell(), "parent.focus();"));

  // Deliver update.
  const base::TimeTicks time2 = time1 + base::Seconds(2);
  PressureUpdate update2(PressureSource::kCpu, PressureState::kFair, time2);
  pressure_manager_overrider_.UpdateClients(std::move(update2));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("fair", EvalJs(shell(), "datasets.frame.samples[0].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("fair",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[0].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(1);"));
    ASSERT_EQ(1, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("fair",
              EvalJs(shell(), "datasets.sharedWorker.samples[0].state;"));
  }
}

IN_PROC_BROWSER_TEST_F(ComputePressureBrowserTest, DeliverDataForPiP) {
  // Play video.
  ASSERT_TRUE(ExecJs(shell(), "video.play();"));
  // Make video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "video.requestPictureInPicture();"));
  EXPECT_TRUE(shell()->web_contents()->HasPictureInPictureVideo());

  // Start PressureObserver in frame, dedicated worker and shared worker.
  ASSERT_TRUE(ExecJs(shell(), "observer.observe('cpu');"));
  ASSERT_TRUE(ExecJs(shell(), "startDedicatedWorker();"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "startSharedWorker();"));
  }

  // Focus on cross-origin iframe, observers can not receive updates by
  // default. If the frame is same origin with initiators of active
  // Picture-in-Picture sessions, observers can receive updates.
  ASSERT_TRUE(ExecJs(shell(), "cross_origin_iframe.focus();"));

  // Deliver update.
  const base::TimeTicks time1 = base::TimeTicks::Now();
  PressureUpdate update1(PressureSource::kCpu, PressureState::kNominal, time1);
  pressure_manager_overrider_.UpdateClients(std::move(update1));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("nominal", EvalJs(shell(), "datasets.frame.samples[0].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("nominal",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[0].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(1);"));
    ASSERT_EQ(1, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("nominal",
              EvalJs(shell(), "datasets.sharedWorker.samples[0].state;"));
  }

  // Exit Picture-in-Picture, so observers can not receive updates.
  ASSERT_TRUE(ExecJs(shell(), "document.exitPictureInPicture();"));
  EXPECT_FALSE(shell()->web_contents()->HasPictureInPictureVideo());

  // Deliver update.
  const base::TimeTicks time2 = time1 + base::Seconds(2);
  PressureUpdate update2(PressureSource::kCpu, PressureState::kFair, time2);
  pressure_manager_overrider_.UpdateClients(std::move(update2));

  // Focus on main frame, observers can receive updates again.
  ASSERT_TRUE(ExecJs(shell(), "parent.focus();"));

  // Deliver update.
  const base::TimeTicks time3 = time2 + base::Seconds(2);
  PressureUpdate update3(PressureSource::kCpu, PressureState::kSerious, time3);
  pressure_manager_overrider_.UpdateClients(std::move(update3));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(2);"));
  ASSERT_EQ(2, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("serious", EvalJs(shell(), "datasets.frame.samples[1].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(2);"));
  ASSERT_EQ(2, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("serious",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[1].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(2);"));
    ASSERT_EQ(2, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("serious",
              EvalJs(shell(), "datasets.sharedWorker.samples[1].state;"));
  }
}

IN_PROC_BROWSER_TEST_F(ComputePressureBrowserTest, DeliverDataForCapturing) {
  // Start capturing.
  ASSERT_TRUE(ExecJs(shell(), "startCapturing();"));

  // Start PressureObserver in frame, dedicated worker and shared worker.
  ASSERT_TRUE(ExecJs(shell(), "observer.observe('cpu');"));
  ASSERT_TRUE(ExecJs(shell(), "startDedicatedWorker();"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "startSharedWorker();"));
  }

  // Focus on cross-origin iframe, observers can not receive updates by
  // default. If the frame is capturing, observers can receive updates.
  ASSERT_TRUE(ExecJs(shell(), "cross_origin_iframe.focus();"));

  // Deliver update.
  const base::TimeTicks time1 = base::TimeTicks::Now();
  PressureUpdate update1(PressureSource::kCpu, PressureState::kNominal, time1);
  pressure_manager_overrider_.UpdateClients(std::move(update1));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("nominal", EvalJs(shell(), "datasets.frame.samples[0].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(1);"));
  ASSERT_EQ(1, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("nominal",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[0].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(1);"));
    ASSERT_EQ(1, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("nominal",
              EvalJs(shell(), "datasets.sharedWorker.samples[0].state;"));
  }

  // Stop capturing.
  ASSERT_TRUE(ExecJs(shell(), "stopCapturing();"));

  // Deliver update.
  const base::TimeTicks time2 = time1 + base::Seconds(2);
  PressureUpdate update2(PressureSource::kCpu, PressureState::kFair, time2);
  pressure_manager_overrider_.UpdateClients(std::move(update2));

  // Focus on main frame, observers can receive updates again.
  ASSERT_TRUE(ExecJs(shell(), "parent.focus();"));

  // Deliver update.
  const base::TimeTicks time3 = time2 + base::Seconds(2);
  PressureUpdate update3(PressureSource::kCpu, PressureState::kSerious, time3);
  pressure_manager_overrider_.UpdateClients(std::move(update3));

  ASSERT_TRUE(ExecJs(shell(), "datasets.frame.waitForUpdates(2);"));
  ASSERT_EQ(2, EvalJs(shell(), "datasets.frame.samples.length;"));
  ASSERT_EQ("serious", EvalJs(shell(), "datasets.frame.samples[1].state;"));
  ASSERT_TRUE(ExecJs(shell(), "datasets.dedicatedWorker.waitForUpdates(2);"));
  ASSERT_EQ(2, EvalJs(shell(), "datasets.dedicatedWorker.samples.length;"));
  ASSERT_EQ("serious",
            EvalJs(shell(), "datasets.dedicatedWorker.samples[1].state;"));
  if (SupportsSharedWorker()) {
    ASSERT_TRUE(ExecJs(shell(), "datasets.sharedWorker.waitForUpdates(2);"));
    ASSERT_EQ(2, EvalJs(shell(), "datasets.sharedWorker.samples.length;"));
    ASSERT_EQ("serious",
              EvalJs(shell(), "datasets.sharedWorker.samples[1].state;"));
  }
}

}  // namespace content
