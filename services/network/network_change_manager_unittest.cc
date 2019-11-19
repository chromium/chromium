// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_change_manager.h"

#include <algorithm>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Type of notification expected in test.
enum NotificationType {
  // Default value.
  NONE,
  // OnInitialConnectionType() notification.
  INITIAL,
  // OnNetworkChanged() notification.
  NETWORK_CHANGED,
};

class TestNetworkChangeManagerClient
    : public mojom::NetworkChangeManagerClient {
 public:
  explicit TestNetworkChangeManagerClient(
      NetworkChangeManager* network_change_manager)
      : num_network_changed_(0),
        run_loop_(std::make_unique<base::RunLoop>()),
        notification_type_to_wait_(NONE),
        connection_type_(mojom::ConnectionType::CONNECTION_UNKNOWN) {
    mojo::Remote<mojom::NetworkChangeManager> manager;
    network_change_manager->AddReceiver(manager.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::NetworkChangeManagerClient> client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver());
    manager->RequestNotifications(std::move(client_remote));
  }

  ~TestNetworkChangeManagerClient() override {}

  // NetworkChangeManagerClient implementation:
  void OnInitialConnectionType(mojom::ConnectionType type) override {
    connection_type_ = type;
    if (notification_type_to_wait_ == INITIAL)
      run_loop_->Quit();
  }

  void OnNetworkChanged(mojom::ConnectionType type) override {
    num_network_changed_++;
    connection_type_ = type;
    if (notification_type_to_wait_ == NETWORK_CHANGED)
      run_loop_->Quit();
  }

  // Returns the number of OnNetworkChanged() notifications.
  size_t num_network_changed() const { return num_network_changed_; }

  void WaitForNotification(NotificationType notification_type) {
    notification_type_to_wait_ = notification_type;
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

  mojom::ConnectionType connection_type() const { return connection_type_; }

 private:
  size_t num_network_changed_;
  std::unique_ptr<base::RunLoop> run_loop_;
  NotificationType notification_type_to_wait_;
  mojom::ConnectionType connection_type_;
  mojo::Receiver<mojom::NetworkChangeManagerClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeManagerClient);
};

}  // namespace

class NetworkChangeManagerTest : public testing::Test {
 public:
  NetworkChangeManagerTest()
      : network_change_manager_(new NetworkChangeManager(
            net::NetworkChangeNotifier::CreateMockIfNeeded())) {
    network_change_manager_client_ =
        std::make_unique<TestNetworkChangeManagerClient>(
            network_change_manager_.get());
  }

  ~NetworkChangeManagerTest() override {}

  TestNetworkChangeManagerClient* network_change_manager_client() {
    return network_change_manager_client_.get();
  }

  NetworkChangeManager* network_change_manager() const {
    return network_change_manager_.get();
  }

  void SimulateNetworkChange(net::NetworkChangeNotifier::ConnectionType type) {
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(type);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkChangeManager> network_change_manager_;
  std::unique_ptr<TestNetworkChangeManagerClient>
      network_change_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeManagerTest);
};

TEST_F(NetworkChangeManagerTest, ClientNotified) {
  // Simulate a new network change.
  SimulateNetworkChange(net::NetworkChangeNotifier::CONNECTION_3G);
  network_change_manager_client()->WaitForNotification(NETWORK_CHANGED);
  EXPECT_EQ(mojom::ConnectionType::CONNECTION_3G,
            network_change_manager_client()->connection_type());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, network_change_manager_client()->num_network_changed());
}

TEST_F(NetworkChangeManagerTest, OneClientPipeBroken) {
  auto network_change_manager_client2 =
      std::make_unique<TestNetworkChangeManagerClient>(
          network_change_manager());

  // Simulate a network change.
  SimulateNetworkChange(net::NetworkChangeNotifier::CONNECTION_WIFI);

  network_change_manager_client()->WaitForNotification(NETWORK_CHANGED);
  network_change_manager_client2->WaitForNotification(NETWORK_CHANGED);
  EXPECT_EQ(mojom::ConnectionType::CONNECTION_WIFI,
            network_change_manager_client2->connection_type());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, network_change_manager()->GetNumClientsForTesting());

  EXPECT_EQ(1u, network_change_manager_client()->num_network_changed());
  EXPECT_EQ(1u, network_change_manager_client2->num_network_changed());
  network_change_manager_client2.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, network_change_manager()->GetNumClientsForTesting());

  // Simulate a second network change, and the remaining client should be
  // notified.
  SimulateNetworkChange(net::NetworkChangeNotifier::CONNECTION_2G);

  network_change_manager_client()->WaitForNotification(NETWORK_CHANGED);
  EXPECT_EQ(mojom::ConnectionType::CONNECTION_2G,
            network_change_manager_client()->connection_type());
  EXPECT_EQ(2u, network_change_manager_client()->num_network_changed());
}

TEST_F(NetworkChangeManagerTest, NewClientReceivesCurrentType) {
  // Simulate a network change.
  SimulateNetworkChange(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);

  network_change_manager_client()->WaitForNotification(NETWORK_CHANGED);
  EXPECT_EQ(mojom::ConnectionType::CONNECTION_BLUETOOTH,
            network_change_manager_client()->connection_type());
  base::RunLoop().RunUntilIdle();

  // Register a new client after the network change and it should receive the
  // up-to-date connection type.
  TestNetworkChangeManagerClient network_change_manager_client2(
      network_change_manager());
  network_change_manager_client2.WaitForNotification(INITIAL);
  EXPECT_EQ(mojom::ConnectionType::CONNECTION_BLUETOOTH,
            network_change_manager_client2.connection_type());
}

TEST(NetworkChangeConnectionTypeTest, ConnectionTypeEnumMatch) {
  for (int typeInt = net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
       typeInt != net::NetworkChangeNotifier::CONNECTION_LAST; typeInt++) {
    mojom::ConnectionType mojoType = mojom::ConnectionType(typeInt);
    switch (typeInt) {
      case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_UNKNOWN, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_ETHERNET, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_WIFI:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_WIFI, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_2G:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_2G, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_3G:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_3G, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_4G:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_4G, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_NONE:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_NONE, mojoType);
        break;
      case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_BLUETOOTH, mojoType);
        EXPECT_EQ(mojom::ConnectionType::CONNECTION_LAST, mojoType);
        break;
    }
  }
}

}  // namespace network
