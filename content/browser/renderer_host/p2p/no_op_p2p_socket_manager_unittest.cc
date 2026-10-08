// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/no_op_p2p_socket_manager.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class FakeNetworkNotificationClient
    : public network::mojom::P2PNetworkNotificationClient {
 public:
  FakeNetworkNotificationClient() = default;
  FakeNetworkNotificationClient(const FakeNetworkNotificationClient&) = delete;
  FakeNetworkNotificationClient& operator=(
      const FakeNetworkNotificationClient&) = delete;
  ~FakeNetworkNotificationClient() override = default;

  mojo::PendingRemote<network::mojom::P2PNetworkNotificationClient>
  BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        [](bool* disconnected) { *disconnected = true; }, &disconnected_));
    return remote;
  }

  bool disconnected() const { return disconnected_; }

  base::test::TestFuture<std::vector<net::NetworkInterface>,
                         net::IPAddress,
                         net::IPAddress>&
  network_list_future() {
    return network_list_future_;
  }

  // network::mojom::P2PNetworkNotificationClient:
  void NetworkListChanged(
      const std::vector<net::NetworkInterface>& networks,
      const net::IPAddress& default_ipv4_local_address,
      const net::IPAddress& default_ipv6_local_address) override {
    network_list_future_.SetValue(networks, default_ipv4_local_address,
                                  default_ipv6_local_address);
  }

 private:
  mojo::Receiver<network::mojom::P2PNetworkNotificationClient> receiver_{this};
  base::test::TestFuture<std::vector<net::NetworkInterface>,
                         net::IPAddress,
                         net::IPAddress>
      network_list_future_;
  bool disconnected_ = false;
};

class NoOpP2PSocketManagerTest : public testing::Test {
 public:
  NoOpP2PSocketManagerTest() {
    NoOpP2PSocketManager::Create(manager_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<network::mojom::P2PSocketManager> manager_;
};

// StartNetworkNotifications() must report an (empty) network list so that the
// renderer does not wait forever for one, and must keep the client connected.
TEST_F(NoOpP2PSocketManagerTest, StartNetworkNotificationsReportsEmptyList) {
  FakeNetworkNotificationClient client;
  manager_->StartNetworkNotifications(client.BindNewPipeAndPassRemote());

  auto& future = client.network_list_future();
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get<0>().empty());
  EXPECT_TRUE(future.Get<1>().empty());
  EXPECT_TRUE(future.Get<2>().empty());

  // The notification client must remain connected; otherwise the renderer
  // would observe a disconnect.
  manager_.FlushForTesting();
  EXPECT_FALSE(client.disconnected());
  EXPECT_TRUE(manager_.is_connected());
}

// A second StartNetworkNotifications() call replaces the previous client and
// also receives an empty network list.
TEST_F(NoOpP2PSocketManagerTest, StartNetworkNotificationsTwice) {
  FakeNetworkNotificationClient first_client;
  manager_->StartNetworkNotifications(first_client.BindNewPipeAndPassRemote());
  ASSERT_TRUE(first_client.network_list_future().Wait());

  FakeNetworkNotificationClient second_client;
  manager_->StartNetworkNotifications(second_client.BindNewPipeAndPassRemote());
  auto& future = second_client.network_list_future();
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get<0>().empty());

  manager_.FlushForTesting();
  EXPECT_TRUE(first_client.disconnected());
  EXPECT_FALSE(second_client.disconnected());
  EXPECT_TRUE(manager_.is_connected());
}

// GetHostAddress() must always run its callback, with no addresses.
TEST_F(NoOpP2PSocketManagerTest, GetHostAddressReturnsNoAddresses) {
  base::test::TestFuture<const std::vector<net::IPAddress>&> future;
  manager_->GetHostAddress("example.com", /*address_family=*/std::nullopt,
                           /*enable_mdns=*/true, future.GetCallback());
  EXPECT_TRUE(future.Get().empty());

  // mDNS-style names must not be resolved either.
  base::test::TestFuture<const std::vector<net::IPAddress>&> mdns_future;
  manager_->GetHostAddress("00000000-0000-0000-0000-000000000000.local",
                           /*address_family=*/std::nullopt,
                           /*enable_mdns=*/true, mdns_future.GetCallback());
  EXPECT_TRUE(mdns_future.Get().empty());

  EXPECT_TRUE(manager_.is_connected());
}

// CreateSocket() must never create a socket. The socket pipe is closed, so the
// renderer treats the socket as failed, while the manager stays connected (so
// the renderer does not attempt to reconnect).
TEST_F(NoOpP2PSocketManagerTest, CreateSocketClosesSocketPipe) {
  mojo::PendingReceiver<network::mojom::P2PSocketClient> client_receiver;
  mojo::PendingRemote<network::mojom::P2PSocketClient> client =
      client_receiver.InitWithNewPipeAndPassRemote();

  mojo::Remote<network::mojom::P2PSocket> socket;
  base::test::TestFuture<void> socket_disconnected;
  auto socket_receiver = socket.BindNewPipeAndPassReceiver();
  socket.set_disconnect_handler(socket_disconnected.GetCallback());

  manager_->CreateSocket(
      network::P2P_SOCKET_UDP,
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
      network::P2PPortRange(),
      network::P2PHostAndIPEndPoint(
          "example.com", net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0)),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      /*devtools_token=*/std::nullopt, std::move(client),
      std::move(socket_receiver));

  EXPECT_TRUE(socket_disconnected.Wait());
  EXPECT_FALSE(socket.is_connected());

  manager_.FlushForTesting();
  EXPECT_TRUE(manager_.is_connected());
}

}  // namespace

}  // namespace content
