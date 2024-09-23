// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_apple.h"

#include <optional>
#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/location.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_config_watcher_apple.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

static const char kIPv4PrivateAddrString1[] = "192.168.0.1";
static const char kIPv4PrivateAddrString2[] = "192.168.0.2";

static const char kIPv6PublicAddrString1[] =
    "2401:fa00:4:1000:be30:5b30:50e5:c0";
static const char kIPv6PublicAddrString2[] =
    "2401:fa00:4:1000:be30:5b30:50e5:c1";
static const char kIPv6LinkLocalAddrString1[] = "fe80::0:1:1:1";
static const char kIPv6LinkLocalAddrString2[] = "fe80::0:2:2:2";

class TestIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  TestIPAddressObserver() { NetworkChangeNotifier::AddIPAddressObserver(this); }

  TestIPAddressObserver(const TestIPAddressObserver&) = delete;
  TestIPAddressObserver& operator=(const TestIPAddressObserver&) = delete;

  ~TestIPAddressObserver() override {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }

  // Implements NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override { ip_address_changed_ = true; }

  bool ip_address_changed() const { return ip_address_changed_; }

 private:
  bool ip_address_changed_ = false;
};

}  // namespace

class NetworkChangeNotifierAppleTest : public WithTaskEnvironment,
                                       public ::testing::TestWithParam<bool> {
 public:
  NetworkChangeNotifierAppleTest() {
    if (ReduceIPAddressChangeNotificationEnabled()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kReduceIPAddressChangeNotification},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kReduceIPAddressChangeNotification});
    }
  }
  NetworkChangeNotifierAppleTest(const NetworkChangeNotifierAppleTest&) =
      delete;
  NetworkChangeNotifierAppleTest& operator=(
      const NetworkChangeNotifierAppleTest&) = delete;
  ~NetworkChangeNotifierAppleTest() override = default;

  void TearDown() override { RunUntilIdle(); }

 protected:
  bool ReduceIPAddressChangeNotificationEnabled() const { return GetParam(); }

  std::unique_ptr<NetworkChangeNotifierApple>
  CreateNetworkChangeNotifierApple() {
    auto notifier = std::make_unique<NetworkChangeNotifierApple>();
    base::RunLoop run_loop;
    notifier->SetCallbacksForTest(
        run_loop.QuitClosure(),
        base::BindRepeating(
            [](std::optional<NetworkInterfaceList>* network_interface_list,
               NetworkInterfaceList* list_out, int) {
              if (!network_interface_list->has_value()) {
                return false;
              }
              *list_out = **network_interface_list;
              return true;
            },
            &network_interface_list_),
        base::BindRepeating(
            [](std::string* ipv4_primary_interface_name, SCDynamicStoreRef)
                -> std::string { return *ipv4_primary_interface_name; },
            &ipv4_primary_interface_name_),
        base::BindRepeating(
            [](std::string* ipv6_primary_interface_name, SCDynamicStoreRef)
                -> std::string { return *ipv6_primary_interface_name; },
            &ipv6_primary_interface_name_));
    run_loop.Run();
    return notifier;
  }

  void SimulateDynamicStoreCallback(NetworkChangeNotifierApple& notifier,
                                    CFStringRef entity) {
    base::RunLoop run_loop;
    notifier.config_watcher_->GetNotifierThreadForTest()
        ->task_runner()
        ->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              base::apple::ScopedCFTypeRef<CFMutableArrayRef> array(
                  CFArrayCreateMutable(nullptr,
                                       /*capacity=*/0, &kCFTypeArrayCallBacks));
              base::apple::ScopedCFTypeRef<CFStringRef> entry_key(
                  SCDynamicStoreKeyCreateNetworkGlobalEntity(
                      nullptr, kSCDynamicStoreDomainState, entity));
              CFArrayAppendValue(array.get(), entry_key.get());
              notifier.OnNetworkConfigChange(array.get());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

 protected:
  std::optional<NetworkInterfaceList> network_interface_list_ =
      NetworkInterfaceList();
  std::string ipv4_primary_interface_name_ = "en0";
  std::string ipv6_primary_interface_name_ = "en0";

 private:
  // Allows us to allocate our own NetworkChangeNotifier for unit testing.
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkChangeNotifierAppleTest,
    ::testing::Values(true, false),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "ReduceIPAddressChangeNotificationEnabled"
                        : "ReduceIPAddressChangeNotificationDisabled";
    });

TEST_P(NetworkChangeNotifierAppleTest, NoInterfaceChange) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback without any change in
  // NetworkInterfaceList
  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  // When kReduceIPAddressChangeNotification feature is enabled, we ignores
  // the OnNetworkConfigChange callback without any network interface change.
  EXPECT_EQ(observer.ip_address_changed(),
            !ReduceIPAddressChangeNotificationEnabled());
}

