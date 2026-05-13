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

TEST_F(BrokerHelperWinTest, IPv6Loopback) {
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress::IPv6Localhost()));
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

// Any address in an RFC1918 (10/8, 172.16/12, 192.168/16) range must always
// be brokered because the Windows firewall may classify the destination as
// "public" on a misconfigured/unprofiled interface and drop the connection
// (see crbug.com/466139402). Verify this even when no matching local
// interface is registered.
TEST_F(BrokerHelperWinTest, BrokersRfc1918WithoutLocalInterface) {
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(10, 0, 0, 1)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(10, 200, 5, 7)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(10, 255, 255, 254)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(172, 16, 0, 1)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(172, 31, 255, 254)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(192, 168, 1, 1)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(192, 168, 30, 128)));
}

TEST_F(BrokerHelperWinTest, BrokersIpv4LinkLocal) {
  // 169.254.0.0/16 is IPv4 link-local.
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(169, 254, 0, 1)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(169, 254, 13, 16)));
}

TEST_F(BrokerHelperWinTest, BrokersIpv6LinkLocal) {
  // fe80::/10 is IPv6 link-local.
  net::IPAddress link_local;
  ASSERT_TRUE(link_local.AssignFromIPLiteral("fe80::1"));
  EXPECT_TRUE(helper_.ShouldBroker(link_local));
}

TEST_F(BrokerHelperWinTest, BrokersIpv6UniqueLocal) {
  // fc00::/7 is IPv6 Unique-Local Addresses (ULA).
  net::IPAddress ula;
  ASSERT_TRUE(ula.AssignFromIPLiteral("fc00::1"));
  EXPECT_TRUE(helper_.ShouldBroker(ula));

  net::IPAddress ula_high;
  ASSERT_TRUE(ula_high.AssignFromIPLiteral("fdff::1"));
  EXPECT_TRUE(helper_.ShouldBroker(ula_high));
}

TEST_F(BrokerHelperWinTest, BrokersCarrierGradeNat) {
  // 100.64.0.0/10 is Carrier-Grade NAT (RFC 6598).
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(100, 64, 0, 1)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(100, 127, 255, 254)));
}

TEST_F(BrokerHelperWinTest, BrokersUnspecified) {
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(0, 0, 0, 0)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress::IPv6AllZeros()));
}

TEST_F(BrokerHelperWinTest, DoesNotBrokerPublicIPv6) {
  net::IPAddress public_v6;
  ASSERT_TRUE(public_v6.AssignFromIPLiteral("2001:4860:4860::8888"));
  EXPECT_FALSE(helper_.ShouldBroker(public_v6));
}

// As a backstop for hosts whose LAN uses publicly routable IPv4 addresses
// (e.g. an organization with an assigned public block), connections to
// addresses on the same prefix as a local interface are still brokered.
TEST_F(BrokerHelperWinTest, LocalInterfacePrefixBackstop) {
  // Use an arbitrary publicly-routable address (matching `NotLocal` above)
  // as the host's interface, so the prefix-backstop path is exercised in
  // isolation from the `IsPubliclyRoutable()` early-return. Documentation
  // ranges such as 192.0.2.0/24, 198.51.100.0/24 and 203.0.113.0/24 would
  // short-circuit before reaching the prefix loop and so are unsuitable.
  helper_.InjectNetworkListForTesting(
      net::NetworkInterfaceList{net::NetworkInterface(
          "fake interface", "fake interface", /*interface_index=*/0, /*type=*/
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET,
          net::IPAddress(8, 8, 8, 1), /*prefix_length=*/24,
          /*ip_address_attributes=*/0, /*mac_address=*/std::nullopt)});
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(8, 8, 8, 1)));
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(8, 8, 8, 2)));
  EXPECT_FALSE(helper_.ShouldBroker(net::IPAddress(8, 8, 9, 1)));
}

// `prefix_length` of 255 is a Windows sentinel for "illegal"; verify the
// backstop interface check degrades gracefully (effectively /32) in that
// case while the IsPubliclyRoutable check still applies elsewhere.
TEST_F(BrokerHelperWinTest, LocalInterfaceInvalidPrefix) {
  // Use 8.8.8.2 (not .1) so that the neighbour we check (8.8.8.3) differs
  // from the interface address only in the lowest-order bit. This catches
  // hypothetical bugs where a buggy mask might fold the low bit(s) of the
  // host portion when the prefix is invalid. .0 is also avoided because
  // it is the reserved network address.
  helper_.InjectNetworkListForTesting(
      net::NetworkInterfaceList{net::NetworkInterface(
          "fake interface", "fake interface", /*interface_index=*/0, /*type=*/
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET,
          net::IPAddress(8, 8, 8, 2), /*prefix_length=*/255,
          /*ip_address_attributes=*/0, /*mac_address=*/std::nullopt)});
  // The interface address itself is brokered.
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(8, 8, 8, 2)));
  // A neighbouring publicly-routable address is *not* brokered, because
  // the invalid prefix degrades to /32.
  EXPECT_FALSE(helper_.ShouldBroker(net::IPAddress(8, 8, 8, 3)));
  EXPECT_FALSE(helper_.ShouldBroker(net::IPAddress(8, 8, 9, 1)));
  // Non-publicly-routable addresses are still brokered regardless.
  EXPECT_TRUE(helper_.ShouldBroker(net::IPAddress(192, 168, 0, 2)));
}

}  // namespace
}  // namespace network
