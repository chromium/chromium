// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/base_requests.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
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

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeUrlFetchRequest : public UrlFetchRequestBase {
 public:
  FakeUrlFetchRequest(RequestSender* sender,
                      PrepareCallback callback,
                      const GURL& url)
      : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
        callback_(std::move(callback)),
        url_(url) {}

  ~FakeUrlFetchRequest() override {}

 protected:
  GURL GetURL() const override { return url_; }
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override {
    std::move(callback_).Run(GetErrorCode());
  }
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override {
    std::move(callback_).Run(code);
  }
  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override {
    if (reason == "rateLimitExceeded")
      return google_apis::HTTP_SERVICE_UNAVAILABLE;
    return code;
  }

  bool IsSuccessfulErrorCode(ApiErrorCode error) override {
    return error == HTTP_SUCCESS;
  }

  PrepareCallback callback_;
  GURL url_;
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
        &BaseRequestsTest::HandleRequest, base::Unretained(this)));
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

  net::HttpStatusCode response_code_;
  std::string response_body_;
};

TEST_F(BaseRequestsTest, ParseValidJson) {
  std::unique_ptr<base::Value> json(ParseJson(kValidJsonString));

  ASSERT_TRUE(json);
  base::Value::Dict* root_dict = json->GetIfDict();
  ASSERT_TRUE(root_dict);

  std::optional<int> int_value = root_dict->FindInt("test");
  ASSERT_TRUE(int_value.has_value());
  EXPECT_EQ(123, *int_value);
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

  ApiErrorCode error = OTHER_ERROR;
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

TEST(BaseRequestsHttpRequestMethodEnumTest, ConvertsToString) {
  EXPECT_EQ(HttpRequestMethodToString(HttpRequestMethod::kGet), "GET");
  EXPECT_EQ(HttpRequestMethodToString(HttpRequestMethod::kPost), "POST");
  EXPECT_EQ(HttpRequestMethodToString(HttpRequestMethod::kPut), "PUT");
  EXPECT_EQ(HttpRequestMethodToString(HttpRequestMethod::kPatch), "PATCH");
  EXPECT_EQ(HttpRequestMethodToString(HttpRequestMethod::kDelete), "DELETE");
}

}  // namespace google_apis
