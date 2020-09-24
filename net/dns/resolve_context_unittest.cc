// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/resolve_context.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_server_iterator.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_socket_allocator.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/socket/socket_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class ResolveContextTest : public TestWithTaskEnvironment {
 public:
  ResolveContextTest() = default;

  scoped_refptr<DnsSession> CreateDnsSession(const DnsConfig& config) {
    auto null_random_callback =
        base::BindRepeating([](int, int) -> int { IMMEDIATE_CRASH(); });
    auto dns_socket_allocator = std::make_unique<DnsSocketAllocator>(
        socket_factory_.get(), config.nameservers, nullptr /* net_log */);

    return base::MakeRefCounted<DnsSession>(
        config, std::move(dns_socket_allocator), null_random_callback,
        nullptr /* netlog */);
  }

 protected:
  test::ScopedMockNetworkChangeNotifier mock_notifier_;

 private:
  std::unique_ptr<MockClientSocketFactory> socket_factory_ =
      std::make_unique<MockClientSocketFactory>();
};

DnsConfig CreateDnsConfig(int num_servers, int num_doh_servers) {
  DnsConfig config;
  for (int i = 0; i < num_servers; ++i) {
    IPEndPoint dns_endpoint(IPAddress(192, 168, 1, static_cast<uint8_t>(i)),
                            dns_protocol::kDefaultPort);
    config.nameservers.push_back(dns_endpoint);
  }
  for (int i = 0; i < num_doh_servers; ++i) {
    std::string server_template(
        base::StringPrintf("https://mock.http/doh_test_%d{?dns}", i));
    config.dns_over_https_servers.push_back(
        DnsOverHttpsServerConfig(server_template, true /* is_post */));
  }

  return config;
}

// Simulate a new session with the same pointer as an old deleted session by
// invalidating WeakPtrs.
TEST_F(ResolveContextTest, ReusedSessionPointer) {
  DnsConfig config =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  // Mark probe success for the "original" (pre-invalidation) session.
  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  ASSERT_TRUE(context.GetDohServerAvailability(1u, session.get()));

  // Simulate session destruction and recreation on the same pointer.
  session->InvalidateWeakPtrsForTesting();

  // Expect |session| should now be treated as a new session, not matching
  // |context|'s "current" session. Expect availability from the "old" session
  // should not be read and RecordServerSuccess() should have no effect because
  // the "new" session has not yet been marked as "current" through
  // InvalidateCaches().
  EXPECT_FALSE(context.GetDohServerAvailability(1u, session.get()));
  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  EXPECT_FALSE(context.GetDohServerAvailability(1u, session.get()));
}

TEST_F(ResolveContextTest, DohServerAvailability_InitialAvailability) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  EXPECT_EQ(context.NumAvailableDohServers(session.get()), 0u);
  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
}

TEST_F(ResolveContextTest, DohServerAvailability_RecordedSuccess) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  ASSERT_EQ(context.NumAvailableDohServers(session.get()), 0u);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  EXPECT_EQ(context.NumAvailableDohServers(session.get()), 1u);
  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
}

TEST_F(ResolveContextTest, DohServerAvailability_NoCurrentSession) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());

  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
  EXPECT_FALSE(context.GetDohServerAvailability(1, session.get()));
}

TEST_F(ResolveContextTest, DohServerAvailability_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session2.get(),
                                            true /* network_change */);

  // Use current session to set a probe result.
  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session2.get());

  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session1->config(), SecureDnsMode::kAutomatic, session1.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
  EXPECT_EQ(0u, context.NumAvailableDohServers(session1.get()));
  EXPECT_FALSE(context.GetDohServerAvailability(1u, session1.get()));

  // Different session for RecordServerFailure() should have no effect.
  ASSERT_TRUE(context.GetDohServerAvailability(1u, session2.get()));
  for (int i = 0; i < ResolveContext::kAutomaticModeFailureLimit; ++i) {
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session1.get());
  }
  EXPECT_TRUE(context.GetDohServerAvailability(1u, session2.get()));
}

TEST_F(ResolveContextTest, DohServerIndexToUse) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              session.get());
  EXPECT_EQ(context.NumAvailableDohServers(session.get()), 1u);
  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  EXPECT_FALSE(doh_itr->AttemptAvailable());
}

