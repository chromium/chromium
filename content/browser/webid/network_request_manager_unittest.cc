// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/network_request_manager.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
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
      FrameTreeNodeId frame_tree_node_id,
      WeakDocumentPtr initiator_document)
      : NetworkRequestManager(relying_party_origin,
                              loader_factory,
                              std::move(client_security_state),
                              destination,
                              frame_tree_node_id,
                              std::move(initiator_document)) {}

  net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() override {
    return net::DefineNetworkTrafficAnnotation("test", "test");
  }

  using NetworkRequestManager::CreateCredentialedResourceRequest;
  using NetworkRequestManager::CreateUncredentialedResourceRequest;
  using NetworkRequestManager::DownloadUrl;
};

class NetworkRequestManagerTest
    : public RenderViewHostTestHarness,
      public ::testing::WithParamInterface<network::mojom::RequestDestination> {
 public:
  NetworkRequestManagerTest() = default;
  ~NetworkRequestManagerTest() override = default;
};

TEST_P(NetworkRequestManagerTest, CreateUncredentialedResourceRequest) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  TestNetworkRequestManager manager(
      rp_origin, nullptr, network::mojom::ClientSecurityState::New(),
      GetParam(), FrameTreeNodeId(), WeakDocumentPtr());
  auto request = manager.CreateUncredentialedResourceRequest(
      GURL("https://idp.example/"), /*send_origin=*/false);
  EXPECT_EQ(GetParam(), request->destination);
}

TEST_P(NetworkRequestManagerTest, CreateCredentialedResourceRequest) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  TestNetworkRequestManager manager(
      rp_origin, nullptr, network::mojom::ClientSecurityState::New(),
      GetParam(), FrameTreeNodeId(), WeakDocumentPtr());
  auto request = manager.CreateCredentialedResourceRequest(
      GURL("https://idp.example/"),
      NetworkRequestManager::CredentialedResourceRequestType::kNoOrigin);
  EXPECT_EQ(GetParam(), request->destination);
}

// Tests that when DownloadUrl() is invoked while the initiator frame is valid,
// the request is sent to URLLoaderFactory and the callback is run upon
// completion.
TEST_P(NetworkRequestManagerTest, DownloadUrlValidFrameSendsRequest) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  network::TestURLLoaderFactory test_url_loader_factory;

  TestNetworkRequestManager manager(
      rp_origin,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      network::mojom::ClientSecurityState::New(), GetParam(),
      main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetWeakDocumentPtr());

  base::test::TestFuture<std::optional<std::string>, int, const std::string&,
                         bool>
      future;
  GURL target_url("https://idp.example/endpoint");
  auto resource_request = manager.CreateUncredentialedResourceRequest(
      target_url, /*send_origin=*/false);

  manager.DownloadUrl(std::move(resource_request), std::nullopt,
                      future.GetCallback(),
                      /*max_download_size=*/1024);

  EXPECT_EQ(1, test_url_loader_factory.NumPending());

  test_url_loader_factory.AddResponse(target_url.spec(), "response_body");

  auto [response_body, response_code, mime_type, cors_error] = future.Get();
  EXPECT_EQ("response_body", response_body);
  EXPECT_EQ(net::HTTP_OK, response_code);
  EXPECT_FALSE(cors_error);
}

// Tests that when DownloadUrl() is invoked while the initiator frame is invalid
// (e.g., user closed the tab), the passed callback is run asynchronously with
// net::ERR_ABORTED.
TEST_P(NetworkRequestManagerTest,
       DownloadUrlInvalidFrameRunsCallbackWithErrAborted) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  network::TestURLLoaderFactory test_url_loader_factory;

  // Simulate the initiator frame being invalidated by passing a default
  // constructed `WeakDocumentPtr`.
  TestNetworkRequestManager manager(
      rp_origin,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      network::mojom::ClientSecurityState::New(), GetParam(), FrameTreeNodeId(),
      WeakDocumentPtr());

  base::test::TestFuture<std::optional<std::string>, int, const std::string&,
                         bool>
      future;
  manager.DownloadUrl(std::make_unique<network::ResourceRequest>(),
                      std::nullopt, future.GetCallback(),
                      /*max_download_size=*/1024);

  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(net::ERR_ABORTED, future.Get<1>());
  EXPECT_EQ(0, test_url_loader_factory.NumPending());
}

// Tests that if NetworkRequestManager is destroyed after calling DownloadUrl()
// on an invalid frame but before the posted task executes, the callback is
// cancelled via WeakPtr and is never invoked, regardless of whether connection
// allowlist check is enabled.
TEST_P(NetworkRequestManagerTest,
       DownloadUrlInvalidFrameCancelledOnManagerDestruction) {
  url::Origin rp_origin = url::Origin::Create(GURL("https://rp.example"));
  network::TestURLLoaderFactory test_url_loader_factory;

  // Simulate the initiator frame being invalidated by passing a default
  // constructed `WeakDocumentPtr`.
  auto manager = std::make_unique<TestNetworkRequestManager>(
      rp_origin,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory),
      network::mojom::ClientSecurityState::New(), GetParam(), FrameTreeNodeId(),
      WeakDocumentPtr());

  base::test::TestFuture<std::optional<std::string>, int, const std::string&,
                         bool>
      future;
  manager->DownloadUrl(std::make_unique<network::ResourceRequest>(),
                       std::nullopt, future.GetCallback(),
                       /*max_download_size=*/1024);

  // Destroy the request manager. Because the run loop has not been run, the
  // manager is destroyed before the callback is invoked.
  EXPECT_FALSE(future.IsReady());
  manager.reset();

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // The callback should be cancelled because it binds a WeakPtr to the request
  // manager.
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(0, test_url_loader_factory.NumPending());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkRequestManagerTest,
    ::testing::Values(network::mojom::RequestDestination::kWebIdentity,
                      network::mojom::RequestDestination::kEmailVerification));

}  // namespace content::webid
