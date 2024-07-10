// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_service_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class NavigationWatchingWebContentsObserver : public WebContentsObserver {
 public:
  explicit NavigationWatchingWebContentsObserver(
      WebContents* contents,
      base::OnceClosure closure_on_navigate)
      : WebContentsObserver(contents),
        closure_on_navigate_(std::move(closure_on_navigate)) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    std::move(closure_on_navigate_).Run();
  }

 private:
  base::OnceClosure closure_on_navigate_;
};

class MockMediaSessionPlayerObserver : public MediaSessionPlayerObserver {
 public:
  explicit MockMediaSessionPlayerObserver(RenderFrameHost* rfh)
      : render_frame_host_(rfh) {}

  ~MockMediaSessionPlayerObserver() override = default;

  void OnSuspend(int player_id) override {}
  void OnResume(int player_id) override {}
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override {}
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override {}
  void OnSeekTo(int player_id, base::TimeDelta seek_time) override {}
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override {
  }
  void OnEnterPictureInPicture(int player_id) override {}
  void OnSetAudioSinkId(int player_id,
                        const std::string& raw_device_id) override {}
  void OnSetMute(int player_id, bool mute) override {}
  void OnRequestMediaRemoting(int player_id) override {}
  void OnRequestVisibility(
      int player_id,
      RequestVisibilityCallback request_visibility_callback) override {}

  std::optional<media_session::MediaPosition> GetPosition(
      int player_id) const override {
    return std::nullopt;
  }

  bool IsPictureInPictureAvailable(int player_id) const override {
    return false;
  }

  bool HasSufficientlyVisibleVideo(int player_id) const override {
    return false;
  }

  bool HasAudio(int player_id) const override { return true; }
  bool HasVideo(int player_id) const override { return false; }
  bool IsPaused(int player_id) const override { return false; }

  std::string GetAudioOutputSinkId(int player_id) const override { return ""; }

  bool SupportsAudioOutputDeviceSwitching(int player_id) const override {
    return false;
  }

  media::MediaContentType GetMediaContentType() const override {
    return media::MediaContentType::kPersistent;
  }

  RenderFrameHost* render_frame_host() const override {
    return render_frame_host_;
  }

 private:
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;
};

void NavigateToURLAndWaitForFinish(Shell* window, const GURL& url) {
  base::RunLoop run_loop;
  NavigationWatchingWebContentsObserver observer(window->web_contents(),
                                                 run_loop.QuitClosure());

  EXPECT_TRUE(NavigateToURL(window, url));
  run_loop.Run();
}

char kSetUpMediaSessionScript[] =
    "navigator.mediaSession.playbackState = \"playing\";\n"
    "navigator.mediaSession.metadata = new MediaMetadata({ title: \"foo\" });";

char kSetUpWebRTCMediaSessionScript[] =
    "navigator.mediaSession.playbackState = \"playing\";\n"
    "navigator.mediaSession.metadata = new MediaMetadata({ title: \"foo\" });\n"
    "navigator.mediaSession.setMicrophoneActive(true);\n"
    "navigator.mediaSession.setCameraActive(true);\n"
    "navigator.mediaSession.setActionHandler(\"togglemicrophone\", _ => {});\n"
    "navigator.mediaSession.setActionHandler(\"togglecamera\", _ => {});\n"
    "navigator.mediaSession.setActionHandler(\"hangup\", _ => {});";

const int kPlayerId = 0;

}  // anonymous namespace

class MediaSessionServiceImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaSession");
  }

  void EnsurePlayer() {
    if (player_)
      return;

    player_ = std::make_unique<MockMediaSessionPlayerObserver>(
        shell()->web_contents()->GetPrimaryMainFrame());

    MediaSessionImpl::Get(shell()->web_contents())
        ->AddPlayer(player_.get(), kPlayerId);
  }

  MediaSessionImpl* GetSession() {
    return MediaSessionImpl::Get(shell()->web_contents());
  }

  MediaSessionServiceImpl* GetService() {
    RenderFrameHost* main_frame =
        shell()->web_contents()->GetPrimaryMainFrame();
    const auto main_frame_id = main_frame->GetGlobalId();
    if (GetSession()->services_.count(main_frame_id))
      return GetSession()->services_[main_frame_id];

    return nullptr;
  }

  void ExecuteScriptToSetUpMediaSessionSync() {
    ASSERT_TRUE(ExecJs(shell(), kSetUpMediaSessionScript));
    media_session::test::MockMediaSessionMojoObserver observer(*GetSession());

    std::set<media_session::mojom::MediaSessionAction> expected_actions;
    expected_actions.insert(media_session::mojom::MediaSessionAction::kPlay);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kPause);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kStop);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kSeekTo);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kScrubTo);
    expected_actions.insert(
        media_session::mojom::MediaSessionAction::kSeekForward);
    expected_actions.insert(
        media_session::mojom::MediaSessionAction::kSeekBackward);

    observer.WaitForExpectedActions(expected_actions);
  }

  void ExecuteScriptToSetUpWebRTCMediaSessionSync() {
    ASSERT_TRUE(ExecJs(shell(), kSetUpWebRTCMediaSessionScript));
    media_session::test::MockMediaSessionMojoObserver observer(*GetSession());

    std::set<media_session::mojom::MediaSessionAction> expected_actions;
    expected_actions.insert(media_session::mojom::MediaSessionAction::kPlay);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kPause);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kStop);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kSeekTo);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kScrubTo);
    expected_actions.insert(
        media_session::mojom::MediaSessionAction::kSeekForward);
    expected_actions.insert(
        media_session::mojom::MediaSessionAction::kSeekBackward);
    expected_actions.insert(
        media_session::mojom::MediaSessionAction::kToggleMicrophone);
    expected_actions.insert(
        media_session::mojom::MediaSessionAction::kToggleCamera);
    expected_actions.insert(media_session::mojom::MediaSessionAction::kHangUp);

    observer.WaitForExpectedActions(expected_actions);
  }

 private:
  std::unique_ptr<MockMediaSessionPlayerObserver> player_;
};

