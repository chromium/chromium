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
      public system_media_controls::SystemMediaControlsObserver {
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
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  // SystemMediaControlsObserver
  void OnServiceReady() override {
    if (waiting_for_service_ready_) {
      ASSERT_NE(service_ready_run_loop_, nullptr);
      waiting_for_service_ready_ = false;
      service_ready_run_loop_->Quit();
    }
  }

  // After media plays, there can be an arbitrary delay before system media
  // controls is ready to receive control requests. This function allows a
  // mechanism to wait until the system media controls fires OnServiceReady
  // before attempting to continue with the test.
  void PrepareWaitForOnServiceReady(base::RunLoop* run_loop) {
    waiting_for_service_ready_ = true;
    service_ready_run_loop_ = run_loop;
  }

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

  system_media_controls::SystemMediaControls* GetBrowserSystemMediaControls() {
    MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
        BrowserMainLoop::GetInstance()->media_keys_listener_manager();
    return media_keys_listener_manager_impl->system_media_controls_.get();
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
  bool waiting_for_service_ready_ = false;
  raw_ptr<base::RunLoop> service_ready_run_loop_ = nullptr;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppSystemMediaControlsBrowserTest,
                       SimpleOneBrowserTest) {
  GURL http_url(https_server()->GetURL("/media/session/media-session.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  base::RunLoop run_loop;
  PrepareWaitForOnServiceReady(&run_loop);

  // Run javascript to play the video, and wait for it to begin playing.
  StartPlaybackAndWait(shell(), "long-video-loop");
  // Check video is playing.
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Occasionally, the browser system media controls can not be ready.
  // If that happens, just retry a few more times.
  int max_retries = 3;
  for (int retry = 0; retry < max_retries; retry++) {
    if (GetBrowserSystemMediaControls() != nullptr) {
      break;
    }

    // Check the video continues to play.
    EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));
  }

  EXPECT_NE(GetBrowserSystemMediaControls(), nullptr);

  GetBrowserSystemMediaControls()->AddObserver(this);
  // Wait till the System Media Controls are ready to be used.
  run_loop.Run();

  // Hit pause via simulating SMTC pause.
  MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
      BrowserMainLoop::GetInstance()->media_keys_listener_manager();
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

  // Press play and wait for each one to start.
  StartPlaybackAndWait(shell(), "long-video-loop");
  StartPlaybackAndWait(browser2, "long-video-loop");
  StartPlaybackAndWait(browser3, "long-video-loop");

  EXPECT_TRUE(IsPlaying(browser3, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(browser2, "long-video-loop"));
  EXPECT_TRUE(IsPlaying(shell(), "long-video-loop"));

  // Now we have 3 things playing at the same time.
  // Browser 3 should have control and be shown in SMTC.

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

}  // namespace content
