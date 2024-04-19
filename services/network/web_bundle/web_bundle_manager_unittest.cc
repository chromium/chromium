// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/web_package/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/web_bundle/web_bundle_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const char kInitiatorUrl[] = "https://example.com/";
const char kBundleUrl[] = "https://example.com/bundle.wbn";
const char kResourceUrl[] = "https://example.com/a.txt";
const char kQuotaExceededErrorMessage[] =
    "Memory quota exceeded. Currently, there is an upper limit on the total "
    "size of subresource web bundles in a process. See "
    "https://crbug.com/1154140 for more details.";

const int32_t process_id1 = 100;
const int32_t process_id2 = 200;

std::string CreateSmallBundleString() {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  auto bundle = builder.CreateBundle();
  return std::string(reinterpret_cast<const char*>(bundle.data()),
                     bundle.size());
}

class TestWebBundleHandle : public mojom::WebBundleHandle {
 public:
  explicit TestWebBundleHandle(
      mojo::PendingReceiver<mojom::WebBundleHandle> receiver) {
    web_bundle_handles_.Add(this, std::move(receiver));
  }

  const std::optional<std::pair<mojom::WebBundleErrorType, std::string>>&
  last_bundle_error() const {
    return last_bundle_error_;
  }

  void RunUntilBundleError() {
    if (last_bundle_error_.has_value())
      return;
    base::RunLoop run_loop;
    quit_closure_for_bundle_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // mojom::WebBundleHandle
  void Clone(mojo::PendingReceiver<mojom::WebBundleHandle> receiver) override {
    web_bundle_handles_.Add(this, std::move(receiver));
  }

  void OnWebBundleError(mojom::WebBundleErrorType type,
                        const std::string& message) override {
    last_bundle_error_ = std::make_pair(type, message);
    if (quit_closure_for_bundle_error_)
      std::move(quit_closure_for_bundle_error_).Run();
  }

  void OnWebBundleLoadFinished(bool success) override {}

 private:
  std::optional<std::pair<mojom::WebBundleErrorType, std::string>>
      last_bundle_error_;
  base::OnceClosure quit_closure_for_bundle_error_;

  mojo::ReceiverSet<network::mojom::WebBundleHandle> web_bundle_handles_;
};

std::tuple<base::WeakPtr<WebBundleURLLoaderFactory>,
           std::unique_ptr<TestWebBundleHandle>>
CreateWebBundleLoaderFactory(WebBundleManager& manager, int32_t process_id) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  mojo::PendingRemote<mojom::WebBundleHandle> remote_handle;
  std::unique_ptr<TestWebBundleHandle> handle =
      std::make_unique<TestWebBundleHandle>(
          remote_handle.InitWithNewPipeAndPassReceiver());
  ResourceRequest::WebBundleTokenParams create_params(GURL(kBundleUrl), token,
                                                      std::move(remote_handle));
  base::WeakPtr<WebBundleURLLoaderFactory> factory =
      manager.CreateWebBundleURLLoaderFactory(
          GURL(kBundleUrl), create_params, process_id,
          /*devtools_observer=*/mojo::PendingRemote<mojom::DevToolsObserver>(),
          /*devtools_request_id=*/std::nullopt, CrossOriginEmbedderPolicy(),
          /*coep_reporter=*/nullptr);

  return std::forward_as_tuple(std::move(factory), std::move(handle));
}

mojo::ScopedDataPipeProducerHandle SetBundleStream(
    WebBundleURLLoaderFactory& factory) {
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  CHECK_EQ(MOJO_RESULT_OK, CreateDataPipe(nullptr, producer, consumer));
  factory.SetBundleStream(std::move(consumer));
  return producer;
}

std::tuple<mojo::Remote<network::mojom::URLLoader>,
           std::unique_ptr<network::TestURLLoaderClient>>