TEST_P(NetworkChangeNotifierAppleTest, IPv4AddressChange) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with IPv4 address change.
  EXPECT_TRUE((*network_interface_list_)[0].address.AssignFromIPLiteral(
      kIPv4PrivateAddrString2));
  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

TEST_P(NetworkChangeNotifierAppleTest, PublicIPv6AddressChange) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv6PublicAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 64, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with a public IPv6 address change.
  EXPECT_TRUE((*network_interface_list_)[0].address.AssignFromIPLiteral(
      kIPv6PublicAddrString2));
  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv6);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

TEST_P(NetworkChangeNotifierAppleTest,
       LinkLocalIPv6AddressChangeOnPrimaryInterface) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv6LinkLocalAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 64, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with a link local IPv6 address
  // change on the primary interface "en0".
  EXPECT_TRUE((*network_interface_list_)[0].address.AssignFromIPLiteral(
      kIPv6LinkLocalAddrString2));
  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

TEST_P(NetworkChangeNotifierAppleTest,
       LinkLocalIPv6AddressChangeOnNonPrimaryInterface) {
  net::IPAddress ip_address1;
  EXPECT_TRUE(ip_address1.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address1, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  net::IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral(kIPv6LinkLocalAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en1", "en1", 2, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address2, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with a link local IPv6 address
  // change on the non-primary interface "en1".
  EXPECT_TRUE((*network_interface_list_)[1].address.AssignFromIPLiteral(
      kIPv6LinkLocalAddrString2));
  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  // When kReduceIPAddressChangeNotification feature is enabled, we ignores
  // the link local IPv6 address change on the non-primary interface.
  EXPECT_EQ(observer.ip_address_changed(),
            !ReduceIPAddressChangeNotificationEnabled());
}

TEST_P(NetworkChangeNotifierAppleTest, NewInterfaceWithIpV4) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with a new interface with a IPv4
  // address.
  net::IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral(kIPv4PrivateAddrString2));
  network_interface_list_->push_back(net::NetworkInterface(
      "en1", "en1", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address2, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

TEST_P(NetworkChangeNotifierAppleTest, NewInterfaceWithLinkLocalIpV6) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 2, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with a new interface with a link
  // local IPv6 address.
  net::IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral(kIPv6LinkLocalAddrString1));
  EXPECT_FALSE(ip_address2.IsPubliclyRoutable());
  network_interface_list_->push_back(net::NetworkInterface(
      "en1", "en1", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address2, 64, net::IP_ADDRESS_ATTRIBUTE_NONE));

  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  // When kReduceIPAddressChangeNotification feature is enabled, we ignores
  // the new link local IPv6 interface.
  EXPECT_EQ(observer.ip_address_changed(),
            !ReduceIPAddressChangeNotificationEnabled());
}

TEST_P(NetworkChangeNotifierAppleTest, NewInterfaceWithPublicIpV6) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 2, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback with a new interface with a
  // public IPv6 address.
  net::IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral(kIPv6PublicAddrString1));
  EXPECT_TRUE(ip_address2.IsPubliclyRoutable());
  network_interface_list_->push_back(net::NetworkInterface(
      "en1", "en1", 2, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address2, 64, net::IP_ADDRESS_ATTRIBUTE_NONE));

  TestIPAddressObserver observer;
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

TEST_P(NetworkChangeNotifierAppleTest, IPv4PrimaryInterfaceChange) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv4PrivateAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));
  net::IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral(kIPv4PrivateAddrString2));
  network_interface_list_->push_back(net::NetworkInterface(
      "en1", "en1", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address2, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback for the IPv4 primary interface
  // change.
  TestIPAddressObserver observer;
  ipv4_primary_interface_name_ = "en1";
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv4);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

TEST_P(NetworkChangeNotifierAppleTest, IPv6PrimaryInterfaceChange) {
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral(kIPv6PublicAddrString1));
  network_interface_list_->push_back(net::NetworkInterface(
      "en0", "en0", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));
  net::IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral(kIPv6PublicAddrString2));
  network_interface_list_->push_back(net::NetworkInterface(
      "en1", "en1", 1, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      ip_address2, 0, net::IP_ADDRESS_ATTRIBUTE_NONE));

  std::unique_ptr<NetworkChangeNotifierApple> notifier =
      CreateNetworkChangeNotifierApple();

  // Simulate OnNetworkConfigChange callback for the IPv6 primary interface
  // change.
  TestIPAddressObserver observer;
  ipv6_primary_interface_name_ = "en1";
  SimulateDynamicStoreCallback(*notifier, kSCEntNetIPv6);
  RunUntilIdle();
  EXPECT_TRUE(observer.ip_address_changed());
}

}  // namespace net
