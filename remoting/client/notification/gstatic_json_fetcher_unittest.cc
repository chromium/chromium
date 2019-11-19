// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/gstatic_json_fetcher.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
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
  if (!arg || !arg->is_dict()) {
    return false;
  }
  const base::Value* a = arg->FindKey("a");
  return a && a->is_int() && a->GetInt() == 1;
}

MATCHER(IsJsonData2, "") {
  if (!arg || !arg->is_list()) {
    return false;
  }
  auto list = arg->GetList();
  return list.size() == 1 && list[0].is_string() &&
         list[0].GetString() == "123";
}

}  // namespace

class GstaticJsonFetcherTest : public testing::Test {
 protected:
  void SetFakeOkResponse(const std::string& relative_path,
                         const std::string& data) {
    url_fetcher_factory_.SetFakeResponse(
        GstaticJsonFetcher::GetFullUrl(relative_path), data, net::HTTP_OK,
        net::URLRequestStatus::SUCCESS);
  }
  void SetFakeFailedResponse(const std::string& relative_path,
                             net::HttpStatusCode status_code) {
    url_fetcher_factory_.SetFakeResponse(
        GstaticJsonFetcher::GetFullUrl(relative_path), "", status_code,
        net::URLRequestStatus::FAILED);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::FakeURLFetcherFactory url_fetcher_factory_{nullptr};
  GstaticJsonFetcher fetcher_{base::ThreadTaskRunnerHandle::Get()};
};

TEST_F(GstaticJsonFetcherTest, FetchJsonFileSuccess) {
  SetFakeOkResponse(kTestPath1, kTestJsonData1);
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(IsJsonData1()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

TEST_F(GstaticJsonFetcherTest, FetchTwoJsonFilesInParallel) {
  SetFakeOkResponse(kTestPath1, kTestJsonData1);
  SetFakeOkResponse(kTestPath2, kTestJsonData2);

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

TEST_F(GstaticJsonFetcherTest, FetchJsonFileNotOk) {
  SetFakeFailedResponse(kTestPath1, net::HTTP_INTERNAL_SERVER_ERROR);
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(NoJsonData()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

TEST_F(GstaticJsonFetcherTest, FetchJsonFileMalformed) {
  SetFakeOkResponse(kTestPath1, "Malformed!");
  base::MockCallback<JsonFetcher::FetchJsonFileCallback> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(NoJsonData()))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  fetcher_.FetchJsonFile(kTestPath1, callback.Get(),
                         TRAFFIC_ANNOTATION_FOR_TESTS);
  run_loop.Run();
}

}  // namespace remoting