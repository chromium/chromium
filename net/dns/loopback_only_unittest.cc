// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/loopback_only.h"

#include <optional>
#include <unordered_set>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX)
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "net/base/address_map_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace net {

#if BUILDFLAG(IS_LINUX)

namespace {

constexpr uint8_t kIpv4LoopbackBytes[] = {127, 0, 0, 1};
constexpr uint8_t kIpv4PrivateAddressBytes[] = {10, 0, 0, 1};
constexpr uint8_t kIpv6LoopbackBytes[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                          0, 0, 0, 0, 0, 0, 0, 1};
constexpr uint8_t kIpv6AddressBytes[] = {0xFE, 0xDC, 0xBA, 0x98, 0, 0, 0, 0,
                                         0,    0,    0,    0,    0, 0, 0, 0};

constexpr uint8_t kIpv4LinkLocalBytes[] = {169, 254, 0, 0};
constexpr uint8_t kIpv4InIpv6LinkLocalBytes[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 169, 254, 0, 0};
constexpr uint8_t kIpv6LinkLocalBytes[] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0,
                                           0,    0,    0, 0, 0, 0, 0, 0};

class StubAddressMapOwnerLinux : public AddressMapOwnerLinux {
 public:
  AddressMap GetAddressMap() const override { return address_map_; }
  std::unordered_set<int> GetOnlineLinks() const override {
    return online_links_;
  }

  AddressMap& address_map() { return address_map_; }
  std::unordered_set<int>& online_links() { return online_links_; }

 private:
  AddressMap address_map_;
  std::unordered_set<int> online_links_;
};

bool GetResultOfRunHaveOnlyLoopbackAddressesJob() {
  bool result = false;
  net::TestClosure completion;
  {
    base::ScopedDisallowBlocking disallow_blocking;
    RunHaveOnlyLoopbackAddressesJob(
        base::BindLambdaForTesting([&](bool loopback_result) {
          result = loopback_result;
          completion.closure().Run();
        }));
  }
  completion.WaitForResult();
  return result;
}

}  // namespace

class LoopbackOnlyTest : public ::testing::Test {
 public:
  static constexpr int kTestInterfaceEth = 1;
  static constexpr int kTestInterfaceLoopback = 2;

  static inline const net::IPAddress kIpv4Loopback{kIpv4LoopbackBytes};
  static inline const net::IPAddress kIpv4PrivateAddress{
      kIpv4PrivateAddressBytes};

  static inline const net::IPAddress kIpv6Loopback{kIpv6LoopbackBytes};
  static inline const net::IPAddress kIpv6Address{kIpv6AddressBytes};

  static inline const net::IPAddress kIpv4LinkLocal{kIpv4LinkLocalBytes};
  static inline const net::IPAddress kIpv4InIpv6LinkLocal{
      kIpv4InIpv6LinkLocalBytes};
  static inline const net::IPAddress kIpv6LinkLocal{kIpv6LinkLocalBytes};

  LoopbackOnlyTest() {
    mock_notifier_.mock_network_change_notifier()->SetAddressMapOwnerLinux(
        &stub_address_map_owner_);
  }
  ~LoopbackOnlyTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  test::ScopedMockNetworkChangeNotifier mock_notifier_;
  StubAddressMapOwnerLinux stub_address_map_owner_;
};