TEST_F(ResolveContextTest, DohServerIndexToUse_NoneEligible) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
}

TEST_F(ResolveContextTest, DohServerIndexToUse_SecureMode) {
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kSecure, session.get());

  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
}

class TestDnsObserver : public NetworkChangeNotifier::DNSObserver {
 public:
  void OnDNSChanged() override { ++dns_changed_calls_; }

  int dns_changed_calls() const { return dns_changed_calls_; }

 private:
  int dns_changed_calls_ = 0;
};

TEST_F(ResolveContextTest, DohServerAvailabilityNotification) {
  TestDnsObserver config_observer;
  NetworkChangeNotifier::AddDNSObserver(&config_observer);

  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  URLRequestContext request_context;
  ResolveContext context(&request_context, true /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(0, config_observer.dns_changed_calls());

  // Expect notification on first available DoH server.
  ASSERT_EQ(0u, context.NumAvailableDohServers(session.get()));
  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              session.get());
  ASSERT_EQ(1u, context.NumAvailableDohServers(session.get()));
  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(1, config_observer.dns_changed_calls());

  // No notifications as additional servers are available or unavailable.
  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(1, config_observer.dns_changed_calls());
  for (int i = 0; i < ResolveContext::kAutomaticModeFailureLimit; ++i) {
    ASSERT_EQ(2u, context.NumAvailableDohServers(session.get()));
    context.RecordServerFailure(0u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
    base::RunLoop().RunUntilIdle();  // Notifications are async.
    EXPECT_EQ(1, config_observer.dns_changed_calls());
  }
  ASSERT_EQ(1u, context.NumAvailableDohServers(session.get()));

  // Expect notification on last server unavailable.
  for (int i = 0; i < ResolveContext::kAutomaticModeFailureLimit; ++i) {
    ASSERT_EQ(1u, context.NumAvailableDohServers(session.get()));
    base::RunLoop().RunUntilIdle();  // Notifications are async.
    EXPECT_EQ(1, config_observer.dns_changed_calls());

    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
  }
  ASSERT_EQ(0u, context.NumAvailableDohServers(session.get()));
  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(2, config_observer.dns_changed_calls());

  NetworkChangeNotifier::RemoveDNSObserver(&config_observer);
}

TEST_F(ResolveContextTest, HostCacheInvalidation) {
  ResolveContext context(nullptr /* url_request_context */,
                         true /* enable_caching */);

  base::TimeTicks now;
  HostCache::Key key("example.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  context.host_cache()->Set(
      key,
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN),
      now, base::TimeDelta::FromSeconds(10));
  ASSERT_TRUE(context.host_cache()->Lookup(key, now));

  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  EXPECT_FALSE(context.host_cache()->Lookup(key, now));

  // Re-add to the host cache and now add some DoH server status.
  context.host_cache()->Set(
      key,
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN),
      now, base::TimeDelta::FromSeconds(10));
  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              session.get());
  ASSERT_TRUE(context.host_cache()->Lookup(key, now));
  ASSERT_TRUE(context.GetDohServerAvailability(0u, session.get()));

  // Invalidate again.
  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);
  context.InvalidateCachesAndPerSessionData(session2.get(),
                                            true /* network_change */);

  EXPECT_FALSE(context.host_cache()->Lookup(key, now));
  EXPECT_FALSE(context.GetDohServerAvailability(0u, session.get()));
  EXPECT_FALSE(context.GetDohServerAvailability(0u, session2.get()));
}

TEST_F(ResolveContextTest, HostCacheInvalidation_SameSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         true /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  // Initial invalidation just to set the session.
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  // Add to the host cache and add some DoH server status.
  base::TimeTicks now;
  HostCache::Key key("example.com", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkIsolationKey());
  context.host_cache()->Set(
      key,
      HostCache::Entry(OK, AddressList(), HostCache::Entry::SOURCE_UNKNOWN),
      now, base::TimeDelta::FromSeconds(10));
  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              session.get());
  ASSERT_TRUE(context.host_cache()->Lookup(key, now));
  ASSERT_TRUE(context.GetDohServerAvailability(0u, session.get()));

  // Invalidate again with the same session.
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  // Expect host cache to be invalidated but not the per-session data.
  EXPECT_FALSE(context.host_cache()->Lookup(key, now));
  EXPECT_TRUE(context.GetDohServerAvailability(0u, session.get()));
}

