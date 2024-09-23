// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/tasks/tasks_api_requests.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "google_apis/tasks/tasks_api_request_types.h"
#include "google_apis/tasks/tasks_api_response_types.h"
#include "google_apis/tasks/tasks_api_task_status.h"
#include "google_apis/tasks/tasks_api_url_generator_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::tasks {
namespace {

constexpr char kTaskListId[] = "random-task-list-id";
constexpr char kTaskId[] = "random-task-id";

}  // namespace

class TasksApiRequestsTest : public testing::Test {
 public:
  TasksApiRequestsTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true)) {}

  void SetUp() override {
    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &TasksApiRequestsTest::HandleDataFileRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "tasks_api_origin_url",
        test_server_.base_url().spec());
    ASSERT_EQ(GaiaUrls::GetInstance()->tasks_api_origin_url(),
              test_server_.base_url().spec());
  }

  RequestSender* request_sender() { return request_sender_.get(); }
  net::test_server::HttpRequest last_request() { return last_request_; }
  void set_test_file_path(const std::string& test_file_path) {
    test_file_path_ = test_file_path;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleDataFileRequest(
      const net::test_server::HttpRequest& request) {
    last_request_ = request;
    return test_util::CreateHttpResponseFromFile(
        test_util::GetTestFilePath(test_file_path_));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<RequestSender> request_sender_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;

  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  net::test_server::HttpRequest last_request_;
  std::string test_file_path_;
};

TEST_F(TasksApiRequestsTest, ListTaskListsRequest) {
  set_test_file_path("tasks/task_lists.json");

  base::test::TestFuture<
      base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListTaskListsRequest>(
      request_sender(), /*page_token=*/"", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(last_request().method, net::test_server::METHOD_GET);
  EXPECT_EQ(last_request().GetURL(),
            GetListTaskListsUrl(/*max_results=*/100, /*page_token=*/""));
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 2u);
}

TEST_F(TasksApiRequestsTest, ListTaskListsWithOptionalArgsRequest) {
  set_test_file_path("tasks/task_lists.json");

  base::test::TestFuture<
      base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListTaskListsRequest>(
      request_sender(), /*page_token=*/"qwerty", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(last_request().method, net::test_server::METHOD_GET);
  EXPECT_EQ(last_request().GetURL(),
            GetListTaskListsUrl(/*max_results=*/100, /*page_token=*/"qwerty"));
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 2u);
}

TEST_F(TasksApiRequestsTest, ListTaskListsRequestHandlesError) {
  set_test_file_path("tasks/invalid_file_to_simulate_404_error.json");

  base::test::TestFuture<
      base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListTaskListsRequest>(request_sender(),
                                                        /*page_token=*/"",
                                                        future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(future.Get(), base::test::ErrorIs(HTTP_NOT_FOUND));
}

TEST_F(TasksApiRequestsTest, ListTasksRequest) {
  set_test_file_path("tasks/tasks.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Tasks>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListTasksRequest>(
      request_sender(), kTaskListId, /*page_token=*/"",
      /*include_assigned=*/false, future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(last_request().method, net::test_server::METHOD_GET);
  EXPECT_EQ(last_request().GetURL(),
            GetListTasksUrl(kTaskListId, /*include_completed=*/false,
                            /*include_assigned=*/false,
                            /*max_results=*/100,
                            /*page_token=*/""));
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 2u);
}

TEST_F(TasksApiRequestsTest, ListTasksWithOptionalArgsRequest) {
  set_test_file_path("tasks/tasks.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Tasks>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListTasksRequest>(
      request_sender(), kTaskListId, /*page_token=*/"qwerty",
      /*include_assigned=*/false, future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(last_request().method, net::test_server::METHOD_GET);
  EXPECT_EQ(last_request().GetURL(),
            GetListTasksUrl(kTaskListId, /*include_completed=*/false,
                            /*include_assigned=*/false,
                            /*max_results=*/100,
                            /*page_token=*/"qwerty"));
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 2u);
}

TEST_F(TasksApiRequestsTest, ListTasksRequestHandlesError) {
  set_test_file_path("tasks/invalid_file_to_simulate_404_error.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Tasks>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListTasksRequest>(
      request_sender(), kTaskListId, /*page_token=*/"",
      /*include_assigned=*/false, future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(future.Get(), base::test::ErrorIs(HTTP_NOT_FOUND));
}

TEST_F(TasksApiRequestsTest, PatchTaskRequest) {
  set_test_file_path("tasks/task.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Task>, ApiErrorCode>>
      future;
  auto request = std::make_unique<PatchTaskRequest>(
      request_sender(), kTaskListId, kTaskId,
      TaskRequestPayload{.status = TaskStatus::kCompleted},
      future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(last_request().method, net::test_server::METHOD_PATCH);
  EXPECT_EQ(last_request().GetURL(), GetPatchTaskUrl(kTaskListId, kTaskId));
  EXPECT_EQ(last_request().headers.at("Content-Type"),
            "application/json; charset=utf-8");
  EXPECT_EQ(last_request().content, "{\"status\":\"completed\"}");
}

TEST_F(TasksApiRequestsTest, PatchTaskRequestHandlesError) {
  set_test_file_path("tasks/invalid_file_to_simulate_404_error.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Task>, ApiErrorCode>>
      future;
  auto request = std::make_unique<PatchTaskRequest>(
      request_sender(), kTaskListId, kTaskId,
      TaskRequestPayload{.status = TaskStatus::kCompleted},
      future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(future.Get(), base::test::ErrorIs(HTTP_NOT_FOUND));
}

TEST_F(TasksApiRequestsTest, InsertTaskRequest) {
  set_test_file_path("tasks/task.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Task>, ApiErrorCode>>
      future;
  auto request = std::make_unique<InsertTaskRequest>(
      request_sender(), kTaskListId, /*previous_task_id=*/"",
      TaskRequestPayload{.title = "New task",
                         .status = TaskStatus::kNeedsAction},
      future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(last_request().method, net::test_server::METHOD_POST);
  EXPECT_EQ(last_request().GetURL(),
            GetInsertTaskUrl(kTaskListId, /*previous_task_id=*/""));
  EXPECT_EQ(last_request().headers.at("Content-Type"),
            "application/json; charset=utf-8");
  EXPECT_EQ(last_request().content,
            "{\"status\":\"needsAction\",\"title\":\"New task\"}");

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value()->id(), "qwe");
}

TEST_F(TasksApiRequestsTest, InsertTaskRequestHandlesError) {
  set_test_file_path("tasks/invalid_file_to_simulate_404_error.json");

  base::test::TestFuture<base::expected<std::unique_ptr<Task>, ApiErrorCode>>
      future;
  auto request = std::make_unique<InsertTaskRequest>(
      request_sender(), kTaskListId, /*previous_task_id=*/"",
      TaskRequestPayload{.title = "New task",
                         .status = TaskStatus::kNeedsAction},
      future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(future.Get(), base::test::ErrorIs(HTTP_NOT_FOUND));
}

}  // namespace google_apis::tasks
