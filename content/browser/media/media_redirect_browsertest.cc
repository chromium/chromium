// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace content {

// Tests that WebMediaPlayer implementations choose the right playback engine
// for content even when the playback extension is not known upfront.
class MediaRedirectTest : public MediaBrowserTest {
 public:
  void RunRedirectTest(const std::string& media_file) {
    std::unique_ptr<net::EmbeddedTestServer> http_test_server(
        new net::EmbeddedTestServer());
    http_test_server->ServeFilesFromSourceDirectory(media::GetTestDataPath());
    CHECK(http_test_server->InitializeAndListen());

    const GURL player_url =
        http_test_server->GetURL("/player.html?video=" + kHiddenPath);
    const GURL dest_url = http_test_server->GetURL("/" + media_file);

    http_test_server->RegisterRequestHandler(
        base::BindRepeating(&MediaRedirectTest::RedirectResponseHandler,
                            base::Unretained(this), dest_url));
    http_test_server->StartAcceptingConnections();

    // Run the normal media playback test.
    EXPECT_EQ(media::kEndedTitle, RunTest(player_url, media::kEndedTitle));
  }

  std::unique_ptr<net::test_server::HttpResponse> RedirectResponseHandler(
      const GURL& dest_url,
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/" + kHiddenPath,
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    http_response->AddCustomHeader("Location", dest_url.spec());
    return std::move(http_response);
  }

 private:
  const std::string kHiddenPath = "hidden_redirect";
};

IN_PROC_BROWSER_TEST_F(MediaRedirectTest, CanPlayHiddenWebm) {
  RunRedirectTest("bear.webm");
}

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
// Flaky, see http://crbug.com/624005
IN_PROC_BROWSER_TEST_F(MediaRedirectTest, DISABLED_CanPlayHiddenHls) {
  RunRedirectTest("bear.m3u8");
}
#endif

}  // namespace content
