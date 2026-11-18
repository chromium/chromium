// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/ipc_socket_factory.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/p2p.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/testing/fake_mojo_binding_context.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/transport/ecn_marking.h"
#include "third_party/webrtc_overrides/environment.h"

namespace blink {
namespace {

class FakeP2PSocketManager : public network::mojom::blink::P2PSocketManager {
 public:
  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<network::mojom::blink::P2PSocketManager>(
            std::move(handle)));
  }

  void Reset() { receiver_.reset(); }

  void StartNetworkNotifications(
      mojo::PendingRemote<network::mojom::blink::P2PNetworkNotificationClient>
          client) override {}

  void GetHostAddress(const String& host_name,
                      std::optional<net::AddressFamily> address_family,
                      bool enable_mdns,
                      GetHostAddressCallback callback) override {
    get_host_address_future_.SetValue(std::move(callback));
  }

  void CreateSocket(
      network::P2PSocketType type,
      const net::IPEndPoint& local_address,
      const network::P2PPortRange& port_range,
      const network::P2PHostAndIPEndPoint& remote_address,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const std::optional<base::UnguessableToken>& devtools_token,
      mojo::PendingRemote<network::mojom::blink::P2PSocketClient> client,
      mojo::PendingReceiver<network::mojom::blink::P2PSocket> socket) override {
  }

  GetHostAddressCallback TakeGetHostAddressCallback() {
    return get_host_address_future_.Take();
  }

 private:
  mojo::Receiver<network::mojom::blink::P2PSocketManager> receiver_{this};
  base::test::TestFuture<GetHostAddressCallback> get_host_address_future_;
};

}  // namespace

class IpcPacketSocketFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    mojo_binding_context_ = MakeGarbageCollected<FakeMojoBindingContext>(
        task_environment_.GetMainThreadTaskRunner());
    socket_factory_ = std::make_unique<IpcPacketSocketFactory>(
        CrossThreadBindRepeating(
            [](base::OnceCallback<void(
                   std::optional<base::UnguessableToken>)>) {}),
        &P2PSocketDispatcher::From(*mojo_binding_context_),
        TRAFFIC_ANNOTATION_FOR_TESTS, false);

    socket_ = socket_factory_->CreateUdpSocket(
        WebRtcEnvironment(), webrtc::SocketAddress("127.0.0.1", 0), 0, 0);
    ASSERT_NE(socket_, nullptr);
  }

  void TearDown() override {
    socket_.reset();
    socket_factory_.reset();
    mojo_binding_context_->Dispose();
    mojo_binding_context_.Clear();
    // Collect P2PSocketDispatcher while task_environment_ is still alive so
    // mojo::SharedRemote frees pending response callbacks via DeleteSoon, then
    // flush that non-nestable task before ~TaskEnvironment() drops it.
    ThreadState::Current()->CollectAllGarbageForTesting();
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostNonNestableTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  Persistent<FakeMojoBindingContext> mojo_binding_context_;
  std::unique_ptr<webrtc::PacketSocketFactory> socket_factory_;
  std::unique_ptr<webrtc::AsyncPacketSocket> socket_;
};

// Verify that the socket correctly handles the OPT_RECV_ECCN option.
TEST_F(IpcPacketSocketFactoryTest, SetOptions) {
  int desired_recv_ecn = 1;
  int recv_ecn_option = 0;
  EXPECT_EQ(0,
            socket_->GetOption(webrtc::Socket::OPT_RECV_ECN, &recv_ecn_option));
  EXPECT_EQ(-1, recv_ecn_option);
  EXPECT_EQ(0,
            socket_->SetOption(webrtc::Socket::OPT_RECV_ECN, desired_recv_ecn));
  EXPECT_EQ(0,
            socket_->GetOption(webrtc::Socket::OPT_RECV_ECN, &recv_ecn_option));
  EXPECT_EQ(webrtc::EcnMarking::kEct1,
            static_cast<webrtc::EcnMarking>(recv_ecn_option));
}

// Verify destroying an active AsyncDnsResolverInterface cancels the underlying
// P2PAsyncAddressResolver before the pending Mojo callback can be dropped.
TEST_F(IpcPacketSocketFactoryTest, DestroyPendingAsyncDnsResolver) {
  FakeP2PSocketManager fake_socket_manager;
  ASSERT_TRUE(
      mojo_binding_context_->GetBrowserInterfaceBroker().SetBinderForTesting(
          network::mojom::blink::P2PSocketManager::Name_,
          base::BindRepeating(&FakeP2PSocketManager::Bind,
                              base::Unretained(&fake_socket_manager))));
  base::ScopedClosureRunner clear_binder(base::BindOnce(
      [](MojoBindingContext* context) {
        context->GetBrowserInterfaceBroker().SetBinderForTesting(
            network::mojom::blink::P2PSocketManager::Name_,
            base::NullCallback());
      },
      WrapPersistent(mojo_binding_context_.Get())));

  auto resolver = socket_factory_->CreateAsyncDnsResolver();
  ASSERT_NE(resolver, nullptr);
  resolver->Start(webrtc::SocketAddress("example.com", 80), [] {});
  resolver.reset();

  // Wait for GetHostAddress to reach the mock interface before tearing down so
  // the pending client-side Mojo response callback is actually exercised.
  auto callback = fake_socket_manager.TakeGetHostAddressCallback();
  // Disconnect the mock endpoint before dropping the response callback.
  fake_socket_manager.Reset();
  callback.Reset();
}

TEST_F(IpcPacketSocketFactoryTest, CreateClientTcpSocketBlockedAndAllowedPort) {
  webrtc::PacketSocketTcpOptions opts;
  // Port 25 is restricted by default.
  auto blocked_socket = socket_factory_->CreateClientTcpSocket(
      WebRtcEnvironment(), webrtc::SocketAddress("127.0.0.1", 0),
      webrtc::SocketAddress("127.0.0.1", 25), opts);
  EXPECT_EQ(nullptr, blocked_socket);

  // Explicitly allow port 25.
  const uint16_t allowed_ports[] = {25};
  SetExplicitlyAllowedPorts(allowed_ports);

  auto allowed_socket = socket_factory_->CreateClientTcpSocket(
      WebRtcEnvironment(), webrtc::SocketAddress("127.0.0.1", 0),
      webrtc::SocketAddress("127.0.0.1", 25), opts);
  EXPECT_NE(nullptr, allowed_socket);

  // Reset explicitly allowed ports.
  SetExplicitlyAllowedPorts({});
}

}  // namespace blink
