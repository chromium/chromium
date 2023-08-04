// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_list_student_submissions_request.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "google_apis/classroom/classroom_api_student_submissions_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::classroom {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;

class TestRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(R"(
        {
        "studentSubmissions": [
          {
            "id": "student-submission-item-1",
            "courseWorkId": "course-work-1",
            "state": "TURNED_IN",
            "assignedGrade": 1.0
          },
          {
            "id": "student-submission-item-2",
            "courseWorkId": "course-work-1",
            "state": "RETURNED"
          },
          {
            "id": "student-submission-item-3",
            "courseWorkId": "course-work-1",
            "state": "NEW",
            "assignedGrade": 3.3
          }
        ]
      })");
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

}  // namespace

class ClassroomApiListStudentSubmissionRequestsTest : public testing::Test {
 public:
  ClassroomApiListStudentSubmissionRequestsTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true)) {}

  void SetUp() override {
    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TestRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));
    ASSERT_TRUE(test_server_.Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "classroom_api_origin_url",
        test_server_.base_url().spec());
    ASSERT_EQ(GaiaUrls::GetInstance()->classroom_api_origin_url(),
              test_server_.base_url().spec());
  }

  TestRequestHandler& request_handler() { return request_handler_; }
  RequestSender* request_sender() { return request_sender_.get(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<RequestSender> request_sender_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  testing::StrictMock<TestRequestHandler> request_handler_;
};

TEST_F(ClassroomApiListStudentSubmissionRequestsTest,
       ListStudentSubmissionsRequest) {
  EXPECT_CALL(request_handler(),
              HandleRequest(AllOf(
                  Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                  Field(&HttpRequest::relative_url,
                        Eq("/v1/courses/course-1/courseWork/course-work-1/"
                           "studentSubmissions?fields=studentSubmissions(id%2C"
                           "courseWorkId%2CupdateTime%2Cstate%2CassignedGrade)%"
                           "2CnextPageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse())));

  base::test::TestFuture<
      base::expected<std::unique_ptr<StudentSubmissions>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListStudentSubmissionsRequest>(
      request_sender(), /*course_id=*/"course-1",
      /*course_work_id=*/"course-work-1",
      /*page_token=*/"", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(future.Get().has_value());
  ASSERT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 3u);
}

TEST_F(ClassroomApiListStudentSubmissionRequestsTest,
       ListStudentSubmissionsRequestWithAdditionalQueryParamaters) {
  EXPECT_CALL(request_handler(),
              HandleRequest(AllOf(
                  Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                  Field(&HttpRequest::relative_url,
                        Eq("/v1/courses/course-1/courseWork/course-work-1/"
                           "studentSubmissions?fields=studentSubmissions(id%2C"
                           "courseWorkId%2CupdateTime%2Cstate%2CassignedGrade)%"
                           "2CnextPageToken"
                           "&pageToken=qwerty")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse())));

  base::test::TestFuture<
      base::expected<std::unique_ptr<StudentSubmissions>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListStudentSubmissionsRequest>(
      request_sender(), /*course_id=*/"course-1",
      /*course_work_id=*/"course-work-1",
      /*page_token=*/"qwerty", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  ASSERT_TRUE(future.Get().has_value());
  ASSERT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 3u);
}

TEST_F(ClassroomApiListStudentSubmissionRequestsTest,
       ListStudentSubmissionsRequestHandlesError) {
  EXPECT_CALL(request_handler(),
              HandleRequest(AllOf(
                  Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET)),
                  Field(&HttpRequest::relative_url,
                        Eq("/v1/courses/course-1/courseWork/course-work-1/"
                           "studentSubmissions?fields=studentSubmissions(id%2C"
                           "courseWorkId%2CupdateTime%2Cstate%2CassignedGrade)%"
                           "2CnextPageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  base::test::TestFuture<
      base::expected<std::unique_ptr<StudentSubmissions>, ApiErrorCode>>
      future;
  auto request = std::make_unique<ListStudentSubmissionsRequest>(
      request_sender(), /*course_id=*/"course-1",
      /*course_work_id=*/"course-work-1",
      /*page_token=*/"", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));
  ASSERT_TRUE(future.Wait());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), HTTP_INTERNAL_SERVER_ERROR);
}

}  // namespace google_apis::classroom