TEST_F(LoopbackOnlyTest, HasOnlyLoopbackIpv4) {
  // Include only a loopback interface.
  stub_address_map_owner_.address_map() = {
      {kIpv4Loopback, ifaddrmsg{
                          .ifa_family = AF_INET,
                          .ifa_flags = IFA_F_TEMPORARY,
                          .ifa_index = kTestInterfaceLoopback,
                      }}};
  // AddressTrackerLinux does not insert loopback interfaces into
  // `online_links`.
  stub_address_map_owner_.online_links() = {};

  EXPECT_TRUE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, HasActiveIPv4Connection) {
  stub_address_map_owner_.address_map() = {
      {kIpv4Loopback, ifaddrmsg{.ifa_family = AF_INET,
                                .ifa_flags = IFA_F_TEMPORARY,
                                .ifa_index = kTestInterfaceLoopback}},
      {kIpv4PrivateAddress, ifaddrmsg{.ifa_family = AF_INET,
                                      .ifa_flags = IFA_F_TEMPORARY,
                                      .ifa_index = kTestInterfaceEth}}};
  // `online_links` includes kTestInterfaceEth so that kIpv4PrivateAddress is
  // the active IPv4 connection. Also, AddressTrackerLinux does not insert
  // loopback interfaces into `online_links`.
  stub_address_map_owner_.online_links() = {kTestInterfaceEth};

  EXPECT_FALSE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, HasInactiveIPv4Connection) {
  stub_address_map_owner_.address_map() = {
      {kIpv4Loopback, ifaddrmsg{.ifa_family = AF_INET,
                                .ifa_flags = IFA_F_TEMPORARY,
                                .ifa_index = kTestInterfaceLoopback}},
      {kIpv4PrivateAddress, ifaddrmsg{.ifa_family = AF_INET,
                                      .ifa_flags = IFA_F_TEMPORARY,
                                      .ifa_index = kTestInterfaceEth}}};
  // `online_links` does not include kTestInterfaceEth so that
  // kIpv4PrivateAddress is the inactive IPv4 connection. Also,
  // AddressTrackerLinux does not insert loopback interfaces into
  // `online_links`.
  stub_address_map_owner_.online_links() = {};

  EXPECT_TRUE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, HasOnlyLoopbackIpv6) {
  // Include only a loopback interface.
  stub_address_map_owner_.address_map() = {
      {kIpv6Loopback, ifaddrmsg{
                          .ifa_family = AF_INET6,
                          .ifa_flags = IFA_F_TEMPORARY,
                          .ifa_index = kTestInterfaceLoopback,
                      }}};
  // AddressTrackerLinux does not insert loopback interfaces into
  // `online_links`.
  stub_address_map_owner_.online_links() = {};

  EXPECT_TRUE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, HasActiveIPv6Connection) {
  stub_address_map_owner_.address_map() = {
      {kIpv6Loopback, ifaddrmsg{.ifa_family = AF_INET6,
                                .ifa_flags = IFA_F_TEMPORARY,
                                .ifa_index = kTestInterfaceLoopback}},
      {kIpv6Address, ifaddrmsg{.ifa_family = AF_INET6,
                               .ifa_flags = IFA_F_TEMPORARY,
                               .ifa_index = kTestInterfaceEth}}};
  // `online_links` includes kTestInterfaceEth so that kIpv6Address is the
  // active IPv6 connection. Also, AddressTrackerLinux does not insert loopback
  // interfaces into `online_links`.
  stub_address_map_owner_.online_links() = {kTestInterfaceEth};

  EXPECT_FALSE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, HasInactiveIPv6Connection) {
  stub_address_map_owner_.address_map() = {
      {kIpv6Loopback, ifaddrmsg{.ifa_family = AF_INET6,
                                .ifa_flags = IFA_F_TEMPORARY,
                                .ifa_index = kTestInterfaceLoopback}},
      {kIpv6Address, ifaddrmsg{.ifa_family = AF_INET6,
                               .ifa_flags = IFA_F_TEMPORARY,
                               .ifa_index = kTestInterfaceEth}}};
  // `online_links` does not include kTestInterfaceEth so that kIpv6Address is
  // the inactive IPv6 connection. Also, AddressTrackerLinux does not insert
  // loopback interfaces into `online_links`.
  stub_address_map_owner_.online_links() = {};

  EXPECT_TRUE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, IPv6LinkLocal) {
  // Include only IPv6 link-local interfaces.
  stub_address_map_owner_.address_map() = {
      {kIpv6LinkLocal, ifaddrmsg{
                           .ifa_family = AF_INET6,
                           .ifa_flags = IFA_F_TEMPORARY,
                           .ifa_index = 3,
                       }}};
  // Mark the IPv6 link-local interface as online.
  stub_address_map_owner_.online_links() = {3};

  EXPECT_TRUE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

TEST_F(LoopbackOnlyTest, ExtraOnlineLinks) {
  // Include only IPv6 link-local interfaces.
  stub_address_map_owner_.address_map() = {
      {kIpv6LinkLocal, ifaddrmsg{
                           .ifa_family = AF_INET6,
                           .ifa_flags = IFA_F_TEMPORARY,
                           .ifa_index = 3,
                       }}};
  // AddressTrackerLinux should not give us online links other than the ones
  // listed in the AddressMap. However, it's better if this code is resilient to
  // a mismatch if there is a bug (for example if the kernel truncates the
  // messages or the buffer the AddressTrackerLinux provides to the kernel is
  // too small). And if this code runs on a different thread from the
  // AddressMapOwnerLinux, AddressMap and online links are updated separately,
  // and so it is possible they can be inconsistent with each other.
  stub_address_map_owner_.online_links() = {1, 2, 3};

  EXPECT_TRUE(GetResultOfRunHaveOnlyLoopbackAddressesJob());
}

// TODO(crbug.com/40270154): Test HaveOnlyLoopbackAddressesUsingGetifaddrs().

#endif  // BUILDFLAG(IS_LINUX)

}  // namespace net
