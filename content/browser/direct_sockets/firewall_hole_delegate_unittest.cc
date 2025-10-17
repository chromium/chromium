// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/firewall_hole_delegate.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const uint16_t kTestPort = 8888;
const char kTestInterface[] = "";  // Empty string for all interfaces.

class FirewallHoleDelegateTest : public testing::Test {
 public:
  FirewallHoleDelegateTest() = default;
  ~FirewallHoleDelegateTest() override = default;

  void SetUp() override { chromeos::PermissionBrokerClient::InitializeFake(); }

  void TearDown() override { chromeos::PermissionBrokerClient::Shutdown(); }

  chromeos::FakePermissionBrokerClient* GetFakeClient() {
    return static_cast<chromeos::FakePermissionBrokerClient*>(
        chromeos::PermissionBrokerClient::Get());
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(FirewallHoleDelegateTest, OpenTCPFirewallHole_Success) {
  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      open_future;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker_remote;
  net::IPEndPoint local_addr(net::IPAddress::IPv4AllZeros(), kTestPort);

  FirewallHoleDelegate::OpenTCPFirewallHole(
      tracker_remote.BindNewPipeAndPassReceiver(), open_future.GetCallback(),
      net::OK, local_addr);

  ASSERT_TRUE(open_future.Wait());

  EXPECT_EQ(open_future.Get<0>(), net::OK);
  EXPECT_EQ(open_future.Get<1>(), local_addr);

  // Verify that the delegate actually requested the firewall hole.
  EXPECT_TRUE(GetFakeClient()->HasTcpHole(kTestPort, kTestInterface));

  // Disconnect the mojo remote, which should trigger the firewall hole to
  // close.
  tracker_remote.reset();
  task_environment_.RunUntilIdle();

  // Verify that the delegate closed the hole.
  EXPECT_FALSE(GetFakeClient()->HasTcpHole(kTestPort, kTestInterface));
}

TEST_F(FirewallHoleDelegateTest, OpenTCPFirewallHole_OpenFailure) {
  GetFakeClient()->AddTcpDenyRule(kTestPort, kTestInterface);

  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      open_future;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker_remote;
  net::IPEndPoint local_addr(net::IPAddress::IPv4AllZeros(), kTestPort);

  FirewallHoleDelegate::OpenTCPFirewallHole(
      tracker_remote.BindNewPipeAndPassReceiver(), open_future.GetCallback(),
      net::OK, local_addr);

  ASSERT_TRUE(open_future.Wait());

  EXPECT_EQ(open_future.Get<0>(), net::ERR_NETWORK_ACCESS_DENIED);
  EXPECT_FALSE(open_future.Get<1>().has_value());
  EXPECT_FALSE(GetFakeClient()->HasTcpHole(kTestPort, kTestInterface));
}

TEST_F(FirewallHoleDelegateTest, OpenUDPFirewallHole_ReuseHole) {
  net::IPEndPoint local_addr(net::IPAddress::IPv4AllZeros(), kTestPort);

  // First request opens the hole.
  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      future1;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker1;
  FirewallHoleDelegate::OpenUDPFirewallHole(
      tracker1.BindNewPipeAndPassReceiver(), future1.GetCallback(), net::OK,
      local_addr);
  ASSERT_TRUE(future1.Wait());
  ASSERT_EQ(future1.Get<0>(), net::OK);
  EXPECT_TRUE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));

  // Second request for the same port should succeed and reuse the existing
  // hole.
  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      future2;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker2;
  FirewallHoleDelegate::OpenUDPFirewallHole(
      tracker2.BindNewPipeAndPassReceiver(), future2.GetCallback(), net::OK,
      local_addr);
  ASSERT_TRUE(future2.Wait());
  ASSERT_EQ(future2.Get<0>(), net::OK);

  // Disconnect the first tracker. The hole should remain open.
  tracker1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));

  // Disconnect the second tracker. Now the hole should be closed.
  tracker2.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));
}

TEST_F(FirewallHoleDelegateTest, OpenUDPFirewallHole_ParallelRequests) {
  net::IPEndPoint local_addr(net::IPAddress::IPv4AllZeros(), kTestPort);

  // Make two requests before the firewall hole opening has completed.
  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      future1;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker1;
  FirewallHoleDelegate::OpenUDPFirewallHole(
      tracker1.BindNewPipeAndPassReceiver(), future1.GetCallback(), net::OK,
      local_addr);

  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      future2;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker2;
  FirewallHoleDelegate::OpenUDPFirewallHole(
      tracker2.BindNewPipeAndPassReceiver(), future2.GetCallback(), net::OK,
      local_addr);

  // Wait for both requests to complete.
  ASSERT_TRUE(future1.Wait());
  ASSERT_TRUE(future2.Wait());

  // Both should succeed.
  EXPECT_EQ(future1.Get<0>(), net::OK);
  EXPECT_EQ(future2.Get<0>(), net::OK);

  // Verify the hole was opened.
  EXPECT_TRUE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));

  // Disconnect the first tracker. The hole should remain open.
  tracker1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));

  // Disconnect the second tracker. Now the hole should be closed.
  tracker2.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));
}

TEST_F(FirewallHoleDelegateTest, ShouldNotOpenFirewallHoleForLoopback) {
  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&>
      open_future;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker_remote;
  net::IPEndPoint local_addr(net::IPAddress::IPv4Localhost(), kTestPort);

  FirewallHoleDelegate::OpenTCPFirewallHole(
      tracker_remote.BindNewPipeAndPassReceiver(), open_future.GetCallback(),
      net::OK, local_addr);

  // The callback should complete immediately.
  ASSERT_TRUE(open_future.IsReady());
  EXPECT_EQ(open_future.Get<0>(), net::OK);
  EXPECT_EQ(open_future.Get<1>(), local_addr);

  // Verify that NO request was sent to the permission broker.
  EXPECT_FALSE(GetFakeClient()->HasTcpHole(kTestPort, kTestInterface));
}

TEST_F(FirewallHoleDelegateTest, OpenUDPFirewallHole_InitialError) {
  base::test::TestFuture<int32_t, const std::optional<net::IPEndPoint>&> future;
  mojo::Remote<network::mojom::SocketConnectionTracker> tracker_remote;

  // Pass an initial error code.
  FirewallHoleDelegate::OpenUDPFirewallHole(
      tracker_remote.BindNewPipeAndPassReceiver(), future.GetCallback(),
      net::ERR_INVALID_ARGUMENT, std::nullopt);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get<0>(), net::ERR_INVALID_ARGUMENT);
  EXPECT_FALSE(GetFakeClient()->HasUdpHole(kTestPort, kTestInterface));
}

}  // namespace
}  // namespace content
