// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/forwarded_race_network_request_url_loader_factory.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class ForwardedRaceNetworkRequestURLLoaderFactoryTest : public testing::Test {
 public:
  ForwardedRaceNetworkRequestURLLoaderFactoryTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

class MockURLLoader : public network::mojom::URLLoader {
 public:
  explicit MockURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver)
      : receiver_(this, std::move(receiver)) {}

  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    called_follow_redirect_ = true;
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    called_set_priority_ = true;
    priority_ = priority;
  }

  bool called_follow_redirect() const { return called_follow_redirect_; }
  bool called_set_priority() const { return called_set_priority_; }
  net::RequestPriority priority() const { return priority_; }

 private:
  mojo::Receiver<network::mojom::URLLoader> receiver_;
  bool called_follow_redirect_ = false;
  bool called_set_priority_ = false;
  net::RequestPriority priority_ = net::MINIMUM_PRIORITY;
};

// Test Case 1: Verify that SetPriority is successfully forwarded through the
// proxy when the proxy is enabled for main resources.
TEST_F(ForwardedRaceNetworkRequestURLLoaderFactoryTest, SetPriorityForwarded) {
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory factory(
      std::move(client_receiver), test_url_loader_factory_.GetSafeWeakWrapper(),
      /*is_main_resource=*/true);

  auto loader_receiver = factory.InitURLLoaderNewPipeAndPassReceiver();
  MockURLLoader mock_loader(std::move(loader_receiver));

  mojo::Remote<network::mojom::URLLoaderFactory> factory_remote;
  factory.Clone(factory_remote.BindNewPipeAndPassReceiver());

  network::ResourceRequest request;
  request.url = GURL("https://example.com/navigation");

  mojo::Remote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  auto client_receiver_for_request = client.InitWithNewPipeAndPassReceiver();

  factory_remote->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 0, 0, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag());

  factory_remote.FlushForTesting();

  // Call SetPriority on the renderer remote. It should be forwarded.
  loader->SetPriority(net::HIGHEST, 0);
  loader.FlushForTesting();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(mock_loader.called_set_priority());
  EXPECT_EQ(net::HIGHEST, mock_loader.priority());
}

// Test Case 2: Verify that FollowRedirect is blocked and triggers a BadMessage
// when the proxy is enabled for main resources.
TEST_F(ForwardedRaceNetworkRequestURLLoaderFactoryTest, FollowRedirectBlocked) {
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory factory(
      std::move(client_receiver), test_url_loader_factory_.GetSafeWeakWrapper(),
      /*is_main_resource=*/true);

  auto loader_receiver = factory.InitURLLoaderNewPipeAndPassReceiver();
  MockURLLoader mock_loader(std::move(loader_receiver));

  mojo::Remote<network::mojom::URLLoaderFactory> factory_remote;
  factory.Clone(factory_remote.BindNewPipeAndPassReceiver());

  network::ResourceRequest request;
  request.url = GURL("https://example.com/navigation");

  mojo::Remote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  auto client_receiver_for_request = client.InitWithNewPipeAndPassReceiver();

  factory_remote->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 0, 0, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag());

  factory_remote.FlushForTesting();

  // Call FollowRedirect on the renderer remote. It should be blocked.
  mojo::test::BadMessageObserver bad_message_observer;
  loader->FollowRedirect({}, {}, {}, std::nullopt);

  EXPECT_EQ("URLLoaderProxy: FollowRedirect is forbidden from renderer.",
            bad_message_observer.WaitForBadMessage());
  EXPECT_FALSE(mock_loader.called_follow_redirect());
}

// Test Case 3: Verify that subresource requests bypass the proxy and fuse
// directly, allowing FollowRedirect to go through without browser intercept.
TEST_F(ForwardedRaceNetworkRequestURLLoaderFactoryTest,
       SubresourceFusesDirectly) {
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

  // is_main_resource = false (subresource)
  ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory factory(
      std::move(client_receiver), test_url_loader_factory_.GetSafeWeakWrapper(),
      /*is_main_resource=*/false);

  auto loader_receiver = factory.InitURLLoaderNewPipeAndPassReceiver();
  MockURLLoader mock_loader(std::move(loader_receiver));

  mojo::Remote<network::mojom::URLLoaderFactory> factory_remote;
  factory.Clone(factory_remote.BindNewPipeAndPassReceiver());

  network::ResourceRequest request;
  request.url = GURL("https://example.com/subresource");

  mojo::Remote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  auto client_receiver_for_request = client.InitWithNewPipeAndPassReceiver();

  factory_remote->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 0, 0, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag());

  factory_remote.FlushForTesting();

  // Call FollowRedirect. It should be forwarded directly and NOT blocked.
  loader->FollowRedirect({}, {}, {}, std::nullopt);
  loader.FlushForTesting();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(mock_loader.called_follow_redirect());
}
}  // namespace content
