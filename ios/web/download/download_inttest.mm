// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/bind.h"
#import "base/run_loop.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/test/download_task_test_util.h"
#import "ios/web/public/test/fakes/fake_download_controller_delegate.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_client.h"
#import "net/http/http_request_headers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {

namespace {

const char kContentDisposition[] = "attachment; filename=download.test";
const char kMimeType[] = "application/vnd.test";
const char kContent[] = "testdata";

// Returns HTTP response which causes WebState to start the download.
std::unique_ptr<net::test_server::HttpResponse> GetDownloadResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();

  std::string user_agent =
      request.headers.at(net::HttpRequestHeaders::kUserAgent);
  if (user_agent == GetWebClient()->GetUserAgent(UserAgentType::MOBILE)) {
    result->set_code(net::HTTP_OK);
    result->set_content(kContent);
    result->AddCustomHeader("Content-Type", kMimeType);
    result->AddCustomHeader("Content-Disposition", kContentDisposition);
  }
  return result;
}

}  // namespace

// Test fixture for DownloadController, DownloadControllerDelegate and
// DownloadTask integration tests.
class DownloadTest : public WebTestWithWebState {
 protected:
  DownloadTest() = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();
    delegate_ =
        std::make_unique<FakeDownloadControllerDelegate>(download_controller());
    server_.RegisterRequestHandler(base::BindRepeating(&GetDownloadResponse));
  }

  DownloadController* download_controller() {
    return DownloadController::FromBrowserState(GetBrowserState());
  }

 protected:
  net::EmbeddedTestServer server_;
  std::unique_ptr<FakeDownloadControllerDelegate> delegate_;
};

// Tests sucessfull download flow.
TEST_F(DownloadTest, SuccessfulDownload) {
  // Load download URL.
  ASSERT_TRUE(server_.Start());
  GURL url(server_.GetURL("/"));
  test::LoadUrl(web_state(), url);

  // Wait until download task is created.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    return !delegate_->alive_download_tasks().empty();
  }));
  ASSERT_EQ(1U, delegate_->alive_download_tasks().size());

  // Verify the initial state of the download task.
  DownloadTask* task = delegate_->alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIdentifier());
  EXPECT_EQ(url, task->GetOriginalUrl());
  EXPECT_FALSE(task->IsDone());
  EXPECT_EQ(0, task->GetErrorCode());
  EXPECT_EQ(static_cast<int64_t>(strlen(kContent)), task->GetTotalBytes());
  EXPECT_EQ(-1, task->GetPercentComplete());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ(kMimeType, task->GetMimeType());
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("download.test")),
            task->GenerateFileName());

  // Start the download task and wait for completion.
  {
    web::test::WaitDownloadTaskDone observer(task);
    task->Start(base::FilePath());
    observer.Wait();
  }

  // Verify the completed state of the download task.
  EXPECT_EQ(0, task->GetErrorCode());
  EXPECT_EQ(static_cast<int64_t>(strlen(kContent)), task->GetTotalBytes());
  EXPECT_EQ(100, task->GetPercentComplete());
  EXPECT_EQ(200, task->GetHttpCode());
  EXPECT_NSEQ(@(kContent),
              [[NSString alloc]
                  initWithData:web::test::GetDownloadTaskResponseData(task)
                      encoding:NSUTF8StringEncoding]);
}

}  // namespace web
