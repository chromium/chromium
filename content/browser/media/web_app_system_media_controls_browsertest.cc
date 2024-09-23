// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/web_app_system_media_controls.h"

#include <optional>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread_win.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/web_app_system_media_controls_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// This test suite tests playing media in a content window and verifies control
// via system media controls controls the expected window. As instanced system
// media controls is developed under kWebAppSystemMediaControls.

// Currently, this test suite only runs on windows.
class WebAppSystemMediaControlsBrowserTest
    : public ContentBrowserTest,
      public WebAppSystemMediaControlsManagerObserver,
      public MediaKeysListenerManagerImplTestObserver {
 public:
  WebAppSystemMediaControlsBrowserTest() = default;

  WebAppSystemMediaControlsBrowserTest(
      const WebAppSystemMediaControlsBrowserTest&) = delete;
  WebAppSystemMediaControlsBrowserTest& operator=(
      const WebAppSystemMediaControlsBrowserTest&) = delete;

  ~WebAppSystemMediaControlsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Start an HTTPS server that will serve files in from "content/test/data".
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(https_server_->Start());

    // Also start listening to events from a few different classes.
    MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
        BrowserMainLoop::GetInstance()->media_keys_listener_manager();
    media_keys_listener_manager_impl->SetObserverForTesting(this);

    WebAppSystemMediaControlsManager* web_app_system_media_controls_manager =
        media_keys_listener_manager_impl->web_app_system_media_controls_manager_
            .get();
    web_app_system_media_controls_manager->SetObserverForTesting(this);

    // Tests may want to utilize this runloop to detect when browser has been
    // bookkeeping added. We need to create this runloop early enough so that
    // the runloop always wins the race between the waiter asking to "wait for
    // browser added" and the browser actually being added.
    browser_added_run_loop_.emplace();

    // Do a similar thing for the web app added run loop.
    web_app_added_run_loop_.emplace();

    // And finally a similar thing for the watching media key run loop.
    start_watching_media_key_run_loop_.emplace();
  }

  void TearDownOnMainThread() override {
    system_media_controls::SystemMediaControls::
        SetVisibilityChangedCallbackForTesting(nullptr);
    ContentBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void StartPlaybackAndWait(Shell* shell, const std::string& id) {
    shell->web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(
            JsReplace("document.getElementById($1).play();", id)),
        base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
    WaitForStart(shell);
  }

  void WaitForStart(Shell* shell) {
    MediaStartStopObserver observer(shell->web_contents(),
                                    MediaStartStopObserver::Type::kStart);
    observer.Wait();
  }

  void WaitForStop(Shell* shell) {
    MediaStartStopObserver observer(shell->web_contents(),
                                    MediaStartStopObserver::Type::kStop);
    observer.Wait();
  }

  bool IsPlaying(Shell* shell, const std::string& id) {
    return EvalJs(shell->web_contents(),
                  JsReplace("!document.getElementById($1).paused;", id))
        .ExtractBool();
  }

  WebAppSystemMediaControlsManager* GetWebAppSystemMediaControlsManager() {
    MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
        BrowserMainLoop::GetInstance()->media_keys_listener_manager();

    return media_keys_listener_manager_impl
        ->web_app_system_media_controls_manager_.get();
  }

  system_media_controls::SystemMediaControls* GetBrowserSystemMediaControls() {
    MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
        BrowserMainLoop::GetInstance()->media_keys_listener_manager();
    return media_keys_listener_manager_impl->browser_system_media_controls_
        .get();
  }

  system_media_controls::SystemMediaControls* GetSystemMediaControlsForWebApp(
      base::UnguessableToken request_id) {
    WebAppSystemMediaControls* web_app_system_media_controls =
        GetWebAppSystemMediaControlsManager()->GetControlsForRequestId(
            request_id);
    EXPECT_NE(web_app_system_media_controls, nullptr);

    system_media_controls::SystemMediaControls* system_media_controls =
        web_app_system_media_controls->GetSystemMediaControls();
    EXPECT_NE(system_media_controls, nullptr);

    return system_media_controls;
  }

  // This method asks the WebAppSystemMediaControlsManager to just assume
  // requests that come in come from a web app.
  void SetAlwaysAssumeWebAppForTesting() {
    GetWebAppSystemMediaControlsManager()->always_assume_web_app_for_testing_ =
        true;
  }

  void SetAlwaysIgnoreMediaSessionForTesting(WebContents* web_contents) {
    MediaSessionImpl* media_session = MediaSessionImpl::Get(web_contents);
    media_session->always_ignore_for_active_session_for_testing_ = true;
  }

  // This mechanism allows us to wait for a web app to be added to the
  // WebAppSystemMediaControls bookkeeping.
  void OnWebAppAdded(base::UnguessableToken request_id) override {
    last_web_app_request_id_ = request_id;
    web_app_added_run_loop_->Quit();
  }

  base::UnguessableToken WaitForWebAppAdded() {
    web_app_added_run_loop_->Run();
    EXPECT_FALSE(last_web_app_request_id_.is_empty());
    base::UnguessableToken cached_request_id = last_web_app_request_id_;
    last_web_app_request_id_ = base::UnguessableToken::Null();
    // Reset the runloop for the next use.
    web_app_added_run_loop_.emplace();
    return cached_request_id;
  }

  // This mechanism allows us to wait for MediaKeysListenerImpl to be ready
  // to listen to keys.
  void OnStartWatchingMediaKey(bool is_pwa) override {
    start_watching_media_key_run_loop_->Quit();
    last_watch_was_for_pwa_ = is_pwa;
  }

  // This function returns whether the last "watching key" event was for a PWA
  // or not.
  bool WaitForStartWatchingMediaKey() {
    start_watching_media_key_run_loop_->Run();
    EXPECT_TRUE(
        last_watch_was_for_pwa_);  // Check the value got set, optional resolves
                                   // to true if the value got set.
    bool cached_last_watch_was_for_pwa = last_watch_was_for_pwa_.value();
    last_watch_was_for_pwa_ = std::nullopt;
    // Reset the runloop for the next use.
    start_watching_media_key_run_loop_.emplace();
    return cached_last_watch_was_for_pwa;
  }

  // Waits for the visibility of the given WebApp's SystemMediaControls to match
  // `desired_visibility`. Returns true if the visibility has successfully
  // matched the expectation. Returns false if we time out before the state
  // changes.
  bool WaitForVisibility(const base::UnguessableToken& request_id,
                         bool desired_visibility) {
    // If the controls are already in the desired visibility state, early
    // return success.
    if (GetSystemMediaControlsForWebApp(request_id)
            ->GetVisibilityForTesting() == desired_visibility) {
      return true;
    }

    // Otherwise, wait for the visibility state to change.
    base::RunLoop wait_for_desired_visibility;
    auto visibility_changed_callback =
        base::BindLambdaForTesting([&](bool is_visible) {
          if (is_visible == desired_visibility) {
            wait_for_desired_visibility.Quit();
          }
        });

    system_media_controls::SystemMediaControls::
        SetVisibilityChangedCallbackForTesting(&visibility_changed_callback);
    wait_for_desired_visibility.Run();

    // Return true if the state is now correct.
    return GetSystemMediaControlsForWebApp(request_id)
               ->GetVisibilityForTesting() == desired_visibility;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    feature_list_.InitAndEnableFeature(features::kWebAppSystemMediaControls);
  }

 private:
  std::optional<base::RunLoop> web_app_added_run_loop_;
  base::UnguessableToken last_web_app_request_id_;

  std::optional<base::RunLoop> browser_added_run_loop_;

  std::optional<base::RunLoop> start_watching_media_key_run_loop_;
  std::optional<bool> last_watch_was_for_pwa_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       SimpleOneBrowserTest) {
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  // Run javascript to play the video, and wait for it to begin playing.
  StartPlaybackAndWait(shell(), "long-video-loop");
  // Check video is playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  bool is_for_pwa = WaitForStartWatchingMediaKey();
  EXPECT_FALSE(is_for_pwa);

  // Hit pause via simulating SMTC pause.
  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();

  // Check video is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  media_keys_listener_manager_impl->OnPause(GetBrowserSystemMediaControls());

  // Check video is paused.
  WaitForStop(shell());
}

