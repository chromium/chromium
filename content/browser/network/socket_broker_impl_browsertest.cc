// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser test that network::SocketBrokerImpl continues working correctly after
// a crash of the network service. This has to go in //content because the
// network service can't implement browser tests.

#include "services/network/public/cpp/socket_broker_impl.h"

#include <stdint.h>

#include <functional>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "net/base/address_family.h"
#include "services/network/public/cpp/socket_broker_client.h"
#include "services/network/public/cpp/transferable_socket.h"
#include "services/network/public/mojom/socket_broker.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace content {

namespace {

class SocketBrokerImplBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    // The network service needs to be out-of-process in order to crash
    // successfully.
    feature_list_.InitAndDisableFeature(features::kNetworkServiceInProcess);
    ASSERT_FALSE(IsInProcessNetworkService());
    ContentBrowserTest::SetUp();
  }

  mojo::PendingRemote<network::mojom::SocketBroker> GetSocketBroker() {
    return socket_broker_.BindNewRemote();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  network::SocketBrokerImpl socket_broker_;
};

// Regression test for https://crbug.com/475587477.
IN_PROC_BROWSER_TEST_F(SocketBrokerImplBrowserTest,
                       CreateTCPSocketAfterNetworkServiceCrash) {
  network::SocketBrokerClient socket_broker_client(GetSocketBroker());

  SimulateNetworkServiceCrash();
  base::test::TestFuture<network::TransferableSocket, int> future;
  socket_broker_client.CreateTcpSocket(net::ADDRESS_FAMILY_IPV4,
                                       future.GetCallback());

  auto [socket, rv] = future.Take();
  EXPECT_NE(socket.TakeSocket(), net::kInvalidSocket);
  EXPECT_EQ(rv, net::OK);
}

// Same as above, but for UDP.
IN_PROC_BROWSER_TEST_F(SocketBrokerImplBrowserTest,
                       CreateUDPSocketAfterNetworkServiceCrash) {
  network::SocketBrokerClient socket_broker_client(GetSocketBroker());

  SimulateNetworkServiceCrash();

  base::test::TestFuture<network::TransferableSocket, int> future;
  socket_broker_client.CreateUdpSocket(net::ADDRESS_FAMILY_IPV4,
                                       future.GetCallback());
  auto [socket, rv] = future.Take();
  EXPECT_NE(socket.TakeSocket(), net::kInvalidSocket);
  EXPECT_EQ(rv, net::OK);
}

}  // namespace

}  // namespace content
