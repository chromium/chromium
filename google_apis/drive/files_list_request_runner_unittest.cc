// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/files_list_request_runner.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "google_apis/drive/request_sender.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_service_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace {

const int kMaxResults = 4;
const char kQuery[] = "testing-query";
const char kFields[] = "testing-fields";
const char kTestUserAgent[] = "test-user-agent";

const char kSuccessResource[] =
    "{\n"
    " \"kind\": \"drive#fileList\",\n"
    " \"etag\": \"etag\",\n"
    " \"items\": []\n"
    "}\n";

const char kResponseTooLargeErrorResource[] =
    "{\n"
    " \"error\": {\n"
    "  \"errors\": [\n"
    "   {\n"
    "    \"reason\": \"responseTooLarge\"\n"
    "   }\n"
    "  ]\n"
    " }\n"
    "}\n";

const char kQuotaExceededErrorResource[] =
    "{\n"
    " \"error\": {\n"
    "  \"errors\": [\n"
    "   {\n"
    "    \"reason\": \"quotaExceeded\"\n"
    "   }\n"
    "  ]\n"
    " }\n"
    "}\n";

}  // namespace

class FilesListRequestRunnerTest : public testing::Test {
 public:
  FilesListRequestRunnerTest() {
    mojo::Remote<network::mojom::NetworkService> network_service_remote;
    network_service_ = network::NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    network_service_remote->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    mojo::PendingRemote<network::mojom::NetworkServiceClient>
        network_service_client_remote;
    network_service_client_ =
        std::make_unique<network::TestNetworkServiceClient>(
            network_service_client_remote.InitWithNewPipeAndPassReceiver());
    network_service_remote->SetClient(
        std::move(network_service_client_remote),
        network::mojom::NetworkServiceParams::New());

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }

  void SetUp() override {
    request_sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), kTestUserAgent,
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(
        base::Bind(&FilesListRequestRunnerTest::OnFilesListRequest,
                   base::Unretained(this), test_server_.base_url()));
    ASSERT_TRUE(test_server_.Start());

    runner_.reset(new FilesListRequestRunner(
        request_sender_.get(),
        google_apis::DriveApiUrlGenerator(test_server_.base_url(),
                                          test_server_.GetURL("/thumbnail/"))));
  }

  void TearDown() override {
    on_completed_callback_ = base::Closure();
    http_request_.reset();
    response_error_.reset();
    response_entry_.reset();
  }

  // Called when the request is completed and no more backoff retries will
  // happen.
  void OnCompleted(DriveApiErrorCode error, std::unique_ptr<FileList> entry) {
    response_error_.reset(new DriveApiErrorCode(error));
    response_entry_ = std::move(entry);
    on_completed_callback_.Run();
  }

 protected:
  // Sets a fake Drive API server response to be returned for the upcoming HTTP
  // request.
  void SetFakeServerResponse(net::HttpStatusCode code,
                             const std::string& content) {
    fake_server_response_.reset(new net::test_server::BasicHttpResponse);
    fake_server_response_->set_code(code);
    fake_server_response_->set_content(content);
    fake_server_response_->set_content_type("application/json");
  }

  // Handles a HTTP request to the Drive API server and returns a fake response.
  std::unique_ptr<net::test_server::HttpResponse> OnFilesListRequest(
      const GURL& base_url,
      const net::test_server::HttpRequest& request) {
    http_request_.reset(new net::test_server::HttpRequest(request));
    return std::move(fake_server_response_);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<RequestSender> request_sender_;
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<FilesListRequestRunner> runner_;
  std::unique_ptr<network::mojom::NetworkService> network_service_;
  std::unique_ptr<network::mojom::NetworkServiceClient> network_service_client_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
  base::Closure on_completed_callback_;

  // Response set by test cases to be returned from the HTTP server.
  std::unique_ptr<net::test_server::BasicHttpResponse> fake_server_response_;

  // A requests and a response stored for verification in test cases.
  std::unique_ptr<net::test_server::HttpRequest> http_request_;
  std::unique_ptr<DriveApiErrorCode> response_error_;
  std::unique_ptr<FileList> response_entry_;
};

