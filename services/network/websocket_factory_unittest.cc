// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

class StubWebSocketHandshakeClient : public mojom::WebSocketHandshakeClient {
 public:
  explicit StubWebSocketHandshakeClient(
      mojo::PendingReceiver<mojom::WebSocketHandshakeClient> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~StubWebSocketHandshakeClient() override = default;

  void OnOpeningHandshakeStarted(mojom::WebSocketHandshakeRequestPtr) override {
  }

  void OnConnectionEstablished(mojo::PendingRemote<mojom::WebSocket>,
                               mojo::PendingReceiver<mojom::WebSocketClient>,
                               mojom::WebSocketHandshakeResponsePtr,
                               mojo::ScopedDataPipeConsumerHandle,
                               mojo::ScopedDataPipeProducerHandle) override {}

  void OnFailure(const std::string&, int32_t, int32_t) override {}

 private:
  mojo::Receiver<mojom::WebSocketHandshakeClient> receiver_;
};

mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
  auto params = CreateNetworkContextParamsForTesting();
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  return params;
}

class WebSocketFactoryTest : public testing::Test {
 public:
  WebSocketFactoryTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()) {
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        CreateNetworkContextParams(), base::DoNothing());
    factory_ = std::make_unique<WebSocketFactory>(network_context_.get());
  }

  void CreateWebSocket(const GURL& url,
                       const std::vector<std::string>& requested_protocols) {
    mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client;
    stub_handshake_client_ = std::make_unique<StubWebSocketHandshakeClient>(
        handshake_client.InitWithNewPipeAndPassReceiver());

    // WebSocket objects are owned by the factory and will be deleted
    // asynchronously.
    factory_->CreateWebSocket(
        url, requested_protocols, net::SiteForCookies(),
        net::StorageAccessApiStatus::kNone, net::IsolationInfo(), {},
        /*process_id=*/0, url::Origin::Create(url),
        /*client_security_state=*/nullptr, /*options=*/0,
        TRAFFIC_ANNOTATION_FOR_TESTS, std::move(handshake_client),
        mojo::NullRemote(), mojo::NullRemote(), mojo::NullRemote(),
        std::nullopt);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<NetworkContext> network_context_;
  std::unique_ptr<WebSocketFactory> factory_;
  std::unique_ptr<StubWebSocketHandshakeClient> stub_handshake_client_;
};

class WebSocketFactoryBadMessageTest : public WebSocketFactoryTest {
 public:
  WebSocketFactoryBadMessageTest()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {
    mojo::SetDefaultProcessErrorHandler(
        future_.GetRepeatingCallback<const std::string&>());
  }

  ~WebSocketFactoryBadMessageTest() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  std::string GetBadMessage() { return future_.Get(); }

 private:
  base::test::TestFuture<std::string> future_;

  // Because we don't really call the factory from mojo, we need to create a
  // fake message context.
  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;
};

TEST_F(WebSocketFactoryBadMessageTest, InvalidScheme) {
  CreateWebSocket(GURL("http://example.com/"), {});
  EXPECT_EQ(GetBadMessage(), "Invalid scheme.");
}

TEST_F(WebSocketFactoryBadMessageTest, InvalidSubprotocol) {
  CreateWebSocket(GURL("ws://example.com/"), {"invalid protocol"});
  EXPECT_EQ(GetBadMessage(), "Invalid protocols.");
}

TEST_F(WebSocketFactoryBadMessageTest, EmptySubprotocol) {
  CreateWebSocket(GURL("ws://example.com/"), {"okay", "", "okay2"});
  EXPECT_EQ(GetBadMessage(), "Invalid protocols.");
}

TEST_F(WebSocketFactoryBadMessageTest, DuplicateSubprotocol) {
  CreateWebSocket(GURL("ws://example.com/"), {"foo", "foo"});
  EXPECT_EQ(GetBadMessage(), "Invalid protocols.");
}

}  // namespace

}  // namespace network
