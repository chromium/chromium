// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_job.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/log/test_net_log.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/stream_socket.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const char kHostName[] = "unresolvable.host.name";

class TransportConnectJobTest : public WithTaskEnvironment,
                                public testing::Test {
 public:
  TransportConnectJobTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        client_socket_factory_(&net_log_),
        common_connect_job_params_(
            &client_socket_factory_,
            &host_resolver_,
            nullptr /* http_auth_cache */,
            nullptr /* http_auth_handler_factory */,
            nullptr /* spdy_session_pool */,
            nullptr /* quic_supported_versions */,
            nullptr /* quic_stream_factory */,
            nullptr /* proxy_delegate */,
            nullptr /* http_user_agent_settings */,
            nullptr /* ssl_client_context */,
            nullptr /* socket_performance_watcher_factory */,
            nullptr /* network_quality_estimator */,
            &net_log_,
            nullptr /* websocket_endpoint_lock_manager */) {}

  ~TransportConnectJobTest() override {}

  static scoped_refptr<TransportSocketParams> DefaultParams() {
    return base::MakeRefCounted<TransportSocketParams>(
        HostPortPair(kHostName, 80), NetworkIsolationKey(),
        false /* disable_secure_dns */, OnHostResolutionCallback());
  }

 protected:
  RecordingTestNetLog net_log_;
  MockHostResolver host_resolver_;
  MockTransportClientSocketFactory client_socket_factory_;
  const CommonConnectJobParams common_connect_job_params_;
};

TEST_F(TransportConnectJobTest, MakeAddrListStartWithIPv4) {
  IPEndPoint addrlist_v4_1(IPAddress(192, 168, 1, 1), 80);
  IPEndPoint addrlist_v4_2(IPAddress(192, 168, 1, 2), 80);
  IPAddress ip_address;
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("2001:4860:b006::64"));
  IPEndPoint addrlist_v6_1(ip_address, 80);
  ASSERT_TRUE(ip_address.AssignFromIPLiteral("2001:4860:b006::66"));
  IPEndPoint addrlist_v6_2(ip_address, 80);

  AddressList addrlist;

  // Test 1: IPv4 only.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(2u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());

  // Test 2: IPv6 only.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(2u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[1].GetFamily());

  // Test 3: IPv4 then IPv6.  Expect no change.
  addrlist.clear();
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());

  // Test 4: IPv6, IPv4, IPv6, IPv4.  Expect first IPv6 moved to the end.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v6_2);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());

  // Test 5: IPv6, IPv6, IPv4, IPv4.  Expect first two IPv6's moved to the end.
  addrlist.clear();
  addrlist.push_back(addrlist_v6_1);
  addrlist.push_back(addrlist_v6_2);
  addrlist.push_back(addrlist_v4_1);
  addrlist.push_back(addrlist_v4_2);
  TransportConnectJob::MakeAddressListStartWithIPv4(&addrlist);
  ASSERT_EQ(4u, addrlist.size());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[0].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, addrlist[1].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[2].GetFamily());
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, addrlist[3].GetFamily());
}

TEST_F(TransportConnectJobTest, HostResolutionFailure) {
  host_resolver_.rules()->AddSimulatedTimeoutFailure(kHostName);

  //  Check sync and async failures.
  for (bool host_resolution_synchronous : {false, true}) {
    host_resolver_.set_synchronous_mode(host_resolution_synchronous);
    TestConnectJobDelegate test_delegate;
    TransportConnectJob transport_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        DefaultParams(), &test_delegate, nullptr /* net_log */);
    test_delegate.StartJobExpectingResult(&transport_connect_job,
                                          ERR_NAME_NOT_RESOLVED,
                                          host_resolution_synchronous);
    EXPECT_THAT(transport_connect_job.GetResolveErrorInfo().error,
                test::IsError(ERR_DNS_TIMED_OUT));
  }
}

TEST_F(TransportConnectJobTest, ConnectionFailure) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool connection_synchronous : {false, true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);
      client_socket_factory_.set_default_client_socket_type(
          connection_synchronous
              ? MockTransportClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET
              : MockTransportClientSocketFactory::
                    MOCK_PENDING_FAILING_CLIENT_SOCKET);
      TestConnectJobDelegate test_delegate;
      TransportConnectJob transport_connect_job(
          DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
          DefaultParams(), &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &transport_connect_job, ERR_CONNECTION_FAILED,
          host_resolution_synchronous && connection_synchronous);
    }
  }
}

