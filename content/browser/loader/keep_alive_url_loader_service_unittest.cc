// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;
using testing::WithArg;

constexpr char kTestRequestUrl[] = "https://example.test";
constexpr char kTestResponseHeaderName[] = "My-Test-Header";
constexpr char kTestResponseHeaderValue[] = "my-test-value";
constexpr char kTestRedirectRequestUrl[] = "https://redirect.test";
constexpr char kTestUnSafeRedirectRequestUrl[] = "about:blank";
constexpr char kTestViolatingCSPRedirectRequestUrl[] =
    "https://violate-csp.test";

// Mock a receiver URLLoaderClient that may exist in renderer.
class MockReceiverURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  MockReceiverURLLoaderClient() = default;
  MockReceiverURLLoaderClient(const MockReceiverURLLoaderClient&) = delete;
  MockReceiverURLLoaderClient& operator=(const MockReceiverURLLoaderClient&) =
      delete;
  ~MockReceiverURLLoaderClient() override {
    if (receiver_.is_bound()) {
      // Flush the pipe to make sure there aren't any lingering events.
      receiver_.FlushForTesting();
    }
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // Note that this also unbinds the receiver.
  void ResetReceiver() { receiver_.reset(); }

  // `network::mojom::URLLoaderClient` overrides:
  MOCK_METHOD1(OnReceiveEarlyHints, void(network::mojom::EarlyHintsPtr));
  MOCK_METHOD3(OnReceiveResponse,
               void(network::mojom::URLResponseHeadPtr,
                    mojo::ScopedDataPipeConsumerHandle,
                    absl::optional<mojo_base::BigBuffer>));
  MOCK_METHOD2(OnReceiveRedirect,
               void(const net::RedirectInfo&,
                    network::mojom::URLResponseHeadPtr));
  MOCK_METHOD3(OnUploadProgress,
               void(int64_t, int64_t, base::OnceCallback<void()>));
  MOCK_METHOD1(OnTransferSizeUpdated, void(int32_t));
  MOCK_METHOD1(OnComplete, void(const network::URLLoaderCompletionStatus&));

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
};

// Fakes a URLLoaderFactory that may exist in renderer, which only delegates to
// `remote_url_loader_factory`.
class FakeRemoteURLLoaderFactory {
 public:
  FakeRemoteURLLoaderFactory() = default;
  FakeRemoteURLLoaderFactory(const FakeRemoteURLLoaderFactory&) = delete;
  FakeRemoteURLLoaderFactory& operator=(const FakeRemoteURLLoaderFactory&) =
      delete;
  ~FakeRemoteURLLoaderFactory() = default;

  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
  BindNewPipeAndPassReceiver() {
    return remote_url_loader_factory.BindNewPipeAndPassReceiver();
  }

  // Binds `remote_url_loader` to a new URLLoader.
  void CreateLoaderAndStart(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool expect_success = true) {
    remote_url_loader_factory->CreateLoaderAndStart(
        remote_url_loader.BindNewPipeAndPassReceiver(),
        /*request_id=*/1, /*options=*/0, request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    remote_url_loader_factory.FlushForTesting();
    ASSERT_EQ(remote_url_loader.is_connected(), expect_success);
  }

  bool is_remote_url_loader_connected() {
    return remote_url_loader.is_connected();
  }
  void reset_remote_url_loader() { remote_url_loader.reset(); }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_url_loader;
};

// Returns true if `arg` has a header of the given `name` and `value`.
// `arg` is an `network::mojom::URLResponseHeadPtr`.
MATCHER_P2(ResponseHasHeader,
           name,
           value,
           base::StringPrintf("Response has %sheader[%s=%s]",
                              negation ? "no " : "",
                              name,
                              value)) {
  return arg->headers->HasHeaderValue(name, value);
}

}  // namespace

class KeepAliveURLLoaderServiceTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    network_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>(
            /*observe_loader_requests=*/true);
    // Intercepts Mojo bad-message error.
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          ASSERT_FALSE(mojo_bad_message_.has_value());
          mojo_bad_message_ = error;
        }));
    RenderViewHostTestHarness::SetUp();

    AddConnectSrcCSPToRFH(kTestRedirectRequestUrl);
  }

  void TearDown() override {
    network_url_loader_factory_ = nullptr;
    loader_service_ = nullptr;
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    mojo_bad_message_ = absl::nullopt;
    RenderViewHostTestHarness::TearDown();
  }

  void ExpectMojoBadMessage(const std::string& message) {
    EXPECT_EQ(mojo_bad_message_, message);
  }

  // Asks KeepAliveURLLoaderService to bind a KeepAliveURLLoaderFactory to the
  // given `remote_url_loader_factory`.
  // More than one factory can be bound to the same service.
  void BindKeepAliveURLLoaderFactory(
      FakeRemoteURLLoaderFactory& remote_url_loader_factory) {
    if (!loader_service_) {
      loader_service_ = std::make_unique<KeepAliveURLLoaderService>();
    }

    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    network_url_loader_factory_->Clone(factory.BindNewPipeAndPassReceiver());
    auto pending_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            factory.Unbind());

    // Remote: `remote_url_loader_factory`
    // Receiver: Held in `loader_service_`.
    loader_service_->BindFactory(
        remote_url_loader_factory.BindNewPipeAndPassReceiver(),
        network::SharedURLLoaderFactory::Create(std::move(pending_factory)),
        static_cast<RenderFrameHostImpl*>(main_rfh())
            ->policy_container_host()
            ->Clone());
  }

  network::ResourceRequest CreateResourceRequest(
      const GURL& url,
      bool keepalive = true,
      bool is_trusted = false,
      absl::optional<network::mojom::RedirectMode> redirect_mode =
          absl::nullopt) {
    network::ResourceRequest request;
    request.url = url;
    request.keepalive = keepalive;
    if (is_trusted) {
      request.trusted_params = network::ResourceRequest::TrustedParams();
    }
    if (redirect_mode) {
      request.redirect_mode = *redirect_mode;
    }
    return request;
  }

  network::mojom::URLResponseHeadPtr CreateResponseHead(
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    auto response = network::mojom::URLResponseHead::New();
    response->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
    for (const auto& header : extra_headers) {
      response->headers->SetHeader(header.first, header.second);
    }
    return response;
  }

  net::RedirectInfo CreateRedirectInfo(const GURL& new_url) {
    net::RedirectInfo redirect_info;
    redirect_info.new_method = "GET";
    redirect_info.new_url = new_url;
    redirect_info.status_code = 301;
    return redirect_info;
  }

  network::mojom::EarlyHintsPtr CreateEarlyHints(
      const GURL& url,
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
    for (const auto& header : extra_headers) {
      response_headers->SetHeader(header.first, header.second);
    }
    return network::mojom::EarlyHints::New(
        network::PopulateParsedHeaders(response_headers.get(), url),
        network::mojom::ReferrerPolicy::kDefault,
        network::mojom::IPAddressSpace::kPublic);
  }

  network::TestURLLoaderFactory::PendingRequest* GetLastPendingRequest() {
    return &network_url_loader_factory_->pending_requests()->back();
  }

  void AddConnectSrcCSPToRFH(const std::string& allowed_url) {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->policy_container_host()
        ->AddContentSecurityPolicies(network::ParseContentSecurityPolicies(
            "connect-src " + allowed_url,
            network::mojom::ContentSecurityPolicyType::kEnforce,
            network::mojom::ContentSecurityPolicySource::kMeta,
            GURL(kTestRequestUrl)));
  }

  network::TestURLLoaderFactory& network_url_loader_factory() {
    return *network_url_loader_factory_;
  }
  KeepAliveURLLoaderService& loader_service() { return *loader_service_; }

 private:
  // Intercepts network facotry requests instead of using production factory.
  std::unique_ptr<network::TestURLLoaderFactory> network_url_loader_factory_ =
      nullptr;
  // The test target.
  std::unique_ptr<KeepAliveURLLoaderService> loader_service_ = nullptr;
  absl::optional<std::string> mojo_bad_message_;
};

TEST_F(KeepAliveURLLoaderServiceTest, LoadNonKeepaliveRequestAndTerminate) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads non-keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/false),
      renderer_loader_client.BindNewPipeAndPassRemote());

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(renderer_loader_factory.is_remote_url_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected `resource_request` in "
      "KeepAliveURLLoaderService::CreateLoaderAndStart(): "
      "resource_request.keepalive must be true");
}

TEST_F(KeepAliveURLLoaderServiceTest, LoadTrustedRequestAndTerminate) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads trusted keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/true),
      renderer_loader_client.BindNewPipeAndPassRemote());

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(renderer_loader_factory.is_remote_url_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected `resource_request` in "
      "KeepAliveURLLoaderService::CreateLoaderAndStart(): "
      "resource_request.trusted_params must not be set");
}

