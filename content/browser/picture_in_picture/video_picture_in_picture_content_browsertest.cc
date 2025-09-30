// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/media_session/public/cpp/features.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

using testing::_;

namespace {

class TestVideoOverlayWindow : public VideoOverlayWindow {
 public:
  TestVideoOverlayWindow() = default;

  TestVideoOverlayWindow(const TestVideoOverlayWindow&) = delete;
  TestVideoOverlayWindow& operator=(const TestVideoOverlayWindow&) = delete;

  ~TestVideoOverlayWindow() override = default;

  bool IsActive() const override { return false; }
  void Close() override { visible_ = false; }
  void ShowInactive() override { visible_ = true; }
  void Hide() override { visible_ = false; }
  bool IsVisible() const override { return visible_; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateNaturalSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {
    playback_state_ = playback_state;

    if (expected_playback_state_) {
      CHECK(playback_state_changed_callback_);
      if (playback_state == expected_playback_state_) {
        expected_playback_state_.reset();
        std::move(playback_state_changed_callback_).Run();
      }
    }
  }
  void SetPlayPauseButtonVisibility(bool is_visible) override {
    play_pause_button_visible_ = is_visible;
  }
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {
    next_track_button_visible_ = is_visible;
  }
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetHidePictureInPictureButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetNextSlideButtonVisibility(bool is_visible) override {}
  void SetPreviousSlideButtonVisibility(bool is_visible) override {}
  MOCK_METHOD(void, SetMediaPosition, (const media_session::MediaPosition&));
  void SetSourceTitle(const std::u16string& source_title) override {
    source_title_ = source_title;
  }
  void SetFaviconImages(
      const std::vector<media_session::MediaImage>& images) override {
    favicon_images_ = images;
    OnSetFaviconImages(images);
  }
  MOCK_METHOD(void,
              OnSetFaviconImages,
              (const std::vector<media_session::MediaImage>& images));
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}

  const std::optional<PlaybackState>& playback_state() const {
    return playback_state_;
  }

  void SetPlaybackStateChangedCallback(PlaybackState state,
                                       base::OnceClosure callback) {
    expected_playback_state_ = state;
    playback_state_changed_callback_ = std::move(callback);
  }

  const std::optional<bool>& play_pause_button_visible() const {
    return play_pause_button_visible_;
  }

  const std::optional<bool>& next_track_button_visible() const {
    return next_track_button_visible_;
  }

  const std::vector<media_session::MediaImage>& favicon_images() const {
    return favicon_images_;
  }

  const std::u16string& source_title() const { return source_title_; }

 private:
  // We maintain the visibility state so that
  // VideoPictureInPictureWindowControllerImpl::Close() sees that the window is
  // visible and proceeds to initiate leaving PiP.
  bool visible_ = false;

  gfx::Size size_;
  std::optional<PlaybackState> playback_state_;

  std::optional<PlaybackState> expected_playback_state_;
  base::OnceClosure playback_state_changed_callback_;

  std::optional<bool> play_pause_button_visible_;
  std::optional<bool> next_track_button_visible_;

  std::vector<media_session::MediaImage> favicon_images_;
  std::u16string source_title_;
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
      WebContents* web_contents) override {
    is_in_picture_in_picture_ = true;
    return PictureInPictureResult::kSuccess;
  }
  void ExitPictureInPicture() override { is_in_picture_in_picture_ = false; }

  bool is_in_picture_in_picture() const { return is_in_picture_in_picture_; }

 private:
  const raw_ptr<Shell, DanglingUntriaged> shell_;
  bool is_in_picture_in_picture_ = false;
};

class VideoPictureInPictureContentBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    content_browser_client_ = std::make_unique<TestContentBrowserClient>();

    web_contents_delegate_ = std::make_unique<TestWebContentsDelegate>(shell());
    shell()->web_contents()->SetDelegate(web_contents_delegate_.get());
  }

  void WaitForPlaybackState(VideoOverlayWindow::PlaybackState playback_state,
                            const base::Location& location = FROM_HERE) {
    if (overlay_window()->playback_state() == playback_state)
      return;

    base::RunLoop run_loop;
    overlay_window()->SetPlaybackStateChangedCallback(playback_state,
                                                      run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_EQ(overlay_window()->playback_state(), playback_state)
        << "The wait was started here: " << location.ToString();
  }

  // Waits until the Shell's WebContents has the expected title.
  void WaitForTitle(const std::u16string& expected_title,
                    const base::Location& location = FROM_HERE) {
    EXPECT_EQ(
        expected_title,
        TitleWatcher(shell()->web_contents(), expected_title).WaitAndGetTitle())
        << "The wait was started here: " << location.ToString();
  }

  TestWebContentsDelegate* web_contents_delegate() {
    return web_contents_delegate_.get();
  }

  VideoPictureInPictureWindowControllerImpl* window_controller() {
    return VideoPictureInPictureWindowControllerImpl::FromWebContents(
        shell()->web_contents());
  }

  TestVideoOverlayWindow* overlay_window() {
    return static_cast<TestVideoOverlayWindow*>(
        window_controller()->GetWindowForTesting());
  }

 private:
  std::unique_ptr<TestWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<TestContentBrowserClient> content_browser_client_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       RequestSecondVideoInSameRFHDoesNotCloseWindow) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "two-videos.html")));

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  WaitForTitle(u"videos[0] playing");

  // Play second video.
  ASSERT_TRUE(ExecJs(shell(), "videos[1].play();"));

  WaitForTitle(u"videos[1] playing");

  ASSERT_FALSE(web_contents_delegate()->is_in_picture_in_picture());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  WaitForTitle(u"videos[0] entered picture-in-picture");
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[1].requestPictureInPicture();"));

  WaitForTitle(u"videos[1] entered picture-in-picture");

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, window_controller()->active_session_for_testing());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       RequestSecondVideoInDifferentRFHDoesNotCloseWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "example.com", "/media/picture_in_picture/two-videos.html")));

  WaitForTitle(u"iframe loaded");

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  WaitForTitle(u"videos[0] playing");

  // Play second video (in iframe).
  ASSERT_TRUE(ExecJs(shell(), "iframeVideos[0].play();"));

  WaitForTitle(u"iframeVideos[0] playing");

  ASSERT_FALSE(web_contents_delegate()->is_in_picture_in_picture());

  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  WaitForTitle(u"videos[0] entered picture-in-picture");
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  // Send second video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "iframeVideos[0].requestPictureInPicture();"));

  WaitForTitle(u"iframeVideos[0] entered picture-in-picture");

  // The session should still be active and ExitPictureInPicture() never called.
  EXPECT_NE(nullptr, window_controller()->active_session_for_testing());
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
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

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
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
IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       EnterPictureInPictureForPausedPlayer) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  // Play and pause the player from script.
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(overlay_window()->playback_state(),
            VideoOverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by interacting with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"play");
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by interacting with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"pause");
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       CanvasCaptureRespondsToUserAction) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture_in_picture/canvas-in-pip.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_page_url));
  ASSERT_EQ(true, EvalJs(shell(), "start();"));
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPlaying);

  window_controller()->TogglePlayPause();
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       PlayerRespondsToUserActionsAfterSrcUpdate) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_EQ(true, EvalJs(shell(), "updateVideoSrcAndPlay();"));

  window_controller()->TogglePlayPause();
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPaused);
}

// Tests that when closing the window after the player was reset, the <video>
// element is still notified.
IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       ClosingWindowWithPlayerResetNotifiesElement) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));

  ASSERT_EQ(true, EvalJs(shell(), "resetVideo();"));

  window_controller()->Close(/*should_pause_video=*/true);

  WaitForTitle(u"leavepictureinpicture");
}

// When the player object associated with a video element is destroyed, closing
// the Picture-in-Picture window is the only interaction possible. Thus the
// play/pause/replay button should be hidden.
IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       ResettingPlayerHidesPlayPause) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_EQ(true, EvalJs(shell(), "resetVideo();"));

  EXPECT_EQ(overlay_window()->play_pause_button_visible().value_or(true),
            false);

  // Load new media on the video element. This creates a new player.
  ASSERT_EQ(true, EvalJs(shell(), "updateVideoSrcAndPlay();"));

  // The play/pause/replay button should be functional again.
  EXPECT_EQ(overlay_window()->play_pause_button_visible().value_or(false),
            true);
  window_controller()->TogglePlayPause();
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPaused);
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       PlaybackStateWhenReopenedAfterEndOfStream) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kEndOfVideo);

  window_controller()->Close(/*should_pause_video=*/false);
  WaitForTitle(u"leavepictureinpicture");

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kEndOfVideo);
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       PlaybackStateWhenSeekingWhilePausedAfterEndOfStream) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kEndOfVideo);

  ASSERT_TRUE(ExecJs(shell(), "video.currentTime = 0;"));

  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPaused);
}