StartSubresourceLoad(WebBundleURLLoaderFactory& factory) {
  mojo::Remote<network::mojom::URLLoader> loader;
  auto client = std::make_unique<network::TestURLLoaderClient>();
  network::ResourceRequest request;
  request.url = GURL(kResourceUrl);
  request.method = "GET";
  request.request_initiator = url::Origin::Create(GURL(kInitiatorUrl));
  request.web_bundle_token_params = ResourceRequest::WebBundleTokenParams();
  request.web_bundle_token_params->bundle_url = GURL(kBundleUrl);
  factory.StartLoader(WebBundleURLLoaderFactory::CreateURLLoader(
      loader.BindNewPipeAndPassReceiver(), request, client->CreateRemote(),
      mojo::Remote<mojom::TrustedHeaderClient>(), base::Time::Now(),
      base::TimeTicks::Now(), base::DoNothing()));
  return std::forward_as_tuple(std::move(loader), std::move(client));
}

}  // namespace

class WebBundleManagerTest : public testing::Test {
 public:
  WebBundleManagerTest() = default;
  ~WebBundleManagerTest() override = default;

 protected:
  void SetMaxMemoryPerProces(WebBundleManager& manager,
                             uint64_t max_memory_per_process) {
    manager.set_max_memory_per_process_for_testing(max_memory_per_process);
  }

  bool IsPendingLoadersEmpty(const WebBundleManager& manager,
                             WebBundleManager::Key key) const {
    return manager.IsPendingLoadersEmptyForTesting(key);
  }

  base::WeakPtr<WebBundleURLLoaderFactory> GetWebBundleURLLoaderFactory(
      WebBundleManager& manager,
      const ResourceRequest::WebBundleTokenParams& params,
      int32_t process_id) {
    return manager.GetWebBundleURLLoaderFactory(
        manager.GetKey(params, process_id));
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleManagerTest, NoFactoryExistsForDifferentProcessId) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      handle.InitWithNewPipeAndPassReceiver();
  ResourceRequest::WebBundleTokenParams create_params(GURL(kBundleUrl), token,
                                                      std::move(handle));

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), create_params, process_id1,
      /*devtools_observer=*/mojo::PendingRemote<mojom::DevToolsObserver>(),
      /*devtools_request_id=*/std::nullopt, CrossOriginEmbedderPolicy(),
      /*coep_reporter=*/nullptr);
  ASSERT_TRUE(factory);

  ResourceRequest::WebBundleTokenParams find_params(GURL(kBundleUrl), token,
                                                    mojom::kInvalidProcessId);
  ASSERT_TRUE(GetWebBundleURLLoaderFactory(manager, find_params, process_id1));
  ASSERT_FALSE(GetWebBundleURLLoaderFactory(manager, find_params, process_id2));
}

TEST_F(WebBundleManagerTest, UseProcesIdInTokenParamsForRequestsFromBrowser) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      handle.InitWithNewPipeAndPassReceiver();
  ResourceRequest::WebBundleTokenParams create_params(GURL(kBundleUrl), token,
                                                      std::move(handle));

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), create_params, process_id1,
      /*devtools_observer=*/mojo::PendingRemote<mojom::DevToolsObserver>(),
      /*devtools_request_id=*/std::nullopt, CrossOriginEmbedderPolicy(),
      /*coep_reporter=*/nullptr);
  ASSERT_TRUE(factory);

  ResourceRequest::WebBundleTokenParams find_params1(GURL(kBundleUrl), token,
                                                     process_id1);
  ASSERT_TRUE(GetWebBundleURLLoaderFactory(manager, find_params1,
                                           mojom::kBrowserProcessId));
  ASSERT_FALSE(
      GetWebBundleURLLoaderFactory(manager, find_params1, process_id2));
  ResourceRequest::WebBundleTokenParams find_params2(GURL(kBundleUrl), token,
                                                     process_id2);
  ASSERT_FALSE(GetWebBundleURLLoaderFactory(manager, find_params2,
                                            mojom::kBrowserProcessId));
}

