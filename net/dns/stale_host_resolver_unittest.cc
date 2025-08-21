// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/stale_host_resolver.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "stale_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kHostname[] = "example.com";
const char kCacheAddress[] = "1.1.1.1";
const char kNetworkAddress[] = "2.2.2.2";
const char kHostsAddress[] = "4.4.4.4";
const int kCacheEntryTTLSec = 300;

const int kNoStaleDelaySec = 0;
const int kLongStaleDelaySec = 3600;
const uint16_t kPort = 12345;

const int kAgeFreshSec = 0;
const int kAgeExpiredSec = kCacheEntryTTLSec * 2;

// How long to wait for resolve calls to return. If the tests are working
// correctly, we won't end up waiting this long -- it's just a backup.
const int kWaitTimeoutSec = 1;

std::vector<IPEndPoint> MakeEndpoints(const char* ip_address_str) {
  IPAddress address;
  bool rv = address.AssignFromIPLiteral(ip_address_str);
  DCHECK(rv);
  return std::vector<IPEndPoint>({{address, 0}});
}

AddressList MakeAddressList(const char* ip_address_str) {
  return AddressList(MakeEndpoints(ip_address_str));
}

std::unique_ptr<DnsClient> CreateMockDnsClientForHosts() {
  DnsConfig config;
  config.nameservers.push_back(IPEndPoint());
  ParseHosts("4.4.4.4 example.com", &config.hosts);

  return std::make_unique<MockDnsClient>(config, MockDnsClientRuleList());
}

// Create a DnsClient where address requests for |kHostname| will hang
// until unblocked via CompleteDelayedTransactions() and then fail.
std::unique_ptr<MockDnsClient> CreateHangingMockDnsClient() {
  DnsConfig config;
  config.nameservers.push_back(IPEndPoint());

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kHostname, dns_protocol::kTypeA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      true /* delay */);
  rules.emplace_back(
      kHostname, dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      true /* delay */);

  return std::make_unique<MockDnsClient>(config, std::move(rules));
}

class MockHostResolverProc : public HostResolverProc {
 public:
  // |result| is the net error code to return from resolution attempts.
  explicit MockHostResolverProc(int result)
      : HostResolverProc(nullptr), result_(result) {}

  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* address_list,
              int* os_error) override {
    *address_list = MakeAddressList(kNetworkAddress);
    return result_;
  }

 protected:
  ~MockHostResolverProc() override = default;

 private:
  // Result code to return from Resolve().
  const int result_;
};

class StaleHostResolverTest : public TestWithTaskEnvironment {
 protected:
  StaleHostResolverTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~StaleHostResolverTest() override {}

  void SetStaleDelay(int stale_delay_sec) {
    DCHECK(!resolver_);

    options_.delay = base::Seconds(stale_delay_sec);
  }

  void SetUseStaleOnNameNotResolved(bool enabled = true) {
    DCHECK(!resolver_);

    options_.use_stale_on_name_not_resolved = enabled;
  }

  void SetStaleUsability(int max_expired_time_sec,
                         int max_stale_uses,
                         bool allow_other_network) {
    DCHECK(!resolver_);

    options_.max_expired_time = base::Seconds(max_expired_time_sec);
    options_.max_stale_uses = max_stale_uses;
    options_.allow_other_network = allow_other_network;
  }

  void SetNetResult(int result) {
    DCHECK(!resolver_);

    mock_proc_ = new MockHostResolverProc(result);
  }

