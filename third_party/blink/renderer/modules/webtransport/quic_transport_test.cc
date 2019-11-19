// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/quic_transport.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_close_info.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class QuicTransportConnector final
    : public mojom::blink::QuicTransportConnector {
 public:
  struct ConnectArgs {
    ConnectArgs(
        const KURL& url,
        mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
            handshake_client)
        : url(url), handshake_client(std::move(handshake_client)) {}

    KURL url;
    mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
        handshake_client;
  };

  void Connect(
      const KURL& url,
      mojo::PendingRemote<network::mojom::blink::QuicTransportHandshakeClient>
          handshake_client) override {
    connect_args_.push_back(ConnectArgs(url, std::move(handshake_client)));
  }

  Vector<ConnectArgs> TakeConnectArgs() { return std::move(connect_args_); }

  void Bind(
      mojo::PendingReceiver<mojom::blink::QuicTransportConnector> receiver) {
    receiver_set_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<mojom::blink::QuicTransportConnector> receiver_set_;
  Vector<ConnectArgs> connect_args_;
};

class MockQuicTransport final : public network::mojom::blink::QuicTransport {
 public:
  MockQuicTransport(mojo::PendingReceiver<network::mojom::blink::QuicTransport>
                        pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  // TODO(ricea): Add methods when there are some.

 private:
  mojo::Receiver<network::mojom::blink::QuicTransport> receiver_;
};

class QuicTransportTest : public ::testing::Test {
 public:
  void AddBinder(const V8TestingScope& scope) {
    service_manager::InterfaceProvider::TestApi(
        scope.GetExecutionContext()->GetInterfaceProvider())
        .SetBinderForName(mojom::blink::QuicTransportConnector::Name_,
                          base::BindRepeating(&QuicTransportTest::BindConnector,
                                              weak_ptr_factory_.GetWeakPtr()));
  }

  // Creates, connects and returns a QuicTransport object with the given |url|.
  // Runs the event loop.
  QuicTransport* ConnectSuccessfully(const V8TestingScope& scope,
                                     const String& url) {
    AddBinder(scope);
    auto* quic_transport =
        QuicTransport::Create(scope.GetScriptState(), url, ASSERT_NO_EXCEPTION);

    test::RunPendingTasks();

    auto args = connector_.TakeConnectArgs();
    if (args.size() != 1u) {
      ADD_FAILURE() << "args.size() should be 1, but is " << args.size();
      return nullptr;
    }

    mojo::Remote<network::mojom::blink::QuicTransportHandshakeClient>
        handshake_client(std::move(args[0].handshake_client));

    mojo::PendingRemote<network::mojom::blink::QuicTransport>
        quic_transport_to_pass;
    mojo::PendingRemote<network::mojom::blink::QuicTransportClient>
        client_remote;

    mock_quic_transport_ = std::make_unique<MockQuicTransport>(
        quic_transport_to_pass.InitWithNewPipeAndPassReceiver());

    handshake_client->OnConnectionEstablished(
        std::move(quic_transport_to_pass),
        client_remote.InitWithNewPipeAndPassReceiver());

    test::RunPendingTasks();

    return quic_transport;
  }

  void BindConnector(mojo::ScopedMessagePipeHandle handle) {
    connector_.Bind(mojo::PendingReceiver<mojom::blink::QuicTransportConnector>(
        std::move(handle)));
  }

  QuicTransportConnector connector_;
  std::unique_ptr<MockQuicTransport> mock_quic_transport_;

  base::WeakPtrFactory<QuicTransportTest> weak_ptr_factory_{this};
};

TEST_F(QuicTransportTest, FailWithNullURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String(), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
}

TEST_F(QuicTransportTest, FailWithEmptyURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String(""), exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL '' is invalid.", exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithHttpsURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String("https://example.com/"),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ(
      "The URL's scheme must be 'quic-transport'. 'https' is not allowed.",
      exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithNoHost) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(), String("quic-transport:///"),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ("The URL 'quic-transport:' is invalid.", exception_state.Message());
}

TEST_F(QuicTransportTest, FailWithURLFragment) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/#failing"),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSyntaxError),
            exception_state.Code());
  EXPECT_EQ(
      "The URL contains a fragment identifier ('#failing'). Fragment "
      "identifiers are not allowed in QuicTransport URLs.",
      exception_state.Message());
}

