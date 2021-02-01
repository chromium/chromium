// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle_manager.h"

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/web_package/test_support/web_bundle_builder.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/web_bundle_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const char kInitiatorUrl[] = "https://example.com/";
const char kBundleUrl[] = "https://example.com/bundle.wbn";
const char kResourceUrl[] = "https://example.com/a.txt";

const int32_t process_id1 = 100;
const int32_t process_id2 = 200;

std::vector<uint8_t> CreateSmallBundle() {
  web_package::test::WebBundleBuilder builder(kResourceUrl,
                                              "" /* manifest_url */);
  builder.AddExchange(kResourceUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "body");
  return builder.CreateBundle();
}

}  // namespace

class WebBundleManagerTest : public testing::Test {
 public:
  WebBundleManagerTest() = default;
  ~WebBundleManagerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleManagerTest, NoFactoryExistsForDifferentProcessId) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = process_id1;
  mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      handle.InitWithNewPipeAndPassReceiver();
  ResourceRequest::WebBundleTokenParams create_params(token, std::move(handle));

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), create_params, factory_params);
  ASSERT_TRUE(factory);

  ResourceRequest::WebBundleTokenParams find_params(token,
                                                    mojom::kInvalidProcessId);
  ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(find_params,
                                                   factory_params->process_id));
  ASSERT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params, process_id2));
}

TEST_F(WebBundleManagerTest, UseProcesIdInTokenParamsForRequestsFromBrowser) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = process_id1;
  mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      handle.InitWithNewPipeAndPassReceiver();
  ResourceRequest::WebBundleTokenParams create_params(token, std::move(handle));

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), create_params, factory_params);
  ASSERT_TRUE(factory);

  ResourceRequest::WebBundleTokenParams find_params1(token, process_id1);
  ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(find_params1,
                                                   mojom::kBrowserProcessId));
  ASSERT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params1, process_id2));
  ResourceRequest::WebBundleTokenParams find_params2(token, process_id2);
  ASSERT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params2,
                                                    mojom::kBrowserProcessId));
}

TEST_F(WebBundleManagerTest, RemoveFactoryWhenDisconnected) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  ResourceRequest::WebBundleTokenParams find_params(token,
                                                    mojom::kInvalidProcessId);
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = 123;
  {
    mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
    mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
        handle.InitWithNewPipeAndPassReceiver();
    ResourceRequest::WebBundleTokenParams create_params(token,
                                                        std::move(handle));

    auto factory = manager.CreateWebBundleURLLoaderFactory(
        GURL(kBundleUrl), create_params, factory_params);
    ASSERT_TRUE(factory);
    ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(
        find_params, factory_params->process_id));
    // Getting out of scope to delete |receiver|.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params,
                                                    factory_params->process_id))
      << "The manager should remove a factory when the handle is disconnected.";
}

TEST_F(WebBundleManagerTest,
       SubresourceRequestArrivesEarlierThanBundleRequest) {
  // Confirm that a subresource is correctly loaded, regardless of the arrival
  // order of a webbundle request and a subresource request in the bundle.
  //
  // For example, given that we have the following main document:
  //
  // <link rel=webbundle href="https://example.com/bundle.wbn"
  // resources="https://example.com/a.txt">
  // <img src="https://example.com/a.txt">  # Please ignore that a.txt is weird
  // for <img>.
  //
  // In this case, a network service should receive the following two resource
  // requests:
  //
  // 1. A request for a bundle, "bundle.wbn"
  // 2. A request for a subresource, "a.txt".
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
  // TODO(crbug.com/1158709): Find a better way to test this scenario.

  WebBundleManager manager;

  // Simulate that a subresource request arrives at first,
  // calling WebBundleManager::AddPendingSubresouceRequest.
  base::UnguessableToken token = base::UnguessableToken::Create();
  network::ResourceRequest request;
  request.url = GURL(kResourceUrl);
  request.method = "GET";
  request.request_initiator = url::Origin::Create(GURL(kInitiatorUrl));

  mojo::Remote<network::mojom::URLLoader> loader;
  auto client = std::make_unique<network::TestURLLoaderClient>();

  manager.AddPendingSubresouceRequest(
      token, 123 /* process id */, loader.BindNewPipeAndPassReceiver(),
      0 /* routing_id */, 0 /* request_id */, 0 /* options */, request,
      client->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Simulate that a webbundle request arrives, calling
  // WebBundleManager::CreateWebBundleURLLoaderFactory.
  ResourceRequest::WebBundleTokenParams token_params;
  token_params.token = token;
  token_params.handle = mojo::PendingRemote<network::mojom::WebBundleHandle>();
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      token_params.handle.InitWithNewPipeAndPassReceiver();

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = 123;

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), token_params, factory_params);

  // Then, simulate that the bundle is loaded from the network, calling
  // SetBundleStream manually.
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  ASSERT_EQ(CreateDataPipe(nullptr, &producer, &consumer), MOJO_RESULT_OK);
  factory->SetBundleStream(std::move(consumer));

  std::vector<uint8_t> bundle = CreateSmallBundle();
  mojo::BlockingCopyFromString(
      std::string(reinterpret_cast<const char*>(bundle.data()), bundle.size()),
      producer);

  producer.reset();

  client->RunUntilComplete();

  // Confirm that a subresource is correctly loaded.
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ(client->response_head()->web_bundle_url, GURL(kBundleUrl));
  std::string body;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &body));
  EXPECT_EQ("body", body);
}

}  // namespace network