TEST_F(WebBundleManagerTest, RemoveFactoryWhenDisconnected) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  ResourceRequest::WebBundleTokenParams find_params(GURL(kBundleUrl), token,
                                                    mojom::kInvalidProcessId);
  {
    mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
    mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
        handle.InitWithNewPipeAndPassReceiver();
    ResourceRequest::WebBundleTokenParams create_params(GURL(kBundleUrl), token,
                                                        std::move(handle));

    auto factory = manager.CreateWebBundleURLLoaderFactory(
        GURL(kBundleUrl), create_params, process_id1,
        /*devtools_observer=*/mojo::PendingRemote<mojom::DevToolsObserver>(),
        /*devtools_request_id=*/std::nullopt, CrossOriginEmbedderPolicy(),
        /*coep_reporter=*/nullptr);
    ASSERT_TRUE(factory);
    ASSERT_TRUE(
        GetWebBundleURLLoaderFactory(manager, find_params, process_id1));
    // Getting out of scope to delete |receiver|.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetWebBundleURLLoaderFactory(manager, find_params, process_id1))
      << "The manager should remove a factory when the handle is disconnected.";
}

TEST_F(WebBundleManagerTest,
       SubresourceRequestArrivesEarlierThanBundleRequest) {
  // Confirm that a subresource is correctly loaded, regardless of the arrival
  // order of a webbundle request and a subresource request in the bundle.
  //
  // For example, given that we have the following main document:
  //
  // <script type=webbundle>
  //   {
  //     "source": "bundle.wbn",
  //     "resources": ["a.png", ...]
  //   }
  // </script>
  // <img src="a.png">
  //
  // In this case, a network service should receive the following two resource
  // requests:
  //
  // 1. A request for a bundle, "bundle.wbn"
  // 2. A request for a subresource, "a.png".
  //
  // Usually, the request 1 arrives earlier than the request 2,
  // however, the arrival order is not guaranteed. The subresource should be
  // loaded even if the request 2 arrives earlier.
  //
  // Since it would be non-trivial to reproduce this scenario in a reliable way,
  // we simulate this scenario by calling WebBundleManager member functions
  // manually here, as network::URLLoaderFactory does, and verify that the
  // subresource request is correctly loaded.
  //
  // TODO(crbug.com/40161416): Find a better way to test this scenario.

  WebBundleManager manager;

  // Simulate that two subresource requests arrives at first, one directly from
  // a renderer and one through the browser, calling
  // WebBundleManager::StartSubresourceRequest.
  base::UnguessableToken token = base::UnguessableToken::Create();

  struct TestRequest {
    int32_t request_process_id;
    int32_t token_params_process_id;
    mojo::Remote<network::mojom::URLLoader> loader;
    std::unique_ptr<network::TestURLLoaderClient> client;
  } test_requests[] = {
      {process_id1, mojom::kInvalidProcessId},
      {mojom::kBrowserProcessId, process_id1},
  };

  for (TestRequest& req : test_requests) {
    network::ResourceRequest request;
    request.url = GURL(kResourceUrl);
    request.method = "GET";
    request.request_initiator = url::Origin::Create(GURL(kInitiatorUrl));
    request.web_bundle_token_params = ResourceRequest::WebBundleTokenParams(
        GURL(kBundleUrl), token, req.token_params_process_id);

    req.client = std::make_unique<network::TestURLLoaderClient>();

    manager.StartSubresourceRequest(req.loader.BindNewPipeAndPassReceiver(),
                                    request, req.client->CreateRemote(),
                                    req.request_process_id,
                                    mojo::Remote<mojom::TrustedHeaderClient>());
  }

  // Simulate that a webbundle request arrives, calling
  // WebBundleManager::CreateWebBundleURLLoaderFactory.
  ResourceRequest::WebBundleTokenParams token_params;
  token_params.bundle_url = GURL(kBundleUrl);
  token_params.token = token;
  token_params.handle = mojo::PendingRemote<network::mojom::WebBundleHandle>();
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      token_params.handle.InitWithNewPipeAndPassReceiver();

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), token_params, process_id1,
      /*devtools_observer=*/mojo::PendingRemote<mojom::DevToolsObserver>(),
      /*devtools_request_id=*/std::nullopt, CrossOriginEmbedderPolicy(),
      /*coep_reporter=*/nullptr);

  // Then, simulate that the bundle is loaded from the network, calling
  // SetBundleStream manually.
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  ASSERT_EQ(CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);
  factory->SetBundleStream(std::move(consumer));

  mojo::BlockingCopyFromString(CreateSmallBundleString(), producer);

  producer.reset();

  // Confirm that subresources are correctly loaded.
  for (const TestRequest& req : test_requests) {
    req.client->RunUntilComplete();

    EXPECT_EQ(net::OK, req.client->completion_status().error_code);
    EXPECT_TRUE(req.client->response_head()->is_web_bundle_inner_response);
    std::string body;
    EXPECT_TRUE(
        mojo::BlockingCopyToString(req.client->response_body_release(), &body));
    EXPECT_EQ("body", body);
  }
}