TEST_F(ResolveContextTest, Failures_Consecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  // Expect server preference to change after |config.attempts| failures.
  for (int i = 0; i < config.attempts; i++) {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);

    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, ERR_FAILED,
                                session.get());
  }

  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
  }

  // Expect failures to be reset on successful request.
  context.RecordServerSuccess(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
  }
}

TEST_F(ResolveContextTest, Failures_NonConsecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  for (int i = 0; i < config.attempts - 1; i++) {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);

    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, ERR_FAILED,
                                session.get());
  }

  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
  }

  context.RecordServerSuccess(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
  }

  // Expect server stay preferred through non-consecutive failures.
  context.RecordServerFailure(1u /* server_index */, false /* is_doh_server */,
                              ERR_FAILED, session.get());
  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
  }
}

TEST_F(ResolveContextTest, Failures_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  // No expected change from recording failures.
  for (int i = 0; i < config.attempts; i++) {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    EXPECT_FALSE(classic_itr->AttemptAvailable());

    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, ERR_FAILED,
                                session.get());
  }
  std::unique_ptr<DnsServerIterator> classic_itr =
      context.GetClassicDnsIterator(session->config(), session.get());

  EXPECT_FALSE(classic_itr->AttemptAvailable());
}

TEST_F(ResolveContextTest, Failures_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session2.get(),
                                            true /* network_change */);

  // No change from recording failures to wrong session.
  for (int i = 0; i < config1.attempts; i++) {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session2->config(), session2.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);

    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, ERR_FAILED,
                                session1.get());
  }
  std::unique_ptr<DnsServerIterator> classic_itr =
      context.GetClassicDnsIterator(session2->config(), session2.get());

  ASSERT_TRUE(classic_itr->AttemptAvailable());
  EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
  ASSERT_TRUE(classic_itr->AttemptAvailable());
  EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
}

// Test 2 of 3 servers failing.
TEST_F(ResolveContextTest, TwoFailures) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(3 /* num_servers */, 2 /* num_doh_servers */);
  config.attempts = 1;
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  // Expect server preference to change after |config.attempts| failures.
  for (int i = 0; i < config.attempts; i++) {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 2u);

    context.RecordServerFailure(0u /* server_index */,
                                false /* is_doh_server */, ERR_FAILED,
                                session.get());
    context.RecordServerFailure(1u /* server_index */,
                                false /* is_doh_server */, ERR_FAILED,
                                session.get());
  }
  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 2u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
  }

  // Expect failures to be reset on successful request.
  context.RecordServerSuccess(0u /* server_index */, false /* is_doh_server */,
                              session.get());
  context.RecordServerSuccess(1u /* server_index */, false /* is_doh_server */,
                              session.get());
  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        context.GetClassicDnsIterator(session->config(), session.get());

    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 1u);
    ASSERT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 2u);
  }
}

class TestDohStatusObserver : public ResolveContext::DohStatusObserver {
 public:
  void OnSessionChanged() override { ++session_changes_; }
  void OnDohServerUnavailable(bool network_change) override {
    ++server_unavailable_notifications_;
  }

  int session_changes() const { return session_changes_; }
  int server_unavailable_notifications() const {
    return server_unavailable_notifications_;
  }

 private:
  int session_changes_ = 0;
  int server_unavailable_notifications_ = 0;
};

TEST_F(ResolveContextTest, DohFailures_Consecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  TestDohStatusObserver observer;
  context.RegisterDohStatusObserver(&observer);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));
    EXPECT_EQ(0, observer.server_unavailable_notifications());
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
  }
  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
  EXPECT_EQ(1, observer.server_unavailable_notifications());

  context.UnregisterDohStatusObserver(&observer);
}

