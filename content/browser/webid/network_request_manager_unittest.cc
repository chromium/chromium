// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/network_request_manager.h"

#include "base/test/task_environment.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

class TestNetworkRequestManager : public NetworkRequestManager {
 public:
  TestNetworkRequestManager(
      const url::Origin& relying_party_origin,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      network::mojom::ClientSecurityStatePtr client_security_state,
      network::mojom::RequestDestination destination,
      content::FrameTreeNodeId frame_tree_node_id)
      : NetworkRequestManager(relying_party_origin,
                              loader_factory,
                              std::move(client_security_state),
                              destination,
                              frame_tree_node_id) {}

  net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() override {
    return net::DefineNetworkTrafficAnnotation("test", "test");
  }

  using NetworkRequestManager::CreateCredentialedResourceRequest;
  using NetworkRequestManager::CreateUncredentialedResourceRequest;
};

class NetworkRequestManagerTest
    : public ::testing::TestWithParam<network::mojom::RequestDestination> {
 public:
  NetworkRequestManagerTest() = default;
  ~NetworkRequestManagerTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(NetworkRequestManagerTest, CreateUncredentialedResourceRequest) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  TestNetworkRequestManager manager(rp_origin, nullptr,
                                    network::mojom::ClientSecurityState::New(),
                                    GetParam(), content::FrameTreeNodeId());
  auto request = manager.CreateUncredentialedResourceRequest(
      GURL("https://idp.example/"), /*send_origin=*/false);
  EXPECT_EQ(GetParam(), request->destination);
}

TEST_P(NetworkRequestManagerTest, CreateCredentialedResourceRequest) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  TestNetworkRequestManager manager(rp_origin, nullptr,
                                    network::mojom::ClientSecurityState::New(),
                                    GetParam(), content::FrameTreeNodeId());
  auto request = manager.CreateCredentialedResourceRequest(
      GURL("https://idp.example/"),
      NetworkRequestManager::CredentialedResourceRequestType::kNoOrigin);
  EXPECT_EQ(GetParam(), request->destination);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkRequestManagerTest,
    ::testing::Values(network::mojom::RequestDestination::kWebIdentity,
                      network::mojom::RequestDestination::kEmailVerification));

}  // namespace content::webid