TEST_F(FilesListRequestRunnerTest, Success_NoBackoff) {
  SetFakeServerResponse(net::HTTP_OK, kSuccessResource);
  runner_->CreateAndStartWithSizeBackoff(
      kMaxResults, FilesListCorpora::DEFAULT, std::string(), kQuery, kFields,
      base::Bind(&FilesListRequestRunnerTest::OnCompleted,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  on_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();

  ASSERT_TRUE(http_request_.get());
  EXPECT_EQ(
      "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
      "&corpora=default&maxResults=4&q=testing-query&fields=testing-fields",
      http_request_->relative_url);

  ASSERT_TRUE(response_error_.get());
  EXPECT_EQ(HTTP_SUCCESS, *response_error_);
  EXPECT_TRUE(response_entry_.get());
}

TEST_F(FilesListRequestRunnerTest, Success_Backoff) {
  SetFakeServerResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                        kResponseTooLargeErrorResource);
  runner_->CreateAndStartWithSizeBackoff(
      kMaxResults, FilesListCorpora::DEFAULT, std::string(), kQuery, kFields,
      base::Bind(&FilesListRequestRunnerTest::OnCompleted,
                 base::Unretained(this)));
  {
    base::RunLoop run_loop;
    runner_->SetRequestCompletedCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(http_request_.get());
    EXPECT_EQ(
        "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
        "&corpora=default&maxResults=4&q=testing-query&fields=testing-fields",
        http_request_->relative_url);
    EXPECT_FALSE(response_error_.get());
  }

  // Backoff will decreasing the number of results by 2, which will succeed.
  {
    SetFakeServerResponse(net::HTTP_OK, kSuccessResource);

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();

    ASSERT_TRUE(http_request_.get());
    EXPECT_EQ(
        "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
        "&corpora=default&maxResults=2&q=testing-query&fields=testing-fields",
        http_request_->relative_url);

    ASSERT_TRUE(response_error_.get());
    EXPECT_EQ(HTTP_SUCCESS, *response_error_);
    EXPECT_TRUE(response_entry_.get());
  }
}

TEST_F(FilesListRequestRunnerTest, Failure_TooManyBackoffs) {
  SetFakeServerResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                        kResponseTooLargeErrorResource);
  runner_->CreateAndStartWithSizeBackoff(
      kMaxResults, FilesListCorpora::DEFAULT, std::string(), kQuery, kFields,
      base::Bind(&FilesListRequestRunnerTest::OnCompleted,
                 base::Unretained(this)));
  {
    base::RunLoop run_loop;
    runner_->SetRequestCompletedCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(http_request_.get());
    EXPECT_EQ(
        "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
        "&corpora=default&maxResults=4&q=testing-query&fields=testing-fields",
        http_request_->relative_url);
    EXPECT_FALSE(response_error_.get());
  }

  // Backoff will decreasing the number of results by 2, which will still fail
  // due to too large response.
  {
    SetFakeServerResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                          kResponseTooLargeErrorResource);

    base::RunLoop run_loop;
    runner_->SetRequestCompletedCallbackForTesting(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(http_request_.get());
    EXPECT_EQ(
        "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
        "&corpora=default&maxResults=2&q=testing-query&fields=testing-fields",
        http_request_->relative_url);
    EXPECT_FALSE(response_error_.get());
  }

  // The last backoff, decreasing the number of results to 1.
  {
    SetFakeServerResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                          kResponseTooLargeErrorResource);

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();

    ASSERT_TRUE(http_request_.get());
    EXPECT_EQ(
        "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
        "&corpora=default&maxResults=1&q=testing-query&fields=testing-fields",
        http_request_->relative_url);

    ASSERT_TRUE(response_error_.get());
    EXPECT_EQ(DRIVE_RESPONSE_TOO_LARGE, *response_error_);
    EXPECT_FALSE(response_entry_.get());
  }
}

TEST_F(FilesListRequestRunnerTest, Failure_AnotherError) {
  SetFakeServerResponse(net::HTTP_INTERNAL_SERVER_ERROR,
                        kQuotaExceededErrorResource);
  runner_->CreateAndStartWithSizeBackoff(
      kMaxResults, FilesListCorpora::DEFAULT, std::string(), kQuery, kFields,
      base::Bind(&FilesListRequestRunnerTest::OnCompleted,
                 base::Unretained(this)));

  base::RunLoop run_loop;
  on_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();

  ASSERT_TRUE(http_request_.get());
  EXPECT_EQ(
      "/drive/v2/files?supportsTeamDrives=true&includeTeamDriveItems=true"
      "&corpora=default&maxResults=4&q=testing-query&fields=testing-fields",
      http_request_->relative_url);

  // There must be no backoff in case of an error different than
  // DRIVE_RESPONSE_TOO_LARGE.
  ASSERT_TRUE(response_error_.get());
  EXPECT_EQ(DRIVE_NO_SPACE, *response_error_);
  EXPECT_FALSE(response_entry_.get());
}

}  // namespace google_apis
