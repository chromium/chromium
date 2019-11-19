// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

class WaitForMediaPlaying : public WebContentsObserver {
 public:
  WaitForMediaPlaying(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver override.
  void MediaStartedPlaying(const MediaPlayerInfo&, const MediaPlayerId&) final {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForMediaPlaying);
};

}  // namespace

class MediaAutoplayTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Test that playing a player from an IPC does not lead to a crash the renderer.
// This is a regression test from a crash from autoplay_initiated_ not being set
// in the Blink's AutoplayPolicy.
IN_PROC_BROWSER_TEST_F(MediaAutoplayTest, Crash_AutoplayInitiated) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/video-player.html")));

  WaitForMediaPlaying wait_for_media_playing(shell()->web_contents());

  RenderFrameHost* main_frame = shell()->web_contents()->GetMainFrame();
  main_frame->Send(
      new MediaPlayerDelegateMsg_Play(main_frame->GetRoutingID(), 1));

  wait_for_media_playing.Wait();
}

}  // namespace content
