// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/people/people_api_requests.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "google_apis/people/people_api_request_types.h"
#include "google_apis/people/people_api_response_types.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace google_apis::people {
namespace {

using ::base::test::ErrorIs;
using ::base::test::IsJson;
using ::base::test::ValueIs;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Return;

constexpr std::string_view kJsonMimeType = "application/json";
constexpr std::string_view kContactJson = R"json({
  "names": [
    {
      "familyName": "Francois",
      "givenName": "Andre"
    }
  ]
})json";
constexpr std::string_view kPersonJsonWithMetadata = R"json({
  "resourceName": "people/c1234567890123456789",
  "etag": "%ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ",
  "metadata": {
    "sources": [
      {
        "type": "CONTACT",
        "id": "0000000000000000",
        "etag": "#ZZZZZZZZZZZZ",
        "updateTime": "2024-10-18T00:00:00.000000Z"
      }
    ],
    "objectType": "PERSON"
  }
})json";
constexpr std::string_view kMissingPersonJson = R"json({
  "error": {
    "code": 400,
    "message": "Request must contain a person.",
    "status": "INVALID_ARGUMENT"
  }
})json";

// Wrapper around an `EmbeddedTestServer` which starts the server in the
// constructor, so a `base_url()` can be obtained immediately after
// construction.
// This is required so `PeopleApiRequestsTest` can initialise
// `gaia_urls_overrider_` in the constructor - which requires `base_url()`.
class MockServer {
 public:
  MockServer() {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &MockServer::HandleRequest, base::Unretained(this)));
    CHECK(test_server_.Start());
  }

  MOCK_METHOD(std::unique_ptr<net::test_server::HttpResponse>,
              HandleRequest,
              (const net::test_server::HttpRequest& request));

  const GURL& base_url() const { return test_server_.base_url(); }

 private:
  net::test_server::EmbeddedTestServer test_server_;
};

class PeopleApiRequestsTest : public testing::Test {
 public:
  PeopleApiRequestsTest()
      : request_sender_(
            std::make_unique<DummyAuthService>(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true),
            task_environment_.GetMainThreadTaskRunner(),
            "test-user-agent",
            TRAFFIC_ANNOTATION_FOR_TESTS),
        gaia_urls_overrider_(base::CommandLine::ForCurrentProcess(),
                             "people_api_origin_url",
                             mock_server_.base_url().spec()) {
    CHECK_EQ(mock_server_.base_url(),
             GaiaUrls::GetInstance()->people_api_origin_url());
  }

  RequestSender& request_sender() { return request_sender_; }
  MockServer& mock_server() { return mock_server_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  RequestSender request_sender_;
  MockServer mock_server_;
  GaiaUrlsOverriderForTesting gaia_urls_overrider_;
};

TEST_F(PeopleApiRequestsTest, CreateContactSuccess) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(kPersonJsonWithMetadata);
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(
      mock_server(),
      HandleRequest(AllOf(
          Field("relative_url", &net::test_server::HttpRequest::relative_url,
                "/v1/people:createContact?personFields=metadata"),
          Field("method", &net::test_server::HttpRequest::method,
                net::test_server::HttpMethod::METHOD_POST),
          Field("content", &net::test_server::HttpRequest::content,
                IsJson(kContactJson)))))
      .WillOnce(Return(std::move(response)));

  Contact contact;
  contact.name.family_name = "Francois";
  contact.name.given_name = "Andre";
  base::test::TestFuture<base::expected<Person, ApiErrorCode>> future;
  auto request = std::make_unique<CreateContactRequest>(
      &request_sender(), std::move(contact), future.GetCallback());
  request_sender().StartRequestWithAuthRetry(std::move(request));

  EXPECT_THAT(future.Take(), ValueIs(FieldsAre("people/c1234567890123456789")));
}

TEST_F(PeopleApiRequestsTest, CreateContactServerInvalidRequest) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_BAD_REQUEST);
  response->set_content(kMissingPersonJson);
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(
      mock_server(),
      HandleRequest(AllOf(
          Field("relative_url", &net::test_server::HttpRequest::relative_url,
                "/v1/people:createContact?personFields=metadata"),
          Field("method", &net::test_server::HttpRequest::method,
                net::test_server::HttpMethod::METHOD_POST))))
      .WillOnce(Return(std::move(response)));

  base::test::TestFuture<base::expected<Person, ApiErrorCode>> future;
  auto request = std::make_unique<CreateContactRequest>(
      &request_sender(), Contact(), future.GetCallback());
  request_sender().StartRequestWithAuthRetry(std::move(request));

  EXPECT_THAT(future.Take(), ErrorIs(ApiErrorCode::HTTP_BAD_REQUEST));
}

TEST_F(PeopleApiRequestsTest, CreateContactServerInvalidJsonResponse) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content("{notJson");
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(
      mock_server(),
      HandleRequest(AllOf(
          Field("relative_url", &net::test_server::HttpRequest::relative_url,
                "/v1/people:createContact?personFields=metadata"),
          Field("method", &net::test_server::HttpRequest::method,
                net::test_server::HttpMethod::METHOD_POST))))
      .WillOnce(Return(std::move(response)));

  Contact contact;
  contact.name.family_name = "Francois";
  contact.name.given_name = "Andre";
  base::test::TestFuture<base::expected<Person, ApiErrorCode>> future;
  auto request = std::make_unique<CreateContactRequest>(
      &request_sender(), std::move(contact), future.GetCallback());
  request_sender().StartRequestWithAuthRetry(std::move(request));

  EXPECT_THAT(future.Take(), ErrorIs(ApiErrorCode::PARSE_ERROR));
}

TEST_F(PeopleApiRequestsTest, CreateContactServerInvalidResponseTypes) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(R"json({"resourceName": 1})json");
  response->set_content_type(kJsonMimeType);
  EXPECT_CALL(
      mock_server(),
      HandleRequest(AllOf(
          Field("relative_url", &net::test_server::HttpRequest::relative_url,
                "/v1/people:createContact?personFields=metadata"),
          Field("method", &net::test_server::HttpRequest::method,
                net::test_server::HttpMethod::METHOD_POST))))
      .WillOnce(Return(std::move(response)));

  Contact contact;
  contact.name.family_name = "Francois";
  contact.name.given_name = "Andre";
  base::test::TestFuture<base::expected<Person, ApiErrorCode>> future;
  auto request = std::make_unique<CreateContactRequest>(
      &request_sender(), std::move(contact), future.GetCallback());
  request_sender().StartRequestWithAuthRetry(std::move(request));

  EXPECT_THAT(future.Take(), ErrorIs(ApiErrorCode::PARSE_ERROR));
}

}  // namespace
}  // namespace google_apis::people
