// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/base_requests.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/drive/test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeUrlFetchRequest : public UrlFetchRequestBase {
 public:
  FakeUrlFetchRequest(RequestSender* sender,
                      const EntryActionCallback& callback,
                      const GURL& url)
      : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
        callback_(callback),
        url_(url) {}

  ~FakeUrlFetchRequest() override {}

 protected:
  GURL GetURL() const override { return url_; }
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override {
    callback_.Run(GetErrorCode());
  }
  void RunCallbackOnPrematureFailure(DriveApiErrorCode code) override {
    callback_.Run(code);
  }

  EntryActionCallback callback_;
  GURL url_;
};

class FakeMultipartUploadRequest : public MultipartUploadRequestBase {
 public:
  FakeMultipartUploadRequest(
      base::SequencedTaskRunner* blocking_task_runner,
      const std::string& metadata_json,
      const std::string& content_type,
      int64_t content_length,
      const base::FilePath& local_file_path,
      const FileResourceCallback& callback,
      const google_apis::ProgressCallback& progress_callback,
      const GURL& url,
      std::string* upload_content_type,
      std::string* upload_content_data)
      : MultipartUploadRequestBase(blocking_task_runner,
                                   metadata_json,
                                   content_type,
                                   content_length,
                                   local_file_path,
                                   callback,
                                   progress_callback),
        url_(url),
        upload_content_type_(upload_content_type),
        upload_content_data_(upload_content_data) {}

  ~FakeMultipartUploadRequest() override {}

  std::string GetRequestType() const override { return "POST"; }

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
  std::string* const upload_content_type_;
  std::string* const upload_content_data_;
};

}  // namespace

class BaseRequestsTest : public testing::Test {
 public:
  BaseRequestsTest() : response_code_(net::HTTP_OK) {
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
    sender_.reset(new RequestSender(std::make_unique<DummyAuthService>(),
                                    test_shared_loader_factory_,
                                    task_environment_.GetMainThreadTaskRunner(),
                                    std::string(), /* custom user agent */
                                    TRAFFIC_ANNOTATION_FOR_TESTS));

    test_server_.RegisterRequestHandler(
        base::Bind(&BaseRequestsTest::HandleRequest, base::Unretained(this)));
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
  std::unique_ptr<network::mojom::NetworkServiceClient> network_service_client_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
  std::unique_ptr<RequestSender> sender_;
  net::EmbeddedTestServer test_server_;

  net::HttpStatusCode response_code_;
  std::string response_body_;
};

typedef BaseRequestsTest MultipartUploadRequestBaseTest;

TEST_F(BaseRequestsTest, ParseValidJson) {
  std::unique_ptr<base::Value> json(ParseJson(kValidJsonString));

  base::DictionaryValue* root_dict = nullptr;
  ASSERT_TRUE(json);
  ASSERT_TRUE(json->GetAsDictionary(&root_dict));

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("test", &int_value));
  EXPECT_EQ(123, int_value);
}

TEST_F(BaseRequestsTest, ParseInvalidJson) {
  EXPECT_FALSE(ParseJson(kInvalidJsonString));
}

TEST_F(BaseRequestsTest, UrlFetchRequestBaseResponseCodeOverride) {
  response_code_ = net::HTTP_FORBIDDEN;
  response_body_ =
      "{\"error\": {\n"
      "  \"errors\": [\n"
      "   {\n"
      "    \"domain\": \"usageLimits\",\n"
      "    \"reason\": \"rateLimitExceeded\",\n"
      "    \"message\": \"Rate Limit Exceeded\"\n"
      "   }\n"
      "  ],\n"
      "  \"code\": 403,\n"
      "  \"message\": \"Rate Limit Exceeded\"\n"
      " }\n"
      "}\n";

  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
  base::RunLoop run_loop;
  sender_->StartRequestWithAuthRetry(std::make_unique<FakeUrlFetchRequest>(
      sender_.get(),
      test_util::CreateQuitCallback(
          &run_loop, test_util::CreateCopyResultCallback(&error)),
      test_server_.base_url()));
  run_loop.Run();

  // HTTP_FORBIDDEN (403) is overridden by the error reason.
  EXPECT_EQ(HTTP_SERVICE_UNAVAILABLE, error);
}

TEST_F(MultipartUploadRequestBaseTest, Basic) {
  response_code_ = net::HTTP_OK;
  response_body_ = "{\"kind\": \"drive#file\", \"id\": \"file_id\"}";
  std::unique_ptr<google_apis::FileResource> file;
  DriveApiErrorCode error = DRIVE_OTHER_ERROR;
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
