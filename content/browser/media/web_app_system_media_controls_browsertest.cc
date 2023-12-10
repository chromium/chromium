// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread_win.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/browser/media/web_app_system_media_controls.h"
#include "content/browser/media/web_app_system_media_controls_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// This test suite tests playing media in a content window and verifies control
// via system media controls controls the expected window.
// As instanced system media controls is developed under
// kWebAppSystemMediaControlsWin this suite will expand to focus on testing
// instanced web app system media controls.

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
    media_keys_listener_manager_impl->SetTestObserver(this);

    WebAppSystemMediaControlsManager* web_app_system_media_controls_manager =
        media_keys_listener_manager_impl->web_app_system_media_controls_manager_
            .get();
    web_app_system_media_controls_manager->SetObserverForTesting(this);
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void StartPlaybackAndWait(Shell* shell, const std::string& id) {
    shell->web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(
            JsReplace("document.getElementById($1).play();", id)),
        base::NullCallback());
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

  // This mechanism allows us to wait for the browser to be added to
  // WebAppSystemMediaControls bookkeeping.
  void OnBrowserAdded() override {
    if (waiting_for_browser_added_) {
      waiting_for_browser_added_ = false;
      CHECK(browser_added_run_loop_);
      browser_added_run_loop_->Quit();
    }
  }

  void PrepareToWaitForBrowserAdded(base::RunLoop* run_loop) {
    browser_added_run_loop_ = run_loop;
    waiting_for_browser_added_ = true;
  }

  void WaitForBrowserAdded() {
    CHECK(browser_added_run_loop_);
    browser_added_run_loop_->Run();
  }

  // This mechanism allows us to wait for a web app to be added to the
  // WebAppSystemMediaControls bookkeeping.
  void OnWebAppAdded(base::UnguessableToken request_id) override {
    if (waiting_for_web_app_added_) {
      waiting_for_web_app_added_ = false;
      CHECK(web_app_added_run_loop_);
      web_app_request_id_ = request_id;
      web_app_added_run_loop_->Quit();
    }
  }

  void PrepareToWaitForWebAppAdded(base::RunLoop* run_loop) {
    web_app_added_run_loop_ = run_loop;
    waiting_for_web_app_added_ = true;
  }

  base::UnguessableToken WaitForWebAppAdded() {
    CHECK(web_app_added_run_loop_);
    web_app_added_run_loop_->Run();
    CHECK(web_app_request_id_ != base::UnguessableToken::Null());
    base::UnguessableToken cached_token = web_app_request_id_;
    web_app_request_id_ = base::UnguessableToken::Null();
    return cached_token;
  }

  // This mechanism allows us to wait for MediaKeysListenerImpl to be ready
  // to listen to keys.
  void OnStartWatchingMediaKey(bool is_pwa) override {
    if (!waiting_for_start_watching_media_key_) {
      return;
    }
    waiting_for_start_watching_media_key_ = false;
    CHECK(start_watching_media_key_run_loop_);
    start_watching_media_key_run_loop_->Quit();
    last_watch_was_for_pwa_ = is_pwa;
  }

  void PrepareToWaitForStartWatchingMediaKey(base::RunLoop* run_loop) {
    start_watching_media_key_run_loop_ = run_loop;
    waiting_for_start_watching_media_key_ = true;
    last_watch_was_for_pwa_ = absl::nullopt;
  }

  bool WaitForStartWatchingMediaKey() {
    CHECK(start_watching_media_key_run_loop_);
    start_watching_media_key_run_loop_->Run();
    EXPECT_TRUE(
        last_watch_was_for_pwa_);  // Check the value got set, optional resolves
                                   // to true if the value got set.
    return last_watch_was_for_pwa_.value();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    feature_list_.InitAndEnableFeature(features::kWebAppSystemMediaControlsWin);
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  bool waiting_for_web_app_added_ = false;
  raw_ptr<base::RunLoop> web_app_added_run_loop_ = nullptr;
  base::UnguessableToken web_app_request_id_ = base::UnguessableToken::Null();

  bool waiting_for_browser_added_ = false;
  raw_ptr<base::RunLoop> browser_added_run_loop_ = nullptr;

  bool waiting_for_start_watching_media_key_ = false;
  raw_ptr<base::RunLoop> start_watching_media_key_run_loop_ = nullptr;
  absl::optional<bool> last_watch_was_for_pwa_;

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       SimpleOneBrowserTest) {
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  base::RunLoop run_loop;
  PrepareToWaitForBrowserAdded(&run_loop);
  base::RunLoop watcher_run_loop;
  PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

  // Run javascript to play the video, and wait for it to begin playing.
  StartPlaybackAndWait(shell(), "long-video-loop");
  // Check video is playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Wait till the WebAppSystemMediaControlsManager adds the browser.
  WaitForBrowserAdded();

  // Hit pause via simulating SMTC pause.
  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();

  // Unfortunately, even though we wait for the browser to be added
  // the MediaKeysListenerManager can still not have the browser
  // registered properly. We have to wait for MKLM to also add it to it's
  // bookkeeping.
  bool is_for_pwa = WaitForStartWatchingMediaKey();
  EXPECT_FALSE(is_for_pwa);

  // Check video is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  media_keys_listener_manager_impl->OnPause(GetBrowserSystemMediaControls());

  // Check video is paused.
  WaitForStop(shell());
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest, ThreeBrowserTest) {
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));

  Shell* browser2 = CreateBrowser();
  Shell* browser3 = CreateBrowser();

  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  EXPECT_TRUE(NavigateToURL(browser2, http_url));
  EXPECT_TRUE(NavigateToURL(browser3, http_url));

  base::RunLoop run_loop;
  PrepareToWaitForBrowserAdded(&run_loop);
  base::RunLoop watcher_run_loop;
  PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

  // Press play and wait for each one to start.
  StartPlaybackAndWait(shell(), "long-video-loop");
  StartPlaybackAndWait(browser2, "long-video-loop");
  StartPlaybackAndWait(browser3, "long-video-loop");

  EXPECT_TRUE(IsPlaying(browser3, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(browser2, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Now we have 3 things playing at the same time.
  // Browser 3 should have control and be shown in SMTC.

  // Wait till the WebAppSystemMediaControlsManager adds the browser.
  WaitForBrowserAdded();

  // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
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
  // navigate two shells to the page.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  Shell* web_app = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(web_app, http_url));

  EXPECT_TRUE(NavigateToURL(shell(), http_url));
  EXPECT_TRUE(NavigateToURL(web_app, http_url));

  // Start two playbacks, but set the testing flag so that the second window
  // will register as a web app to WebAppSystemMediaControlsManager.
  {
    base::RunLoop watcher_run_loop;
    PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

    StartPlaybackAndWait(shell(), "long-video-loop");
    EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

    // We need to be careful here that this first play is completely done
    // before we set the flag to pretend subsequent plays are from apps.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_FALSE(is_for_pwa);

    EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));
  }

  SetAlwaysAssumeWebAppForTesting();

  base::RunLoop run_loop;
  PrepareToWaitForWebAppAdded(&run_loop);
  base::RunLoop watcher_run_loop;
  PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

  StartPlaybackAndWait(web_app, "long-video-loop");
  base::UnguessableToken request_id = WaitForWebAppAdded();

  EXPECT_TRUE(IsPlaying(web_app, "long-video-loop"));

  EXPECT_NE(request_id, base::UnguessableToken::Null());

  // Now retrieve the SMC and make a call to pause the
  // video.
  system_media_controls::SystemMediaControls* system_media_controls =
      GetSystemMediaControlsForWebApp(request_id);

  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();

  // Also wait for MediaKeysListenerManagerImpl to also start watching.
  bool is_for_pwa = WaitForStartWatchingMediaKey();
  EXPECT_TRUE(is_for_pwa);

  media_keys_listener_manager_impl->OnPause(system_media_controls);

  // the "web app" should be paused.
  WaitForStop(web_app);

  // the browser is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // now start the webapp again.
  media_keys_listener_manager_impl->OnPlay(system_media_controls);
  WaitForStart(web_app);

  // the browser is still playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));
}

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest, ThreeWebAppTest) {
  // navigate two shells to the page.
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  // We're mostly going to ignore this shell() based browser.

  Shell* web_app1 = CreateBrowser();
  Shell* web_app2 = CreateBrowser();
  Shell* web_app3 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(web_app1, http_url));
  EXPECT_TRUE(NavigateToURL(web_app2, http_url));
  EXPECT_TRUE(NavigateToURL(web_app3, http_url));

  // start all the playbacks.
  SetAlwaysAssumeWebAppForTesting();

  base::UnguessableToken web_app1_request_id;
  base::UnguessableToken web_app2_request_id;
  base::UnguessableToken web_app3_request_id;

  {
    base::RunLoop watcher_run_loop;
    PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

    base::RunLoop run_loop;
    PrepareToWaitForWebAppAdded(&run_loop);
    StartPlaybackAndWait(web_app1, "long-video-loop");
    web_app1_request_id = WaitForWebAppAdded();

    // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_TRUE(is_for_pwa);
  }

  {
    base::RunLoop watcher_run_loop;
    PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

    base::RunLoop run_loop;
    PrepareToWaitForWebAppAdded(&run_loop);
    StartPlaybackAndWait(web_app2, "long-video-loop");
    web_app2_request_id = WaitForWebAppAdded();

    // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_TRUE(is_for_pwa);
  }

  {
    base::RunLoop watcher_run_loop;
    PrepareToWaitForStartWatchingMediaKey(&watcher_run_loop);

    base::RunLoop run_loop;
    PrepareToWaitForWebAppAdded(&run_loop);
    StartPlaybackAndWait(web_app3, "long-video-loop");
    web_app3_request_id = WaitForWebAppAdded();

    // Also wait until MediaKeysListenerManagerImpl starts listening for keys.
    bool is_for_pwa = WaitForStartWatchingMediaKey();
    EXPECT_TRUE(is_for_pwa);
  }

  // all request ids should be valid
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

  // pause 3, only 1 remains.
  media_keys_listener_manager_impl->OnPause(web_app3_system_media_controls);
  WaitForStop(web_app3);

  EXPECT_TRUE(IsPlaying(web_app1, "long-video-loop"));

  // pause 1, only 1 remains.
  media_keys_listener_manager_impl->OnPause(web_app1_system_media_controls);
  WaitForStop(web_app1);
}

}  // namespace content