TEST_F(TransportConnectJobTest, HostResolutionTimeout) {
  const base::TimeDelta kTinyTime = base::TimeDelta::FromMicroseconds(1);

  // Make request hang.
  host_resolver_.set_ondemand_mode(true);

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  ASSERT_THAT(transport_connect_job.Connect(), test::IsError(ERR_IO_PENDING));

  // Right up until just before expiration, the job does not time out.
  FastForwardBy(TransportConnectJob::ConnectionTimeout() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // But at the exact time of expiration, the job fails.
  FastForwardBy(kTinyTime);
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

TEST_F(TransportConnectJobTest, ConnectionTimeout) {
  const base::TimeDelta kTinyTime = base::TimeDelta::FromMicroseconds(1);

  // Half the timeout time. In the async case, spend half the time waiting on
  // host resolution, half on connecting.
  const base::TimeDelta kFirstHalfOfTimeout =
      TransportConnectJob::ConnectionTimeout() / 2;

  const base::TimeDelta kSecondHalfOfTimeout =
      TransportConnectJob::ConnectionTimeout() - kFirstHalfOfTimeout;
  ASSERT_LE(kTinyTime, kSecondHalfOfTimeout);

  // Make connection attempts hang.
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET);

  for (bool host_resolution_synchronous : {false, true}) {
    host_resolver_.set_ondemand_mode(!host_resolution_synchronous);
    TestConnectJobDelegate test_delegate;
    TransportConnectJob transport_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        DefaultParams(), &test_delegate, nullptr /* net_log */);
    EXPECT_THAT(transport_connect_job.Connect(), test::IsError(ERR_IO_PENDING));

    // After half the timeout, connection does not timeout.
    FastForwardBy(kFirstHalfOfTimeout);
    EXPECT_FALSE(test_delegate.has_result());

    // In the async case, the host resolution completes now.
    if (!host_resolution_synchronous)
      host_resolver_.ResolveOnlyRequestNow();

    // After (almost) the second half of timeout, just before the full timeout
    // period, the ConnectJob is still live.
    FastForwardBy(kSecondHalfOfTimeout - kTinyTime);
    EXPECT_FALSE(test_delegate.has_result());

    // But at the exact timeout time, the job fails.
    FastForwardBy(kTinyTime);
    EXPECT_TRUE(test_delegate.has_result());
    EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
  }
}

TEST_F(TransportConnectJobTest, ConnectionSuccess) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool connection_synchronous : {false, true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);
      client_socket_factory_.set_default_client_socket_type(
          connection_synchronous
              ? MockTransportClientSocketFactory::MOCK_CLIENT_SOCKET
              : MockTransportClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);
      TestConnectJobDelegate test_delegate;
      TransportConnectJob transport_connect_job(
          DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
          DefaultParams(), &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &transport_connect_job, OK,
          host_resolution_synchronous && connection_synchronous);
    }
  }
}

TEST_F(TransportConnectJobTest, DisableSecureDns) {
  for (bool disable_secure_dns : {false, true}) {
    TestConnectJobDelegate test_delegate;
    TransportConnectJob transport_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        base::MakeRefCounted<TransportSocketParams>(
            HostPortPair(kHostName, 80), NetworkIsolationKey(),
            disable_secure_dns, OnHostResolutionCallback()),
        &test_delegate, nullptr /* net_log */);
    test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                          false /* expect_sync_result */);
    EXPECT_EQ(disable_secure_dns,
              host_resolver_.last_secure_dns_mode_override().has_value());
    if (disable_secure_dns) {
      EXPECT_EQ(net::SecureDnsMode::kOff,
                host_resolver_.last_secure_dns_mode_override().value());
    }
  }
}

