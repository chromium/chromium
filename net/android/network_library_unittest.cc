// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/test/task_environment.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_socket.h"
#include "net/socket/udp_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::android {

TEST(NetworkLibraryTest, CaptivePortal) {
  EXPECT_FALSE(android::GetIsCaptivePortal());
}

TEST(NetworkLibraryTest, GetWifiSignalLevel) {
  std::optional<int32_t> signal_strength = android::GetWifiSignalLevel();
  if (!signal_strength.has_value())
    return;
  EXPECT_LE(0, signal_strength.value());
  EXPECT_GE(4, signal_strength.value());
}

TEST(NetworkLibraryTest, GetDnsSearchDomains) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    GTEST_SKIP() << "Cannot call or test GetDnsServers() in pre-M.";
  }

  std::vector<IPEndPoint> dns_servers;
  bool dns_over_tls_active;
  std::string dns_over_tls_hostname;
  std::vector<std::string> search_suffixes;

  if (!GetCurrentDnsServers(&dns_servers, &dns_over_tls_active,
                            &dns_over_tls_hostname, &search_suffixes)) {
    return;
  }

  for (std::string suffix : search_suffixes) {
    EXPECT_FALSE(suffix.empty());
  }
}

TEST(NetworkLibraryTest, GetDnsSearchDomainsForNetwork) {
  base::test::TaskEnvironment task_environment;

  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_P) {
    GTEST_SKIP() << "Cannot call or test GetDnsServersForNetwork() in pre-P.";
  }

  NetworkChangeNotifierFactoryAndroid ncn_factory;
  NetworkChangeNotifier::DisableForTest ncn_disable_for_test;
  std::unique_ptr<NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  EXPECT_TRUE(NetworkChangeNotifier::AreNetworkHandlesSupported());

  auto default_network_handle = NetworkChangeNotifier::GetDefaultNetwork();
  if (default_network_handle == handles::kInvalidNetworkHandle)
    GTEST_SKIP() << "Could not retrieve a working active network handle.";

  std::vector<IPEndPoint> dns_servers;
  bool dns_over_tls_active;
  std::string dns_over_tls_hostname;
  std::vector<std::string> search_suffixes;

  if (!GetDnsServersForNetwork(&dns_servers, &dns_over_tls_active,
                               &dns_over_tls_hostname, &search_suffixes,
                               default_network_handle)) {
    return;
  }

  for (std::string suffix : search_suffixes) {
    EXPECT_FALSE(suffix.empty());
  }
}

TEST(NetworkLibraryTest, BindToNetwork) {
  base::test::TaskEnvironment task_environment;

  NetworkChangeNotifierFactoryAndroid ncn_factory;
  NetworkChangeNotifier::DisableForTest ncn_disable_for_test;
  std::unique_ptr<NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  std::unique_ptr<TCPSocket> socket_tcp_ipv4 =
      TCPSocket::Create(nullptr, nullptr, NetLogSource());
  ASSERT_EQ(OK, socket_tcp_ipv4->Open(ADDRESS_FAMILY_IPV4));
  std::unique_ptr<TCPSocket> socket_tcp_ipv6 =
      TCPSocket::Create(nullptr, nullptr, NetLogSource());
  ASSERT_EQ(OK, socket_tcp_ipv6->Open(ADDRESS_FAMILY_IPV6));
  UDPSocket socket_udp_ipv4(DatagramSocket::DEFAULT_BIND, nullptr,
                            NetLogSource());
  ASSERT_EQ(OK, socket_udp_ipv4.Open(ADDRESS_FAMILY_IPV4));
  UDPSocket socket_udp_ipv6(DatagramSocket::DEFAULT_BIND, nullptr,
                            NetLogSource());
  ASSERT_EQ(OK, socket_udp_ipv6.Open(ADDRESS_FAMILY_IPV6));
  std::array sockets{socket_tcp_ipv4->SocketDescriptorForTesting(),
                     socket_tcp_ipv6->SocketDescriptorForTesting(),
                     socket_udp_ipv4.SocketDescriptorForTesting(),
                     socket_udp_ipv6.SocketDescriptorForTesting()};

  for (SocketDescriptor socket : sockets) {
    if (base::android::BuildInfo::GetInstance()->sdk_int() >=
        base::android::SDK_VERSION_LOLLIPOP) {
      EXPECT_TRUE(NetworkChangeNotifier::AreNetworkHandlesSupported());
      // Test successful binding.
      handles::NetworkHandle existing_network_handle =
          NetworkChangeNotifier::GetDefaultNetwork();
      if (existing_network_handle != handles::kInvalidNetworkHandle) {
        EXPECT_EQ(OK, BindToNetwork(socket, existing_network_handle));
      }
      // Test invalid binding.
      EXPECT_EQ(ERR_INVALID_ARGUMENT,
                BindToNetwork(socket, handles::kInvalidNetworkHandle));
    }

    // Attempt to bind to a not existing handles::NetworkHandle.
    constexpr handles::NetworkHandle wrong_network_handle = 65536;
    int rv = BindToNetwork(socket, wrong_network_handle);
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_LOLLIPOP) {
      EXPECT_EQ(ERR_NOT_IMPLEMENTED, rv);
    } else if (base::android::BuildInfo::GetInstance()->sdk_int() >=
                   base::android::SDK_VERSION_LOLLIPOP &&
               base::android::BuildInfo::GetInstance()->sdk_int() <
                   base::android::SDK_VERSION_MARSHMALLOW) {
      // On Lollipop, we assume if the user has a handles::NetworkHandle that
      // they must have gotten it from a legitimate source, so if binding to the
      // network fails it's assumed to be because the network went away so
      // ERR_NETWORK_CHANGED is returned. In this test the network never existed
      // anyhow. ConnectivityService.MAX_NET_ID is 65535, so 65536 won't be
      // used.
      EXPECT_EQ(ERR_NETWORK_CHANGED, rv);
    } else if (base::android::BuildInfo::GetInstance()->sdk_int() >=
               base::android::SDK_VERSION_MARSHMALLOW) {
      // On Marshmallow and newer releases, the handles::NetworkHandle is munged
      // by Network.getNetworkHandle() and 65536 isn't munged so it's rejected.
      EXPECT_EQ(ERR_INVALID_ARGUMENT, rv);
    }
  }
}

}  // namespace net::android
