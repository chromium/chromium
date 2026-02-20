// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/url_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

class URLFetcherTest : public TestWithTaskEnvironment {
 protected:
  URLFetcherTest() : context_(CreateTestURLRequestContextBuilder()->Build()) {
    URLRequestFailedJob::AddUrlHandler();
  }

  ~URLFetcherTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  URLRequestContext* context() { return context_.get(); }

 private:
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(URLFetcherTest, BasicSuccess) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        response->set_code(HTTP_OK);
        response->set_content("test data");
        return response;
      }));
  ASSERT_TRUE(server.Start());

  auto fetcher =
      std::make_unique<URLFetcher>(context(), server.GetURL("/"), std::nullopt);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "test data");
}

TEST_F(URLFetcherTest, AsyncErrorOnRead) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_ASYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(context(), url, std::nullopt);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

TEST_F(URLFetcherTest, SyncErrorOnRead) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_SYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(context(), url, std::nullopt);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

TEST_F(URLFetcherTest, Non2xxResponse) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        response->set_code(HTTP_NOT_FOUND);
        response->set_content("not found");
        return response;
      }));
  ASSERT_TRUE(server.Start());

  auto fetcher =
      std::make_unique<URLFetcher>(context(), server.GetURL("/"), std::nullopt);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "not found");
}

TEST_F(URLFetcherTest, FollowRedirect) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        if (request.relative_url == "/redirect") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_FOUND);
          response->AddCustomHeader("Location", "/target");
          return response;
        } else if (request.relative_url == "/target") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_OK);
          response->set_content("target data");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(server.Start());

  auto fetcher = std::make_unique<URLFetcher>(
      context(), server.GetURL("/redirect"), std::nullopt);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "target data");
}

TEST_F(URLFetcherTest, ImmediateErrorInOnResponseStarted) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_SYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(context(), url, std::nullopt);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

}  // namespace

}  // namespace net::device_bound_sessions
