// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/broker_helper_win.h"

#include "base/test/task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class BrokerHelperWinTest : public testing::Test {
 public:
  BrokerHelperWinTest() = default;
  ~BrokerHelperWinTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  BrokerHelperWin helper_;
};

TEST_F(BrokerHelperWinTest, Loopback) {
  net::IPAddress loopback(127, 0, 0, 1);

  EXPECT_TRUE(loopback.IsLoopback());
  EXPECT_TRUE(helper_.ShouldBroker(loopback));
}

TEST_F(BrokerHelperWinTest, LocalInterface) {
  net::NetworkInterfaceList interfaces;
  EXPECT_TRUE(net::GetNetworkList(&interfaces,
                                  net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES));

  if (interfaces.empty()) {
    // Could happen in certain test environments?
    GTEST_SKIP();
  }

  EXPECT_TRUE(helper_.ShouldBroker(interfaces[0].address));
}

TEST_F(BrokerHelperWinTest, NotLocal) {
  net::IPAddress google_dns(8, 8, 8, 8);
  EXPECT_FALSE(helper_.ShouldBroker(google_dns));
}

}  // namespace
}  // namespace network