TEST_F(ResolveContextTest, DohFailures_NonConsecutive) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  TestDohStatusObserver observer;
  context.RegisterDohStatusObserver(&observer);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit - 1; i++) {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
  }
  {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
  }
  EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
  }
  EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));

  // Expect a single additional failure should not make a DoH server unavailable
  // because the success resets failure tracking.
  context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                              ERR_FAILED, session.get());
  {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
  }
  EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));

  EXPECT_EQ(0, observer.server_unavailable_notifications());
  context.UnregisterDohStatusObserver(&observer);
}

TEST_F(ResolveContextTest, DohFailures_SuccessAfterFailures) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  TestDohStatusObserver observer;
  context.RegisterDohStatusObserver(&observer);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
  }
  ASSERT_EQ(0u, context.NumAvailableDohServers(session.get()));
  EXPECT_EQ(1, observer.server_unavailable_notifications());

  // Expect a single success to make an unavailable DoH server available again.
  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
  }
  EXPECT_EQ(1u, context.NumAvailableDohServers(session.get()));

  EXPECT_EQ(1, observer.server_unavailable_notifications());
  context.UnregisterDohStatusObserver(&observer);
}

TEST_F(ResolveContextTest, DohFailures_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());

  // No expected change from recording failures.
  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
  }
  EXPECT_EQ(0u, context.NumAvailableDohServers(session.get()));
}

TEST_F(ResolveContextTest, DohFailures_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session2.get(),
                                            true /* network_change */);

  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session2.get());
  ASSERT_EQ(1u, context.NumAvailableDohServers(session2.get()));

  // No change from recording failures to wrong session.
  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    EXPECT_EQ(1u, context.NumAvailableDohServers(session2.get()));
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session1.get());
  }
  EXPECT_EQ(1u, context.NumAvailableDohServers(session2.get()));
}

// Test 2 of 3 DoH servers failing.
TEST_F(ResolveContextTest, TwoDohFailures) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              session.get());
  context.RecordServerSuccess(1u /* server_index */, true /* is_doh_server */,
                              session.get());
  context.RecordServerSuccess(2u /* server_index */, true /* is_doh_server */,
                              session.get());

  // Expect server preference to change after |config.attempts| failures.
  for (int i = 0; i < config.attempts; i++) {
    std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
        session->config(), SecureDnsMode::kAutomatic, session.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 2u);

    context.RecordServerFailure(0u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
    context.RecordServerFailure(1u /* server_index */, true /* is_doh_server */,
                                ERR_FAILED, session.get());
  }

  std::unique_ptr<DnsServerIterator> doh_itr = context.GetDohIterator(
      session->config(), SecureDnsMode::kAutomatic, session.get());

  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 2u);
}

// Expect default calculated timeout to be within 10ms of |DnsConfig::timeout|.
TEST_F(ResolveContextTest, Timeout_Default) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  base::TimeDelta delta =
      context.NextClassicTimeout(0 /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0 /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect short calculated timeout to be within 10ms of |DnsConfig::timeout|.
TEST_F(ResolveContextTest, Timeout_ShortConfigured) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  config.timeout = base::TimeDelta::FromMilliseconds(15);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  base::TimeDelta delta =
      context.NextClassicTimeout(0 /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0 /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect long calculated timeout to be equal to |DnsConfig::timeout|.
// (Default max timeout is 5 seconds, so NextTimeout should return exactly
// the config timeout.)
TEST_F(ResolveContextTest, Timeout_LongConfigured) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  config.timeout = base::TimeDelta::FromSeconds(15);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  EXPECT_EQ(context.NextClassicTimeout(0 /* server_index */, 0 /* attempt */,
                                       session.get()),
            config.timeout);
  EXPECT_EQ(context.NextDohTimeout(0 /* doh_server_index */, session.get()),
            config.timeout);
}

// Expect timeouts to increase on recording long round-trip times.
TEST_F(ResolveContextTest, Timeout_LongRtt) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
    context.RecordRtt(1u /* server_index */, true /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
  }

  // Expect servers with high recorded RTT to have increased timeouts (>10ms).
  base::TimeDelta delta =
      context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_GT(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(1u, session.get()) - config.timeout;
  EXPECT_GT(delta, base::TimeDelta::FromMilliseconds(10));

  // Servers without recorded RTT expected to remain the same (<=10ms).
  delta = context.NextClassicTimeout(1u /* server_index */, 0 /* attempt */,
                                     session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0u /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect recording round-trip times to have no affect on timeout without a
// current session.
TEST_F(ResolveContextTest, Timeout_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);

  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
    context.RecordRtt(1u /* server_index */, true /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session.get());
  }

  base::TimeDelta delta =
      context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                 session.get()) -
      config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(1u /* doh_server_index */, session.get()) -
          config.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
}

