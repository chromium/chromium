// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/media_session/public/cpp/features.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

namespace {

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override = default;

  bool IsActive() override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() override { return false; }
  bool IsAlwaysOnTop() override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {
    playback_state_ = playback_state;
  }
  void SetPlayPauseButtonVisibility(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override { return nullptr; }

  const base::Optional<PlaybackState>& playback_state() const {
    return playback_state_;
  }

 private:
  gfx::Size size_;
  base::Optional<PlaybackState> playback_state_;

  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override {
    return std::make_unique<TestOverlayWindow>();
  }
  bool CanEnterFullscreenWithoutUserActivation() override { return true; }
};

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  explicit TestWebContentsDelegate(Shell* shell) : shell_(shell) {}

  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override {
    shell_->EnterFullscreenModeForTab(requesting_frame, options);
  }
  void ExitFullscreenModeForTab(WebContents* web_contents) override {
    shell_->ExitFullscreenModeForTab(web_contents);
  }
  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override {
    return shell_->IsFullscreenForTabOrPending(web_contents);
  }
  PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents,
      const viz::SurfaceId&,
      const gfx::Size& natural_size) override {
    is_in_picture_in_picture_ = true;
    return PictureInPictureResult::kSuccess;
  }
  void ExitPictureInPicture() override { is_in_picture_in_picture_ = false; }

  bool is_in_picture_in_picture() const { return is_in_picture_in_picture_; }

 private:
  Shell* const shell_;
  bool is_in_picture_in_picture_ = false;
};

class PictureInPictureContentBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "PictureInPictureAPI");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    old_browser_client_ = SetBrowserClientForTesting(&content_browser_client_);

    web_contents_delegate_ = std::make_unique<TestWebContentsDelegate>(shell());
    shell()->web_contents()->SetDelegate(web_contents_delegate_.get());
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(old_browser_client_);

    ContentBrowserTest::TearDownOnMainThread();
  }

  void WaitForPlaybackState(OverlayWindow::PlaybackState playback_state) {
    // Make sure to wait if not yet in the |playback_state| state.
    if (overlay_window()->playback_state() != playback_state) {
      MediaStartStopObserver observer(
          shell()->web_contents(),
          playback_state == OverlayWindow::PlaybackState::kPlaying
              ? MediaStartStopObserver::Type::kStart
              : MediaStartStopObserver::Type::kStop);
      observer.Wait();
    }
  }

  TestWebContentsDelegate* web_contents_delegate() {
    return web_contents_delegate_.get();
  }

  PictureInPictureWindowControllerImpl* window_controller() {
    return PictureInPictureWindowControllerImpl::FromWebContents(
        shell()->web_contents());
  }

  TestOverlayWindow* overlay_window() {
    return static_cast<TestOverlayWindow*>(
        window_controller()->GetWindowForTesting());
  }

 private:
  std::unique_ptr<TestWebContentsDelegate> web_contents_delegate_;
  ContentBrowserClient* old_browser_client_ = nullptr;
  TestContentBrowserClient content_browser_client_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       RequestSecondVideoInSameRFHDoesNotCloseWindow) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "two-videos.html")));

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  std::u16string expected_title = u"videos[0] playing";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Play second video.
  ASSERT_TRUE(ExecJs(shell(), "videos[1].play();"));

  expected_title = u"videos[1] playing";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  ASSERT_FALSE(web_contents_delegate()->is_in_picture_in_picture());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  expected_title = u"videos[0] entered picture-in-picture";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[1].requestPictureInPicture();"));

  expected_title = u"videos[1] entered picture-in-picture";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, window_controller()->active_session_for_testing());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       RequestSecondVideoInDifferentRFHDoesNotCloseWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "example.com", "/media/picture_in_picture/two-videos.html")));

  std::u16string expected_title = u"iframe loaded";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  expected_title = u"videos[0] playing";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Play second video (in iframe).
  ASSERT_TRUE(ExecJs(shell(), "iframeVideos[0].play();"));

  expected_title = u"iframeVideos[0] playing";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  ASSERT_FALSE(web_contents_delegate()->is_in_picture_in_picture());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  expected_title = u"videos[0] entered picture-in-picture";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "iframeVideos[0].requestPictureInPicture();"));

  expected_title = u"iframeVideos[0] entered picture-in-picture";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, window_controller()->active_session_for_testing());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       EnterPictureInPictureThenFullscreen) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // The Picture-in-Picture window should be closed upon entering fullscreen.
  ASSERT_EQ(true, EvalJs(shell(), "enterFullscreen();"));

  EXPECT_TRUE(shell()->web_contents()->IsFullscreen());
  EXPECT_FALSE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       EnterFullscreenThenPictureInPicture) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterFullscreen();"));
  ASSERT_TRUE(shell()->web_contents()->IsFullscreen());

  // We should leave fullscreen upon entering Picture-in-Picture.
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  EXPECT_FALSE(shell()->web_contents()->IsFullscreen());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

// Check that the playback state in the Picture-in-Picture window follows the
// state of the media player.
IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       EnterPictureInPictureForPausedPlayer) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  // Play and pause the player from script.
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "video.pause();"));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by interacting with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();

  std::u16string expected_title = u"play";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by interacting with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();

  expected_title = u"pause";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureContentBrowserTest,
                       PlayerRespondsToUserActionsAfterSrcUpdate) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_EQ(true, EvalJs(shell(), "updateVideoSrcAndPlay();"));

  window_controller()->TogglePlayPause();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

class MediaSessionPictureInPictureContentBrowserTest
    : public PictureInPictureContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PictureInPictureAPI,MediaSession");
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that the playback state in the Picture-in-Picture window follows the
// state of the media player.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       EnterPictureInPictureForPausedPlayer) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  // Play and pause the player from script.
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(), "video.pause();"));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(overlay_window()->playback_state(),
            OverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by invoking the Media Session "play" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPlayActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();

  std::u16string expected_title = u"play";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by invoking the Media Session "pause" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPauseActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();

  expected_title = u"pause";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       CanvasCaptureControlledByMediaSession) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture_in_picture/canvas-in-pip.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_page_url));
  ASSERT_EQ(true, EvalJs(shell(), "start();"));
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPlaying);

  window_controller()->TogglePlayPause();
  WaitForPlaybackState(OverlayWindow::PlaybackState::kPaused);
}

class AutoPictureInPictureContentBrowserTest
    : public PictureInPictureContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "PictureInPictureAPI,AutoPictureInPicture");
  }
};

// Show/hide fullscreen page and check that Auto Picture-in-Picture is
// triggered.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureContentBrowserTest,
                       AutoPictureInPictureTriggeredWhenFullscreen) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterFullscreen();"));

  ASSERT_TRUE(ExecJs(shell(), "video.autoPictureInPicture = true;"));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));

  // Hide page and check that video entered Picture-in-Picture automatically.
  shell()->web_contents()->WasHidden();
  std::u16string expected_title = u"enterpictureinpicture";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());

  // Show page and check that video left Picture-in-Picture automatically.
  shell()->web_contents()->WasShown();
  expected_title = u"leavepictureinpicture";
  EXPECT_EQ(
      expected_title,
      TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle());
}

}  // namespace content
