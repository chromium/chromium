// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_base_requests.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_requests.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

class FakeMultipartUploadRequest : public MultipartUploadRequestBase {
 public:
  FakeMultipartUploadRequest(base::SequencedTaskRunner* blocking_task_runner,
                             const std::string& metadata_json,
                             const std::string& content_type,
                             int64_t content_length,
                             const base::FilePath& local_file_path,
                             FileResourceCallback callback,
                             google_apis::ProgressCallback progress_callback,
                             const GURL& url,
                             std::string* upload_content_type,
                             std::string* upload_content_data)
      : MultipartUploadRequestBase(blocking_task_runner,
                                   metadata_json,
                                   content_type,
                                   content_length,
                                   local_file_path,
                                   std::move(callback),
                                   progress_callback),
        url_(url),
        upload_content_type_(upload_content_type),
        upload_content_data_(upload_content_data) {}

  ~FakeMultipartUploadRequest() override = default;

  HttpRequestMethod GetRequestType() const override {
    return HttpRequestMethod::kPost;
  }

  bool GetContentData(std::string* content_type,
                      std::string* content_data) override {
    const bool result =
        MultipartUploadRequestBase::GetContentData(content_type, content_data);
    *upload_content_type_ = *content_type;
    *upload_content_data_ = *content_data;
    return result;
  }

 protected:
  GURL GetURL() const override { return url_; }

 private:
  const GURL url_;
  const raw_ptr<std::string> upload_content_type_;
  const raw_ptr<std::string> upload_content_data_;
};

}  // namespace

class DriveBaseRequestsTest : public testing::Test {
 public:
  DriveBaseRequestsTest() {
    mojo::Remote<network::mojom::NetworkService> network_service_remote;
    network_service_ = network::NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());
    network::mojom::NetworkContextParamsPtr context_params =
        network::mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_service_remote->CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    mojo::PendingReceiver<network::mojom::URLLoaderNetworkServiceObserver>
        default_observer_receiver;
    network::mojom::NetworkServiceParamsPtr network_service_params =
        network::mojom::NetworkServiceParams::New();
    network_service_params->default_observer =
        default_observer_receiver.InitWithNewPipeAndPassRemote();
    network_service_remote->SetParams(std::move(network_service_params));

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = network::mojom::kBrowserProcessId;
    params->is_orb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }

  void SetUp() override {
    sender_ = std::make_unique<RequestSender>(
        std::make_unique<DummyAuthService>(), test_shared_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(),
        std::string(), /* custom user agent */
        TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &DriveBaseRequestsTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    // Deleting the sender here will delete all request objects.
    sender_.reset();
    // Wait for any DeleteSoon tasks to run.
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(response_code_);
    response->set_content(response_body_);
    response->set_content_type("application/json");
    return std::move(response);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<network::mojom::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
  std::unique_ptr<RequestSender> sender_;
  net::EmbeddedTestServer test_server_;

  net::HttpStatusCode response_code_ = net::HTTP_OK;
  std::string response_body_;
};

typedef DriveBaseRequestsTest MultipartUploadRequestBaseTest;

TEST_F(MultipartUploadRequestBaseTest, Basic) {
  response_code_ = net::HTTP_OK;
  response_body_ = "{\"kind\": \"drive#file\", \"id\": \"file_id\"}";
  std::unique_ptr<google_apis::FileResource> file;
  ApiErrorCode error = OTHER_ERROR;
  base::RunLoop run_loop;
  const base::FilePath source_path =
      google_apis::test_util::GetTestFilePath("drive/text.txt");
  std::string upload_content_type;
  std::string upload_content_data;
  std::unique_ptr<FakeMultipartUploadRequest> multipart_request =
      std::make_unique<FakeMultipartUploadRequest>(
          sender_->blocking_task_runner(), "{json:\"test\"}", "text/plain", 10,
          source_path,
          test_util::CreateQuitCallback(
              &run_loop, test_util::CreateCopyResultCallback(&error, &file)),
          ProgressCallback(), test_server_.base_url(), &upload_content_type,
          &upload_content_data);
  multipart_request->SetBoundaryForTesting("TESTBOUNDARY");
  sender_->StartRequestWithAuthRetry(
      std::make_unique<drive::SingleBatchableDelegateRequest>(
          sender_.get(), std::move(multipart_request)));
  run_loop.Run();
  EXPECT_EQ("multipart/related; boundary=TESTBOUNDARY", upload_content_type);
  EXPECT_EQ(
      "--TESTBOUNDARY\n"
      "Content-Type: application/json\n"
      "\n"
      "{json:\"test\"}\n"
      "--TESTBOUNDARY\n"
      "Content-Type: text/plain\n"
      "\n"
      "This is a sample file. I like chocolate and chips.\n"
      "\n"
      "--TESTBOUNDARY--",
      upload_content_data);
  ASSERT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ("file_id", file->file_id());
}

}  // namespace google_apis