TEST_F(WebBundleManagerTest, CleanUpPendingLoadersIfWebBundleRequestIsBlocked) {
  // The test is similar to
  // WebBundleManagerTest::SubresourceRequestArrivesEarlierThanBundleRequest.
  // The difference is that a request for a WebBundle doesn't reach to Network
  // Service. See crbug.com/1355162 for the context.
  //
  // Ensure that pending subresource URL Loaders are surely cleaned up from
  // WebBundleManager even if a request for the WebBundle never comes to the
  // Network Service.

  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  int32_t process_id = mojom::kInvalidProcessId;

  network::ResourceRequest request;
  request.url = GURL(kResourceUrl);
  request.method = "GET";
  request.request_initiator = url::Origin::Create(GURL(kInitiatorUrl));
  request.web_bundle_token_params = ResourceRequest::WebBundleTokenParams(
      GURL(kBundleUrl), token, process_id);

  mojo::Remote<network::mojom::URLLoader> loader;
  auto client = std::make_unique<network::TestURLLoaderClient>();

  manager.StartSubresourceRequest(
      loader.BindNewPipeAndPassReceiver(), request, client->CreateRemote(),
      mojom::kInvalidProcessId, mojo::Remote<mojom::TrustedHeaderClient>());

  ASSERT_FALSE(IsPendingLoadersEmpty(manager, {process_id, token}));

  // Let the subresource request fails, simulating that a renderer cancels a
  // subresource loader when they know the bundle is blocked before the bundle
  // request reaches to the Network Service.
  loader.reset();

  client->RunUntilDisconnect();

  EXPECT_TRUE(IsPendingLoadersEmpty(manager, {process_id, token}));
}

TEST_F(WebBundleManagerTest, MemoryQuota_StartRequestAfterError) {
  base::HistogramTester histogram_tester;
  WebBundleManager manager;

  std::string bundle = CreateSmallBundleString();
  SetMaxMemoryPerProces(manager, bundle.size() - 1);

  // Start loading the bundle which size is larger than the quota.
  auto [factory, handle] = CreateWebBundleLoaderFactory(manager, process_id1);
  // Input the bundle to the factory.
  auto producer = SetBundleStream(*factory);
  mojo::BlockingCopyFromString(bundle, producer);
  producer.reset();
  // TestWebBundleHandle must receive the error.
  handle->RunUntilBundleError();
  ASSERT_TRUE(handle->last_bundle_error().has_value());
  EXPECT_EQ(handle->last_bundle_error()->first,
            mojom::WebBundleErrorType::kMemoryQuotaExceeded);
  EXPECT_EQ(handle->last_bundle_error()->second, kQuotaExceededErrorMessage);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
          kMemoryQuotaExceeded,
      1);

  // Start the subresource request after triggering the quota error.
  auto [loader, client] = StartSubresourceLoad(*factory);
  // The subresource request must fail.
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            client->completion_status().error_code);
}