// Tests that the pip window bounds are accordingly updated when the window size
// is updated.
IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       CheckWindowBounds) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "two-videos.html")));

  // Play first video.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].play();"));

  WaitForTitle(u"videos[0] playing");
  // Send first video in Picture-in-Picture.
  ASSERT_TRUE(ExecJs(shell(), "videos[0].requestPictureInPicture();"));

  WaitForTitle(u"videos[0] entered picture-in-picture");
  EXPECT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  ASSERT_TRUE(overlay_window()->IsVisible());
  gfx::Size new_size(50, 50);
  overlay_window()->UpdateNaturalSize(new_size);

  EXPECT_EQ(window_controller()->GetWindowBounds().value(),
            gfx::Rect(new_size));
}

class MediaSessionPictureInPictureContentBrowserTest
    : public VideoPictureInPictureContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaSession");
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
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  EXPECT_EQ(overlay_window()->playback_state(),
            VideoOverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by invoking the Media Session "play" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPlayActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"play");
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by invoking the Media Session "pause" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPauseActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();

  WaitForTitle(u"pause");
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPaused);
}

// Tests that an audio player and a canvas-capture player in Picture-in-Picture
// running in one frame coexist peacefully: Media Session actions should work.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       AudioPlayerWithPictureInPictureCanvasPlayer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL test_page_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture_in_picture/audio-and-canvas.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_page_url));

  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPlayActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionPauseActionHandler();"));

  // Start the audio player and enter PiP for the canvas player. The PiP
  // window should be in the "playing" state.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  ASSERT_EQ(true, EvalJs(shell(), "start();"));
  WaitForTitle(u"play");
  ASSERT_EQ(overlay_window()->playback_state(),
            VideoOverlayWindow::PlaybackState::kPlaying);

  // Simulate pausing playback by invoking the Media Session "pause" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPauseEventListener();"));
  window_controller()->TogglePlayPause();
  WaitForTitle(u"pause");
  ASSERT_EQ(overlay_window()->playback_state(),
            VideoOverlayWindow::PlaybackState::kPaused);

  // Simulate resuming playback by invoking the Media Session "play" action
  // through interaction with the PiP window.
  ASSERT_TRUE(ExecJs(shell(), "addPlayEventListener();"));
  window_controller()->TogglePlayPause();
  WaitForTitle(u"play");
  ASSERT_EQ(overlay_window()->playback_state(),
            VideoOverlayWindow::PlaybackState::kPlaying);
}

// Tests Media Session action availability upon reaching the end of stream by
// verifying that the "nexttrack" action can be invoked after playing through
// to the end of media.
// TODO(https://crbug.com/422414020): This is failing on Windows arm64.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_ActionAvailableAfterEndOfStreamAndSrcUpdate \
  DISABLED_ActionAvailableAfterEndOfStreamAndSrcUpdate
#else
#define MAYBE_ActionAvailableAfterEndOfStreamAndSrcUpdate \
  ActionAvailableAfterEndOfStreamAndSrcUpdate