  std::unique_ptr<ContextHostResolver> CreateMockInnerResolverWithDnsClient(
      std::unique_ptr<DnsClient> dns_client,
      URLRequestContext* context = nullptr) {
    std::unique_ptr<ContextHostResolver> inner_resolver(
        HostResolver::CreateStandaloneContextResolver(nullptr));
    if (context) {
      inner_resolver->SetRequestContext(context);
    }

    HostResolverSystemTask::Params system_params(mock_proc_, 1u);
    inner_resolver->SetHostResolverSystemParamsForTest(system_params);
    if (dns_client) {
      inner_resolver->GetManagerForTesting()->SetDnsClientForTesting(
          std::move(dns_client));
      inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
          /*enabled=*/true,
          /*additional_dns_types_enabled=*/true);
    } else {
      inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
          /*enabled=*/false,
          /*additional_dns_types_enabled=*/false);
    }
    return inner_resolver;
  }

  void CreateResolverWithDnsClient(std::unique_ptr<DnsClient> dns_client) {
    DCHECK(!resolver_);

    stale_resolver_ = std::make_unique<StaleHostResolver>(
        CreateMockInnerResolverWithDnsClient(std::move(dns_client)), options_);
    resolver_ = stale_resolver_.get();
  }

  void SetResolver(StaleHostResolver* stale_resolver,
                   net::URLRequestContext* context = nullptr) {
    DCHECK(!resolver_);
    stale_resolver->inner_resolver_ =
        CreateMockInnerResolverWithDnsClient(nullptr /* dns_client */, context);
    resolver_ = stale_resolver;
  }

  void CreateResolver() { CreateResolverWithDnsClient(nullptr); }

  void DestroyResolver() {
    DCHECK(stale_resolver_);

    resolver_ = nullptr;
    stale_resolver_.reset();
  }

  void DropResolver() { resolver_ = nullptr; }

  // Creates a cache entry for |kHostname| that is |age_sec| seconds old.
  void CreateCacheEntry(int age_sec, int error) {
    DCHECK(resolver_);
    DCHECK(resolver_->GetHostCache());

    base::TimeDelta ttl(base::Seconds(kCacheEntryTTLSec));
    HostCache::Key key(kHostname, DnsQueryType::UNSPECIFIED, 0,
                       HostResolverSource::ANY, NetworkAnonymizationKey());
    HostCache::Entry entry(
        error,
        error == OK ? MakeEndpoints(kCacheAddress) : std::vector<IPEndPoint>(),
        /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN, ttl);
    auto age = base::Seconds(age_sec);
    auto then = base::TimeTicks::Now() - age;
    resolver_->GetHostCache()->Set(key, entry, then, ttl);
  }

  void OnNetworkChange() {
    // Real network changes on Android will send both notifications.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
    base::RunLoop().RunUntilIdle();  // Wait for notification.
  }

  void LookupStale() {
    DCHECK(resolver_);
    DCHECK(resolver_->GetHostCache());

    HostCache::Key key(kHostname, DnsQueryType::UNSPECIFIED, 0,
                       HostResolverSource::ANY, NetworkAnonymizationKey());
    auto now = base::TimeTicks::Now();
    HostCache::EntryStaleness stale;
    EXPECT_TRUE(resolver_->GetHostCache()->LookupStale(key, now, &stale));
    EXPECT_TRUE(stale.is_stale());
  }

  void Resolve(const std::optional<StaleHostResolver::ResolveHostParameters>&
                   optional_parameters) {
    DCHECK(resolver_);
    EXPECT_FALSE(resolve_pending_);

    request_ = resolver_->CreateRequest(
        HostPortPair(kHostname, kPort), NetworkAnonymizationKey(),
        NetLogWithSource(), optional_parameters);
    resolve_pending_ = true;
    resolve_complete_ = false;
    resolve_error_ = ERR_UNEXPECTED;

    int rv = request_->Start(base::BindOnce(
        &StaleHostResolverTest::OnResolveComplete, base::Unretained(this)));
    if (rv != ERR_IO_PENDING) {
      resolve_pending_ = false;
      resolve_complete_ = true;
      resolve_error_ = rv;
    }
  }

  void WaitForResolve() {
    if (!resolve_pending_) {
      return;
    }

    base::RunLoop run_loop;

    // Run until resolve completes or timeout.
    resolve_closure_ = run_loop.QuitWhenIdleClosure();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, resolve_closure_, base::Seconds(kWaitTimeoutSec));
    run_loop.Run();
  }

  void WaitForIdle() {
    base::RunLoop run_loop;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  void WaitForNetworkResolveComplete() {
    // The stale host resolver cache is initially setup with |kCacheAddress|,
    // so getting that address means that network resolve is still pending.
    // The network resolve is guaranteed to return |kNetworkAddress| at some
    // point because inner resolver is using MockHostResolverProc that always
    // returns |kNetworkAddress|.
    while (resolve_error() != OK ||
           resolve_addresses()[0].ToStringWithoutPort() != kNetworkAddress) {
      Resolve(std::nullopt);
      WaitForResolve();
    }
  }

  void Cancel() {
    DCHECK(resolver_);
    EXPECT_TRUE(resolve_pending_);

    request_ = nullptr;

    resolve_pending_ = false;
  }

  void OnResolveComplete(int error) {
    EXPECT_TRUE(resolve_pending_);

    resolve_error_ = error;
    resolve_pending_ = false;
    resolve_complete_ = true;

    if (!resolve_closure_.is_null()) {
      std::move(resolve_closure_).Run();
    }
  }

  bool resolve_complete() const { return resolve_complete_; }
  int resolve_error() const { return resolve_error_; }
  const AddressList& resolve_addresses() const {
    DCHECK(resolve_complete_);
    return request_->GetAddressResults();
  }

 private:
  std::unique_ptr<net::NetworkChangeNotifier> mock_network_change_notifier_{
      net::NetworkChangeNotifier::CreateMockIfNeeded()};

  scoped_refptr<MockHostResolverProc> mock_proc_{
      base::MakeRefCounted<MockHostResolverProc>(OK)};
  StaleHostResolver::StaleOptions options_;

  // Must outlive `resolver_`.
  std::unique_ptr<StaleHostResolver> stale_resolver_;

  raw_ptr<HostResolver> resolver_{nullptr};

  std::unique_ptr<HostResolver::ResolveHostRequest> request_;
  bool resolve_pending_{false};
  bool resolve_complete_{false};
  int resolve_error_;

  base::RepeatingClosure resolve_closure_;
};