TEST_F(WebBundleManagerTest, MemoryQuota_StartRequestBeforeReceivingBundle) {
  WebBundleManager manager;

  std::string bundle = CreateSmallBundleString();
  SetMaxMemoryPerProces(manager, bundle.size() - 1);

  // Start loading the bundle which size is larger than the quota.
  auto [factory, handle] = CreateWebBundleLoaderFactory(manager, process_id1);

  // Start the subresource request.
  auto [loader, client] = StartSubresourceLoad(*factory);

  // Input the bundle to the factory after starting the subresource load.
  auto producer = SetBundleStream(*factory);
  mojo::BlockingCopyFromString(bundle, producer);
  producer.reset();

  // TestWebBundleHandle must receive the error.
  handle->RunUntilBundleError();
  ASSERT_TRUE(handle->last_bundle_error().has_value());
  EXPECT_EQ(handle->last_bundle_error()->first,
            mojom::WebBundleErrorType::kMemoryQuotaExceeded);
  EXPECT_EQ(handle->last_bundle_error()->second, kQuotaExceededErrorMessage);

  // The subresource request must fail.
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            client->completion_status().error_code);
}

TEST_F(WebBundleManagerTest, MemoryQuota_QuotaErrorWhileReadingBody) {
  WebBundleManager manager;

  // Create a not small size bundle to trigger the quota error while reading the
  // body of the subresource.
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      std::string(10000, 'X'));
  std::vector<uint8_t> bundle = builder.CreateBundle();
  std::string bundle_string =
      std::string(reinterpret_cast<const char*>(bundle.data()), bundle.size());

  // Set the max memory to trigger the quota error while reading the body of
  // the subresource.
  // Note: When WebBundleParser::MetadataParser parses the metadata, it reads
  // "[fallback URL length] + kMaxSectionLengthsCBORSize(8192) +
  // kMaxCBORItemHeaderSize(9) * 2" bytes after reading 10 bytes of
  // kBundleMagicBytes and 5 bytes of kVersionB1MagicBytes and (1, 2, 3, 5 or 9)
  // bytes of CBORHeader of fallback URL. If we set the quota smaller than
  // this value, the quota error is triggered while parsing the metadata.
  uint64_t required_bytes_for_parsing_metadata =
      10 +                        // size of BundleMagicBytes
      5 +                         // size of VersionB1MagicBytes
      2 +                         // CBORHeader size for kResourceUrl string
      sizeof(kResourceUrl) - 1 +  // len(kResourceUrl)
      8192 +                      // kMaxSectionLengthsCBORSize
      9 * 2;                      //  kMaxCBORItemHeaderSize * 2
  SetMaxMemoryPerProces(manager, required_bytes_for_parsing_metadata);
  ASSERT_GT(bundle_string.size(), required_bytes_for_parsing_metadata);

  // Start loading the bundle.
  auto [factory, handle] = CreateWebBundleLoaderFactory(manager, process_id1);

  // Start the subresource request.
  auto [loader, client] = StartSubresourceLoad(*factory);

  // Input the first |required_bytes_for_parsing_metadata| bytes of the bundle
  // to the factory.
  auto producer = SetBundleStream(*factory);
  mojo::BlockingCopyFromString(
      bundle_string.substr(0, required_bytes_for_parsing_metadata), producer);

  // The subresource request must be able to receive the response header.
  client->RunUntilResponseReceived();
  EXPECT_TRUE(client->has_received_response());

  // Input the remaining bytes of the bundle to the factory.
  mojo::BlockingCopyFromString(
      bundle_string.substr(required_bytes_for_parsing_metadata), producer);
  producer.reset();

  // TestWebBundleHandle must receive the error.
  handle->RunUntilBundleError();
  ASSERT_TRUE(handle->last_bundle_error().has_value());
  EXPECT_EQ(handle->last_bundle_error()->first,
            mojom::WebBundleErrorType::kMemoryQuotaExceeded);
  EXPECT_EQ(handle->last_bundle_error()->second, kQuotaExceededErrorMessage);

  // The subresource request must receive the error.
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            client->completion_status().error_code);
}

