// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/network_request_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

namespace {

constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kTargetUrl[] = "https://idp.example/endpoint";
constexpr size_t kMaxDownloadSize = 1024;
constexpr char kRequestIdKey[] = "requestId";
constexpr char kTypeKey[] = "type";
constexpr char kErrorTextKey[] = "errorText";
constexpr char kBodyKey[] = "body";
constexpr char kBase64EncodedKey[] = "base64Encoded";
constexpr char kRequestUrlPath[] = "request.url";
constexpr char kRequestPostDataPath[] = "request.postData";
constexpr char kInitiatorTypePath[] = "initiator.type";
constexpr char kResponseStatusPath[] = "response.status";
constexpr char kRequestWillBeSentEvent[] = "Network.requestWillBeSent";
constexpr char kResponseReceivedEvent[] = "Network.responseReceived";
constexpr char kLoadingFinishedEvent[] = "Network.loadingFinished";
constexpr char kLoadingFailedEvent[] = "Network.loadingFailed";

const char* ExpectedDevToolsType(
    network::mojom::RequestDestination destination) {
  switch (destination) {
    case network::mojom::RequestDestination::kWebIdentity:
      return "FedCM";
    case network::mojom::RequestDestination::kEmailVerification:
      return "EmailVerification";
    default:
      NOTREACHED();
  }
}

}  // namespace

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
      public TestDevToolsProtocolClient,
      public ::testing::WithParamInterface<network::mojom::RequestDestination> {
 public:
  NetworkRequestManagerTest() = default;
  ~NetworkRequestManagerTest() override = default;

  // RenderViewHostTestHarness:
  void TearDown() override {
    DetachProtocolClient();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  // Attaches the DevTools protocol client to `web_contents()` and enables
  // browser-side `Network` domain tracking.
  void AttachDevToolsAndEnableNetwork() {
    AttachToWebContents(web_contents());
    // `RenderViewHostTestHarness` uses a mock renderer process, so
    // `Network.enable`'s renderer-side fall-through never replies;
    // `NetworkHandler` enables its browser-side state synchronously when the
    // command is dispatched.
    SendCommandAsync("Network.enable");
  }

  // Creates a `TestNetworkRequestManager` bound to `main_rfh()` and
  // `test_url_loader_factory_` for the parameterized `RequestDestination`.
  TestNetworkRequestManager CreateManager() {
    return TestNetworkRequestManager(
        url::Origin::Create(GURL(kRpUrl)),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        network::mojom::ClientSecurityState::New(), GetParam(),
        main_rfh()->GetFrameTreeNodeId(), main_rfh()->GetWeakDocumentPtr());
  }

  // Waits for a `Network.requestWillBeSent` event for `kTargetUrl`, verifies
  // its resource and initiator types (and `request.postData` when
  // `expected_post_data` is present, or absence of `request.postData` when it
  // is `std::nullopt`), and writes its `requestId` to `out_request_id`.
  void ExpectRequestWillBeSent(
      std::string_view expected_type,
      std::optional<std::string_view> expected_post_data,
      std::string* out_request_id) {
    const base::DictValue will_be_sent = WaitForMatchingNotification(
        kRequestWillBeSentEvent,
        base::BindLambdaForTesting([](const base::DictValue& params) {
          const std::string* url =
              params.FindStringByDottedPath(kRequestUrlPath);
          return url && *url == kTargetUrl;
        }));
    const std::string* request_id = will_be_sent.FindString(kRequestIdKey);
    ASSERT_TRUE(request_id);
    *out_request_id = *request_id;
    const std::string* request_type = will_be_sent.FindString(kTypeKey);
    ASSERT_TRUE(request_type);
    EXPECT_EQ(expected_type, *request_type);
    const std::string* initiator_type =
        will_be_sent.FindStringByDottedPath(kInitiatorTypePath);
    ASSERT_TRUE(initiator_type);
    EXPECT_EQ(expected_type, *initiator_type);
    const std::string* post_data =
        will_be_sent.FindStringByDottedPath(kRequestPostDataPath);
    if (expected_post_data.has_value()) {
      ASSERT_TRUE(post_data);
      EXPECT_EQ(*expected_post_data, *post_data);
    } else {
      EXPECT_FALSE(post_data);
    }
  }

  // Waits for a `Network.loadingFailed` event for `request_id` and verifies its
  // resource `type`, `errorText`, and that no response or completion events
  // were emitted.
  void ExpectLoadingFailed(std::string_view request_id,
                           std::string_view expected_type,
                           int expected_net_error) {
    const base::DictValue loading_failed = WaitForMatchingNotification(
        kLoadingFailedEvent,
        base::BindLambdaForTesting([&](const base::DictValue& params) {
          const std::string* id = params.FindString(kRequestIdKey);
          return id && *id == request_id;
        }));
    const std::string* failed_type = loading_failed.FindString(kTypeKey);
    ASSERT_TRUE(failed_type);
    EXPECT_EQ(expected_type, *failed_type);
    const std::string* error_text = loading_failed.FindString(kErrorTextKey);
    ASSERT_TRUE(error_text);
    EXPECT_EQ(net::ErrorToString(expected_net_error), *error_text);
    EXPECT_FALSE(HasExistingNotification(kResponseReceivedEvent));
    EXPECT_FALSE(HasExistingNotification(kLoadingFinishedEvent));
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
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

// Tests that `DownloadUrl()` reports the DevTools resource and initiator types
// matching the request destination on `Network.requestWillBeSent` and
// `Network.responseReceived`, and records the response body for DevTools.
TEST_P(NetworkRequestManagerTest, DownloadUrlReportsTypesToDevTools) {
  constexpr char kPostData[] = R"({"email":"jane@example.com"})";
  constexpr char kResponseBody[] = R"({"status":"ok"})";

  NavigateAndCommit(GURL(kRpUrl));
  AttachDevToolsAndEnableNetwork();

  TestNetworkRequestManager manager = CreateManager();
  base::test::TestFuture<std::optional<std::string>, int, const std::string&,
                         bool>
      future;
  manager.DownloadUrl(
      manager.CreateCredentialedResourceRequest(
          GURL(kTargetUrl),
          NetworkRequestManager::CredentialedResourceRequestType::kNoOrigin),
      kPostData, future.GetCallback(), kMaxDownloadSize,
      /*allow_http_error_results=*/false,
      /*content_type=*/"application/json");

  test_url_loader_factory_.AddResponse(kTargetUrl, kResponseBody);
  EXPECT_EQ(kResponseBody, future.Get<0>());
  EXPECT_EQ(net::HTTP_OK, future.Get<1>());

  const char* expected_type = ExpectedDevToolsType(GetParam());
  std::string request_id;
  ASSERT_NO_FATAL_FAILURE(
      ExpectRequestWillBeSent(expected_type, kPostData, &request_id));

  const base::DictValue response_received = WaitForMatchingNotification(
      kResponseReceivedEvent,
      base::BindLambdaForTesting([&](const base::DictValue& params) {
        const std::string* id = params.FindString(kRequestIdKey);
        return id && *id == request_id;
      }));
  const std::string* response_type = response_received.FindString(kTypeKey);
  ASSERT_TRUE(response_type);
  EXPECT_EQ(expected_type, *response_type);
  EXPECT_EQ(net::HTTP_OK,
            response_received.FindIntByDottedPath(kResponseStatusPath));

  WaitForMatchingNotification(
      kLoadingFinishedEvent,
      base::BindLambdaForTesting([&](const base::DictValue& params) {
        const std::string* id = params.FindString(kRequestIdKey);
        return id && *id == request_id;
      }));

  const base::DictValue* body_result =
      SendCommandSync("Network.getResponseBody",
                      base::DictValue().Set(kRequestIdKey, request_id));
  ASSERT_TRUE(body_result);
  const std::string* body = body_result->FindString(kBodyKey);
  ASSERT_TRUE(body);
  EXPECT_EQ(kResponseBody, *body);
  EXPECT_EQ(false, body_result->FindBool(kBase64EncodedKey));
}

// Tests that a failed `DownloadUrl()` reports the DevTools resource and
// initiator types matching the request destination on
// `Network.requestWillBeSent` and `Network.loadingFailed`.
TEST_P(NetworkRequestManagerTest, DownloadUrlFailureReportsTypesToDevTools) {
  NavigateAndCommit(GURL(kRpUrl));
  AttachDevToolsAndEnableNetwork();

  TestNetworkRequestManager manager = CreateManager();
  base::test::TestFuture<std::optional<std::string>, int, const std::string&,
                         bool>
      future;
  manager.DownloadUrl(manager.CreateUncredentialedResourceRequest(
                          GURL(kTargetUrl), /*send_origin=*/false),
                      /*post_data=*/std::nullopt, future.GetCallback(),
                      kMaxDownloadSize);

  test_url_loader_factory_.AddResponse(
      GURL(kTargetUrl), network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
  EXPECT_FALSE(future.Get<0>().has_value());
  EXPECT_EQ(net::ERR_FAILED, future.Get<1>());

  const char* expected_type = ExpectedDevToolsType(GetParam());
  std::string request_id;
  ASSERT_NO_FATAL_FAILURE(ExpectRequestWillBeSent(
      expected_type, /*expected_post_data=*/std::nullopt, &request_id));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLoadingFailed(request_id, expected_type, net::ERR_FAILED));
}

// Tests that a `DownloadUrl()` blocked by the initiator frame's
// `Connection-Allowlist` reports the DevTools resource and initiator types
// matching the request destination on both `Network.requestWillBeSent` and
// `Network.loadingFailed` without issuing a network request.
TEST_P(NetworkRequestManagerTest,
       DownloadUrlBlockedByConnectionAllowlistReportsTypesToDevTools) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(GURL(kRpUrl), main_rfh());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(std::move(response_headers));
  navigation->Commit();

  AttachDevToolsAndEnableNetwork();

  TestNetworkRequestManager manager = CreateManager();
  base::test::TestFuture<std::optional<std::string>, int, const std::string&,
                         bool>
      future;
  manager.DownloadUrl(manager.CreateUncredentialedResourceRequest(
                          GURL(kTargetUrl), /*send_origin=*/false),
                      /*post_data=*/std::nullopt, future.GetCallback(),
                      kMaxDownloadSize);

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  EXPECT_FALSE(future.Get<0>().has_value());
  EXPECT_EQ(net::ERR_NETWORK_ACCESS_REVOKED, future.Get<1>());

  const char* expected_type = ExpectedDevToolsType(GetParam());
  std::string request_id;
  ASSERT_NO_FATAL_FAILURE(ExpectRequestWillBeSent(
      expected_type, /*expected_post_data=*/std::nullopt, &request_id));
  ASSERT_NO_FATAL_FAILURE(ExpectLoadingFailed(request_id, expected_type,
                                              net::ERR_NETWORK_ACCESS_REVOKED));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkRequestManagerTest,
    ::testing::Values(network::mojom::RequestDestination::kWebIdentity,
                      network::mojom::RequestDestination::kEmailVerification));

}  // namespace content::webid