// Make sure that test harness can be created and destroyed without crashing.
TEST_F(StaleHostResolverTest, Null) {}

// Make sure that resolver can be created and destroyed without crashing.
TEST_F(StaleHostResolverTest, Create) {
  CreateResolver();
}

TEST_F(StaleHostResolverTest, Network) {
  CreateResolver();

  Resolve(std::nullopt);
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kNetworkAddress, resolve_addresses()[0].ToStringWithoutPort());
}

TEST_F(StaleHostResolverTest, Hosts) {
  CreateResolverWithDnsClient(CreateMockDnsClientForHosts());

  Resolve(std::nullopt);
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kHostsAddress, resolve_addresses()[0].ToStringWithoutPort());
}

TEST_F(StaleHostResolverTest, FreshCache) {
  CreateResolver();
  CreateCacheEntry(kAgeFreshSec, OK);

  Resolve(std::nullopt);

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());

  WaitForIdle();
}

// Make sure that the default options are not changed unintentionally.
// Check with usages owners if this test failed due to your change.
TEST_F(StaleHostResolverTest, DefaultOptions) {
  StaleHostResolver::StaleOptions stale_options;

  EXPECT_TRUE(stale_options.allow_other_network);
  EXPECT_TRUE(stale_options.use_stale_on_name_not_resolved);
  EXPECT_EQ(base::Hours(6), stale_options.max_expired_time);
  EXPECT_EQ(1, stale_options.max_stale_uses);
}

// Flaky on Linux ASan, crbug.com/838524.
TEST_F(StaleHostResolverTest, StaleCache) {
  SetStaleDelay(kNoStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, OK);

  Resolve(std::nullopt);
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());
}

// If the resolver is destroyed before a stale cache entry is returned, the
// resolve should not complete.
TEST_F(StaleHostResolverTest, StaleCache_DestroyedResolver) {
  SetStaleDelay(kNoStaleDelaySec);
  CreateResolverWithDnsClient(CreateHangingMockDnsClient());
  CreateCacheEntry(kAgeExpiredSec, OK);

  Resolve(std::nullopt);
  DestroyResolver();
  WaitForResolve();

  EXPECT_FALSE(resolve_complete());
}

// Ensure that |use_stale_on_name_not_resolved| causes stale results to be
// returned when ERR_NAME_NOT_RESOLVED is returned from network resolution.
TEST_F(StaleHostResolverTest, StaleCacheNameNotResolvedEnabled) {
  SetStaleDelay(kLongStaleDelaySec);
  SetUseStaleOnNameNotResolved();
  SetNetResult(ERR_NAME_NOT_RESOLVED);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, OK);

  Resolve(std::nullopt);
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());
}

// Ensure that without |use_stale_on_name_not_resolved| network resolution
// failing causes StaleHostResolver jobs to fail with the same error code.
TEST_F(StaleHostResolverTest, StaleCacheNameNotResolvedDisabled) {
  SetStaleDelay(kLongStaleDelaySec);
  SetNetResult(ERR_NAME_NOT_RESOLVED);
  SetUseStaleOnNameNotResolved(false);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, OK);

  Resolve(std::nullopt);
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, resolve_error());
}

TEST_F(StaleHostResolverTest, NetworkWithStaleCache) {
  SetStaleDelay(kLongStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, OK);

  Resolve(std::nullopt);
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kNetworkAddress, resolve_addresses()[0].ToStringWithoutPort());
}

TEST_F(StaleHostResolverTest, CancelWithNoCache) {
  SetStaleDelay(kNoStaleDelaySec);
  CreateResolver();

  Resolve(std::nullopt);

  Cancel();

  EXPECT_FALSE(resolve_complete());

  // Make sure there's no lingering |OnResolveComplete()| callback waiting.
  WaitForIdle();
}

TEST_F(StaleHostResolverTest, CancelWithStaleCache) {
  SetStaleDelay(kLongStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, OK);

  Resolve(std::nullopt);

  Cancel();

  EXPECT_FALSE(resolve_complete());

  // Make sure there's no lingering |OnResolveComplete()| callback waiting.
  WaitForIdle();
}

