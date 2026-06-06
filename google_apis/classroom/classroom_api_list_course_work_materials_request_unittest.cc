// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/classroom/classroom_api_list_course_work_materials_request.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "google_apis/classroom/classroom_api_course_work_materials_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace google_apis::classroom {
namespace {

constexpr char kTestCourseId[] = "course-1";
constexpr char kTestPageToken[] = "qwerty";

constexpr char kResponseBody[] = R"(
    {
      "courseWorkMaterial": [
        {
          "id": "course-work-material-1",
          "title": "Math assignment material",
          "state": "PUBLISHED",
          "alternateLink": "https://classroom.google.com/c/ab/a/cd/details"
        }
      ]
    })";

GURL GetExpectedUrl(const std::string& course_id) {
  auto url = GaiaUrls::GetInstance()->classroom_api_origin_url().Resolve(
      "/v1/courses/" + course_id + "/courseWorkMaterials");
  url = net::AppendOrReplaceQueryParameter(
      url, "fields",
      "courseWorkMaterial(id,title,state,alternateLink,creationTime,updateTime,"
      "materials(youtubeVideo(title),link(title),form(title),"
      "guidedLearning(title),notebook(title),driveFile(driveFile(title)))),"
      "nextPageToken");
  return url;
}

class ClassroomApiListCourseWorkMaterialsRequestTest : public testing::Test {
 public:
  ClassroomApiListCourseWorkMaterialsRequestTest() = default;

  void SetUp() override {
    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(),
        url_loader_factory_.GetSafeWeakWrapper(),
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  RequestSender* request_sender() { return request_sender_.get(); }
  network::TestURLLoaderFactory& url_loader_factory() {
    return url_loader_factory_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<RequestSender> request_sender_;
};

TEST_F(ClassroomApiListCourseWorkMaterialsRequestTest,
       ListCourseWorkMaterialsRequestSuccess) {
  base::test::TestFuture<
      base::expected<std::unique_ptr<CourseWorkMaterial>, ApiErrorCode>>
      future;
  const GURL expected_url = GetExpectedUrl(kTestCourseId);
  url_loader_factory().AddResponse(expected_url.spec(), kResponseBody);
  auto request = std::make_unique<ListCourseWorkMaterialRequest>(
      request_sender(), kTestCourseId, /*page_token=*/"", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 1u);
}

TEST_F(ClassroomApiListCourseWorkMaterialsRequestTest,
       ListCourseWorkMaterialsRequestWithAdditionalQueryParameters) {
  base::test::TestFuture<
      base::expected<std::unique_ptr<CourseWorkMaterial>, ApiErrorCode>>
      future;
  const GURL expected_url = net::AppendOrReplaceQueryParameter(
      GetExpectedUrl(kTestCourseId), "pageToken", kTestPageToken);
  url_loader_factory().AddResponse(expected_url.spec(), kResponseBody);
  auto request = std::make_unique<ListCourseWorkMaterialRequest>(
      request_sender(), kTestCourseId, kTestPageToken, future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_TRUE(future.Get().value());
  EXPECT_EQ(future.Get().value()->items().size(), 1u);
}

TEST_F(ClassroomApiListCourseWorkMaterialsRequestTest,
       ListCourseWorkMaterialsRequestHandlesError) {
  base::test::TestFuture<
      base::expected<std::unique_ptr<CourseWorkMaterial>, ApiErrorCode>>
      future;
  const GURL expected_url = GetExpectedUrl(kTestCourseId);
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->ReplaceStatusLine(
      "HTTP/1.1 500 Internal Server Error");
  url_loader_factory().AddResponse(expected_url, std::move(response_head),
                                   /*content=*/"",
                                   network::URLLoaderCompletionStatus(net::OK));
  auto request = std::make_unique<ListCourseWorkMaterialRequest>(
      request_sender(), kTestCourseId, /*page_token=*/"", future.GetCallback());
  request_sender()->StartRequestWithAuthRetry(std::move(request));

  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), HTTP_INTERNAL_SERVER_ERROR);
}

}  // namespace
}  // namespace google_apis::classroom