// Test the case of the IPv6 address stalling, and falling back to the IPv4
// socket which finishes first.
TEST_F(TransportConnectJobTest, IPv6FallbackSocketIPv4FinishesFirst) {
  MockTransportClientSocketFactory::ClientSocketType case_types[] = {
      // This is the IPv6 socket. It stalls, but presents one failed connection
      // attempt on GetConnectionAttempts.
      MockTransportClientSocketFactory::MOCK_STALLED_FAILING_CLIENT_SOCKET,
      // This is the IPv4 socket.
      MockTransportClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET};

  client_socket_factory_.set_client_socket_types(case_types, 2);

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_.rules()->AddIPLiteralRule(kHostName, "2:abcd::3:4:ff,2.2.2.2",
                                           std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv4());

  // Check that the failed connection attempt on the main socket is collected.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  ASSERT_EQ(1u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_TRUE(attempts[0].endpoint.address().IsIPv6());

  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// Test the case of the IPv6 address being slow, thus falling back to trying to
// connect to the IPv4 address, but having the connect to the IPv6 address
// finish first.
TEST_F(TransportConnectJobTest, IPv6FallbackSocketIPv6FinishesFirst) {
  MockTransportClientSocketFactory::ClientSocketType case_types[] = {
      // This is the IPv6 socket.
      MockTransportClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
      // This is the IPv4 socket. It stalls, but presents one failed connection
      // attempt on GetConnectionAttempts.
      MockTransportClientSocketFactory::MOCK_STALLED_FAILING_CLIENT_SOCKET};

  client_socket_factory_.set_client_socket_types(case_types, 2);
  client_socket_factory_.set_delay(base::TimeDelta::FromMilliseconds(
      TransportConnectJob::kIPv6FallbackTimerInMs + 50));

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_.rules()->AddIPLiteralRule(kHostName, "2:abcd::3:4:ff,2.2.2.2",
                                           std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv6());

  // Check that the failed connection attempt on the fallback socket is
  // collected.
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  ASSERT_EQ(1u, attempts.size());
  EXPECT_THAT(attempts[0].result, test::IsError(ERR_CONNECTION_FAILED));
  EXPECT_TRUE(attempts[0].endpoint.address().IsIPv4());

  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

TEST_F(TransportConnectJobTest, IPv6NoIPv4AddressesToFallbackTo) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET);

  // Resolve an AddressList with only IPv6 addresses.
  host_resolver_.rules()->AddIPLiteralRule(
      kHostName, "2:abcd::3:4:ff,3:abcd::3:4:ff", std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv6());
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  EXPECT_EQ(0u, attempts.size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(TransportConnectJobTest, IPv4HasNoFallback) {
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET);

  // Resolve an AddressList with only IPv4 addresses.
  host_resolver_.rules()->AddIPLiteralRule(kHostName, "1.1.1.1", std::string());

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);
  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        false /* expect_sync_result */);

  IPEndPoint endpoint;
  test_delegate.socket()->GetLocalAddress(&endpoint);
  EXPECT_TRUE(endpoint.address().IsIPv4());
  ConnectionAttempts attempts;
  test_delegate.socket()->GetConnectionAttempts(&attempts);
  EXPECT_EQ(0u, attempts.size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(TransportConnectJobTest, DnsAliases) {
  host_resolver_.set_synchronous_mode(true);
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::MOCK_CLIENT_SOCKET);

  // Resolve an AddressList with DNS aliases.
  std::vector<std::string> aliases({"alias1", "alias2", kHostName});
  host_resolver_.rules()->AddIPLiteralRuleWithDnsAliases(kHostName, "2.2.2.2",
                                                         std::move(aliases));

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);

  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        true /* expect_sync_result */);

  // Verify that the elements of the alias list are those from the
  // parameter vector.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre("alias1", "alias2", kHostName));
}

TEST_F(TransportConnectJobTest, NoAdditionalDnsAliases) {
  host_resolver_.set_synchronous_mode(true);
  client_socket_factory_.set_default_client_socket_type(
      MockTransportClientSocketFactory::MOCK_CLIENT_SOCKET);

  // Resolve an AddressList without additional DNS aliases. (The parameter
  // is an empty vector.)
  std::vector<std::string> aliases;
  host_resolver_.rules()->AddIPLiteralRuleWithDnsAliases(kHostName, "2.2.2.2",
                                                         std::move(aliases));

  TestConnectJobDelegate test_delegate;
  TransportConnectJob transport_connect_job(
      DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
      DefaultParams(), &test_delegate, nullptr /* net_log */);

  test_delegate.StartJobExpectingResult(&transport_connect_job, OK,
                                        true /* expect_sync_result */);

  // Verify that the alias list only contains kHostName.
  EXPECT_THAT(test_delegate.socket()->GetDnsAliases(),
              testing::ElementsAre(kHostName));
}

}  // namespace
}  // namespace net