TEST_F(StaleHostResolverTest, ReturnStaleCacheSync) {
  SetStaleDelay(kLongStaleDelaySec);
  CreateResolver();
  CreateCacheEntry(kAgeExpiredSec, OK);

  StaleHostResolver::ResolveHostParameters parameters;
  parameters.cache_usage =
      StaleHostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  Resolve(parameters);

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());

  WaitForIdle();
}

// CancelWithFreshCache makes no sense; the request would've returned
// synchronously.

TEST_F(StaleHostResolverTest, StaleUsability) {
  struct TestCase {
    int max_expired_time_sec;
    int max_stale_uses;
    bool allow_other_network;

    int age_sec;
    int stale_use;
    int network_changes;
    int error;

    bool usable;
  };

  const auto kUsabilityTestCases = std::to_array<TestCase>({
      // Fresh data always accepted.
      {0, 0, true, -1, 1, 0, OK, true},
      {1, 1, false, -1, 1, 0, OK, true},

      // Unlimited expired time accepts non-zero time.
      {0, 0, true, 1, 1, 0, OK, true},

      // Limited expired time accepts before but not after limit.
      {2, 0, true, 1, 1, 0, OK, true},
      {2, 0, true, 3, 1, 0, OK, false},

      // Unlimited stale uses accepts first and later uses.
      {2, 0, true, 1, 1, 0, OK, true},
      {2, 0, true, 1, 9, 0, OK, true},

      // Limited stale uses accepts up to and including limit.
      {2, 2, true, 1, 1, 0, OK, true},
      {2, 2, true, 1, 2, 0, OK, true},
      {2, 2, true, 1, 3, 0, OK, false},
      {2, 2, true, 1, 9, 0, OK, false},

      // Allowing other networks accepts zero or more network changes.
      {2, 0, true, 1, 1, 0, OK, true},
      {2, 0, true, 1, 1, 1, OK, true},
      {2, 0, true, 1, 1, 9, OK, true},

      // Disallowing other networks only accepts zero network changes.
      {2, 0, false, 1, 1, 0, OK, true},
      {2, 0, false, 1, 1, 1, OK, false},
      {2, 0, false, 1, 1, 9, OK, false},

      // Errors are only accepted if fresh.
      {0, 0, true, -1, 1, 0, ERR_NAME_NOT_RESOLVED, true},
      {1, 1, false, -1, 1, 0, ERR_NAME_NOT_RESOLVED, true},
      {0, 0, true, 1, 1, 0, ERR_NAME_NOT_RESOLVED, false},
      {2, 0, true, 1, 1, 0, ERR_NAME_NOT_RESOLVED, false},
      {2, 0, true, 1, 1, 0, ERR_NAME_NOT_RESOLVED, false},
      {2, 2, true, 1, 2, 0, ERR_NAME_NOT_RESOLVED, false},
      {2, 0, true, 1, 1, 1, ERR_NAME_NOT_RESOLVED, false},
      {2, 0, false, 1, 1, 0, ERR_NAME_NOT_RESOLVED, false},
  });

  SetStaleDelay(kNoStaleDelaySec);

  for (size_t i = 0; i < kUsabilityTestCases.size(); ++i) {
    SCOPED_TRACE(i);
    const auto& test_case = kUsabilityTestCases[i];

    SetStaleUsability(test_case.max_expired_time_sec, test_case.max_stale_uses,
                      test_case.allow_other_network);
    CreateResolver();
    CreateCacheEntry(kCacheEntryTTLSec + test_case.age_sec, test_case.error);

    FastForwardBy(base::Milliseconds(1));
    for (int j = 0; j < test_case.network_changes; ++j) {
      OnNetworkChange();
    }

    FastForwardBy(base::Milliseconds(1));
    for (int j = 0; j < test_case.stale_use - 1; ++j) {
      LookupStale();
    }

    FastForwardBy(base::Milliseconds(1));
    Resolve(std::nullopt);
    WaitForResolve();
    EXPECT_TRUE(resolve_complete());

    if (test_case.error == OK) {
      EXPECT_EQ(test_case.error, resolve_error());
      EXPECT_EQ(1u, resolve_addresses().size());
      {
        const char* expected =
            test_case.usable ? kCacheAddress : kNetworkAddress;
        EXPECT_EQ(expected, resolve_addresses()[0].ToStringWithoutPort());
      }
    } else {
      if (test_case.usable) {
        EXPECT_EQ(test_case.error, resolve_error());
      } else {
        EXPECT_EQ(OK, resolve_error());
        EXPECT_EQ(1u, resolve_addresses().size());
        EXPECT_EQ(kNetworkAddress,
                  resolve_addresses()[0].ToStringWithoutPort());
      }
    }
    // Make sure that all tasks complete so jobs are freed properly.
    FastForwardBy(base::Seconds(kLongStaleDelaySec));
    WaitForNetworkResolveComplete();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();

    DestroyResolver();
  }
}

}  // namespace

}  // namespace net
