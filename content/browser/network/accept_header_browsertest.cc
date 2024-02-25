// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/media_buildflags.h"
#include "net/test/embedded_test_server/http_request.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/test/ppapi/ppapi_test.h"
#endif

namespace content {
namespace {

class AcceptHeaderTest : public ContentBrowserTest {
 public:
  AcceptHeaderTest() {}

  AcceptHeaderTest(const AcceptHeaderTest&) = delete;
  AcceptHeaderTest& operator=(const AcceptHeaderTest&) = delete;

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &AcceptHeaderTest::Monitor, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string GetFor(const std::string& path) {
    {
      base::AutoLock auto_lock(map_lock_);
      auto it = url_accept_header_.find(path);
      if (it != url_accept_header_.end())
        return it->second;
    }

    {
      base::AutoLock auto_lock(waiting_lock_);
      waiting_for_path_ = path;
      waiting_run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
    }

    waiting_run_loop_->Run();

    {
      base::AutoLock auto_lock(waiting_lock_);
      waiting_for_path_ = std::string();
      waiting_run_loop_.reset();
    }

    auto it = url_accept_header_.find(path);
    return it->second;
  }

  std::string GetOptionalImageCodecs() const {
    std::string result;
#if BUILDFLAG(ENABLE_AV1_DECODER)
    result.append("image/avif,");
#endif
    return result;
  }

 private:
  void Monitor(const net::test_server::HttpRequest& request) {
    auto it = request.headers.find("Accept");
    if (it == request.headers.end())
      return;

    {
      base::AutoLock auto_lock(map_lock_);
      url_accept_header_[request.relative_url] = it->second;
    }

    {
      base::AutoLock auto_lock(waiting_lock_);
      if (request.relative_url == waiting_for_path_)
        waiting_run_loop_->Quit();
    }
  }

  base::Lock map_lock_;
  std::map<std::string, std::string> url_accept_header_;

  base::Lock waiting_lock_;
  std::unique_ptr<base::RunLoop> waiting_run_loop_;
  std::string waiting_for_path_;
};

IN_PROC_BROWSER_TEST_F(AcceptHeaderTest, Check) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/accept-header.html")));

  // ResourceType::kMainFrame
  std::string expected_main_frame_accept_header =
      "text/html,application/xhtml+xml,application/xml;q=0.9," +
      GetOptionalImageCodecs() +
      "image/webp,image/apng,*/*;q=0.8,"
      "application/signed-exchange;v=b3;q=0.7";
  EXPECT_EQ(expected_main_frame_accept_header, GetFor("/accept-header.html"));

  // ResourceType::kSubFrame
  std::string expected_sub_frame_accept_header =
      "text/html,application/xhtml+xml,application/xml;q=0.9," +
      GetOptionalImageCodecs() +
      "image/webp,image/apng,*/*;q=0.8,"
      "application/signed-exchange;v=b3;q=0.7";
  EXPECT_EQ(expected_sub_frame_accept_header, GetFor("/iframe.html"));

  // ResourceType::kStylesheet
  EXPECT_EQ("text/css,*/*;q=0.1", GetFor("/test.css"));
  EXPECT_EQ("text/css,*/*;q=0.1", GetFor("/test-css-module"));

  // ResourceType::kJson
  EXPECT_EQ("application/json,*/*;q=0.5", GetFor("/test-json-module"));

  // ResourceType::kScript
  EXPECT_EQ("*/*", GetFor("/test.js"));

  // ResourceType::kImage
  std::string expected_image_accept_header =
      GetOptionalImageCodecs() +
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  EXPECT_EQ(expected_image_accept_header, GetFor("/image.gif"));

  // ResourceType::kFontResource
  EXPECT_EQ("*/*", GetFor("/test.js"));

  // ResourceType::kMedia
  EXPECT_EQ("*/*", GetFor("/media.mp4"));

  // ResourceType::kWorker
  EXPECT_EQ("*/*", GetFor("/worker.js"));

// Shared workers aren't implemented on Android.
// https://bugs.chromium.org/p/chromium/issues/detail?id=154571
#if !BUILDFLAG(IS_ANDROID)
  // ResourceType::kSharedWorker
  EXPECT_EQ("*/*", GetFor("/shared_worker.js"));
#endif

  // ResourceType::kPrefetch
  EXPECT_EQ(expected_main_frame_accept_header, GetFor("/prefetch"));

  // ResourceType::kXhr
  EXPECT_EQ("*/*", GetFor("/xhr"));

  // ResourceType::kPing
  EXPECT_EQ("*/*", GetFor("/ping"));

  // ResourceType::kServiceWorker
  EXPECT_EQ("*/*", GetFor("/service_worker.js"));

  // ResourceType::kCspReport
  EXPECT_EQ("*/*", GetFor("/csp"));

  // Ensure that if an Accept header is already set, it is not overwritten.
  EXPECT_EQ("custom/type", GetFor("/xhr_with_accept_header"));

  shell()->web_contents()->GetPrimaryPage().GetManifest(base::DoNothing());

  // ResourceType::kSubResource
  EXPECT_EQ("*/*", GetFor("/manifest"));

  // ResourceType::kObject and ResourceType::kFavicon are tested in src/chrome's
  // ChromeAcceptHeaderTest.ObjectAndFavicon.
}

#if BUILDFLAG(ENABLE_PPAPI)
// Checks Accept header for ResourceType::kPluginResource.
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, PluginAcceptHeader) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTP);
  server.ServeFilesFromSourceDirectory("ppapi/tests");
  base::Lock plugin_accept_header_lock;
  std::string plugin_accept_header;
  server.RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        // Note this callback runs on the EmbeddedTestServer's background
        // thread.
        base::AutoLock lock(plugin_accept_header_lock);
        if (request.relative_url == "/test_url_loader_data/hello.txt") {
          auto it = request.headers.find("Accept");
          if (it != request.headers.end())
            plugin_accept_header = it->second;
        }
      }));
  ASSERT_TRUE(server.Start());

  RunTestURL(
      server.GetURL(BuildQuery("/test_case.html?", "URLLoader_BasicGET")));

  {
    base::AutoLock lock(plugin_accept_header_lock);
    ASSERT_EQ("*/*", plugin_accept_header);
  }

  // Since the server uses local variables.
  ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
}
#endif  // BUILDFLAG(ENABLE_PPAPI)

}  //  namespace
}  //  namespace content
