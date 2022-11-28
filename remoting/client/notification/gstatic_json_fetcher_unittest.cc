// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/gstatic_json_fetcher.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::Return;

constexpr char kTestPath1[] = "test_1.json";
constexpr char kTestPath2[] = "test_2.json";
constexpr char kTestJsonData1[] = R"({"a": 1})";
constexpr char kTestJsonData2[] = R"(["123"])";

MATCHER(NoJsonData, "") {
  return !arg;
}

MATCHER(IsJsonData1, "") {
  if (!arg) {
    return false;
  }
  return *arg == base::test::ParseJson(R"({"a":1})");
}

MATCHER(IsJsonData2, "") {
  if (!arg) {
    return false;
  }
  return *arg == base::test::ParseJson(R"(["123"])");
}

class TestURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  TestURLRequestInterceptor(const std::string& headers,
                            const std::string& response)
      : headers_(headers), response_(response) {}

  ~TestURLRequestInterceptor() override = default;

  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    return std::make_unique<net::URLRequestTestJob>(
        request, headers_, response_, /*auto_advance=*/true);
  }

 private:
  const std::string headers_;
  const std::string response_;
};

// Creates URLRequestJobs that fail at the specified phase.
class TestFailingURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  TestFailingURLRequestInterceptor() = default;

  ~TestFailingURLRequestInterceptor() override = default;

  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    return std::make_unique<net::URLRequestErrorJob>(request, net::ERR_FAILED);
  }
};

}  // namespace

class GstaticJsonFetcherTest : public testing::Test {
 protected:
  ~GstaticJsonFetcherTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void SetFakeResponse(const std::string& relative_path,
                       const std::string& headers,
                       const std::string& data) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        GstaticJsonFetcher::GetFullUrl(relative_path),
        std::make_unique<TestURLRequestInterceptor>(headers, data));
  }
  void SetFakeFailedResponse(const std::string& relative_path) {
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        GstaticJsonFetcher::GetFullUrl(relative_path),
        std::make_unique<TestFailingURLRequestInterceptor>());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  GstaticJsonFetcher fetcher_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};
};

TEST_F(GstaticJsonFetcherTest, FetchJsonFileSuccess) {
  SetFakeResponse(kTestPath1, net::URLRequestTestJob::test_headers(),
                  kTestJsonData1);
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(IsJsonData1()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

TEST_F(GstaticJsonFetcherTest, FetchTwoJsonFilesInParallel) {
  SetFakeResponse(kTestPath1, net::URLRequestTestJob::test_headers(),
                  kTestJsonData1);
  SetFakeResponse(kTestPath2, net::URLRequestTestJob::test_headers(),
                  kTestJsonData2);

  base::RunLoop run_loop;
  base::MockRepeatingClosure quit_on_second_run;
  EXPECT_CALL(quit_on_second_run, Run())
      .WillOnce(Return())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback1;
  EXPECT_CALL(callback1, Run(IsJsonData1()))
      .WillOnce(base::test::RunClosure(quit_on_second_run.Get()));
  fetcher_.FetchJsonFile(kTestPath1, callback1.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);

  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback2;
  EXPECT_CALL(callback2, Run(IsJsonData2()))
      .WillOnce(base::test::RunClosure(quit_on_second_run.Get()));
  fetcher_.FetchJsonFile(kTestPath2, callback2.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);

  run_loop.Run();
}

TEST_F(GstaticJsonFetcherTest, FetchJsonFileStatusNotOk) {
  SetFakeResponse(kTestPath1,
                  "HTTP/1.1 500 Internal Error\n"
                  "Content-type: text/html\n"
                  "\n",
                  kTestJsonData1);
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(NoJsonData()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

TEST_F(GstaticJsonFetcherTest, FetchJsonFileNetworkError) {
  SetFakeFailedResponse(kTestPath1);
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(NoJsonData()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

TEST_F(GstaticJsonFetcherTest, FetchJsonFileMalformed) {
  SetFakeResponse(kTestPath1, net::URLRequestTestJob::test_headers(),
                  "Malformed!");
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(NoJsonData()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

}  // namespace remoting
