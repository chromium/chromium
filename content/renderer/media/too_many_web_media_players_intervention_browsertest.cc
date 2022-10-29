// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/renderer/media/media_factory.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class TooManyWebMediaPlayersIntervention : public content::ContentBrowserTest {
 public:
  using Super = content::ContentBrowserTest;

  TooManyWebMediaPlayersIntervention() = default;
  ~TooManyWebMediaPlayersIntervention() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    Super::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kMaxWebMediaPlayerCount, "1");
  }

  void SetUpOnMainThread() override {
    Super::SetUpOnMainThread();
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("media"));
    ASSERT_TRUE(base::PathExists(test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(TooManyWebMediaPlayersIntervention,
                       WebMediaPlayerCreationFails) {
  const GURL url(embedded_test_server()->GetURL(
      "a.com", "/too_many_web_media_players_intervention_test.html"));
  auto* web_contents = shell()->web_contents();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  // In the test environment media players are capped at one. See
  // SetUpCommandLine above.
  EXPECT_TRUE(content::ExecJs(web_contents, "CreateVideo();"));

  // Create one player too many and expect everything to explode.
  content::WebContentsConsoleObserver console(web_contents);
  console.SetPattern("Blocked attempt to create a WebMediaPlayer *");
  EXPECT_TRUE(content::ExecJs(web_contents, "CreateVideo();"));
  ASSERT_TRUE(console.Wait());
  EXPECT_EQ(1u, console.messages().size());
}

}  // namespace media
