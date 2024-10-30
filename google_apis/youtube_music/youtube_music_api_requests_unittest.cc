// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/youtube_music/youtube_music_api_requests.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis::youtube_music {

namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::NotNull;
using ::testing::Optional;

constexpr char kTestJson[] = R"(
{
  "error": {
    "code": 400,
    "message": "Invalid Request.",
    "status": "INVALID_REQUEST",
    "details": [
    {
      "@type": "type.googleapis.com/google.rpc.ErrorInfo",
      "reason": "UPDATE_REQUIRED",
      "domain": "googleapis.com",
      "metadata": {
        "method": "google.youtube.mediaconnect.v1.TrackService.DownloadTrack",
        "service": "youtubemediaconnect.googleapis.com",
        "mediaConnectError": "UPDATE_REQUIRED"
      }
    },
    {
      "@type": "type.googleapis.com/google.rpc.LocalizedMessage",
      "locale": "en-US",
      "message": "Upgrade to a newer version of YouTube Music."
    }
    ]
  }
})";

constexpr char kPlaybackResponse[] = R"(
{
  "playbackReportingToken": "IamAToken"
})";

// Returns an arbitrary but valid payload object.
std::unique_ptr<ReportPlaybackRequestPayload> TestPlaybackPayload() {
  const std::vector<ReportPlaybackRequestPayload::WatchTimeSegment>
      watch_time_segments;
  const std::string reporting_token = "reportingToken";

  ReportPlaybackRequestPayload::Params params(
      /*initial_report=*/true,
      /*playback_reporting_token=*/reporting_token,
      /*client_time=*/base::Time(),
      /*playback_start_offset=*/base::TimeDelta(),
      /*media_time_current=*/base::TimeDelta(),
      /*connection_type=*/ReportPlaybackRequestPayload::ConnectionType::kWired,
      /*playback_state=*/ReportPlaybackRequestPayload::PlaybackState::kPlaying,
      /*watch_time_segments=*/watch_time_segments);
  return std::make_unique<ReportPlaybackRequestPayload>(params);
}

class YoutubeMusicApiRequestsTest : public testing::Test {
 public:
  YoutubeMusicApiRequestsTest()
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
        &YoutubeMusicApiRequestsTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  RequestSender* request_sender() { return request_sender_.get(); }

  void set_response(net::HttpStatusCode code, std::string response) {
    response_code_ = code;
    response_content_ = std::move(response);
  }

  const GURL& TestServerUrl() const { return test_server_.base_url(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(response_code_);
    response->set_content(response_content_);
    response->set_content_type("application/json");
    return response;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<RequestSender> request_sender_;
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_loader_factory_;

  net::test_server::HttpRequest last_request_;
  net::HttpStatusCode response_code_;
  std::string response_content_;
};

TEST_F(YoutubeMusicApiRequestsTest, ReportPlaybackResult) {
  base::RunLoop loop;

  set_response(net::HttpStatusCode::HTTP_OK, kPlaybackResponse);

  std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult> response;
  auto request = std::make_unique<
      google_apis::youtube_music::ReportPlaybackRequest>(
      request_sender(), TestPlaybackPayload(),
      base::BindLambdaForTesting(
          [&](base::expected<
              std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult>,
              google_apis::youtube_music::ApiError> result) {
            response = std::move(result.value());
            loop.Quit();
          }));

  set_response(net::HttpStatusCode::HTTP_OK, kPlaybackResponse);
  request->SetBaseUrlForTesting(TestServerUrl());

  // Add a signing header placeholder.
  request->SetSigningHeaders({"Device-Info: device-info"});

  request_sender()->StartRequestWithAuthRetry(std::move(request));
  loop.Run();
  EXPECT_THAT(response, NotNull());
}

TEST_F(YoutubeMusicApiRequestsTest, ClassifyUpdateRequiredError) {
  set_response(net::HttpStatusCode::HTTP_BAD_REQUEST, kTestJson);

  base::RunLoop loop;

  std::optional<google_apis::youtube_music::ApiError> error;
  std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult> response;
  auto request = std::make_unique<
      google_apis::youtube_music::ReportPlaybackRequest>(
      request_sender(), TestPlaybackPayload(),
      base::BindLambdaForTesting(
          [&](base::expected<
              std::unique_ptr<google_apis::youtube_music::ReportPlaybackResult>,
              google_apis::youtube_music::ApiError> result) {
            ASSERT_FALSE(result.has_value());
            error = result.error();
            loop.Quit();
          }));
  request->SetBaseUrlForTesting(TestServerUrl());
  // Add a signing header placeholder.
  request->SetSigningHeaders({"Device-Info: device-info"});

  request_sender()->StartRequestWithAuthRetry(std::move(request));
  loop.Run();
  EXPECT_THAT(error,
              Optional(Field(&google_apis::youtube_music::ApiError::error_code,
                             Eq(YOUTUBE_MUSIC_UPDATE_REQUIRED))));
}

}  // namespace

}  // namespace google_apis::youtube_music