// Expect recording round-trip times to have no affect on timeout without a
// current session.
TEST_F(ResolveContextTest, Timeout_DifferentSession) {
  DnsConfig config1 =
      CreateDnsConfig(1 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session1 = CreateDnsSession(config1);

  DnsConfig config2 =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session2 = CreateDnsSession(config2);

  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  context.InvalidateCachesAndPerSessionData(session2.get(),
                                            true /* network_change */);

  // Record RTT's to increase timeouts for current session.
  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session2.get());
    context.RecordRtt(1u /* server_index */, true /* is_doh_server */,
                      base::TimeDelta::FromMinutes(10), OK, session2.get());
  }

  // Expect normal short timeouts for other session.
  base::TimeDelta delta =
      context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                 session1.get()) -
      config1.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));
  delta = context.NextDohTimeout(0u /* doh_server_index */, session1.get()) -
          config1.timeout;
  EXPECT_LE(delta, base::TimeDelta::FromMilliseconds(10));

  // Recording RTT's for other session should have no effect on current session
  // timeouts.
  base::TimeDelta timeout = context.NextClassicTimeout(
      0u /* server_index */, 0 /* attempt */, session2.get());
  for (int i = 0; i < 50; ++i) {
    context.RecordRtt(0u /* server_index */, false /* is_doh_server */,
                      base::TimeDelta::FromMilliseconds(1), OK, session1.get());
  }
  EXPECT_EQ(timeout,
            context.NextClassicTimeout(0u /* server_index */, 0 /* attempt */,
                                       session2.get()));
}

// Ensures that reported negative RTT values don't cause a crash. Regression
// test for https://crbug.com/753568.
TEST_F(ResolveContextTest, NegativeRtt) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);
  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 2 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  context.RecordRtt(0 /* server_index */, false /* is_doh_server */,
                    base::TimeDelta::FromMilliseconds(-1), OK /* rv */,
                    session.get());
  context.RecordRtt(0 /* server_index */, true /* is_doh_server */,
                    base::TimeDelta::FromMilliseconds(-1), OK /* rv */,
                    session.get());
}

TEST_F(ResolveContextTest, SessionChange) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);

  TestDohStatusObserver observer;
  context.RegisterDohStatusObserver(&observer);

  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 3 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  EXPECT_EQ(observer.session_changes(), 1);
  // Should get a server unavailable notification because there is >0 DoH
  // servers that are reset on cache invalidation.
  EXPECT_EQ(observer.server_unavailable_notifications(), 1);

  context.UnregisterDohStatusObserver(&observer);
}

TEST_F(ResolveContextTest, SessionChange_NoSession) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);

  TestDohStatusObserver observer;
  context.RegisterDohStatusObserver(&observer);

  context.InvalidateCachesAndPerSessionData(nullptr /* new_session */,
                                            false /* network_change */);

  EXPECT_EQ(observer.session_changes(), 1);
  EXPECT_EQ(observer.server_unavailable_notifications(), 0);

  context.UnregisterDohStatusObserver(&observer);
}

TEST_F(ResolveContextTest, SessionChange_NoDohServers) {
  ResolveContext context(nullptr /* url_request_context */,
                         false /* enable_caching */);

  TestDohStatusObserver observer;
  context.RegisterDohStatusObserver(&observer);

  DnsConfig config =
      CreateDnsConfig(2 /* num_servers */, 0 /* num_doh_servers */);
  scoped_refptr<DnsSession> session = CreateDnsSession(config);
  context.InvalidateCachesAndPerSessionData(session.get(),
                                            false /* network_change */);

  EXPECT_EQ(observer.session_changes(), 1);
  EXPECT_EQ(observer.server_unavailable_notifications(), 0);

  context.UnregisterDohStatusObserver(&observer);
}

}  // namespace
}  // namespace net