// Two windows from the same BrowserContext.
IN_PROC_BROWSER_TEST_F(MediaSessionServiceImplBrowserTest,
                       CrashMessageOnUnload) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("media/session", "embedder.html")));
  // Navigate to a chrome:// URL to avoid render process re-use.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("chrome://gpu")));
  // Should not crash.
}

// Tests for checking if the media session service members are correctly reset
// when navigating. Due to the mojo services have different message queues, it's
// hard to wait for the messages to arrive. Temporarily, the tests are using
// observers to wait for the message to be processed on the MediaSessionObserver
// side.

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
// crbug.com/927234.
#define MAYBE_ResetServiceWhenNavigatingAway \
  DISABLED_ResetServiceWhenNavigatingAway
#else
#define MAYBE_ResetServiceWhenNavigatingAway ResetServiceWhenNavigatingAway
#endif
IN_PROC_BROWSER_TEST_F(MediaSessionServiceImplBrowserTest,
                       MAYBE_ResetServiceWhenNavigatingAway) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "title1.html")));
  EnsurePlayer();

  ExecuteScriptToSetUpMediaSessionSync();

  EXPECT_EQ(blink::mojom::MediaSessionPlaybackState::PLAYING,
            GetService()->playback_state());
  EXPECT_TRUE(GetService()->metadata());
  EXPECT_EQ(0u, GetService()->actions().size());

  // Start a non-same-page navigation and check the playback state, metadata,
  // actions are reset.
  NavigateToURLAndWaitForFinish(shell(), GetTestUrl(".", "title2.html"));

  // The service should be destroyed.
  EXPECT_EQ(GetService(), nullptr);
}

// crbug.com/927234.
IN_PROC_BROWSER_TEST_F(MediaSessionServiceImplBrowserTest,
                       DISABLED_DontResetServiceForSameDocumentNavigation) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "title1.html")));
  EnsurePlayer();

  ExecuteScriptToSetUpMediaSessionSync();

  // Start a fragment navigation and check the playback state, metadata,
  // actions are not reset.
  GURL fragment_change_url = GetTestUrl(".", "title1.html");
  fragment_change_url = GURL(fragment_change_url.spec() + "#some-anchor");
  NavigateToURLAndWaitForFinish(shell(), fragment_change_url);

  EXPECT_EQ(blink::mojom::MediaSessionPlaybackState::PLAYING,
            GetService()->playback_state());
  EXPECT_TRUE(GetService()->metadata());
  EXPECT_EQ(0u, GetService()->actions().size());
}

IN_PROC_BROWSER_TEST_F(MediaSessionServiceImplBrowserTest,
                       MicrophoneAndCameraStatesInitiallyUnknown) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "title1.html")));
  EnsurePlayer();

  ExecuteScriptToSetUpMediaSessionSync();

  media_session::test::MockMediaSessionMojoObserver observer(*GetSession());
  observer.WaitForMicrophoneState(
      media_session::mojom::MicrophoneState::kUnknown);
  observer.WaitForCameraState(media_session::mojom::CameraState::kUnknown);
}

IN_PROC_BROWSER_TEST_F(MediaSessionServiceImplBrowserTest,
                       MicrophoneAndCameraStatesCanBeSet) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "title1.html")));
  EnsurePlayer();

  ExecuteScriptToSetUpWebRTCMediaSessionSync();

  media_session::test::MockMediaSessionMojoObserver observer(*GetSession());
  observer.WaitForMicrophoneState(
      media_session::mojom::MicrophoneState::kUnmuted);
  observer.WaitForCameraState(media_session::mojom::CameraState::kTurnedOn);
}

}  // namespace content