TEST_F(WebBundleManagerTest, MemoryQuota_QuotaErrorWhileParsingManifest) {
  WebBundleManager manager;

  std::string bundle = CreateSmallBundleString();

  // Set the max memory to trigger the quota error while reading the manifest of
  // the web bundle.
  SetMaxMemoryPerProces(manager, 10);

  // Start loading the bundle.
  auto [factory, handle] = CreateWebBundleLoaderFactory(manager, process_id1);

  // Input the bundle to the factory byte by byte.
  auto producer = SetBundleStream(*factory);
  for (size_t i = 0; i < bundle.size(); ++i) {
    mojo::BlockingCopyFromString(bundle.substr(i, 1), producer);
    // Run the RunLoop to trigger OnDataAvailable() byte by byte.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  producer.reset();

  // TestWebBundleHandle must receive the error.
  handle->RunUntilBundleError();
  ASSERT_TRUE(handle->last_bundle_error().has_value());
  EXPECT_EQ(handle->last_bundle_error()->first,
            mojom::WebBundleErrorType::kMemoryQuotaExceeded);
  EXPECT_EQ(handle->last_bundle_error()->second, kQuotaExceededErrorMessage);

  // Start the subresource request.
  auto [loader, client] = StartSubresourceLoad(*factory);

  // The subresource request must fail.
  client->RunUntilComplete();
  EXPECT_FALSE(client->has_received_response());
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            client->completion_status().error_code);
}

TEST_F(WebBundleManagerTest, MemoryQuota_ProcessIsolation) {
  base::HistogramTester histogram_tester;
  WebBundleManager manager;

  std::string bundle = CreateSmallBundleString();

  // Set the max memory to trigger the quota error while loading the third
  // web bundle.
  SetMaxMemoryPerProces(manager, bundle.size() * 2.5);

  // Start loading the first web bundle in the process 1.
  auto [factory1_1, handle1_1] =
      CreateWebBundleLoaderFactory(manager, process_id1);
  auto producer1_1 = SetBundleStream(*factory1_1);
  mojo::BlockingCopyFromString(bundle, producer1_1);
  producer1_1.reset();

  // Start loading the subresource from the first web bundle.
  auto [loader1_1, client1_1] = StartSubresourceLoad(*factory1_1);
  // Confirm that the subresource is correctly loaded.
  client1_1->RunUntilComplete();
  EXPECT_EQ(net::OK, client1_1->completion_status().error_code);
  EXPECT_TRUE(client1_1->response_head()->is_web_bundle_inner_response);
  std::string body1_1;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client1_1->response_body_release(), &body1_1));
  EXPECT_EQ("body", body1_1);
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ReceivedSize",
                                      bundle.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::kSuccess, 1);

  // Start loading the second web bundle in the process 1.
  auto [factory1_2, handle1_2] =
      CreateWebBundleLoaderFactory(manager, process_id1);
  auto producer1_2 = SetBundleStream(*factory1_2);
  mojo::BlockingCopyFromString(bundle, producer1_2);
  producer1_2.reset();

  // Start loading the subresource from the second web bundle.
  auto [loader1_2, client1_2] = StartSubresourceLoad(*factory1_2);
  // Confirm that the subresource is correctly loaded.
  client1_2->RunUntilComplete();
  EXPECT_EQ(net::OK, client1_2->completion_status().error_code);
  EXPECT_TRUE(client1_2->response_head()->is_web_bundle_inner_response);
  std::string body1_2;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client1_2->response_body_release(), &body1_2));
  EXPECT_EQ("body", body1_2);
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ReceivedSize",
                                      bundle.size(), 2);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::kSuccess, 2);

  // Start loading the third web bundle in the process 1.
  auto [factory1_3, handle1_3] =
      CreateWebBundleLoaderFactory(manager, process_id1);
  auto producer1_3 = SetBundleStream(*factory1_3);
  mojo::BlockingCopyFromString(bundle, producer1_3);
  producer1_3.reset();
  // TestWebBundleHandle must receive the error.
  handle1_3->RunUntilBundleError();
  ASSERT_TRUE(handle1_3->last_bundle_error().has_value());
  EXPECT_EQ(handle1_3->last_bundle_error()->first,
            mojom::WebBundleErrorType::kMemoryQuotaExceeded);
  EXPECT_EQ(handle1_3->last_bundle_error()->second, kQuotaExceededErrorMessage);

  // Start loading the subresource from the second web bundle.
  auto [loader1_3, client1_3] = StartSubresourceLoad(*factory1_3);
  // The subresource request must fail.
  client1_3->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            client1_3->completion_status().error_code);
  histogram_tester.ExpectBucketCount(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
          kMemoryQuotaExceeded,
      1);

  // Start loading the third web bundle in the process 2.
  auto [factory2, handle2] = CreateWebBundleLoaderFactory(manager, process_id2);
  auto producer2 = SetBundleStream(*factory2);
  mojo::BlockingCopyFromString(bundle, producer2);
  producer2.reset();
  // Start loading the subresource from the third web bundle.
  auto [loader2, client2] = StartSubresourceLoad(*factory2);
  // Confirm that the subresource is correctly loaded.
  client2->RunUntilComplete();
  EXPECT_EQ(net::OK, client2->completion_status().error_code);
  EXPECT_TRUE(client2->response_head()->is_web_bundle_inner_response);
  std::string body2;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client2->response_body_release(), &body2));
  EXPECT_EQ("body", body2);
  histogram_tester.ExpectUniqueSample("SubresourceWebBundles.ReceivedSize",
                                      bundle.size(), 3);
  histogram_tester.ExpectBucketCount(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::kSuccess, 3);

  // Reset handles and RunUntilIdle to trigger MaxMemoryUsagePerProcess
  // histogram count.
  handle1_1.reset();
  handle1_2.reset();
  handle1_3.reset();
  handle2.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "SubresourceWebBundles.MaxMemoryUsagePerProcess", bundle.size() * 2, 1);
  histogram_tester.ExpectBucketCount(
      "SubresourceWebBundles.MaxMemoryUsagePerProcess", bundle.size(), 1);
}