TEST_F(KeepAliveURLLoaderServiceTest, LoadRequestAfterPageIsUnloaded) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Deletes the current RenderFrameHost and then loads a keepalive request.
  DeleteContents();
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote(),
      /*expect_success=*/true);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 1);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnReceiveResponse) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveResponse:
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client,
              OnReceiveResponse(ResponseHasHeader(kTestResponseHeaderName,
                                                  kTestResponseHeaderValue),
                                _, Eq(absl::nullopt)))
      .Times(1);
  // Simluates receiving response in the network service.
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kTestResponseHeaderName, kTestResponseHeaderValue}}),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveResponseAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnReceiveResponse:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveResponse(_, _, _)).Times(0);
  // Simluates receiving response in the network service.
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kTestResponseHeaderName, kTestResponseHeaderValue}}),
      /*body=*/{}, absl::nullopt);
  base::RunLoop().RunUntilIdle();
  // The loader should have been deleted by the service.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnReceiveRedirect) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveRedirect:
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client,
              OnReceiveRedirect(_, ResponseHasHeader(kTestResponseHeaderName,
                                                     kTestResponseHeaderValue)))
      .Times(1);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kFollow),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, SizeIs(1));
  EXPECT_EQ(params[0].new_url, absl::nullopt);
  EXPECT_THAT(params[0].removed_headers, IsEmpty());
  EXPECT_TRUE(params[0].modified_headers.IsEmpty());
  EXPECT_TRUE(params[0].modified_cors_exempt_headers.IsEmpty());
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectToUnSafeTargetAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kFollow),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving unsafe redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestUnSafeRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is NOT sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, IsEmpty());
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectWithErrorRedirectModeAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first, with error redirect_mode:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kError),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is NOT sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, IsEmpty());
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectViolatingCSPAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kFollow),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestViolatingCSPRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is NOT sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, IsEmpty());
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnReceiveEarlyHints) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveEarlyHints:
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client, OnReceiveEarlyHints(_)).Times(1);
  // Simluates receiving early hints in the network service.
  GetLastPendingRequest()->client->OnReceiveEarlyHints(
      CreateEarlyHints(GURL(kTestRequestUrl)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveEarlyHintsAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnReceiveEarlyHints:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveEarlyHints(_)).Times(0);
  // Simluates receiving early hints in the network service.
  GetLastPendingRequest()->client->OnReceiveEarlyHints(
      CreateEarlyHints(GURL(kTestRequestUrl)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnUploadProgress) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnUploadProgress:
  const int64_t current_position = 5;
  const int64_t total_size = 100;
  base::OnceCallback<void()> callback;
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client,
              OnUploadProgress(Eq(current_position), Eq(total_size), _))
      .Times(1)
      .WillOnce(WithArg<2>([](base::OnceCallback<void()> callback) {
        // must be consumed.
        std::move(callback).Run();
      }));
  // Simluates receiving upload progress in the network service.
  GetLastPendingRequest()->client->OnUploadProgress(
      current_position, total_size, std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnTransferSizeUpdated) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnTransferSizeUpdated:
  const int32_t size_diff = 5;
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client, OnTransferSizeUpdated(Eq(size_diff)))
      .Times(1);
  // Simluates receiving transfer size update in the network service.
  GetLastPendingRequest()->client->OnTransferSizeUpdated(size_diff);
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnTransferSizeUpdatedAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnTransferSizeUpdated:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnTransferSizeUpdated(_)).Times(0);
  // Simluates receiving transfer size update in the network service.
  const int32_t size_diff = 5;
  GetLastPendingRequest()->client->OnTransferSizeUpdated(size_diff);
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnComplete) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnComplete:
  const network::URLLoaderCompletionStatus status{net::OK};
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client, OnComplete(Eq(status))).Times(1);
  // Simluates receiving completion status in the network service.
  GetLastPendingRequest()->client->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  // The KeepAliveURLLoader should have been deleted.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, OnCompleteAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnComplete:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  // Simluates receiving completion status in the network service.
  const network::URLLoaderCompletionStatus status{net::OK};
  GetLastPendingRequest()->client->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  // The KeepAliveURLLoader should have been deleted.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, RendererDisconnectedBeforeOnComplete) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveResponse
  // Simluates receiving response in the network service.
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kTestResponseHeaderName, kTestResponseHeaderValue}}),
      /*body=*/{}, absl::nullopt);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // The KeepAliveURLLoader should have been deleted.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

}  // namespace content