#endif
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureContentBrowserTest,
                       MAYBE_ActionAvailableAfterEndOfStreamAndSrcUpdate) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));

  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionNextTrackActionHandler();"));
  ASSERT_TRUE(ExecJs(shell(), "addPictureInPictureEventListeners();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  // Check twice, because the action handler updates the 'src' attribute and we
  // want to make sure this doesn't break the action handling.
  for (int i = 0; i < 2; ++i) {
    // Play through to the end of the media resource.
    ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
    WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kEndOfVideo);

    // Simulate the user clicking the "next track" button and verify the
    // associated Media Session action that we set earlier is invoked.
    window_controller()->NextTrack();
    WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPlaying);
  }

  // After closing and reopening the PiP window the Media Session action should
  // still work.
  ASSERT_EQ(true, EvalJs(shell(), "playToEnd();"));
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kEndOfVideo);

  window_controller()->Close(/*should_pause_video=*/false);
  WaitForTitle(u"leavepictureinpicture");

  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  window_controller()->NextTrack();
  WaitForPlaybackState(VideoOverlayWindow::PlaybackState::kPlaying);
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       EnterPictureInPictureHasNoChildWebContents) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_TRUE(web_contents_delegate()->is_in_picture_in_picture());

  ASSERT_TRUE(window_controller()->GetWebContents());
  ASSERT_FALSE(window_controller()->GetChildWebContents());
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       SeeksVideoAndUpdatesMediaPosition) {
  // Open a page with a paused player in pip.
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));
  ASSERT_TRUE(ExecJs(shell(), "setMediaSessionSeekToActionHandler();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  // `SeekTo()` should properly seek the video and give the updated media
  // position to the overlay window.
  EXPECT_CALL(*overlay_window(), SetMediaPosition(_))
      .WillOnce([](const media_session::MediaPosition& position) {
        EXPECT_EQ(position.GetPosition(), base::Seconds(2));
        EXPECT_EQ(position.playback_rate(), 0);
      });
  window_controller()->SeekTo(base::Seconds(2));
  WaitForTitle(u"seekto 2");
  testing::Mock::VerifyAndClearExpectations(overlay_window());

  // If the website seeks without going through the controller, it should still
  // give the updated media position to the overlay window.
  EXPECT_CALL(*overlay_window(), SetMediaPosition(_))
      .WillOnce([](const media_session::MediaPosition& position) {
        EXPECT_EQ(position.GetPosition(), base::Seconds(4));
        EXPECT_EQ(position.playback_rate(), 0);
      });
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.currentTime = 4;"));
  testing::Mock::VerifyAndClearExpectations(overlay_window());
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       SeeksVideoWithoutMediaSessionHandler) {
  // Open a page with a paused player in pip.
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_TRUE(ExecJs(shell(), "video.load();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));
  ASSERT_EQ(0, EvalJs(shell(), "video.currentTime"));
  ASSERT_TRUE(ExecJs(shell(), "setTitleToVideoCurrentTime();"));

  // `SeekTo()` should properly seek the video and give the updated media
  // position to the overlay window, even if no media session seekto action
  // handler is set (it should instead update the media player directly).
  EXPECT_CALL(*overlay_window(), SetMediaPosition(_))
      .WillOnce([](const media_session::MediaPosition& position) {
        EXPECT_EQ(position.GetPosition(), base::Seconds(2));
        EXPECT_EQ(position.playback_rate(), 0);
      });
  window_controller()->SeekTo(base::Seconds(2));

  // We need to wait until the currentTime has actually updated.
  WaitForTitle(u"currentTime 2");
  EXPECT_EQ(2, EvalJs(shell(), "video.currentTime"));
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       SendsFaviconImagesToOverlayWindow) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  // The overlay window should have received the favicon image.
  ASSERT_EQ(overlay_window()->favicon_images().size(), 1u);
  const std::string icon_src = overlay_window()->favicon_images()[0].src.spec();
  EXPECT_TRUE(base::Contains(icon_src, "test.ico"))
      << "The icon source: \"" << icon_src << "\" should contain \"test.ico\"";

  // The overlay window should be able to retrieve the favicon image.
  base::RunLoop wait_for_image_loop;
  window_controller()->GetMediaImage(
      overlay_window()->favicon_images()[0], 16, 16,
      base::BindOnce(
          [](base::OnceClosure wait_closure, const SkBitmap& image) {
            std::move(wait_closure).Run();
          },
          wait_for_image_loop.QuitClosure()));
  wait_for_image_loop.Run();

  // If the favicon changes, then the overlay window should receive the new
  // favicon image.
  EXPECT_CALL(*overlay_window(), OnSetFaviconImages(_))
      .WillOnce([](const std::vector<media_session::MediaImage>& images) {
        ASSERT_EQ(images.size(), 1u);
        const std::string icon_src = images[0].src.spec();
        EXPECT_TRUE(base::Contains(icon_src, "new.ico"))
            << "The icon source: \"" << icon_src
            << "\" should contain \"new.ico\"";
      });
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "updateFaviconSrc('/new.ico');"));
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       SendsSourceTitleToOverlayWindow_File) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/picture_in_picture", "one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  EXPECT_EQ(overlay_window()->source_title(), u"File on your device");
}

IN_PROC_BROWSER_TEST_F(VideoPictureInPictureContentBrowserTest,
                       SendsSourceTitleToOverlayWindow_Web) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "example.com", "/media/picture_in_picture/one-video.html")));
  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "video.pause();"));
  ASSERT_EQ(true, EvalJs(shell(), "enterPictureInPicture();"));

  // The actual origin and source title contains a port number that changes. We
  // only care that it starts with "example.com" as expected.
  const std::u16string expected_title_prefix(u"example.com");
  ASSERT_GE(overlay_window()->source_title().size(),
            expected_title_prefix.size());
  EXPECT_EQ(
      overlay_window()->source_title().substr(0, expected_title_prefix.size()),
      expected_title_prefix);
}

}  // namespace content
