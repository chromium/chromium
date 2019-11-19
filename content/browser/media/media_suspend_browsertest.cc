// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/media/media_browsertest.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/test_data_util.h"

namespace content {

static void SuspendAllMediaPlayersInRenderFrame(RenderFrameHost* rfh) {
  rfh->Send(
      new MediaPlayerDelegateMsg_SuspendAllMediaPlayers(rfh->GetRoutingID()));
}

// This browser test ensures the force suspend IPC messages are working properly
// and that players suspended in this way can be resumed. Note: This does not
// test suspend in various ready states; those tests are handled by web tests
// for ease of writing and ready state manipulation.
class MediaSuspendTest : public MediaBrowserTest {
 public:
  void RunSuspendTest(const std::string& load_until) {
    base::StringPairs query_params;
    query_params.emplace_back("event", load_until);

    GURL gurl = GetFileUrlWithQuery(
        media::GetTestDataFilePath("media_suspend_test.html"),
        media::GetURLQueryString(query_params));

    const base::string16 kError = base::ASCIIToUTF16(media::kError);

    {
      VLOG(0) << "Waiting for test URL: " << gurl << ", to load.";
      const base::string16 kLoaded = base::ASCIIToUTF16("LOADED");
      TitleWatcher title_watcher(shell()->web_contents(), kLoaded);
      title_watcher.AlsoWaitForTitle(kError);
      EXPECT_TRUE(NavigateToURL(shell(), gurl));
      ASSERT_EQ(kLoaded, title_watcher.WaitAndGetTitle());
    }

    {
      VLOG(0) << "Suspending and waiting for suspend to occur.";
      const base::string16 kSuspended = base::ASCIIToUTF16("SUSPENDED");
      TitleWatcher title_watcher(shell()->web_contents(), kSuspended);
      title_watcher.AlsoWaitForTitle(kError);
      shell()->web_contents()->ForEachFrame(
          base::BindRepeating(&SuspendAllMediaPlayersInRenderFrame));
      ASSERT_EQ(kSuspended, title_watcher.WaitAndGetTitle());
    }

    // Once a forced suspend is issued, the frame must be shown again to allow
    // players to emerge from the suspended state.
    shell()->web_contents()->WasHidden();
    shell()->web_contents()->WasShown();

    {
      VLOG(0) << "Waiting for playback to resume.";
      const base::string16 kEnded = base::ASCIIToUTF16(media::kEnded);
      TitleWatcher title_watcher(shell()->web_contents(), kEnded);
      title_watcher.AlsoWaitForTitle(kError);
      ASSERT_TRUE(ExecuteScript(shell(), "video.play();"));
      ASSERT_EQ(kEnded, title_watcher.WaitAndGetTitle());
    }

    CleanupTest();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }
};

IN_PROC_BROWSER_TEST_F(MediaSuspendTest, ForcedSrcSuspend) {
  RunSuspendTest("canplaythrough");
}

// TODO(dalecurtis): Add an MSE variant of this test.

}  // namespace content