// TODO: crbug.com/361543620 - Fix the test on Win11 arm64 debug platform.
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_ThreeBrowserTest DISABLED_ThreeBrowserTest
#else
#define MAYBE_ThreeBrowserTest ThreeBrowserTest
#endif
IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       MAYBE_ThreeBrowserTest) {
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));

  Shell* browser2 = CreateBrowser();
  Shell* browser3 = CreateBrowser();

  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  EXPECT_TRUE(NavigateToURL(browser2, http_url));
  EXPECT_TRUE(NavigateToURL(browser3, http_url));

  // Press play and wait for each one to start.
  StartPlaybackAndWait(shell(), "long-video-loop");
  StartPlaybackAndWait(browser2, "long-video-loop");
  StartPlaybackAndWait(browser3, "long-video-loop");

  EXPECT_TRUE(IsPlaying(browser3, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(browser2, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Now we have 3 things playing at the same time. Browser 3 should have
  // control and be shown in SMTC.

  // Wait until MediaKeysListenerManagerImpl starts listening for keys.
  bool is_for_pwa = WaitForStartWatchingMediaKey();
  EXPECT_FALSE(is_for_pwa);

  // Hit pause via simulating SMTC pause.
  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();
  media_keys_listener_manager_impl->OnPause(GetBrowserSystemMediaControls());

  // Check audio is paused for browser3.
  WaitForStop(browser3);

  // The other stuff should be continuing to loop.
  EXPECT_TRUE(IsPlaying(browser2, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       BrowserAndWebAppTest) {
  // Navigate two shells to the page.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  Shell* web_app = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(web_app, http_url));

  // Start two playbacks, but set the testing flags so that the second window
  // will register as a web app to WebAppSystemMediaControlsManager.
  {
    StartPlaybackAndWait(shell(), "long-video-loop");
    EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

    // We need to be careful here that this first play is completely done before
    // we set the flag to pretend subsequent plays are from apps.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_FALSE(is_for_pwa);

    EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));
  }

  SetAlwaysAssumeWebAppForTesting();
  SetAlwaysIgnoreMediaSessionForTesting(web_app->web_contents());

  // Start the playback in the web app, wait for state to be ready, then hit
  // pause to the web app.

  StartPlaybackAndWait(web_app, "long-video-loop");
  base::UnguessableToken request_id = WaitForWebAppAdded();

  EXPECT_TRUE(IsPlaying(web_app, "long-video-loop"));

  EXPECT_FALSE(request_id.is_empty());

  // Wait for MediaKeysListenerManagerImpl to also start watching.
  bool is_for_pwa = WaitForStartWatchingMediaKey();
  EXPECT_TRUE(is_for_pwa);

  // Now retrieve the SMC and make a call to pause the video.
  system_media_controls::SystemMediaControls* system_media_controls =
      GetSystemMediaControlsForWebApp(request_id);

  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();
  media_keys_listener_manager_impl->OnPause(system_media_controls);

  // The "web app" should be paused.
  WaitForStop(web_app);

  // The browser is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Now start the webapp again.
  media_keys_listener_manager_impl->OnPlay(system_media_controls);
  WaitForStart(web_app);

  // The browser is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest, ThreeWebAppTest) {
  // Navigate two shells to the page.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  // We're mostly going to ignore this shell() based browser.

  Shell* web_app1 = CreateBrowser();
  Shell* web_app2 = CreateBrowser();
  Shell* web_app3 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(web_app1, http_url));
  EXPECT_TRUE(NavigateToURL(web_app2, http_url));
  EXPECT_TRUE(NavigateToURL(web_app3, http_url));

  // Start all the playbacks.
  SetAlwaysAssumeWebAppForTesting();
  SetAlwaysIgnoreMediaSessionForTesting(web_app1->web_contents());
  SetAlwaysIgnoreMediaSessionForTesting(web_app2->web_contents());
  SetAlwaysIgnoreMediaSessionForTesting(web_app3->web_contents());

  base::UnguessableToken web_app1_request_id;
  base::UnguessableToken web_app2_request_id;
  base::UnguessableToken web_app3_request_id;

  {
    StartPlaybackAndWait(web_app1, "long-video-loop");
    web_app1_request_id = WaitForWebAppAdded();

    // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_TRUE(is_for_pwa);
  }

  {
    StartPlaybackAndWait(web_app2, "long-video-loop");
    web_app2_request_id = WaitForWebAppAdded();

    // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_TRUE(is_for_pwa);
  }

  {
    StartPlaybackAndWait(web_app3, "long-video-loop");
    web_app3_request_id = WaitForWebAppAdded();

    // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_TRUE(is_for_pwa);
  }

  // All request ids should be valid.
  EXPECT_NE(web_app1_request_id, base::UnguessableToken::Null());
  EXPECT_NE(web_app2_request_id, base::UnguessableToken::Null());
  EXPECT_NE(web_app3_request_id, base::UnguessableToken::Null());

  system_media_controls::SystemMediaControls* web_app1_system_media_controls =
      GetSystemMediaControlsForWebApp(web_app1_request_id);
  system_media_controls::SystemMediaControls* web_app2_system_media_controls =
      GetSystemMediaControlsForWebApp(web_app2_request_id);
  system_media_controls::SystemMediaControls* web_app3_system_media_controls =
      GetSystemMediaControlsForWebApp(web_app3_request_id);

  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();

  media_keys_listener_manager_impl->OnPause(web_app2_system_media_controls);
  WaitForStop(web_app2);

  // The other stuff should be continuing to loop.
  EXPECT_TRUE(IsPlaying(web_app1, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(web_app3, "long-video-loop"));

  // Pause 3, only 1 remains.
  media_keys_listener_manager_impl->OnPause(web_app3_system_media_controls);
  WaitForStop(web_app3);

  EXPECT_TRUE(IsPlaying(web_app1, "long-video-loop"));

  // Pause 1, only 1 remains.
  media_keys_listener_manager_impl->OnPause(web_app1_system_media_controls);
  WaitForStop(web_app1);
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest, TelemetryTest) {
  base::HistogramTester histogram_tester;
  Shell* web_app1 = CreateBrowser();
  Shell* web_app2 = CreateBrowser();

  // Navigate two shells to the page.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(web_app1, http_url));
  EXPECT_TRUE(NavigateToURL(web_app2, http_url));

  // Start playback.
  SetAlwaysAssumeWebAppForTesting();
  SetAlwaysIgnoreMediaSessionForTesting(web_app1->web_contents());
  SetAlwaysIgnoreMediaSessionForTesting(web_app2->web_contents());

  // Load a video from web_app1.
  StartPlaybackAndWait(web_app1, "long-video-loop");
  base::UnguessableToken web_app1_request_id = WaitForWebAppAdded();
  EXPECT_TRUE(web_app1_request_id);
  EXPECT_TRUE(WaitForStartWatchingMediaKey());
  system_media_controls::SystemMediaControls* web_app1_system_media_controls =
      GetSystemMediaControlsForWebApp(web_app1_request_id);

  // Load a video from web_app2.
  StartPlaybackAndWait(web_app2, "long-video-loop");
  base::UnguessableToken web_app2_request_id = WaitForWebAppAdded();
  EXPECT_TRUE(WaitForStartWatchingMediaKey());
  system_media_controls::SystemMediaControls* web_app2_system_media_controls =
      GetSystemMediaControlsForWebApp(web_app2_request_id);

  MediaKeysListenerManagerImpl* media_keys_listener_manager =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();

  // Simulate a bunch of actions from SystemMediaTransportControls center.
  {
    media_keys_listener_manager->OnPause(web_app1_system_media_controls);
    WaitForStop(web_app1);
    media_keys_listener_manager->OnPlay(web_app1_system_media_controls);
    WaitForStart(web_app1);
    media_keys_listener_manager->OnPause(web_app2_system_media_controls);
    WaitForStop(web_app2);
    media_keys_listener_manager->OnPlay(web_app2_system_media_controls);
    WaitForStart(web_app2);

    FetchHistogramsFromChildProcesses();

    // 4 starts hence 4 PwaPlayingMedia events.
    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaPlayingMedia, 4);

    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaSmcPlay, 2);

    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaSmcPause, 2);
  }

  // Now just simulate some of the other ones. We can't do next/prev because
  // those are disabled when there are no next or previous tracks. so just skip
  // those.
  {
    media_keys_listener_manager->OnPlayPause(web_app1_system_media_controls);
    media_keys_listener_manager->OnStop(web_app1_system_media_controls);
    media_keys_listener_manager->OnSeek(web_app1_system_media_controls,
                                        base::Seconds(2));
    media_keys_listener_manager->OnSeekTo(web_app1_system_media_controls,
                                          base::Seconds(2));

    FetchHistogramsFromChildProcesses();

    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaSmcPlayPause, 1);

    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaSmcStop, 1);

    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaSmcSeek, 1);

    histogram_tester.ExpectBucketCount(
        "WebApp.Media.SystemMediaControls",
        WebAppSystemMediaControlsEvent::kPwaSmcSeekTo, 1);
  }
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest, TwoBrowserTest) {
  // Navigate two shells to the page.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  Shell* browser2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(browser2, http_url));

  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  EXPECT_TRUE(NavigateToURL(browser2, http_url));

  // Start two playbacks, both from the browser.
  {
    StartPlaybackAndWait(shell(), "long-video-loop");
    EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

    // Wait for the playing audio to register in the media keys listener
    // manager.
    WaitForStartWatchingMediaKey();

    StartPlaybackAndWait(browser2, "long-video-loop");
    EXPECT_TRUE(IsPlaying(browser2, "long-video-loop"));

    WaitForStartWatchingMediaKey();
  }

  // Now retrieve the SMC used for the browser and make a call to pause the
  // video.
  system_media_controls::SystemMediaControls* system_media_controls =
      GetBrowserSystemMediaControls();

  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();

  media_keys_listener_manager_impl->OnPause(system_media_controls);

  // The browser2 should be paused.
  WaitForStop(browser2);

  // The shell is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Close browser2, this will cause the controlled browser to fall back to
  // shell.
  browser2->Close();

  // Wait a little for the fallback to occur.
  WaitForStartWatchingMediaKey();

  // Now hit pause again, it should pause shell().
  media_keys_listener_manager_impl->OnPause(system_media_controls);
  WaitForStop(shell());

  // The browser is still playing.
  EXPECT_FALSE(IsPlaying(shell(), "long-video-loop"));
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       SMTCHiddenOnNavigationAway) {
  // Set up a media session in 1 PWA.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  Shell* web_app = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(web_app, http_url));
  SetAlwaysAssumeWebAppForTesting();
  SetAlwaysIgnoreMediaSessionForTesting(web_app->web_contents());

  // Start the media session and wait for the controls to become visible.
  StartPlaybackAndWait(web_app, "long-video-loop");
  base::UnguessableToken request_id = WaitForWebAppAdded();
  EXPECT_TRUE(WaitForVisibility(request_id, /*desired_visibility=*/true));

  // Check the pwa is still playing, and navigate away to a different url.
  EXPECT_TRUE(IsPlaying(web_app, "long-video-loop"));
  GURL http_url2(https_server()->GetURL("/media/session/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_app, http_url2));

  // The controls should hide now.
  EXPECT_TRUE(WaitForVisibility(request_id, /*desired_visibility=*/false));
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       SMTCHiddenOnAudioEnd) {
  // Set up a media session in 1 PWA.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  Shell* web_app = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(web_app, http_url));
  SetAlwaysAssumeWebAppForTesting();
  SetAlwaysIgnoreMediaSessionForTesting(web_app->web_contents());

  // Start the media session and wait for the controls to become visible.
  StartPlaybackAndWait(web_app, "short-video");
  base::UnguessableToken request_id = WaitForWebAppAdded();
  EXPECT_TRUE(WaitForVisibility(request_id, /*desired_visibility=*/true));

  // Wait for the audio track to end on its own.
  WaitForStop(web_app);
  EXPECT_FALSE(IsPlaying(web_app, "short-video"));

  // The controls should hide now.
  EXPECT_TRUE(WaitForVisibility(request_id, /*desired_visibility=*/false));
}

}  // namespace content
