// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_loader_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/fake_network.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerScriptLoaderFactoryTest : public testing::Test {
 public:
  ServiceWorkerScriptLoaderFactoryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        interceptor_(base::BindRepeating(&FakeNetwork::HandleRequest,
                                         base::Unretained(&fake_network_))) {}
  ~ServiceWorkerScriptLoaderFactoryTest() override = default;

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    ServiceWorkerContextCore* context = helper_->context();

    scope_ = GURL("https://host/scope");
    script_url_ = GURL("https://host/script.js");

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = ServiceWorkerRegistration::Create(
        options,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_)),
        1L /* registration_id */, context->AsWeakPtr(),
        blink::mojom::AncestorFrameType::kNormalFrame);
    version_ = CreateNewServiceWorkerVersion(
        context->registry(), registration_.get(), script_url_,
        blink::mojom::ScriptType::kClassic);
    DCHECK(version_);

    worker_host_ = CreateServiceWorkerHost(helper_->mock_render_process_id(),
                                           true /* is_parent_frame_secure */,
                                           *version_, context->AsWeakPtr());

    factory_ = std::make_unique<ServiceWorkerScriptLoaderFactory>(
        helper_->context()->AsWeakPtr(), worker_host_->GetWeakPtr(),
        helper_->GetNetworkFactory());
  }

 protected:
  mojo::PendingRemote<network::mojom::URLLoader> CreateTestLoaderAndStart(
      network::TestURLLoaderClient* client) {
    mojo::PendingRemote<network::mojom::URLLoader> loader;
    network::ResourceRequest resource_request;
    resource_request.url = script_url_;
    resource_request.destination =
        network::mojom::RequestDestination::kServiceWorker;
    factory_->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0 /* request_id */,
        network::mojom::kURLLoadOptionNone, resource_request,
        client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    return loader;
  }

  BrowserTaskEnvironment task_environment_;
  FakeNetwork fake_network_;
  URLLoaderInterceptor interceptor_;

  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  GURL scope_;
  GURL script_url_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  std::unique_ptr<ServiceWorkerHost> worker_host_;
  std::unique_ptr<ServiceWorkerScriptLoaderFactory> factory_;
};

TEST_F(ServiceWorkerScriptLoaderFactoryTest, Success) {
  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
}

TEST_F(ServiceWorkerScriptLoaderFactoryTest, Redundant) {
  version_->SetStatus(ServiceWorkerVersion::REDUNDANT);

  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

TEST_F(ServiceWorkerScriptLoaderFactoryTest, NoWorkerHost) {
  worker_host_.reset();

  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

TEST_F(ServiceWorkerScriptLoaderFactoryTest, ContextDestroyed) {
  helper_->ShutdownContext();
  base::RunLoop().RunUntilIdle();

  network::TestURLLoaderClient client;
  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(&client);
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

// This tests copying script and creating resume type
// ServiceWorkerNewScriptLoaders.
class ServiceWorkerScriptLoaderFactoryCopyResumeTest
    : public ServiceWorkerScriptLoaderFactoryTest {
 public:
  ~ServiceWorkerScriptLoaderFactoryCopyResumeTest() override = default;

  void SetUp() override {
    ServiceWorkerScriptLoaderFactoryTest::SetUp();
    WriteToDiskCacheWithIdSync(helper_->context()->GetStorageControl(),
                               script_url_, kOldResourceId, kOldHeaders,
                               kOldData, std::string());
  }

  void CheckResponse(const std::string& expected_body) {
    // The response should also be stored in the storage.
    EXPECT_TRUE(ServiceWorkerUpdateCheckTestUtils::VerifyStoredResponse(
        version_->script_cache_map()->LookupResourceId(script_url_),
        helper_->context()->GetStorageControl(), expected_body));

    EXPECT_TRUE(client_.has_received_response());
    EXPECT_TRUE(client_.response_body().is_valid());

    std::string response;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(client_.response_body_release(), &response));
    EXPECT_EQ(expected_body, response);
  }

 protected:
  network::TestURLLoaderClient client_;
  const std::vector<std::pair<std::string, std::string>> kOldHeaders = {
      {"Content-Type", "text/javascript"},
      {"Content-Length", "15"}};
  const std::string kOldData = "old-script-data";
  const int64_t kOldResourceId = 1;
  const int64_t kNewResourceId = 2;
};

// Tests scripts are copied and loaded locally when compared to be
// identical in update check.
TEST_F(ServiceWorkerScriptLoaderFactoryCopyResumeTest, CopyScript) {
  ServiceWorkerUpdateCheckTestUtils::SetComparedScriptInfoForVersion(
      script_url_, kOldResourceId,
      ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical, nullptr,
      version_.get());

  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(&client_);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // Checks the received response data.
  CheckResponse(kOldData);
}

// Tests loader factory creates resume type ServiceWorkerNewScriptLoader to
// continue paused download in update check.
TEST_F(ServiceWorkerScriptLoaderFactoryCopyResumeTest,
       CreateResumeTypeScriptLoader) {
  const std::string kNewHeaders =
      "HTTP/1.0 200 OK\0Content-Type: text/javascript\0Content-Length: 0\0\0";
  const std::string kNewData;

  ServiceWorkerUpdateCheckTestUtils::CreateAndSetComparedScriptInfoForVersion(
      script_url_, 0, kNewHeaders, kNewData, kOldResourceId, kNewResourceId,
      helper_.get(), ServiceWorkerUpdatedScriptLoader::LoaderState::kCompleted,
      ServiceWorkerUpdatedScriptLoader::WriterState::kCompleted,
      ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent,
      version_.get(), nullptr);

  mojo::PendingRemote<network::mojom::URLLoader> loader =
      CreateTestLoaderAndStart(&client_);
  client_.RunUntilComplete();

  EXPECT_EQ(net::OK, client_.completion_status().error_code);

  // The received response has no body because kNewData is empty.
  CheckResponse(kNewData);
}

}  // namespace content