TEST_F(QuicTransportTest, FailByCSP) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();
  scope.GetExecutionContext()
      ->GetContentSecurityPolicyForWorld()
      ->DidReceiveHeader("connect-src 'none'",
                         kContentSecurityPolicyHeaderTypeEnforce,
                         kContentSecurityPolicyHeaderSourceHTTP);
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/"),
                        exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kSecurityError),
            exception_state.Code());
  EXPECT_EQ("Failed to connect to 'quic-transport://example.com/'",
            exception_state.Message());
}

TEST_F(QuicTransportTest, PassCSP) {
  V8TestingScope scope;
  // This doesn't work without the https:// prefix, even thought it should
  // according to
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/connect-src.
  // TODO(ricea): Make it work with the quic-transport: scheme.
  auto& exception_state = scope.GetExceptionState();
  scope.GetExecutionContext()
      ->GetContentSecurityPolicyForWorld()
      ->DidReceiveHeader("connect-src https://example.com:443",
                         kContentSecurityPolicyHeaderTypeEnforce,
                         kContentSecurityPolicyHeaderSourceHTTP);
  QuicTransport::Create(scope.GetScriptState(),
                        String("quic-transport://example.com/"),
                        exception_state);
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(QuicTransportTest, SendConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* quic_transport = QuicTransport::Create(
      scope.GetScriptState(), String("quic-transport://example.com/"),
      ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());
  EXPECT_EQ(KURL("quic-transport://example.com/"), args[0].url);
  EXPECT_TRUE(quic_transport->HasPendingActivity());
}

TEST_F(QuicTransportTest, SuccessfulConnect) {
  V8TestingScope scope;
  auto* quic_transport =
      ConnectSuccessfully(scope, "quic-transport://example.com");
  EXPECT_TRUE(quic_transport->HasPendingActivity());
}

TEST_F(QuicTransportTest, FailedConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* quic_transport = QuicTransport::Create(
      scope.GetScriptState(), String("quic-transport://example.com/"),
      ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  mojo::Remote<network::mojom::blink::QuicTransportHandshakeClient>
      handshake_client(std::move(args[0].handshake_client));

  handshake_client->OnHandshakeFailed();

  test::RunPendingTasks();
  EXPECT_FALSE(quic_transport->HasPendingActivity());
}

TEST_F(QuicTransportTest, CloseDuringConnect) {
  V8TestingScope scope;
  AddBinder(scope);
  auto* quic_transport = QuicTransport::Create(
      scope.GetScriptState(), String("quic-transport://example.com/"),
      ASSERT_NO_EXCEPTION);

  test::RunPendingTasks();

  auto args = connector_.TakeConnectArgs();
  ASSERT_EQ(1u, args.size());

  quic_transport->close(nullptr);

  test::RunPendingTasks();

  EXPECT_FALSE(quic_transport->HasPendingActivity());
}

TEST_F(QuicTransportTest, CloseAfterConnection) {
  V8TestingScope scope;
  auto* quic_transport =
      ConnectSuccessfully(scope, "quic-transport://example.com");

  WebTransportCloseInfo close_info;
  close_info.setErrorCode(42);
  close_info.setReason("because");
  quic_transport->close(&close_info);

  test::RunPendingTasks();

  // TODO(ricea): Check that the close info is sent through correctly, once we
  // start sending it.

  EXPECT_FALSE(quic_transport->HasPendingActivity());

  // Calling close again does nothing.
  quic_transport->close(nullptr);
}

// A live connection will be kept alive even if there is no explicit reference.
// When the underlying connection is shut down, the connection will be swept.
TEST_F(QuicTransportTest, GarbageCollection) {
  V8TestingScope scope;
  WeakPersistent<QuicTransport> quic_transport =
      ConnectSuccessfully(scope, "quic-transport://example.com");

  // Pretend the stack is empty. This will avoid accidentally treating any
  // copies of the |quic_transport| pointer as references.
  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  EXPECT_TRUE(quic_transport);

  quic_transport->close(nullptr);

  test::RunPendingTasks();

  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  EXPECT_FALSE(quic_transport);
}

TEST_F(QuicTransportTest, GarbageCollectMojoConnectionError) {
  V8TestingScope scope;
  WeakPersistent<QuicTransport> quic_transport =
      ConnectSuccessfully(scope, "quic-transport://example.com");

  // Deleting the server-side object causes a mojo connection error.
  mock_quic_transport_ = nullptr;

  test::RunPendingTasks();

  V8GCController::CollectAllGarbageForTesting(
      scope.GetIsolate(), v8::EmbedderHeapTracer::EmbedderStackState::kEmpty);

  EXPECT_FALSE(quic_transport);
}

}  // namespace

}  // namespace blink
