// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MANAGER_UNITTEST_H_
#define NET_DNS_HOST_RESOLVER_MANAGER_UNITTEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_dns_task.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/test_dns_config_service.h"
#include "net/log/net_log_with_source.h"
#include "net/test/test_with_task_environment.h"

namespace net {

class MockHostResolverProc;

class HostResolverManagerTest : public TestWithTaskEnvironment {
 public:
  static const int kDefaultPort = 80;

  explicit HostResolverManagerTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME);

  ~HostResolverManagerTest() override;

  void CreateResolver(bool check_ipv6_on_wifi = true);

  virtual void DestroyResolver();

  // This HostResolverManager will only allow 1 outstanding resolve at a time
  // and perform no retries.
  void CreateSerialResolver(bool check_ipv6_on_wifi = true,
                            bool ipv6_reachable = true,
                            bool is_async = false);

  void StaleAllowedFromIpTest(bool is_async);
  void LocalOnlyFromIpTest(bool is_async);
  void ChangePriorityTest(bool is_async);
  void AbortOnlyExistingRequestsOnIPAddressChangeTest(bool is_async);
  void FlushCacheOnIPAddressChangeTest(bool is_async);
  void AbortOnIPAddressChangedTest(bool is_async);
  void NumericIPv6AddressTest(bool is_async);
  void NumericIPv6AddressWithSchemeTest(bool is_async);
  void LocalhostIPV4IPV6LookupTest(bool is_async);
  void IPv4AddressLiteralInIPv6OnlyNetworkTest(bool is_async);
  void IPv4AddressLiteralInIPv6OnlyNetworkPort443Test(bool is_async);
  void IPv4AddressLiteralInIPv6OnlyNetworkNoDns64Test(bool is_async);
  void IPv4AddressLiteralInIPv6OnlyNetworkBadAddressTest(bool is_async);

 protected:
  // testing::Test implementation:
  void SetUp() override;
  void TearDown() override;

  void CreateResolverWithLimitsAndParams(
      size_t max_concurrent_resolves,
      const HostResolverSystemTask::Params& params,
      bool ipv6_reachable,
      bool check_ipv6_on_wifi,
      bool is_async = false);

  virtual HostResolver::ManagerOptions DefaultOptions();

  virtual void CreateResolverWithOptionsAndParams(
      HostResolver::ManagerOptions options,
      const HostResolverSystemTask::Params& params,
      bool ipv6_reachable,
      bool is_async = false,
      bool ipv4_reachable = true);

  // Friendship is not inherited, so use proxies to access those.
  size_t num_running_dispatcher_jobs() const;

  void set_allow_fallback_to_systemtask(bool allow_fallback_to_systemtask);

  static unsigned maximum_insecure_dns_task_failures() {
    return DnsClient::kMaxInsecureFallbackFailures;
  }

  int StartIPv6ReachabilityCheck(
      const NetLogWithSource& net_log,
      raw_ptr<ClientSocketFactory> client_socket_factory,
      CompletionOnceCallback callback);

  bool GetLastIpv6ProbeResult();

  void PopulateCache(const HostCache::Key& key, IPEndPoint endpoint);

  const std::pair<const HostCache::Key, HostCache::Entry>* GetCacheHit(
      const HostCache::Key& key);

  void MakeCacheStale();

  IPEndPoint CreateExpected(const std::string& ip_literal, uint16_t port);

  scoped_refptr<MockHostResolverProc> proc_;
  std::unique_ptr<HostResolverManager> resolver_;
  std::unique_ptr<URLRequestContext> request_context_;
  std::unique_ptr<ResolveContext> resolve_context_;
};

// Specialized fixture for tests of DnsTask.
class HostResolverManagerDnsTest : public HostResolverManagerTest {
 public:
  explicit HostResolverManagerDnsTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  ~HostResolverManagerDnsTest() override;

  void DestroyResolver() override;

  // Note that this clears `mock_dns_client_`.
  void SetDnsClient(std::unique_ptr<DnsClient> dns_client);

  void Ipv6UnreachableTest(bool is_async);
  void Ipv6UnreachableInvalidConfigTest(bool is_async);

 protected:
  void TearDown() override;

  // HostResolverManagerTest implementation:
  HostResolver::ManagerOptions DefaultOptions() override;

  void CreateResolverWithOptionsAndParams(
      HostResolver::ManagerOptions options,
      const HostResolverSystemTask::Params& params,
      bool ipv6_reachable,
      bool is_async = false,
      bool ipv4_reachable = true) override;

  // Call after CreateResolver() to update the resolver with a new MockDnsClient
  // using`config` and `rules`.
  void UseMockDnsClient(const DnsConfig& config, MockDnsClientRuleList rules);

  static MockDnsClientRuleList CreateDefaultDnsRules();

  // Adds a rule to `rules`.
  static void AddDnsRule(MockDnsClientRuleList* rules,
                         const std::string& prefix,
                         uint16_t qtype,
                         MockDnsClientRule::ResultType result_type,
                         bool delay);

  static void AddDnsRule(MockDnsClientRuleList* rules,
                         const std::string& prefix,
                         uint16_t qtype,
                         const IPAddress& result_ip,
                         bool delay);

  static void AddDnsRule(MockDnsClientRuleList* rules,
                         const std::string& prefix,
                         uint16_t qtype,
                         IPAddress result_ip,
                         std::string cannonname,
                         bool delay);

  static void AddDnsRule(MockDnsClientRuleList* rules,
                         const std::string& prefix,
                         uint16_t qtype,
                         DnsResponse dns_test_response,
                         bool delay);

  static void AddSecureDnsRule(MockDnsClientRuleList* rules,
                               const std::string& prefix,
                               uint16_t qtype,
                               MockDnsClientRule::ResultType result_type,
                               bool delay);

  void ChangeDnsConfig(const DnsConfig& config);

  void InvalidateDnsConfig();

  void SetInitialDnsConfig(const DnsConfig& config);

  void TriggerInsecureFailureCondition();

  scoped_refptr<base::TestMockTimeTaskRunner> notifier_task_runner_;
  raw_ptr<TestDnsConfigService, DanglingUntriaged> config_service_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> notifier_;

  // Owned by `resolver_`.
  raw_ptr<MockDnsClient> mock_dns_client_ = nullptr;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MANAGER_UNITTEST_H_
