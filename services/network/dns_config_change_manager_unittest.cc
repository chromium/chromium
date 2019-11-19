// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/dns_config_change_manager.h"

#include <climits>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TestDnsConfigChangeManagerClient
    : public mojom::DnsConfigChangeManagerClient {
 public:
  explicit TestDnsConfigChangeManagerClient(DnsConfigChangeManager* manager) {
    mojo::Remote<mojom::DnsConfigChangeManager> manager_remote;
    manager->AddReceiver(manager_remote.BindNewPipeAndPassReceiver());
    manager_remote->RequestNotifications(receiver_.BindNewPipeAndPassRemote());
  }

  void OnDnsConfigChanged() override {
    num_notifications_++;
    if (num_notifications_ >= num_notifications_expected_)
      run_loop_.Quit();
  }

  int num_notifications() { return num_notifications_; }

  void WaitForNotification(int num_notifications_expected) {
    num_notifications_expected_ = num_notifications_expected;
    if (num_notifications_ < num_notifications_expected_)
      run_loop_.Run();
  }

 private:
  int num_notifications_ = 0;
  int num_notifications_expected_ = INT_MAX;
  base::RunLoop run_loop_;
  mojo::Receiver<mojom::DnsConfigChangeManagerClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(TestDnsConfigChangeManagerClient);
};

class DnsConfigChangeManagerTest : public testing::Test {
 public:
  DnsConfigChangeManagerTest() {}

  DnsConfigChangeManager* manager() { return &manager_; }
  TestDnsConfigChangeManagerClient* client() { return &client_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> notifier_mock_ =
      net::NetworkChangeNotifier::CreateMockIfNeeded();
  DnsConfigChangeManager manager_;
  TestDnsConfigChangeManagerClient client_{&manager_};

  DISALLOW_COPY_AND_ASSIGN(DnsConfigChangeManagerTest);
};

TEST_F(DnsConfigChangeManagerTest, Notification) {
  EXPECT_EQ(0, client()->num_notifications());

  net::NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
  client()->WaitForNotification(1);
  EXPECT_EQ(1, client()->num_notifications());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, client()->num_notifications());
}

TEST_F(DnsConfigChangeManagerTest, MultipleNotification) {
  EXPECT_EQ(0, client()->num_notifications());

  net::NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
  net::NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
  client()->WaitForNotification(2);
  EXPECT_EQ(2, client()->num_notifications());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, client()->num_notifications());
}

TEST_F(DnsConfigChangeManagerTest, MultipleClients) {
  TestDnsConfigChangeManagerClient client2(manager());

  EXPECT_EQ(0, client()->num_notifications());
  EXPECT_EQ(0, client2.num_notifications());

  net::NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
  client()->WaitForNotification(1);
  client2.WaitForNotification(1);
  EXPECT_EQ(1, client()->num_notifications());
  EXPECT_EQ(1, client2.num_notifications());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, client()->num_notifications());
  EXPECT_EQ(1, client2.num_notifications());
}

}  // namespace
}  // namespace network