TEST_F(WebBundleManagerTest, WebBundleURLRedirection) {
  base::HistogramTester histogram_tester;
  WebBundleManager manager;

  base::UnguessableToken token = base::UnguessableToken::Create();
  mojo::PendingRemote<mojom::WebBundleHandle> remote_handle;
  std::unique_ptr<TestWebBundleHandle> handle =
      std::make_unique<TestWebBundleHandle>(
          remote_handle.InitWithNewPipeAndPassReceiver());
  ResourceRequest::WebBundleTokenParams create_params(GURL(kBundleUrl), token,
                                                      std::move(remote_handle));

  // Create a WebBundleURLLoaderFactory where bundle request URL is different
  // from WebBundleTokenParams::bundle_url. This happens when WebBundle request
  // is readirected by WebRequest extension API.
  GURL redirected_bundle_url("https://redirected.example.com/bundle.wbn");
  base::WeakPtr<WebBundleURLLoaderFactory> factory =
      manager.CreateWebBundleURLLoaderFactory(
          redirected_bundle_url, create_params, process_id1,
          /*devtools_observer=*/{},
          /*devtools_request_id=*/std::nullopt, CrossOriginEmbedderPolicy(),
          /*coep_reporter=*/nullptr);

  // TestWebBundleHandle must receive an error.
  handle->RunUntilBundleError();
  ASSERT_TRUE(handle->last_bundle_error().has_value());
  EXPECT_EQ(handle->last_bundle_error()->first,
            mojom::WebBundleErrorType::kWebBundleRedirected);
  histogram_tester.ExpectUniqueSample(
      "SubresourceWebBundles.LoadResult",
      WebBundleURLLoaderFactory::SubresourceWebBundleLoadResult::
          kWebBundleRedirected,
      1);

  // Subresource request must fail.
  auto [loader, client] = StartSubresourceLoad(*factory);
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_INVALID_WEB_BUNDLE,
            client->completion_status().error_code);
}

}  // namespace network
