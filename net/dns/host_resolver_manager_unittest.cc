// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/host_resolver_manager_unittest.h"

#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/connection_endpoint_metadata_test_util.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/schemeful_site.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_cache.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_internal_result_test_util.h"
#include "net/dns/host_resolver_results_test_util.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/mock_mdns_client.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/resolve_context.h"
#include "net/dns/test_dns_config_service.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif  // BUILDFLAG(ENABLE_MDNS)

using net::test::IsError;
using net::test::IsOk;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Between;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

const size_t kMaxJobs = 10u;
const size_t kMaxRetryAttempts = 4u;

HostResolverSystemTask::Params DefaultParams(
    scoped_refptr<HostResolverProc> resolver_proc) {
  return HostResolverSystemTask::Params(std::move(resolver_proc),
                                        kMaxRetryAttempts);
}

class ResolveHostResponseHelper {
 public:
  using Callback =
      base::OnceCallback<void(CompletionOnceCallback completion_callback,
                              int error)>;

  ResolveHostResponseHelper() = default;
  explicit ResolveHostResponseHelper(
      std::unique_ptr<HostResolver::ResolveHostRequest> request)
      : request_(std::move(request)) {
    top_level_result_error_ = request_->Start(base::BindOnce(
        &ResolveHostResponseHelper::OnComplete, base::Unretained(this)));
  }
  ResolveHostResponseHelper(
      std::unique_ptr<HostResolver::ResolveHostRequest> request,
      Callback custom_callback)
      : request_(std::move(request)) {
    top_level_result_error_ = request_->Start(
        base::BindOnce(std::move(custom_callback),
                       base::BindOnce(&ResolveHostResponseHelper::OnComplete,
                                      base::Unretained(this))));
  }

  ResolveHostResponseHelper(const ResolveHostResponseHelper&) = delete;
  ResolveHostResponseHelper& operator=(const ResolveHostResponseHelper&) =
      delete;

  bool complete() const { return top_level_result_error_ != ERR_IO_PENDING; }

  int top_level_result_error() {
    WaitForCompletion();
    return top_level_result_error_;
  }

  int result_error() {
    WaitForCompletion();
    return request_->GetResolveErrorInfo().error;
  }

  HostResolver::ResolveHostRequest* request() { return request_.get(); }

  void CancelRequest() {
    DCHECK(request_);
    DCHECK(!complete());

    request_ = nullptr;
  }

  void OnComplete(int error) {
    DCHECK(!complete());
    top_level_result_error_ = error;

    run_loop_.Quit();
  }

 private:
  void WaitForCompletion() {
    DCHECK(request_);
    if (complete()) {
      return;
    }
    run_loop_.Run();
    DCHECK(complete());
  }

  std::unique_ptr<HostResolver::ResolveHostRequest> request_;
  int top_level_result_error_ = ERR_IO_PENDING;
  base::RunLoop run_loop_;
};

// Using LookupAttemptHostResolverProc simulate very long lookups, and control
// which attempt resolves the host.
class LookupAttemptHostResolverProc : public HostResolverProc {
 public:
  LookupAttemptHostResolverProc(HostResolverProc* previous,
                                int attempt_number_to_resolve,
                                int total_attempts)
      : HostResolverProc(previous),
        attempt_number_to_resolve_(attempt_number_to_resolve),
        total_attempts_(total_attempts),
        all_done_(&lock_),
        blocked_attempt_signal_(&lock_) {}

  // Test harness will wait for all attempts to finish before checking the
  // results.
  void WaitForAllAttemptsToFinish() {
    base::AutoLock auto_lock(lock_);
    while (total_attempts_resolved_ != total_attempts_) {
      all_done_.Wait();
    }
  }

  void WaitForNAttemptsToBeBlocked(int n) {
    base::AutoLock auto_lock(lock_);
    while (num_attempts_waiting_ < n) {
      blocked_attempt_signal_.Wait();
    }
  }

  // All attempts will wait for an attempt to resolve the host.
  void WaitForAnAttemptToComplete() {
    {
      base::AutoLock auto_lock(lock_);
      base::ScopedAllowBaseSyncPrimitivesForTesting
          scoped_allow_base_sync_primitives;
      while (resolved_attempt_number_ == 0)
        all_done_.Wait();
    }
    all_done_.Broadcast();  // Tell all waiting attempts to proceed.
  }

  // Returns the number of attempts that have finished the Resolve() method.
  int GetTotalAttemptsResolved() {
    base::AutoLock auto_lock(lock_);
    return total_attempts_resolved_;
  }

  // Sets the resolved attempt number and unblocks waiting
  // attempts.
  void SetResolvedAttemptNumber(int n) {
    base::AutoLock auto_lock(lock_);
    EXPECT_EQ(0, resolved_attempt_number_);
    resolved_attempt_number_ = n;
    all_done_.Broadcast();
  }

  // HostResolverProc methods.
  int Resolve(const std::string& host,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override {
    bool wait_for_right_attempt_to_complete = true;
    {
      base::AutoLock auto_lock(lock_);
      ++current_attempt_number_;
      ++num_attempts_waiting_;
      if (current_attempt_number_ == attempt_number_to_resolve_) {
        resolved_attempt_number_ = current_attempt_number_;
        wait_for_right_attempt_to_complete = false;
      }
    }

    blocked_attempt_signal_.Broadcast();

    if (wait_for_right_attempt_to_complete)
      // Wait for the attempt_number_to_resolve_ attempt to resolve.
      WaitForAnAttemptToComplete();

    int result = ResolveUsingPrevious(host, address_family, host_resolver_flags,
                                      addrlist, os_error);

    {
      base::AutoLock auto_lock(lock_);
      ++total_attempts_resolved_;
      --num_attempts_waiting_;
    }

    all_done_.Broadcast();  // Tell all attempts to proceed.

    // Since any negative number is considered a network error, with -1 having
    // special meaning (ERR_IO_PENDING). We could return the attempt that has
    // resolved the host as a negative number. For example, if attempt number 3
    // resolves the host, then this method returns -4.
    if (result == OK)
      return -1 - resolved_attempt_number_;
    else
      return result;
  }

 protected:
  ~LookupAttemptHostResolverProc() override = default;

 private:
  int attempt_number_to_resolve_;
  int current_attempt_number_ = 0;  // Incremented whenever Resolve is called.
  int total_attempts_;
  int total_attempts_resolved_ = 0;
  int resolved_attempt_number_ = 0;
  int num_attempts_waiting_ = 0;

  // All attempts wait for right attempt to be resolve.
  base::Lock lock_;
  base::ConditionVariable all_done_;
  base::ConditionVariable blocked_attempt_signal_;
};

// TestHostResolverManager's sole purpose is to mock the IPv6 reachability test.
// By default, this pretends that IPv6 is globally reachable.
// This class is necessary so unit tests run the same on dual-stack machines as
// well as IPv4 only machines.
class TestHostResolverManager : public HostResolverManager {
 public:
  TestHostResolverManager(const HostResolver::ManagerOptions& options,
                          SystemDnsConfigChangeNotifier* notifier,
                          NetLog* net_log,
                          bool ipv6_reachable = true,
                          bool ipv4_reachable = true,
                          bool is_async = false)
      : HostResolverManager(options, notifier, net_log),
        ipv6_reachable_(ipv6_reachable),
        ipv4_reachable_(ipv4_reachable),
        is_async_(is_async) {}

  ~TestHostResolverManager() override = default;

 private:
  const bool ipv6_reachable_;
  const bool ipv4_reachable_;
  const bool is_async_;

  int StartGloballyReachableCheck(const IPAddress& dest,
                                  const NetLogWithSource& net_log,
                                  ClientSocketFactory* client_socket_factory,
                                  CompletionOnceCallback callback) override {
    int rv = OK;
    if (dest.IsIPv6()) {
      rv = ipv6_reachable_ ? OK : ERR_FAILED;
    } else {
      rv = ipv4_reachable_ ? OK : ERR_FAILED;
    }
    if (is_async_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), rv));
      return ERR_IO_PENDING;
    }
    return rv;
  }
};

bool HasAddress(const IPAddress& search_address,
                const std::vector<IPEndPoint>& addresses) {
  for (const auto& address : addresses) {
    if (search_address == address.address())
      return true;
  }
  return false;
}

void TestBothLoopbackIPs(const std::string& host) {
  std::vector<IPEndPoint> addresses;
  EXPECT_TRUE(ResolveLocalHostname(host, &addresses));
  EXPECT_EQ(2u, addresses.size());
  EXPECT_TRUE(HasAddress(IPAddress::IPv4Localhost(), addresses));
  EXPECT_TRUE(HasAddress(IPAddress::IPv6Localhost(), addresses));
}

// Returns the DoH provider entry in `DohProviderEntry::GetList()` that matches
// `provider`. Crashes if there is no matching entry.
const DohProviderEntry& GetDohProviderEntryForTesting(
    std::string_view provider) {
  auto provider_list = DohProviderEntry::GetList();
  auto it =
      base::ranges::find(provider_list, provider, &DohProviderEntry::provider);
  CHECK(it != provider_list.end());
  return **it;
}

void DisableHostResolverCache(base::test::ScopedFeatureList& feature_list) {
  // The HappyEyeballsV3 feature depends on the UseHostResolverCache feature.
  // Disable them together for tests that disables the UseHostResolverCache
  // feature.
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kUseHostResolverCache,
                             features::kHappyEyeballsV3});
}

}  // namespace

HostResolverManagerTest::HostResolverManagerTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : TestWithTaskEnvironment(time_source),
      proc_(base::MakeRefCounted<MockHostResolverProc>()) {}

HostResolverManagerTest::~HostResolverManagerTest() = default;

void HostResolverManagerTest::CreateResolver(bool check_ipv6_on_wifi) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    check_ipv6_on_wifi);
}

void HostResolverManagerTest::DestroyResolver() {
  if (!resolver_) {
    return;
  }

  resolver_->DeregisterResolveContext(resolve_context_.get());
  resolver_ = nullptr;
}

  // This HostResolverManager will only allow 1 outstanding resolve at a time
  // and perform no retries.
void HostResolverManagerTest::CreateSerialResolver(bool check_ipv6_on_wifi,
                                                   bool ipv6_reachable,
                                                   bool is_async) {
  HostResolverSystemTask::Params params = DefaultParams(proc_);
  params.max_retry_attempts = 0u;
  CreateResolverWithLimitsAndParams(1u, params, ipv6_reachable,
                                    check_ipv6_on_wifi, is_async);
}

void HostResolverManagerTest::SetUp() {
  request_context_ = CreateTestURLRequestContextBuilder()->Build();
  resolve_context_ = std::make_unique<ResolveContext>(
      request_context_.get(), true /* enable_caching */);
  CreateResolver();
}

void HostResolverManagerTest::TearDown() {
  if (resolver_) {
    EXPECT_EQ(0u, resolver_->num_running_dispatcher_jobs_for_tests());
  }
  DestroyResolver();
  EXPECT_FALSE(proc_->HasBlockedRequests());
}

void HostResolverManagerTest::CreateResolverWithLimitsAndParams(
    size_t max_concurrent_resolves,
    const HostResolverSystemTask::Params& params,
    bool ipv6_reachable,
    bool check_ipv6_on_wifi,
    bool is_async) {
  HostResolver::ManagerOptions options = DefaultOptions();
  options.max_concurrent_resolves = max_concurrent_resolves;
  options.check_ipv6_on_wifi = check_ipv6_on_wifi;

  CreateResolverWithOptionsAndParams(std::move(options), params, ipv6_reachable,
                                     is_async);
}

HostResolver::ManagerOptions HostResolverManagerTest::DefaultOptions() {
  HostResolver::ManagerOptions options;
  options.max_concurrent_resolves = kMaxJobs;
  options.max_system_retry_attempts = kMaxRetryAttempts;
  return options;
}

void HostResolverManagerTest::CreateResolverWithOptionsAndParams(
    HostResolver::ManagerOptions options,
    const HostResolverSystemTask::Params& params,
    bool ipv6_reachable,
    bool is_async,
    bool ipv4_reachable) {
  // Use HostResolverManagerDnsTest if enabling DNS client.
  DCHECK(!options.insecure_dns_client_enabled);

  DestroyResolver();

  resolver_ = std::make_unique<TestHostResolverManager>(
      options, nullptr /* notifier */, nullptr /* net_log */, ipv6_reachable,
      ipv4_reachable, is_async);
  resolver_->set_host_resolver_system_params_for_test(params);
  resolver_->RegisterResolveContext(resolve_context_.get());
}

size_t HostResolverManagerTest::num_running_dispatcher_jobs() const {
  DCHECK(resolver_.get());
  return resolver_->num_running_dispatcher_jobs_for_tests();
}

void HostResolverManagerTest::set_allow_fallback_to_systemtask(
    bool allow_fallback_to_systemtask) {
  DCHECK(resolver_.get());
  resolver_->allow_fallback_to_systemtask_ = allow_fallback_to_systemtask;
}

int HostResolverManagerTest::StartIPv6ReachabilityCheck(
    const NetLogWithSource& net_log,
    raw_ptr<ClientSocketFactory> client_socket_factory,
    CompletionOnceCallback callback) {
  return resolver_->StartIPv6ReachabilityCheck(net_log, client_socket_factory,
                                               std::move(callback));
}

bool HostResolverManagerTest::GetLastIpv6ProbeResult() {
  return resolver_->last_ipv6_probe_result_;
}

void HostResolverManagerTest::PopulateCache(const HostCache::Key& key,
                                            IPEndPoint endpoint) {
  resolver_->CacheResult(resolve_context_->host_cache(), key,
                         HostCache::Entry(OK, {endpoint}, /*aliases=*/{},
                                          HostCache::Entry::SOURCE_UNKNOWN),
                         base::Seconds(1));
}

const std::pair<const HostCache::Key, HostCache::Entry>*
HostResolverManagerTest::GetCacheHit(const HostCache::Key& key) {
  DCHECK(resolve_context_->host_cache());
  return resolve_context_->host_cache()->LookupStale(
      key, base::TimeTicks(), nullptr, false /* ignore_secure */);
}

void HostResolverManagerTest::MakeCacheStale() {
  DCHECK(resolve_context_->host_cache());
  resolve_context_->host_cache()->Invalidate();
}

IPEndPoint HostResolverManagerTest::CreateExpected(
    const std::string& ip_literal,
    uint16_t port) {
  IPAddress ip;
  bool result = ip.AssignFromIPLiteral(ip_literal);
  DCHECK(result);
  return IPEndPoint(ip, port);
}

TEST_F(HostResolverManagerTest, AsynchronousLookup) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.top_level_result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                                 0 /* host_resolver_flags */,
                                 HostResolverSource::ANY,
                                 NetworkAnonymizationKey()));
  EXPECT_TRUE(cache_result);

  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "just.testing", NetworkAnonymizationKey(), DnsQueryType::A,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  "just.testing", DnsQueryType::A,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ElementsAre(CreateExpected("192.168.1.42", 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "just.testing", NetworkAnonymizationKey(), DnsQueryType::AAAA,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalErrorResult(
                  "just.testing", DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ERR_NAME_NOT_RESOLVED)));
}

// TODO(crbug.com/40181080): Confirm scheme behavior once it affects behavior.
TEST_F(HostResolverManagerTest, AsynchronousLookupWithScheme) {
  proc_->AddRuleForAllFamilies("host.test", "192.168.1.42");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpScheme, "host.test", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.top_level_result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());

  EXPECT_EQ("host.test", proc_->GetCaptureList()[0].hostname);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(
          HostCache::Key(url::SchemeHostPort(url::kHttpScheme, "host.test", 80),
                         DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
                         HostResolverSource::ANY, NetworkAnonymizationKey()));
  EXPECT_TRUE(cache_result);
}

TEST_F(HostResolverManagerTest, AsynchronousIpv6Lookup) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  proc_->AddRuleForAllFamilies("foo.test", "2001:db8:1::");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpScheme, "foo.test", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              Pointee(ElementsAre(ExpectEndpointResult(
                  ElementsAre(CreateExpected("2001:db8:1::", 80))))));

  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "foo.test", NetworkAnonymizationKey(), DnsQueryType::A,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalErrorResult(
                  "foo.test", DnsQueryType::A,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "foo.test", NetworkAnonymizationKey(), DnsQueryType::AAAA,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  "foo.test", DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ElementsAre(CreateExpected("2001:db8:1::", 0)))));
}

TEST_F(HostResolverManagerTest, AsynchronousAllFamilyLookup) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  proc_->AddRuleForAllFamilies("foo.test", "192.168.1.43,2001:db8:2::");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpScheme, "foo.test", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              Pointee(ElementsAre(ExpectEndpointResult(
                  UnorderedElementsAre(CreateExpected("2001:db8:2::", 80),
                                       CreateExpected("192.168.1.43", 80))))));

  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "foo.test", NetworkAnonymizationKey(), DnsQueryType::A,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  "foo.test", DnsQueryType::A,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ElementsAre(CreateExpected("192.168.1.43", 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "foo.test", NetworkAnonymizationKey(), DnsQueryType::AAAA,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  "foo.test", DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ElementsAre(CreateExpected("2001:db8:2::", 0)))));
}

TEST_F(HostResolverManagerTest, JobsClearedOnCompletion) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(1u, resolver_->num_jobs_for_testing());

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_EQ(0u, resolver_->num_jobs_for_testing());
}

TEST_F(HostResolverManagerTest, JobsClearedOnCompletion_MultipleRequests) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(1u, resolver_->num_jobs_for_testing());

  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_EQ(0u, resolver_->num_jobs_for_testing());
}

TEST_F(HostResolverManagerTest, JobsClearedOnCompletion_Failure) {
  proc_->AddRuleForAllFamilies(std::string(),
                               "0.0.0.1");  // Default to failures.
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(1u, resolver_->num_jobs_for_testing());

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_EQ(0u, resolver_->num_jobs_for_testing());
}

TEST_F(HostResolverManagerTest, JobsClearedOnCompletion_Abort) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(1u, resolver_->num_jobs_for_testing());

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  proc_->SignalMultiple(1u);

  EXPECT_THAT(response.result_error(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_EQ(0u, resolver_->num_jobs_for_testing());
}

TEST_F(HostResolverManagerTest, DnsQueryType) {
  proc_->AddRule("host", ADDRESS_FAMILY_IPV4, "192.168.1.20");
  proc_->AddRule("host", ADDRESS_FAMILY_IPV6, "::5");

  HostResolver::ResolveHostParameters parameters;

  parameters.dns_query_type = DnsQueryType::A;
  ResolveHostResponseHelper v4_response(resolver_->CreateRequest(
      HostPortPair("host", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));

  parameters.dns_query_type = DnsQueryType::AAAA;
  ResolveHostResponseHelper v6_response(resolver_->CreateRequest(
      HostPortPair("host", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));

  proc_->SignalMultiple(2u);

  EXPECT_THAT(v4_response.result_error(), IsOk());
  EXPECT_THAT(v4_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.20", 80)));
  EXPECT_THAT(v4_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.20", 80))))));

  EXPECT_THAT(v6_response.result_error(), IsOk());
  EXPECT_THAT(v6_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::5", 80)));
  EXPECT_THAT(v6_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::5", 80))))));
}

TEST_F(HostResolverManagerTest, DnsQueryWithoutAliases) {
  proc_->AddRule("host", ADDRESS_FAMILY_IPV4, "192.168.1.20");

  HostResolver::ResolveHostParameters parameters;

  parameters.dns_query_type = DnsQueryType::A;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));

  proc_->SignalMultiple(1u);

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.20", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.20", 80))))));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));
}

void HostResolverManagerTest::LocalhostIPV4IPV6LookupTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  HostResolver::ResolveHostParameters parameters;

  parameters.dns_query_type = DnsQueryType::A;
  ResolveHostResponseHelper v4_v4_response(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(v4_v4_response.result_error(), IsOk());
  EXPECT_THAT(v4_v4_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(v4_v4_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));

  parameters.dns_query_type = DnsQueryType::AAAA;
  ResolveHostResponseHelper v4_v6_response(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(v4_v6_response.result_error(), IsOk());
  EXPECT_THAT(v4_v6_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::1", 80)));
  EXPECT_THAT(v4_v6_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::1", 80))))));

  ResolveHostResponseHelper v4_unsp_response(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(v4_unsp_response.result_error(), IsOk());
  EXPECT_THAT(v4_unsp_response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      v4_unsp_response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

TEST_F(HostResolverManagerTest, LocalhostIPV4IPV6LookupAsync) {
  LocalhostIPV4IPV6LookupTest(true);
}

TEST_F(HostResolverManagerTest, LocalhostIPV4IPV6LookupSync) {
  LocalhostIPV4IPV6LookupTest(false);
}

TEST_F(HostResolverManagerTest, ResolveIPLiteralWithHostResolverSystemOnly) {
  const char kIpLiteral[] = "178.78.32.1";
  // Add a mapping to tell if the resolver proc was called (if it was called,
  // then the result will be the remapped value. Otherwise it will be the IP
  // literal).
  proc_->AddRuleForAllFamilies(kIpLiteral, "183.45.32.1");

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::SYSTEM;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kIpLiteral, 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  // IP literal resolution is expected to take precedence over source, so the
  // result is expected to be the input IP, not the result IP from the proc rule
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected(kIpLiteral, 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected(kIpLiteral, 80))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}

TEST_F(HostResolverManagerTest, EmptyListMeansNameNotResolved) {
  proc_->AddRuleForAllFamilies("just.testing", "");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(response.request()->GetStaleInfo());

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);
}

TEST_F(HostResolverManagerTest, FailedAsynchronousLookup) {
  proc_->AddRuleForAllFamilies(std::string(),
                               "0.0.0.1");  // Default to failures.
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.top_level_result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(response.request()->GetStaleInfo());

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);

  // Also test that the error is not cached.
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                                 0 /* host_resolver_flags */,
                                 HostResolverSource::ANY,
                                 NetworkAnonymizationKey()));
  EXPECT_FALSE(cache_result);

  // Expect system resolve failures never cached.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      "just.testing", NetworkAnonymizationKey()));
}

TEST_F(HostResolverManagerTest, AbortedAsynchronousLookup) {
  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response0.complete());
  ASSERT_TRUE(proc_->WaitFor(1u));

  // Resolver is destroyed while job is running on WorkerPool.
  DestroyResolver();

  proc_->SignalAll();

  // To ensure there was no spurious callback, complete with a new resolver.
  CreateResolver();
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  proc_->SignalMultiple(2u);

  EXPECT_THAT(response1.result_error(), IsOk());

  // This request was canceled.
  EXPECT_FALSE(response0.complete());
}

TEST_F(HostResolverManagerTest, NumericIPv4Address) {
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("127.1.2.3", 5555), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.1.2.3", 5555)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::UnorderedElementsAre(CreateExpected("127.1.2.3", 5555))))));
}

TEST_F(HostResolverManagerTest, NumericIPv4AddressWithScheme) {
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, "127.1.2.3", 5555),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.1.2.3", 5555)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::UnorderedElementsAre(CreateExpected("127.1.2.3", 5555))))));
}

void HostResolverManagerTest::NumericIPv6AddressTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  // Resolve a plain IPv6 address.  Don't worry about [brackets], because
  // the caller should have removed them.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("2001:db8::1", 5555), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("2001:db8::1", 5555)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  ExpectEndpointResult(testing::UnorderedElementsAre(
                      CreateExpected("2001:db8::1", 5555))))));
}

TEST_F(HostResolverManagerTest, NumericIPv6AddressAsync) {
  NumericIPv6AddressTest(true);
}

TEST_F(HostResolverManagerTest, NumericIPv6AddressSync) {
  NumericIPv6AddressTest(false);
}

void HostResolverManagerTest::NumericIPv6AddressWithSchemeTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kFtpScheme, "[2001:db8::1]", 5555),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("2001:db8::1", 5555)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  ExpectEndpointResult(testing::UnorderedElementsAre(
                      CreateExpected("2001:db8::1", 5555))))));
}

TEST_F(HostResolverManagerTest, NumericIPv6AddressWithSchemeAsync) {
  NumericIPv6AddressWithSchemeTest(true);
}

TEST_F(HostResolverManagerTest, NumericIPv6AddressWithSchemeSync) {
  NumericIPv6AddressWithSchemeTest(false);
}

// Regression test for https://crbug.com/1432508.
//
// Tests that if a new request is made while the loop within
// FinishIPv6ReachabilityCheck is still running, and the new request needs to
// wait on a new IPv6 probe to complete, the new request does not try to modify
// the same vector that FinishIPv6ReachabilityCheck is iterating over.
TEST_F(HostResolverManagerTest, AddRequestDuringFinishIPv6ReachabilityCheck) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, true);

  // Reset `last_ipv6_probe_time_` if `reset_ipv6_probe_time` true so a new
  // request kicks off a new reachability probe.
  auto custom_callback_template = base::BindLambdaForTesting(
      [&](bool reset_ipv6_probe_time, const HostPortPair& next_host,
          std::unique_ptr<ResolveHostResponseHelper>* next_response,
          CompletionOnceCallback completion_callback, int error) {
        if (reset_ipv6_probe_time) {
          resolver_->ResetIPv6ProbeTimeForTesting();
        }
        *next_response = std::make_unique<ResolveHostResponseHelper>(
            resolver_->CreateRequest(next_host, NetworkAnonymizationKey(),
                                     NetLogWithSource(), std::nullopt,
                                     resolve_context_.get()));
        std::move(completion_callback).Run(error);
      });

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> next_responses(3);

  ResolveHostResponseHelper response0(
      resolver_->CreateRequest(HostPortPair("2001:db8::1", 5555),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      base::BindOnce(custom_callback_template, true, HostPortPair("zzz", 80),
                     &next_responses[0]));

  // New requests made by response1 and response2 will wait for a new
  // reachability probe to complete.
  ResolveHostResponseHelper response1(
      resolver_->CreateRequest(HostPortPair("2001:db8::1", 5555),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      base::BindOnce(custom_callback_template, false, HostPortPair("aaa", 80),
                     &next_responses[1]));

  ResolveHostResponseHelper response2(
      resolver_->CreateRequest(HostPortPair("2001:db8::1", 5555),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      base::BindOnce(custom_callback_template, false, HostPortPair("eee", 80),
                     &next_responses[2]));

  // Unblock all calls to proc.
  proc_->SignalMultiple(6u);

  // All requests should return OK.
  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(next_responses[0]->result_error(), IsOk());
  EXPECT_THAT(next_responses[1]->result_error(), IsOk());
  EXPECT_THAT(next_responses[2]->result_error(), IsOk());
}

TEST_F(HostResolverManagerTest, EmptyHost) {
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(std::string(), 5555), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerTest, EmptyDotsHost) {
  for (int i = 0; i < 16; ++i) {
    ResolveHostResponseHelper response(resolver_->CreateRequest(
        HostPortPair(std::string(i, '.'), 5555), NetworkAnonymizationKey(),
        NetLogWithSource(), std::nullopt, resolve_context_.get()));

    EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
    EXPECT_THAT(response.request()->GetAddressResults(),
                AnyOf(nullptr, Pointee(IsEmpty())));
    EXPECT_THAT(response.request()->GetEndpointResults(),
                AnyOf(nullptr, Pointee(IsEmpty())));
  }
}

TEST_F(HostResolverManagerTest, LongHost) {
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(std::string(4097, 'a'), 5555), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerTest, DeDupeRequests) {
  // Start 5 requests, duplicating hosts "a" and "b". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  for (auto& response : responses) {
    ASSERT_FALSE(response->complete());
  }

  proc_->SignalMultiple(2u);  // One for "a:80", one for "b:80".

  for (auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsOk());
  }
}

TEST_F(HostResolverManagerTest, CancelMultipleRequests) {
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  for (auto& response : responses) {
    ASSERT_FALSE(response->complete());
  }

  // Cancel everything except request for requests[3] ("a", 80).
  responses[0]->CancelRequest();
  responses[1]->CancelRequest();
  responses[2]->CancelRequest();
  responses[4]->CancelRequest();

  proc_->SignalMultiple(2u);  // One for "a", one for "b".

  EXPECT_THAT(responses[3]->result_error(), IsOk());

  EXPECT_FALSE(responses[0]->complete());
  EXPECT_FALSE(responses[1]->complete());
  EXPECT_FALSE(responses[2]->complete());
  EXPECT_FALSE(responses[4]->complete());
}

TEST_F(HostResolverManagerTest, CanceledRequestsReleaseJobSlots) {
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;

  // Fill up the dispatcher and queue.
  for (unsigned i = 0; i < kMaxJobs + 1; ++i) {
    std::string hostname = "a_";
    hostname[1] = 'a' + i;

    responses.emplace_back(
        std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
            HostPortPair(hostname, 80), NetworkAnonymizationKey(),
            NetLogWithSource(), std::nullopt, resolve_context_.get())));
    ASSERT_FALSE(responses.back()->complete());

    responses.emplace_back(
        std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
            HostPortPair(hostname, 80), NetworkAnonymizationKey(),
            NetLogWithSource(), std::nullopt, resolve_context_.get())));
    ASSERT_FALSE(responses.back()->complete());
  }

  ASSERT_TRUE(proc_->WaitFor(kMaxJobs));

  // Cancel all but last two.
  for (unsigned i = 0; i < responses.size() - 2; ++i) {
    responses[i]->CancelRequest();
  }

  ASSERT_TRUE(proc_->WaitFor(kMaxJobs + 1));

  proc_->SignalAll();

  size_t num_requests = responses.size();
  EXPECT_THAT(responses[num_requests - 1]->result_error(), IsOk());
  EXPECT_THAT(responses[num_requests - 2]->result_error(), IsOk());
  for (unsigned i = 0; i < num_requests - 2; ++i) {
    EXPECT_FALSE(responses[i]->complete());
  }
}

TEST_F(HostResolverManagerTest, CancelWithinCallback) {
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  auto custom_callback = base::BindLambdaForTesting(
      [&](CompletionOnceCallback completion_callback, int error) {
        for (auto& response : responses) {
          // Cancelling request is required to complete first, so that it can
          // attempt to cancel the others.  This test assumes all jobs are
          // completed in order.
          DCHECK(!response->complete());

          response->CancelRequest();
        }
        std::move(completion_callback).Run(error);
      });

  ResolveHostResponseHelper cancelling_response(
      resolver_->CreateRequest(HostPortPair("a", 80), NetworkAnonymizationKey(),
                               NetLogWithSource(), std::nullopt,
                               resolve_context_.get()),
      std::move(custom_callback));

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  proc_->SignalMultiple(2u);  // One for "a". One for "finalrequest".

  EXPECT_THAT(cancelling_response.result_error(), IsOk());

  ResolveHostResponseHelper final_response(resolver_->CreateRequest(
      HostPortPair("finalrequest", 70), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(final_response.result_error(), IsOk());

  for (auto& response : responses) {
    EXPECT_FALSE(response->complete());
  }
}

TEST_F(HostResolverManagerTest, DeleteWithinCallback) {
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  auto custom_callback = base::BindLambdaForTesting(
      [&](CompletionOnceCallback completion_callback, int error) {
        for (auto& response : responses) {
          // Deleting request is required to be first, so the other requests
          // will still be running to be deleted. This test assumes that the
          // Jobs will be Aborted in order and the requests in order within the
          // jobs.
          DCHECK(!response->complete());
        }

        DestroyResolver();
        std::move(completion_callback).Run(error);
      });

  ResolveHostResponseHelper deleting_response(
      resolver_->CreateRequest(HostPortPair("a", 80), NetworkAnonymizationKey(),
                               NetLogWithSource(), std::nullopt,
                               resolve_context_.get()),
      std::move(custom_callback));

  // Start additional requests to be cancelled as part of the first's deletion.
  // Assumes all requests for a job are handled in order so that the deleting
  // request will run first and cancel the rest.
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  proc_->SignalMultiple(3u);

  EXPECT_THAT(deleting_response.result_error(), IsOk());

  base::RunLoop().RunUntilIdle();
  for (auto& response : responses) {
    EXPECT_FALSE(response->complete());
  }
}

TEST_F(HostResolverManagerTest, DeleteWithinAbortedCallback) {
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  ResolveHostResponseHelper::Callback custom_callback =
      base::BindLambdaForTesting(
          [&](CompletionOnceCallback completion_callback, int error) {
            for (auto& response : responses) {
              // Deleting request is required to be first, so the other requests
              // will still be running to be deleted. This test assumes that the
              // Jobs will be Aborted in order and the requests in order within
              // the jobs.
              DCHECK(!response->complete());
            }
            DestroyResolver();
            std::move(completion_callback).Run(error);
          });

  ResolveHostResponseHelper deleting_response(
      resolver_->CreateRequest(HostPortPair("a", 80), NetworkAnonymizationKey(),
                               NetLogWithSource(), std::nullopt,
                               resolve_context_.get()),
      std::move(custom_callback));

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 82), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 82), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  // Wait for all calls to queue up, trigger abort via IP address change, then
  // signal all the queued requests to let them all try to finish.
  EXPECT_TRUE(proc_->WaitFor(2u));
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  proc_->SignalAll();

  EXPECT_THAT(deleting_response.result_error(), IsError(ERR_NETWORK_CHANGED));
  base::RunLoop().RunUntilIdle();
  for (auto& response : responses) {
    EXPECT_FALSE(response->complete());
  }
}

TEST_F(HostResolverManagerTest, StartWithinCallback) {
  std::unique_ptr<ResolveHostResponseHelper> new_response;
  auto custom_callback = base::BindLambdaForTesting(
      [&](CompletionOnceCallback completion_callback, int error) {
        new_response = std::make_unique<ResolveHostResponseHelper>(
            resolver_->CreateRequest(
                HostPortPair("new", 70), NetworkAnonymizationKey(),
                NetLogWithSource(), std::nullopt, resolve_context_.get()));
        std::move(completion_callback).Run(error);
      });

  ResolveHostResponseHelper starting_response(
      resolver_->CreateRequest(HostPortPair("a", 80), NetworkAnonymizationKey(),
                               NetLogWithSource(), std::nullopt,
                               resolve_context_.get()),
      std::move(custom_callback));

  proc_->SignalMultiple(2u);  // One for "a". One for "new".

  EXPECT_THAT(starting_response.result_error(), IsOk());
  EXPECT_THAT(new_response->result_error(), IsOk());
}

TEST_F(HostResolverManagerTest, StartWithinEvictionCallback) {
  CreateSerialResolver();
  resolver_->SetMaxQueuedJobsForTesting(2);

  std::unique_ptr<ResolveHostResponseHelper> new_response;
  auto custom_callback = base::BindLambdaForTesting(
      [&](CompletionOnceCallback completion_callback, int error) {
        new_response = std::make_unique<ResolveHostResponseHelper>(
            resolver_->CreateRequest(
                HostPortPair("new", 70), NetworkAnonymizationKey(),
                NetLogWithSource(), std::nullopt, resolve_context_.get()));
        std::move(completion_callback).Run(error);
      });

  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("initial", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper evictee1_response(
      resolver_->CreateRequest(HostPortPair("evictee1", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      std::move(custom_callback));
  ResolveHostResponseHelper evictee2_response(resolver_->CreateRequest(
      HostPortPair("evictee2", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  // Now one running request ("initial") and two queued requests ("evictee1" and
  // "evictee2"). Any further requests will cause evictions.
  ResolveHostResponseHelper evictor_response(resolver_->CreateRequest(
      HostPortPair("evictor", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(evictee1_response.result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));

  // "new" should evict "evictee2"
  EXPECT_THAT(evictee2_response.result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));

  proc_->SignalMultiple(3u);

  EXPECT_THAT(initial_response.result_error(), IsOk());
  EXPECT_THAT(evictor_response.result_error(), IsOk());
  EXPECT_THAT(new_response->result_error(), IsOk());
}

// Test where we start a new request within an eviction callback that itself
// evicts the first evictor.
TEST_F(HostResolverManagerTest, StartWithinEvictionCallback_DoubleEviction) {
  CreateSerialResolver();
  resolver_->SetMaxQueuedJobsForTesting(1);

  std::unique_ptr<ResolveHostResponseHelper> new_response;
  auto custom_callback = base::BindLambdaForTesting(
      [&](CompletionOnceCallback completion_callback, int error) {
        new_response = std::make_unique<ResolveHostResponseHelper>(
            resolver_->CreateRequest(
                HostPortPair("new", 70), NetworkAnonymizationKey(),
                NetLogWithSource(), std::nullopt, resolve_context_.get()));
        std::move(completion_callback).Run(error);
      });

  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("initial", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper evictee_response(
      resolver_->CreateRequest(HostPortPair("evictee", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      std::move(custom_callback));

  // Now one running request ("initial") and one queued requests ("evictee").
  // Any further requests will cause evictions.
  ResolveHostResponseHelper evictor_response(resolver_->CreateRequest(
      HostPortPair("evictor", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(evictee_response.result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));

  // "new" should evict "evictor"
  EXPECT_THAT(evictor_response.result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));

  proc_->SignalMultiple(2u);

  EXPECT_THAT(initial_response.result_error(), IsOk());
  EXPECT_THAT(new_response->result_error(), IsOk());
}

TEST_F(HostResolverManagerTest, StartWithinEvictionCallback_SameRequest) {
  CreateSerialResolver();
  resolver_->SetMaxQueuedJobsForTesting(2);

  std::unique_ptr<ResolveHostResponseHelper> new_response;
  auto custom_callback = base::BindLambdaForTesting(
      [&](CompletionOnceCallback completion_callback, int error) {
        new_response = std::make_unique<ResolveHostResponseHelper>(
            resolver_->CreateRequest(
                HostPortPair("evictor", 80), NetworkAnonymizationKey(),
                NetLogWithSource(), std::nullopt, resolve_context_.get()));
        std::move(completion_callback).Run(error);
      });

  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("initial", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper evictee_response(
      resolver_->CreateRequest(HostPortPair("evictee", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      std::move(custom_callback));
  ResolveHostResponseHelper additional_response(resolver_->CreateRequest(
      HostPortPair("additional", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  // Now one running request ("initial") and two queued requests ("evictee" and
  // "additional"). Any further requests will cause evictions.
  ResolveHostResponseHelper evictor_response(resolver_->CreateRequest(
      HostPortPair("evictor", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(evictee_response.result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));

  // Second "evictor" should be joined with the first and not evict "additional"

  // Only 3 proc requests because both "evictor" requests are combined.
  proc_->SignalMultiple(3u);

  EXPECT_THAT(initial_response.result_error(), IsOk());
  EXPECT_THAT(additional_response.result_error(), IsOk());
  EXPECT_THAT(evictor_response.result_error(), IsOk());
  EXPECT_THAT(new_response->result_error(), IsOk());
}

TEST_F(HostResolverManagerTest, BypassCache) {
  proc_->SignalMultiple(2u);

  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(initial_response.result_error(), IsOk());
  EXPECT_EQ(1u, proc_->GetCaptureList().size());

  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsOk());
  // Expect no increase to calls to |proc_| because result was cached.
  EXPECT_EQ(1u, proc_->GetCaptureList().size());

  HostResolver::ResolveHostParameters parameters;
  parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::DISALLOWED;
  ResolveHostResponseHelper cache_bypassed_response(resolver_->CreateRequest(
      HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(cache_bypassed_response.result_error(), IsOk());
  // Expect call to |proc_| because cache was bypassed.
  EXPECT_EQ(2u, proc_->GetCaptureList().size());
}

void HostResolverManagerTest::FlushCacheOnIPAddressChangeTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  proc_->SignalMultiple(2u);  // One before the flush, one after.

  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("host1", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(initial_response.result_error(), IsOk());
  EXPECT_EQ(1u, proc_->GetCaptureList().size());

  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("host1", 75), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsOk());
  EXPECT_EQ(1u, proc_->GetCaptureList().size());  // No expected increase.

  // Flush cache by triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();  // Notification happens async.

  // Resolve "host1" again -- this time it won't be served from cache, so it
  // will complete asynchronously.
  ResolveHostResponseHelper flushed_response(resolver_->CreateRequest(
      HostPortPair("host1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(flushed_response.result_error(), IsOk());
  EXPECT_EQ(2u, proc_->GetCaptureList().size());  // Expected increase.
}

// Test that IP address changes flush the cache but initial DNS config reads
// do not.
TEST_F(HostResolverManagerTest, FlushCacheOnIPAddressChangeAsync) {
  FlushCacheOnIPAddressChangeTest(true);
}
TEST_F(HostResolverManagerTest, FlushCacheOnIPAddressChangeSync) {
  FlushCacheOnIPAddressChangeTest(false);
}

void HostResolverManagerTest::AbortOnIPAddressChangedTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host1", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  ASSERT_FALSE(response.complete());
  if (is_async) {
    base::RunLoop().RunUntilIdle();
  }
  ASSERT_TRUE(proc_->WaitFor(1u));

  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();  // Notification happens async.
  proc_->SignalAll();

  EXPECT_THAT(response.result_error(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_EQ(0u, resolve_context_->host_cache()->size());
}

// Test that IP address changes send ERR_NETWORK_CHANGED to pending requests.
TEST_F(HostResolverManagerTest, AbortOnIPAddressChangedAsync) {
  AbortOnIPAddressChangedTest(true);
}
TEST_F(HostResolverManagerTest, AbortOnIPAddressChangedSync) {
  AbortOnIPAddressChangedTest(false);
}

// Obey pool constraints after IP address has changed.
TEST_F(HostResolverManagerTest, ObeyPoolConstraintsAfterIPAddressChange) {
  // Runs at most one job at a time.
  CreateSerialResolver();

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("b", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("c", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  for (auto& response : responses) {
    ASSERT_FALSE(response->complete());
  }
  ASSERT_TRUE(proc_->WaitFor(1u));

  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  base::RunLoop().RunUntilIdle();  // Notification happens async.
  proc_->SignalMultiple(3u);  // Let the false-start go so that we can catch it.

  // Requests should complete one at a time, with the first failing.
  EXPECT_THAT(responses[0]->result_error(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  EXPECT_FALSE(responses[1]->complete());
  EXPECT_FALSE(responses[2]->complete());

  EXPECT_THAT(responses[1]->result_error(), IsOk());
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  EXPECT_FALSE(responses[2]->complete());

  EXPECT_THAT(responses[2]->result_error(), IsOk());
}

void HostResolverManagerTest::AbortOnlyExistingRequestsOnIPAddressChangeTest(
    bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  auto custom_callback_template = base::BindLambdaForTesting(
      [&](const HostPortPair& next_host,
          std::unique_ptr<ResolveHostResponseHelper>* next_response,
          CompletionOnceCallback completion_callback, int error) {
        *next_response = std::make_unique<ResolveHostResponseHelper>(
            resolver_->CreateRequest(next_host, NetworkAnonymizationKey(),
                                     NetLogWithSource(), std::nullopt,
                                     resolve_context_.get()));
        std::move(completion_callback).Run(error);
      });

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> next_responses(3);

  ResolveHostResponseHelper response0(
      resolver_->CreateRequest(HostPortPair("bbb", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      base::BindOnce(custom_callback_template, HostPortPair("zzz", 80),
                     &next_responses[0]));

  ResolveHostResponseHelper response1(
      resolver_->CreateRequest(HostPortPair("eee", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      base::BindOnce(custom_callback_template, HostPortPair("aaa", 80),
                     &next_responses[1]));

  ResolveHostResponseHelper response2(
      resolver_->CreateRequest(HostPortPair("ccc", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()),
      base::BindOnce(custom_callback_template, HostPortPair("eee", 80),
                     &next_responses[2]));

  if (is_async) {
    base::RunLoop().RunUntilIdle();
  }
  // Wait until all are blocked;
  ASSERT_TRUE(proc_->WaitFor(3u));

  // Trigger an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // This should abort all running jobs.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(response0.result_error(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_THAT(response1.result_error(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_THAT(response2.result_error(), IsError(ERR_NETWORK_CHANGED));

  EXPECT_FALSE(next_responses[0]->complete());
  EXPECT_FALSE(next_responses[1]->complete());
  EXPECT_FALSE(next_responses[2]->complete());

  // Unblock all calls to proc.
  proc_->SignalMultiple(6u);

  // Run until the re-started requests finish.
  EXPECT_THAT(next_responses[0]->result_error(), IsOk());
  EXPECT_THAT(next_responses[1]->result_error(), IsOk());
  EXPECT_THAT(next_responses[2]->result_error(), IsOk());

  // Verify that results of aborted Jobs were not cached.
  EXPECT_EQ(6u, proc_->GetCaptureList().size());
  EXPECT_EQ(3u, resolve_context_->host_cache()->size());
}
// Tests that a new Request made from the callback of a previously aborted one
// will not be aborted.
TEST_F(HostResolverManagerTest,
       AbortOnlyExistingRequestsOnIPAddressChangeAsync) {
  AbortOnlyExistingRequestsOnIPAddressChangeTest(true);
}
TEST_F(HostResolverManagerTest,
       AbortOnlyExistingRequestsOnIPAddressChangeSync) {
  AbortOnlyExistingRequestsOnIPAddressChangeTest(false);
}

// Tests that when the maximum threads is set to 1, requests are dequeued
// in order of priority.
TEST_F(HostResolverManagerTest, HigherPriorityRequestsStartedFirst) {
  CreateSerialResolver();

  HostResolver::ResolveHostParameters low_priority;
  low_priority.initial_priority = LOW;
  HostResolver::ResolveHostParameters medium_priority;
  medium_priority.initial_priority = MEDIUM;
  HostResolver::ResolveHostParameters highest_priority;
  highest_priority.initial_priority = HIGHEST;

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req0", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req1", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req2", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req3", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req4", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), highest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req5", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req6", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req5", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), highest_priority, resolve_context_.get())));

  for (const auto& response : responses) {
    ASSERT_FALSE(response->complete());
  }

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(responses.size());  // More than needed.

  // Wait for all the requests to complete successfully.
  for (auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsOk());
  }

  // Since we have restricted to a single concurrent thread in the jobpool,
  // the requests should complete in order of priority (with the exception
  // of the first request, which gets started right away, since there is
  // nothing outstanding).
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(7u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req4", capture_list[1].hostname);
  EXPECT_EQ("req5", capture_list[2].hostname);
  EXPECT_EQ("req1", capture_list[3].hostname);
  EXPECT_EQ("req2", capture_list[4].hostname);
  EXPECT_EQ("req3", capture_list[5].hostname);
  EXPECT_EQ("req6", capture_list[6].hostname);
}

void HostResolverManagerTest::ChangePriorityTest(bool is_async) {
  CreateSerialResolver(true /* check_ipv6_on_wifi */, true /* ipv6_reachable */,
                       is_async);

  HostResolver::ResolveHostParameters lowest_priority;
  lowest_priority.initial_priority = LOWEST;
  HostResolver::ResolveHostParameters low_priority;
  low_priority.initial_priority = LOW;
  HostResolver::ResolveHostParameters medium_priority;
  medium_priority.initial_priority = MEDIUM;

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req0", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req1", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req2", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), lowest_priority, resolve_context_.get())));

  // req0 starts immediately; without ChangePriority, req1 and then req2 should
  // run.
  for (const auto& response : responses) {
    ASSERT_FALSE(response->complete());
  }

  // Changing req2 to HIGHEST should make it run before req1.
  // (It can't run before req0, since req0 started immediately.)
  responses[2]->request()->ChangeRequestPriority(HIGHEST);

  // Let all 3 requests finish.
  proc_->SignalMultiple(3u);

  for (auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsOk());
  }

  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(3u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req2", capture_list[1].hostname);
  EXPECT_EQ("req1", capture_list[2].hostname);
}

// Test that changing a job's priority affects the dequeueing order.
TEST_F(HostResolverManagerTest, ChangePriorityAsync) {
  ChangePriorityTest(true);
}

TEST_F(HostResolverManagerTest, ChangePrioritySync) {
  ChangePriorityTest(false);
}

// Try cancelling a job which has not started yet.
TEST_F(HostResolverManagerTest, CancelPendingRequest) {
  CreateSerialResolver();

  HostResolver::ResolveHostParameters lowest_priority;
  lowest_priority.initial_priority = LOWEST;
  HostResolver::ResolveHostParameters low_priority;
  low_priority.initial_priority = LOW;
  HostResolver::ResolveHostParameters medium_priority;
  medium_priority.initial_priority = MEDIUM;
  HostResolver::ResolveHostParameters highest_priority;
  highest_priority.initial_priority = HIGHEST;

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req0", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), lowest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req1", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), highest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req2", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req3", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req4", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), highest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req5", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), lowest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req6", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));

  // Cancel some requests
  responses[1]->CancelRequest();
  responses[4]->CancelRequest();
  responses[5]->CancelRequest();

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(responses.size());  // More than needed.

  // Let everything try to finish.
  base::RunLoop().RunUntilIdle();

  // Wait for all the requests to complete succesfully.
  EXPECT_THAT(responses[0]->result_error(), IsOk());
  EXPECT_THAT(responses[2]->result_error(), IsOk());
  EXPECT_THAT(responses[3]->result_error(), IsOk());
  EXPECT_THAT(responses[6]->result_error(), IsOk());

  // Cancelled requests shouldn't complete.
  EXPECT_FALSE(responses[1]->complete());
  EXPECT_FALSE(responses[4]->complete());
  EXPECT_FALSE(responses[5]->complete());

  // Verify that they called out to the resolver proc (which runs on the
  // resolver thread) in the expected order.
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(4u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req2", capture_list[1].hostname);
  EXPECT_EQ("req6", capture_list[2].hostname);
  EXPECT_EQ("req3", capture_list[3].hostname);
}

// Test that when too many requests are enqueued, old ones start to be aborted.
TEST_F(HostResolverManagerTest, QueueOverflow) {
  CreateSerialResolver();

  // Allow only 3 queued jobs.
  const size_t kMaxPendingJobs = 3u;
  resolver_->SetMaxQueuedJobsForTesting(kMaxPendingJobs);

  HostResolver::ResolveHostParameters lowest_priority;
  lowest_priority.initial_priority = LOWEST;
  HostResolver::ResolveHostParameters low_priority;
  low_priority.initial_priority = LOW;
  HostResolver::ResolveHostParameters medium_priority;
  medium_priority.initial_priority = MEDIUM;
  HostResolver::ResolveHostParameters highest_priority;
  highest_priority.initial_priority = HIGHEST;

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req0", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), lowest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req1", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), highest_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req2", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req3", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));

  // At this point, there are 3 enqueued jobs (and one "running" job).
  // Insertion of subsequent requests will cause evictions.

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req4", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), low_priority, resolve_context_.get())));
  EXPECT_THAT(responses[4]->result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));  // Evicts self.
  EXPECT_THAT(responses[4]->request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(responses[4]->request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req5", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  EXPECT_THAT(responses[2]->result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));
  EXPECT_THAT(responses[2]->request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(responses[2]->request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req6", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), highest_priority, resolve_context_.get())));
  EXPECT_THAT(responses[3]->result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));
  EXPECT_THAT(responses[3]->request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(responses[3]->request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("req7", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), medium_priority, resolve_context_.get())));
  EXPECT_THAT(responses[5]->result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));
  EXPECT_THAT(responses[5]->request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(responses[5]->request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(4u);

  // The rest should succeed.
  EXPECT_THAT(responses[0]->result_error(), IsOk());
  EXPECT_TRUE(responses[0]->request()->GetAddressResults());
  EXPECT_TRUE(responses[0]->request()->GetEndpointResults());
  EXPECT_THAT(responses[1]->result_error(), IsOk());
  EXPECT_TRUE(responses[1]->request()->GetAddressResults());
  EXPECT_TRUE(responses[1]->request()->GetEndpointResults());
  EXPECT_THAT(responses[6]->result_error(), IsOk());
  EXPECT_TRUE(responses[6]->request()->GetAddressResults());
  EXPECT_TRUE(responses[6]->request()->GetEndpointResults());
  EXPECT_THAT(responses[7]->result_error(), IsOk());
  EXPECT_TRUE(responses[7]->request()->GetAddressResults());
  EXPECT_TRUE(responses[7]->request()->GetEndpointResults());

  // Verify that they called out the the resolver proc (which runs on the
  // resolver thread) in the expected order.
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(4u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req1", capture_list[1].hostname);
  EXPECT_EQ("req6", capture_list[2].hostname);
  EXPECT_EQ("req7", capture_list[3].hostname);

  // Verify that the evicted (incomplete) requests were not cached.
  EXPECT_EQ(4u, resolve_context_->host_cache()->size());

  for (size_t i = 0; i < responses.size(); ++i) {
    EXPECT_TRUE(responses[i]->complete()) << i;
  }
}

// Tests that jobs can self-evict by setting the max queue to 0.
TEST_F(HostResolverManagerTest, QueueOverflow_SelfEvict) {
  CreateSerialResolver();
  resolver_->SetMaxQueuedJobsForTesting(0);

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.

  ResolveHostResponseHelper run_response(resolver_->CreateRequest(
      HostPortPair("run", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  ResolveHostResponseHelper evict_response(resolver_->CreateRequest(
      HostPortPair("req1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(evict_response.result_error(),
              IsError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE));
  EXPECT_THAT(evict_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(evict_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  proc_->SignalMultiple(1u);

  EXPECT_THAT(run_response.result_error(), IsOk());
  EXPECT_TRUE(run_response.request()->GetAddressResults());
  EXPECT_TRUE(run_response.request()->GetEndpointResults());
}

// Make sure that the dns query type parameter is respected when raw IPs are
// passed in.
TEST_F(HostResolverManagerTest, AddressFamilyWithRawIPs) {
  HostResolver::ResolveHostParameters v4_parameters;
  v4_parameters.dns_query_type = DnsQueryType::A;

  HostResolver::ResolveHostParameters v6_parameters;
  v6_parameters.dns_query_type = DnsQueryType::AAAA;

  ResolveHostResponseHelper v4_v4_request(resolver_->CreateRequest(
      HostPortPair("127.0.0.1", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), v4_parameters, resolve_context_.get()));
  EXPECT_THAT(v4_v4_request.result_error(), IsOk());
  EXPECT_THAT(v4_v4_request.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(
      v4_v4_request.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));

  ResolveHostResponseHelper v4_v6_request(resolver_->CreateRequest(
      HostPortPair("127.0.0.1", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), v6_parameters, resolve_context_.get()));
  EXPECT_THAT(v4_v6_request.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  ResolveHostResponseHelper v4_unsp_request(resolver_->CreateRequest(
      HostPortPair("127.0.0.1", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(v4_unsp_request.result_error(), IsOk());
  EXPECT_THAT(v4_unsp_request.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(
      v4_unsp_request.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));

  ResolveHostResponseHelper v6_v4_request(resolver_->CreateRequest(
      HostPortPair("::1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      v4_parameters, resolve_context_.get()));
  EXPECT_THAT(v6_v4_request.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  ResolveHostResponseHelper v6_v6_request(resolver_->CreateRequest(
      HostPortPair("::1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      v6_parameters, resolve_context_.get()));
  EXPECT_THAT(v6_v6_request.result_error(), IsOk());
  EXPECT_THAT(v6_v6_request.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::1", 80)));
  EXPECT_THAT(
      v6_v6_request.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("::1", 80))))));

  ResolveHostResponseHelper v6_unsp_request(resolver_->CreateRequest(
      HostPortPair("::1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(v6_unsp_request.result_error(), IsOk());
  EXPECT_THAT(v6_unsp_request.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::1", 80)));
  EXPECT_THAT(
      v6_unsp_request.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("::1", 80))))));
}

TEST_F(HostResolverManagerTest, LocalOnly_FromCache) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);  // Need only one.

  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  // First NONE query expected to complete synchronously with a cache miss.
  ResolveHostResponseHelper cache_miss_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));
  EXPECT_TRUE(cache_miss_request.complete());
  EXPECT_THAT(cache_miss_request.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(cache_miss_request.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cache_miss_request.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(cache_miss_request.request()->GetStaleInfo());

  // Normal query to populate the cache.
  ResolveHostResponseHelper normal_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(normal_request.result_error(), IsOk());
  EXPECT_FALSE(normal_request.request()->GetStaleInfo());

  // Second NONE query expected to complete synchronously with cache hit.
  ResolveHostResponseHelper cache_hit_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));
  EXPECT_TRUE(cache_hit_request.complete());
  EXPECT_THAT(cache_hit_request.result_error(), IsOk());
  EXPECT_THAT(cache_hit_request.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(
      cache_hit_request.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_FALSE(cache_hit_request.request()->GetStaleInfo().value().is_stale());
}

TEST_F(HostResolverManagerTest, LocalOnly_StaleEntry) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);  // Need only one.

  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  // First NONE query expected to complete synchronously with a cache miss.
  ResolveHostResponseHelper cache_miss_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));
  EXPECT_TRUE(cache_miss_request.complete());
  EXPECT_THAT(cache_miss_request.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(cache_miss_request.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cache_miss_request.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(cache_miss_request.request()->GetStaleInfo());

  // Normal query to populate the cache.
  ResolveHostResponseHelper normal_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(normal_request.result_error(), IsOk());
  EXPECT_FALSE(normal_request.request()->GetStaleInfo());

  MakeCacheStale();

  // Second NONE query still expected to complete synchronously with cache miss.
  ResolveHostResponseHelper stale_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));
  EXPECT_TRUE(stale_request.complete());
  EXPECT_THAT(stale_request.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(stale_request.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(stale_request.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(stale_request.request()->GetStaleInfo());
}

void HostResolverManagerTest::LocalOnlyFromIpTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("1.2.3.4", 56), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));

  // If IPv6 reachability is asynchronous, the first request will return
  // NAME_NOT_RESOLVED. Do a second request to confirm that it returns OK once
  // reachability check completes.
  if (is_async) {
    // Expected to resolve synchronously.
    EXPECT_TRUE(response.complete());
    EXPECT_EQ(response.result_error(), ERR_NAME_NOT_RESOLVED);
    EXPECT_THAT(response.request()->GetAddressResults(),
                AnyOf(nullptr, Pointee(IsEmpty())));
    EXPECT_THAT(response.request()->GetEndpointResults(),
                AnyOf(nullptr, Pointee(IsEmpty())));
    EXPECT_FALSE(response.request()->GetStaleInfo());
    base::RunLoop().RunUntilIdle();

    ResolveHostResponseHelper response2(resolver_->CreateRequest(
        HostPortPair("1.2.3.4", 56), NetworkAnonymizationKey(),
        NetLogWithSource(), source_none_parameters, resolve_context_.get()));
    EXPECT_TRUE(response2.complete());
    EXPECT_THAT(response2.result_error(), IsOk());
    EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected("1.2.3.4", 56)));
    EXPECT_THAT(response2.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected("1.2.3.4", 56))))));
    EXPECT_FALSE(response2.request()->GetStaleInfo());
  } else {
    // Expected to resolve synchronously.
    EXPECT_TRUE(response.complete());
    EXPECT_THAT(response.result_error(), IsOk());
    EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected("1.2.3.4", 56)));
    EXPECT_THAT(response.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected("1.2.3.4", 56))))));
    EXPECT_FALSE(response.request()->GetStaleInfo());
  }
}

TEST_F(HostResolverManagerTest, LocalOnly_FromIpAsync) {
  LocalOnlyFromIpTest(true);
}

TEST_F(HostResolverManagerTest, LocalOnly_FromIpSync) {
  LocalOnlyFromIpTest(false);
}

TEST_F(HostResolverManagerTest, LocalOnly_InvalidName) {
  proc_->AddRuleForAllFamilies("foo,bar.com", "192.168.1.42");

  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("foo,bar.com", 57), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));

  // Expected to fail synchronously.
  EXPECT_TRUE(response.complete());
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}

TEST_F(HostResolverManagerTest, LocalOnly_InvalidLocalhost) {
  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("foo,bar.localhost", 58), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));

  // Expected to fail synchronously.
  EXPECT_TRUE(response.complete());
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}

TEST_F(HostResolverManagerTest, StaleAllowed) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);  // Need only one.

  HostResolver::ResolveHostParameters stale_allowed_parameters;
  stale_allowed_parameters.source = HostResolverSource::LOCAL_ONLY;
  stale_allowed_parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  // First query expected to complete synchronously as a cache miss.
  ResolveHostResponseHelper cache_miss_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), stale_allowed_parameters, resolve_context_.get()));
  EXPECT_TRUE(cache_miss_request.complete());
  EXPECT_THAT(cache_miss_request.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(cache_miss_request.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cache_miss_request.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(cache_miss_request.request()->GetStaleInfo());

  // Normal query to populate cache
  ResolveHostResponseHelper normal_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(normal_request.result_error(), IsOk());
  EXPECT_FALSE(normal_request.request()->GetStaleInfo());

  MakeCacheStale();

  // Second NONE query expected to get a stale cache hit.
  ResolveHostResponseHelper stale_request(resolver_->CreateRequest(
      HostPortPair("just.testing", 84), NetworkAnonymizationKey(),
      NetLogWithSource(), stale_allowed_parameters, resolve_context_.get()));
  EXPECT_TRUE(stale_request.complete());
  EXPECT_THAT(stale_request.result_error(), IsOk());
  EXPECT_THAT(stale_request.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 84)));
  EXPECT_THAT(
      stale_request.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 84))))));
  EXPECT_TRUE(stale_request.request()->GetStaleInfo().value().is_stale());
}

TEST_F(HostResolverManagerTest, StaleAllowed_NonLocal) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.2.42");
  proc_->SignalMultiple(1u);  // Need only one.

  HostResolver::ResolveHostParameters stale_allowed_parameters;
  stale_allowed_parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  // Normal non-local resolves should still work normally with the STALE_ALLOWED
  // parameter, and there should be no stale info.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 85), NetworkAnonymizationKey(),
      NetLogWithSource(), stale_allowed_parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.2.42", 85)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.2.42", 85))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}

void HostResolverManagerTest::StaleAllowedFromIpTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  HostResolver::ResolveHostParameters stale_allowed_parameters;
  stale_allowed_parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("1.2.3.4", 57), NetworkAnonymizationKey(),
      NetLogWithSource(), stale_allowed_parameters, resolve_context_.get()));

  if (!is_async) {
    // Expected to resolve synchronously without stale info.
    EXPECT_TRUE(response.complete());
  }
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("1.2.3.4", 57)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("1.2.3.4", 57))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}

TEST_F(HostResolverManagerTest, StaleAllowed_FromIpAsync) {
  StaleAllowedFromIpTest(true);
}

TEST_F(HostResolverManagerTest, StaleAllowed_FromIpSync) {
  StaleAllowedFromIpTest(false);
}

// TODO(mgersh): add a test case for errors with positive TTL after
// https://crbug.com/115051 is fixed.

// Test the retry attempts simulating host resolver proc that takes too long.
TEST_F(HostResolverManagerTest, MultipleAttempts) {
  // Total number of attempts would be 3 and we want the 3rd attempt to resolve
  // the host. First and second attempt will be forced to wait until they get
  // word that a resolution has completed. The 3rd resolution attempt will try
  // to get done ASAP, and won't wait.
  int kAttemptNumberToResolve = 3;
  int kTotalAttempts = 3;

  // Add a little bit of extra fudge to the delay to allow reasonable
  // flexibility for time > vs >= etc.  We don't need to fail the test if we
  // retry at t=6001 instead of t=6000.
  base::TimeDelta kSleepFudgeFactor = base::Milliseconds(1);

  auto resolver_proc = base::MakeRefCounted<LookupAttemptHostResolverProc>(
      nullptr, kAttemptNumberToResolve, kTotalAttempts);

  HostResolverSystemTask::Params params = DefaultParams(resolver_proc);
  base::TimeDelta unresponsive_delay = params.unresponsive_delay;
  int retry_factor = params.retry_factor;

  CreateResolverWithLimitsAndParams(kMaxJobs, params, true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);

  // Override the current thread task runner, so we can simulate the passage of
  // time and avoid any actual sleeps.
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      task_runner_current_default_handle_override(test_task_runner);

  // Resolve "host1".
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host1", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(response.complete());

  resolver_proc->WaitForNAttemptsToBeBlocked(1);
  EXPECT_FALSE(response.complete());

  test_task_runner->FastForwardBy(unresponsive_delay + kSleepFudgeFactor);
  resolver_proc->WaitForNAttemptsToBeBlocked(2);
  EXPECT_FALSE(response.complete());

  test_task_runner->FastForwardBy(unresponsive_delay * retry_factor +
                                  kSleepFudgeFactor);

  resolver_proc->WaitForAllAttemptsToFinish();
  test_task_runner->RunUntilIdle();

  // Resolve returns -4 to indicate that 3rd attempt has resolved the host.
  // Since we're using a TestMockTimeTaskRunner, the RunLoop stuff in
  // result_error() will fail if it actually has to wait, but unless there's an
  // error, the result should be immediately ready by this point.
  EXPECT_EQ(-4, response.result_error());

  // We should be done with retries, but make sure none erroneously happen.
  test_task_runner->FastForwardUntilNoTasksRemain();

  EXPECT_EQ(resolver_proc->GetTotalAttemptsResolved(), kTotalAttempts);
}

// Regression test for https://crbug.com/976948.
//
// Tests that when the maximum number of retries is set to
// |HostResolver::ManagerOptions::kDefaultRetryAttempts| the
// number of retries used is 4 rather than something higher.
TEST_F(HostResolverManagerTest, DefaultMaxRetryAttempts) {
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      task_runner_current_default_handle_override(test_task_runner);

  // Instantiate a ResolverProc that will block all incoming requests.
  auto resolver_proc = base::MakeRefCounted<LookupAttemptHostResolverProc>(
      nullptr, std::numeric_limits<size_t>::max(),
      std::numeric_limits<size_t>::max());

  // This corresponds to kDefaultMaxRetryAttempts in
  // HostResolverSystemTask::Params::HostResolverSystemTask::Params(). The
  // correspondence is verified below, since that symbol is not exported.
  const size_t expected_max_retries = 4;

  // Use the special value |ManagerOptions::kDefaultRetryAttempts|, which is
  // expected to translate into |expected_num_retries|.
  ASSERT_NE(HostResolverSystemTask::Params::kDefaultRetryAttempts,
            expected_max_retries);
  HostResolverSystemTask::Params params(
      resolver_proc, HostResolverSystemTask::Params::kDefaultRetryAttempts);
  ASSERT_EQ(params.max_retry_attempts, expected_max_retries);

  CreateResolverWithLimitsAndParams(kMaxJobs, params,
                                    false /* ipv6_reachable */,
                                    false /* check_ipv6_on_wifi */);
  // Resolve "host1". The resolver proc will hang all requests so this
  // resolution should remain stalled until calling SetResolvedAttemptNumber().
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host1", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(response.complete());

  // Simulate running the main thread (network task runner) for a long
  // time. Because none of the attempts posted to worker pool can complete, this
  // should cause all of the retry attempts to get posted, according to the
  // exponential backoff schedule.
  test_task_runner->FastForwardBy(base::Minutes(20));

  // Unblock the resolver proc, then wait for all the worker pool and main
  // thread tasks to complete. Note that the call to SetResolvedAttemptNumber(1)
  // will cause all the blocked resolver procs tasks fail with -2.
  resolver_proc->SetResolvedAttemptNumber(1);
  const int kExpectedError = -2;
  base::ThreadPoolInstance::Get()->FlushForTesting();
  test_task_runner->RunUntilIdle();

  ASSERT_TRUE(response.complete());
  EXPECT_EQ(kExpectedError, response.result_error());

  // Ensure that the original attempt was executed on the worker pool, as well
  // as the maximum number of allowed retries, and no more.
  EXPECT_EQ(static_cast<int>(expected_max_retries + 1),
            resolver_proc->GetTotalAttemptsResolved());
}

// If a host resolves to a list that includes 127.0.53.53, this is treated as
// an error. 127.0.53.53 is a localhost address, however it has been given a
// special significance by ICANN to help surface name collision resulting from
// the new gTLDs.
TEST_F(HostResolverManagerTest, NameCollisionIcann) {
  proc_->AddRuleForAllFamilies("single", "127.0.53.53");
  proc_->AddRuleForAllFamilies("multiple", "127.0.0.1,127.0.53.53");
  proc_->AddRuleForAllFamilies("ipv6", "::127.0.53.53");
  proc_->AddRuleForAllFamilies("not_reserved1", "53.53.0.127");
  proc_->AddRuleForAllFamilies("not_reserved2", "127.0.53.54");
  proc_->AddRuleForAllFamilies("not_reserved3", "10.0.53.53");
  proc_->SignalMultiple(6u);

  ResolveHostResponseHelper single_response(resolver_->CreateRequest(
      HostPortPair("single", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(single_response.result_error(),
              IsError(ERR_ICANN_NAME_COLLISION));
  EXPECT_THAT(single_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(single_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // ERR_ICANN_NAME_COLLISION is cached like any other error, using a fixed TTL
  // for failed entries from proc-based resolver. That said, the fixed TTL is 0,
  // so it should never be cached.
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(HostCache::Key(
          "single", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
          HostResolverSource::ANY, NetworkAnonymizationKey()));
  EXPECT_FALSE(cache_result);

  ResolveHostResponseHelper multiple_response(resolver_->CreateRequest(
      HostPortPair("multiple", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(multiple_response.result_error(),
              IsError(ERR_ICANN_NAME_COLLISION));

  // Resolving an IP literal of 127.0.53.53 however is allowed.
  ResolveHostResponseHelper literal_response(resolver_->CreateRequest(
      HostPortPair("127.0.53.53", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(literal_response.result_error(), IsOk());

  // Moreover the address should not be recognized when embedded in an IPv6
  // address.
  ResolveHostResponseHelper ipv6_response(resolver_->CreateRequest(
      HostPortPair("127.0.53.53", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(ipv6_response.result_error(), IsOk());

  // Try some other IPs which are similar, but NOT an exact match on
  // 127.0.53.53.
  ResolveHostResponseHelper similar_response1(resolver_->CreateRequest(
      HostPortPair("not_reserved1", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(similar_response1.result_error(), IsOk());

  ResolveHostResponseHelper similar_response2(resolver_->CreateRequest(
      HostPortPair("not_reserved2", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(similar_response2.result_error(), IsOk());

  ResolveHostResponseHelper similar_response3(resolver_->CreateRequest(
      HostPortPair("not_reserved3", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(similar_response3.result_error(), IsOk());
}

TEST_F(HostResolverManagerTest, StartIPv6ReachabilityCheck) {
  // The real HostResolverManager is needed since TestHostResolverManager will
  // bypass the IPv6 reachability tests.
  DestroyResolver();
  resolver_ = std::make_unique<HostResolverManager>(
      DefaultOptions(), nullptr /* system_dns_config_notifier */,
      nullptr /* net_log */);
  // Verify that two consecutive calls return the same value.
  RecordingNetLogObserver net_log_observer;
  NetLogWithSource net_log =
      NetLogWithSource::Make(net::NetLog::Get(), NetLogSourceType::NONE);
  MockClientSocketFactory socket_factory;
  SequencedSocketData sync_connect(MockConnect(SYNCHRONOUS, OK),
                                   base::span<net::MockRead>(),
                                   base::span<net::MockWrite>());
  SequencedSocketData async_connect(MockConnect(ASYNC, OK),
                                    base::span<net::MockRead>(),
                                    base::span<net::MockWrite>());
  socket_factory.AddSocketDataProvider(&sync_connect);
  socket_factory.AddSocketDataProvider(&async_connect);

  int attempt1 = StartIPv6ReachabilityCheck(net_log, &socket_factory,
                                            base::DoNothingAs<void(int)>());
  EXPECT_EQ(attempt1, OK);
  int result1 = GetLastIpv6ProbeResult();

  int attempt2 = StartIPv6ReachabilityCheck(net_log, &socket_factory,
                                            base::DoNothingAs<void(int)>());
  EXPECT_EQ(attempt2, OK);
  int result2 = GetLastIpv6ProbeResult();
  EXPECT_EQ(result1, result2);

  // Verify that async socket connections also return the same value.
  resolver_->ResetIPv6ProbeTimeForTesting();
  TestCompletionCallback callback;
  int attempt3 =
      StartIPv6ReachabilityCheck(net_log, &socket_factory, callback.callback());
  EXPECT_EQ(attempt3, ERR_IO_PENDING);
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  int result3 = GetLastIpv6ProbeResult();
  EXPECT_EQ(result1, result3);

  // Filter reachability check events and verify that there are three of them.
  auto probe_event_list = net_log_observer.GetEntriesWithType(
      NetLogEventType::HOST_RESOLVER_MANAGER_IPV6_REACHABILITY_CHECK);
  ASSERT_EQ(3U, probe_event_list.size());
  // Verify that the first and third requests were not cached and the second one
  // was.
  EXPECT_FALSE(GetBooleanValueFromParams(probe_event_list[0], "cached"));
  EXPECT_TRUE(GetBooleanValueFromParams(probe_event_list[1], "cached"));
  EXPECT_FALSE(GetBooleanValueFromParams(probe_event_list[0], "cached"));
}

TEST_F(HostResolverManagerTest, IncludeCanonicalName) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42",
                               HOST_RESOLVER_CANONNAME, "canon.name");
  proc_->SignalMultiple(2u);

  HostResolver::ResolveHostParameters parameters;
  parameters.include_canonical_name = true;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response_no_flag(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::UnorderedElementsAre("canon.name")));

  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "just.testing", NetworkAnonymizationKey(), DnsQueryType::A,
          HostResolverSource::SYSTEM, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          "just.testing", DnsQueryType::A,
          HostResolverInternalResult::Source::kUnknown, _, _, "canon.name")));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "just.testing", NetworkAnonymizationKey(), DnsQueryType::AAAA,
          HostResolverSource::SYSTEM, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          "just.testing", DnsQueryType::AAAA,
          HostResolverInternalResult::Source::kUnknown, _, _, "canon.name")));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "canon.name", NetworkAnonymizationKey(), DnsQueryType::A,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  "canon.name", DnsQueryType::A,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ElementsAre(CreateExpected("192.168.1.42", 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "canon.name", NetworkAnonymizationKey(), DnsQueryType::AAAA,
                  HostResolverSource::SYSTEM, /*secure=*/false),
              Pointee(ExpectHostResolverInternalErrorResult(
                  "canon.name", DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kUnknown, _, _,
                  ERR_NAME_NOT_RESOLVED)));

  EXPECT_THAT(response_no_flag.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverManagerTest, FixupCanonicalName) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42", /*flags=*/0,
                               "CANON.name");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::UnorderedElementsAre("canon.name")));
}

TEST_F(HostResolverManagerTest, IncludeCanonicalNameButNotReceived) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42",
                               HOST_RESOLVER_CANONNAME);
  proc_->SignalMultiple(2u);

  HostResolver::ResolveHostParameters parameters;
  parameters.include_canonical_name = true;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response_no_flag(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));

  EXPECT_THAT(response_no_flag.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

// If `ResolveHostParameters::include_canonical_name` is set, canonical name
// should be returned exactly as received from the system resolver, without any
// attempt to do URL hostname canonicalization on it.
TEST_F(HostResolverManagerTest, IncludeCanonicalNameSkipsUrlCanonicalization) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42",
                               HOST_RESOLVER_CANONNAME, "CANON.name");
  proc_->SignalMultiple(2u);

  HostResolver::ResolveHostParameters parameters;
  parameters.include_canonical_name = true;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response_no_flag(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::UnorderedElementsAre("CANON.name")));

  EXPECT_THAT(response_no_flag.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverManagerTest, LoopbackOnly) {
  proc_->AddRuleForAllFamilies("otherlocal", "127.0.0.1",
                               HOST_RESOLVER_LOOPBACK_ONLY);
  proc_->SignalMultiple(2u);

  HostResolver::ResolveHostParameters parameters;
  parameters.loopback_only = true;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("otherlocal", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response_no_flag(resolver_->CreateRequest(
      HostPortPair("otherlocal", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));

  EXPECT_THAT(response_no_flag.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverManagerTest, IsSpeculative) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);

  HostResolver::ResolveHostParameters parameters;
  parameters.is_speculative = true;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  ASSERT_EQ(1u, proc_->GetCaptureList().size());
  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);

  // Reresolve without the |is_speculative| flag should immediately return from
  // cache.
  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(
      response2.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);
  EXPECT_EQ(1u, proc_->GetCaptureList().size());  // No increase.
}

TEST_F(HostResolverManagerTest, AvoidMulticastResolutionParameter) {
  proc_->AddRuleForAllFamilies("avoid.multicast.test", "123.123.123.123",
                               HOST_RESOLVER_AVOID_MULTICAST);
  proc_->SignalMultiple(2u);

  HostResolver::ResolveHostParameters parameters;
  parameters.avoid_multicast_resolution = true;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("avoid.multicast.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response_no_flag(resolver_->CreateRequest(
      HostPortPair("avoid.multicast.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("123.123.123.123", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("123.123.123.123", 80))))));

  EXPECT_THAT(response_no_flag.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

#if BUILDFLAG(ENABLE_MDNS)
const uint8_t kMdnsResponseA[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x01,              // TYPE is A.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x10,  // TTL is 16 (seconds)
    0x00, 0x04,              // RDLENGTH is 4 bytes.
    0x01, 0x02, 0x03, 0x04,  // 1.2.3.4
};

const uint8_t kMdnsResponseA2[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x01,              // TYPE is A.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x10,  // TTL is 16 (seconds)
    0x00, 0x04,              // RDLENGTH is 4 bytes.
    0x05, 0x06, 0x07, 0x08,  // 5.6.7.8
};

const uint8_t kMdnsResponseA2Goodbye[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x01,              // TYPE is A.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x00,  // TTL is 0 (signaling "goodbye" removal of result)
    0x00, 0x04,              // RDLENGTH is 4 bytes.
    0x05, 0x06, 0x07, 0x08,  // 5.6.7.8
};

const uint8_t kMdnsResponseAAAA[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x1C,              // TYPE is AAAA.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x10,  // TTL is 16 (seconds)
    0x00, 0x10,              // RDLENGTH is 16 bytes.

    // 000a:0000:0000:0000:0001:0002:0003:0004
    0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x04};

// An MDNS response indicating that the responder owns the hostname, but the
// specific requested type (AAAA) does not exist because the responder only has
// A addresses.
const uint8_t kMdnsResponseNsec[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x2f,              // TYPE is NSEC.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x10,  // TTL is 16 (seconds)
    0x00, 0x06,              // RDLENGTH is 6 bytes.
    0xc0, 0x0c,  // Next Domain Name (always pointer back to name in MDNS)
    0x00,        // Bitmap block number (always 0 in MDNS)
    0x02,        // Bitmap length is 2
    0x00, 0x08   // A type only
};

const uint8_t kMdnsResponseTxt[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x10,              // TYPE is TXT.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x11,  // TTL is 17 (seconds)
    0x00, 0x08,              // RDLENGTH is 8 bytes.

    // "foo"
    0x03, 0x66, 0x6f, 0x6f,
    // "bar"
    0x03, 0x62, 0x61, 0x72};

const uint8_t kMdnsResponsePtr[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x0c,              // TYPE is PTR.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x12,  // TTL is 18 (seconds)
    0x00, 0x09,              // RDLENGTH is 9 bytes.

    // "foo.com."
    0x03, 'f', 'o', 'o', 0x03, 'c', 'o', 'm', 0x00};

const uint8_t kMdnsResponsePtrRoot[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x0c,              // TYPE is PTR.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x13,  // TTL is 19 (seconds)
    0x00, 0x01,              // RDLENGTH is 1 byte.

    // "." (the root domain)
    0x00};

const uint8_t kMdnsResponseSrv[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x21,              // TYPE is SRV.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x13,  // TTL is 19 (seconds)
    0x00, 0x0f,              // RDLENGTH is 15 bytes.

    0x00, 0x05,  // Priority 5
    0x00, 0x01,  // Weight 1
    0x20, 0x49,  // Port 8265

    // "foo.com."
    0x03, 'f', 'o', 'o', 0x03, 'c', 'o', 'm', 0x00};

const uint8_t kMdnsResponseSrvUnrestricted[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "foo bar(A1B2)._ipps._tcp.local"
    0x0d, 'f', 'o', 'o', ' ', 'b', 'a', 'r', '(', 'A', '1', 'B', '2', ')', 0x05,
    '_', 'i', 'p', 'p', 's', 0x04, '_', 't', 'c', 'p', 0x05, 'l', 'o', 'c', 'a',
    'l', 0x00,

    0x00, 0x21,              // TYPE is SRV.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x13,  // TTL is 19 (seconds)
    0x00, 0x0f,              // RDLENGTH is 15 bytes.

    0x00, 0x05,  // Priority 5
    0x00, 0x01,  // Weight 1
    0x20, 0x49,  // Port 8265

    // "foo.com."
    0x03, 'f', 'o', 'o', 0x03, 'c', 'o', 'm', 0x00};

const uint8_t kMdnsResponseSrvUnrestrictedResult[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // "myhello.local."
    0x07, 'm', 'y', 'h', 'e', 'l', 'l', 'o', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x21,              // TYPE is SRV.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00, 0x00, 0x13,  // TTL is 19 (seconds)
    0x00, 0x15,              // RDLENGTH is 21 bytes.

    0x00, 0x05,  // Priority 5
    0x00, 0x01,  // Weight 1
    0x20, 0x49,  // Port 8265

    // "foo bar.local"
    0x07, 'f', 'o', 'o', ' ', 'b', 'a', 'r', 0x05, 'l', 'o', 'c', 'a', 'l',
    0x00};

TEST_F(HostResolverManagerTest, Mdns) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(4);

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseA, sizeof(kMdnsResponseA));
  socket_factory_ptr->SimulateReceive(kMdnsResponseAAAA,
                                      sizeof(kMdnsResponseAAAA));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(
      response.request()->GetAddressResults()->endpoints(),
      testing::UnorderedElementsAre(
          CreateExpected("1.2.3.4", 80),
          CreateExpected("000a:0000:0000:0000:0001:0002:0003:0004", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("000a:0000:0000:0000:0001:0002:0003:0004", 80),
              CreateExpected("1.2.3.4", 80))))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerTest, Mdns_AaaaOnly) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::AAAA;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseAAAA,
                                      sizeof(kMdnsResponseAAAA));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected(
                  "000a:0000:0000:0000:0001:0002:0003:0004", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  ExpectEndpointResult(testing::ElementsAre(CreateExpected(
                      "000a:0000:0000:0000:0001:0002:0003:0004", 80))))));
}

TEST_F(HostResolverManagerTest, Mdns_Txt) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseTxt,
                                      sizeof(kMdnsResponseTxt));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              testing::Pointee(testing::ElementsAre("foo", "bar")));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerTest, Mdns_Ptr) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 83), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponsePtr,
                                      sizeof(kMdnsResponsePtr));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(
      response.request()->GetHostnameResults(),
      testing::Pointee(testing::ElementsAre(HostPortPair("foo.com", 83))));
}

TEST_F(HostResolverManagerTest, Mdns_Srv) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 83), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseSrv,
                                      sizeof(kMdnsResponseSrv));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(
      response.request()->GetHostnameResults(),
      testing::Pointee(testing::ElementsAre(HostPortPair("foo.com", 8265))));
}

// Test that we are able to create multicast DNS requests that contain
// characters not permitted in the DNS spec such as spaces and parenthesis.
TEST_F(HostResolverManagerTest, Mdns_Srv_Unrestricted) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("foo bar(A1B2)._ipps._tcp.local", 83),
      NetworkAnonymizationKey(), NetLogWithSource(), parameters,
      resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseSrvUnrestricted,
                                      sizeof(kMdnsResponseSrvUnrestricted));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(
      response.request()->GetHostnameResults(),
      testing::Pointee(testing::ElementsAre(HostPortPair("foo.com", 8265))));
}

// Test that we are able to create multicast DNS requests that contain
// characters not permitted in the DNS spec such as spaces and parenthesis.
TEST_F(HostResolverManagerTest, Mdns_Srv_Result_Unrestricted) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 83), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(
      kMdnsResponseSrvUnrestrictedResult,
      sizeof(kMdnsResponseSrvUnrestrictedResult));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              testing::Pointee(
                  testing::ElementsAre(HostPortPair("foo bar.local", 8265))));
}

// Test multicast DNS handling of NSEC responses (used for explicit negative
// response).
TEST_F(HostResolverManagerTest, Mdns_Nsec) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::AAAA;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseNsec,
                                      sizeof(kMdnsResponseNsec));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerTest, Mdns_NoResponse) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(4);

  // Add a little bit of extra fudge to the delay to allow reasonable
  // flexibility for time > vs >= etc.  We don't need to fail the test if we
  // timeout at t=6001 instead of t=6000.
  base::TimeDelta kSleepFudgeFactor = base::Milliseconds(1);

  // Override the current thread task runner, so we can simulate the passage of
  // time to trigger the timeout.
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      task_runner_current_default_handle_override(test_task_runner);

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  ASSERT_TRUE(test_task_runner->HasPendingTask());
  test_task_runner->FastForwardBy(MDnsTransaction::kTransactionTimeout +
                                  kSleepFudgeFactor);

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  test_task_runner->FastForwardUntilNoTasksRemain();
}

TEST_F(HostResolverManagerTest, Mdns_WrongType) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  // Add a little bit of extra fudge to the delay to allow reasonable
  // flexibility for time > vs >= etc.  We don't need to fail the test if we
  // timeout at t=6001 instead of t=6000.
  base::TimeDelta kSleepFudgeFactor = base::Milliseconds(1);

  // Override the current thread task runner, so we can simulate the passage of
  // time to trigger the timeout.
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      task_runner_current_default_handle_override(test_task_runner);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::A;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  // Not the requested type. Should be ignored.
  socket_factory_ptr->SimulateReceive(kMdnsResponseTxt,
                                      sizeof(kMdnsResponseTxt));

  ASSERT_TRUE(test_task_runner->HasPendingTask());
  test_task_runner->FastForwardBy(MDnsTransaction::kTransactionTimeout +
                                  kSleepFudgeFactor);

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  test_task_runner->FastForwardUntilNoTasksRemain();
}

// Test for a request for both A and AAAA results where results only exist for
// one type.
TEST_F(HostResolverManagerTest, Mdns_PartialResults) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(4);

  // Add a little bit of extra fudge to the delay to allow reasonable
  // flexibility for time > vs >= etc.  We don't need to fail the test if we
  // timeout at t=6001 instead of t=6000.
  base::TimeDelta kSleepFudgeFactor = base::Milliseconds(1);

  // Override the current thread task runner, so we can simulate the passage of
  // time to trigger the timeout.
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      task_runner_current_default_handle_override(test_task_runner);

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  ASSERT_TRUE(test_task_runner->HasPendingTask());

  socket_factory_ptr->SimulateReceive(kMdnsResponseA, sizeof(kMdnsResponseA));
  test_task_runner->FastForwardBy(MDnsTransaction::kTransactionTimeout +
                                  kSleepFudgeFactor);

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("1.2.3.4", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::UnorderedElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("1.2.3.4", 80))))));

  test_task_runner->FastForwardUntilNoTasksRemain();
}

TEST_F(HostResolverManagerTest, Mdns_Cancel) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(4);

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  response.CancelRequest();

  socket_factory_ptr->SimulateReceive(kMdnsResponseA, sizeof(kMdnsResponseA));
  socket_factory_ptr->SimulateReceive(kMdnsResponseAAAA,
                                      sizeof(kMdnsResponseAAAA));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
}

// Test for a two-transaction query where the first fails to start. The second
// should be cancelled.
TEST_F(HostResolverManagerTest, Mdns_PartialFailure) {
  // Setup a mock MDnsClient where the first transaction will always return
  // |false| immediately on Start(). Second transaction may or may not be
  // created, but if it is, Start() not expected to be called because the
  // overall request should immediately fail.
  auto transaction1 = std::make_unique<MockMDnsTransaction>();
  EXPECT_CALL(*transaction1, Start()).WillOnce(Return(false));
  auto transaction2 = std::make_unique<MockMDnsTransaction>();
  EXPECT_CALL(*transaction2, Start()).Times(0);

  auto client = std::make_unique<MockMDnsClient>();
  EXPECT_CALL(*client, CreateTransaction(_, _, _, _))
      .Times(Between(1, 2))  // Second transaction optionally created.
      .WillOnce(Return(ByMove(std::move(transaction1))))
      .WillOnce(Return(ByMove(std::move(transaction2))));
  EXPECT_CALL(*client, IsListening()).WillRepeatedly(Return(true));
  resolver_->SetMdnsClientForTesting(std::move(client));

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_FAILED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerTest, Mdns_ListenFailure) {
  // Inject an MdnsClient mock that will always fail to start listening.
  auto client = std::make_unique<MockMDnsClient>();
  EXPECT_CALL(*client, StartListening(_)).WillOnce(Return(ERR_FAILED));
  EXPECT_CALL(*client, IsListening()).WillRepeatedly(Return(false));
  resolver_->SetMdnsClientForTesting(std::move(client));

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::MULTICAST_DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_FAILED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Implementation of HostResolver::MdnsListenerDelegate that records all
// received results in maps.
class TestMdnsListenerDelegate : public HostResolver::MdnsListener::Delegate {
 public:
  using UpdateKey = std::pair<MdnsListenerUpdateType, DnsQueryType>;

  void OnAddressResult(MdnsListenerUpdateType update_type,
                       DnsQueryType result_type,
                       IPEndPoint address) override {
    address_results_.insert({{update_type, result_type}, address});
  }

  void OnTextResult(MdnsListenerUpdateType update_type,
                    DnsQueryType result_type,
                    std::vector<std::string> text_records) override {
    for (auto& text_record : text_records) {
      text_results_.insert(
          {{update_type, result_type}, std::move(text_record)});
    }
  }

  void OnHostnameResult(MdnsListenerUpdateType update_type,
                        DnsQueryType result_type,
                        HostPortPair host) override {
    hostname_results_.insert({{update_type, result_type}, std::move(host)});
  }

  void OnUnhandledResult(MdnsListenerUpdateType update_type,
                         DnsQueryType result_type) override {
    unhandled_results_.insert({update_type, result_type});
  }

  const std::multimap<UpdateKey, IPEndPoint>& address_results() {
    return address_results_;
  }

  const std::multimap<UpdateKey, std::string>& text_results() {
    return text_results_;
  }

  const std::multimap<UpdateKey, HostPortPair>& hostname_results() {
    return hostname_results_;
  }

  const std::multiset<UpdateKey>& unhandled_results() {
    return unhandled_results_;
  }

  template <typename T>
  static std::pair<UpdateKey, T> CreateExpectedResult(
      MdnsListenerUpdateType update_type,
      DnsQueryType query_type,
      T result) {
    return std::pair(std::pair(update_type, query_type), result);
  }

 private:
  std::multimap<UpdateKey, IPEndPoint> address_results_;
  std::multimap<UpdateKey, std::string> text_results_;
  std::multimap<UpdateKey, HostPortPair> hostname_results_;
  std::multiset<UpdateKey> unhandled_results_;
};

TEST_F(HostResolverManagerTest, MdnsListener) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  auto cache_cleanup_timer = std::make_unique<base::MockOneShotTimer>();
  auto* cache_cleanup_timer_ptr = cache_cleanup_timer.get();
  auto mdns_client =
      std::make_unique<MDnsClientImpl>(&clock, std::move(cache_cleanup_timer));
  ASSERT_THAT(mdns_client->StartListening(socket_factory.get()), IsOk());
  resolver_->SetMdnsClientForTesting(std::move(mdns_client));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 80),
                                    DnsQueryType::A);

  ASSERT_THAT(listener->Start(&delegate), IsOk());
  ASSERT_THAT(delegate.address_results(), testing::IsEmpty());

  socket_factory->SimulateReceive(kMdnsResponseA, sizeof(kMdnsResponseA));
  socket_factory->SimulateReceive(kMdnsResponseA2, sizeof(kMdnsResponseA2));
  socket_factory->SimulateReceive(kMdnsResponseA2Goodbye,
                                  sizeof(kMdnsResponseA2Goodbye));

  // Per RFC6762 section 10.1, removals take effect 1 second after receiving the
  // goodbye message.
  clock.Advance(base::Seconds(1));
  cache_cleanup_timer_ptr->Fire();

  // Expect 1 record adding "1.2.3.4", another changing to "5.6.7.8", and a
  // final removing "5.6.7.8".
  EXPECT_THAT(delegate.address_results(),
              testing::ElementsAre(
                  TestMdnsListenerDelegate::CreateExpectedResult(
                      MdnsListenerUpdateType::kAdded, DnsQueryType::A,
                      CreateExpected("1.2.3.4", 80)),
                  TestMdnsListenerDelegate::CreateExpectedResult(
                      MdnsListenerUpdateType::kChanged, DnsQueryType::A,
                      CreateExpected("5.6.7.8", 80)),
                  TestMdnsListenerDelegate::CreateExpectedResult(
                      MdnsListenerUpdateType::kRemoved, DnsQueryType::A,
                      CreateExpected("5.6.7.8", 80))));

  EXPECT_THAT(delegate.text_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.hostname_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverManagerTest, MdnsListener_StartListenFailure) {
  // Inject an MdnsClient mock that will always fail to start listening.
  auto client = std::make_unique<MockMDnsClient>();
  EXPECT_CALL(*client, StartListening(_)).WillOnce(Return(ERR_TIMED_OUT));
  EXPECT_CALL(*client, IsListening()).WillRepeatedly(Return(false));
  resolver_->SetMdnsClientForTesting(std::move(client));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 80),
                                    DnsQueryType::A);

  EXPECT_THAT(listener->Start(&delegate), IsError(ERR_TIMED_OUT));
  EXPECT_THAT(delegate.address_results(), testing::IsEmpty());
}

// Test that removal notifications are sent on natural expiration of MDNS
// records.
TEST_F(HostResolverManagerTest, MdnsListener_Expiration) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  base::SimpleTestClock clock;
  clock.SetNow(base::Time::Now());
  auto cache_cleanup_timer = std::make_unique<base::MockOneShotTimer>();
  auto* cache_cleanup_timer_ptr = cache_cleanup_timer.get();
  auto mdns_client =
      std::make_unique<MDnsClientImpl>(&clock, std::move(cache_cleanup_timer));
  ASSERT_THAT(mdns_client->StartListening(socket_factory.get()), IsOk());
  resolver_->SetMdnsClientForTesting(std::move(mdns_client));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 100),
                                    DnsQueryType::A);

  ASSERT_THAT(listener->Start(&delegate), IsOk());
  ASSERT_THAT(delegate.address_results(), testing::IsEmpty());

  socket_factory->SimulateReceive(kMdnsResponseA, sizeof(kMdnsResponseA));

  EXPECT_THAT(
      delegate.address_results(),
      testing::ElementsAre(TestMdnsListenerDelegate::CreateExpectedResult(
          MdnsListenerUpdateType::kAdded, DnsQueryType::A,
          CreateExpected("1.2.3.4", 100))));

  clock.Advance(base::Seconds(16));
  cache_cleanup_timer_ptr->Fire();

  EXPECT_THAT(delegate.address_results(),
              testing::ElementsAre(
                  TestMdnsListenerDelegate::CreateExpectedResult(
                      MdnsListenerUpdateType::kAdded, DnsQueryType::A,
                      CreateExpected("1.2.3.4", 100)),
                  TestMdnsListenerDelegate::CreateExpectedResult(
                      MdnsListenerUpdateType::kRemoved, DnsQueryType::A,
                      CreateExpected("1.2.3.4", 100))));

  EXPECT_THAT(delegate.text_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.hostname_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverManagerTest, MdnsListener_Txt) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 12),
                                    DnsQueryType::TXT);

  ASSERT_THAT(listener->Start(&delegate), IsOk());
  ASSERT_THAT(delegate.text_results(), testing::IsEmpty());

  socket_factory_ptr->SimulateReceive(kMdnsResponseTxt,
                                      sizeof(kMdnsResponseTxt));

  EXPECT_THAT(
      delegate.text_results(),
      testing::ElementsAre(
          TestMdnsListenerDelegate::CreateExpectedResult(
              MdnsListenerUpdateType::kAdded, DnsQueryType::TXT, "foo"),
          TestMdnsListenerDelegate::CreateExpectedResult(
              MdnsListenerUpdateType::kAdded, DnsQueryType::TXT, "bar")));

  EXPECT_THAT(delegate.address_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.hostname_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverManagerTest, MdnsListener_Ptr) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 13),
                                    DnsQueryType::PTR);

  ASSERT_THAT(listener->Start(&delegate), IsOk());
  ASSERT_THAT(delegate.text_results(), testing::IsEmpty());

  socket_factory_ptr->SimulateReceive(kMdnsResponsePtr,
                                      sizeof(kMdnsResponsePtr));

  EXPECT_THAT(
      delegate.hostname_results(),
      testing::ElementsAre(TestMdnsListenerDelegate::CreateExpectedResult(
          MdnsListenerUpdateType::kAdded, DnsQueryType::PTR,
          HostPortPair("foo.com", 13))));

  EXPECT_THAT(delegate.address_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.text_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverManagerTest, MdnsListener_Srv) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 14),
                                    DnsQueryType::SRV);

  ASSERT_THAT(listener->Start(&delegate), IsOk());
  ASSERT_THAT(delegate.text_results(), testing::IsEmpty());

  socket_factory_ptr->SimulateReceive(kMdnsResponseSrv,
                                      sizeof(kMdnsResponseSrv));

  EXPECT_THAT(
      delegate.hostname_results(),
      testing::ElementsAre(TestMdnsListenerDelegate::CreateExpectedResult(
          MdnsListenerUpdateType::kAdded, DnsQueryType::SRV,
          HostPortPair("foo.com", 8265))));

  EXPECT_THAT(delegate.address_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.text_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.unhandled_results(), testing::IsEmpty());
}

// Ensure query types we are not listening for do not affect MdnsListener.
TEST_F(HostResolverManagerTest, MdnsListener_NonListeningTypes) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 41),
                                    DnsQueryType::A);

  ASSERT_THAT(listener->Start(&delegate), IsOk());

  socket_factory_ptr->SimulateReceive(kMdnsResponseAAAA,
                                      sizeof(kMdnsResponseAAAA));

  EXPECT_THAT(delegate.address_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.text_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.hostname_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.unhandled_results(), testing::IsEmpty());
}

TEST_F(HostResolverManagerTest, MdnsListener_RootDomain) {
  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));

  TestMdnsListenerDelegate delegate;
  std::unique_ptr<HostResolver::MdnsListener> listener =
      resolver_->CreateMdnsListener(HostPortPair("myhello.local", 5),
                                    DnsQueryType::PTR);

  ASSERT_THAT(listener->Start(&delegate), IsOk());

  socket_factory_ptr->SimulateReceive(kMdnsResponsePtrRoot,
                                      sizeof(kMdnsResponsePtrRoot));

  EXPECT_THAT(delegate.unhandled_results(),
              testing::ElementsAre(std::pair(MdnsListenerUpdateType::kAdded,
                                             DnsQueryType::PTR)));

  EXPECT_THAT(delegate.address_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.text_results(), testing::IsEmpty());
  EXPECT_THAT(delegate.hostname_results(), testing::IsEmpty());
}
#endif  // BUILDFLAG(ENABLE_MDNS)

DnsConfig CreateUpgradableDnsConfig() {
  DnsConfig config;
  config.secure_dns_mode = SecureDnsMode::kAutomatic;
  config.allow_dns_over_https_upgrade = true;

  auto ProviderHasAddr = [](std::string_view provider, const IPAddress& addr) {
    return base::Contains(GetDohProviderEntryForTesting(provider).ip_addresses,
                          addr);
  };

  // Cloudflare upgradeable IPs
  IPAddress dns_ip0(1, 0, 0, 1);
  IPAddress dns_ip1;
  EXPECT_TRUE(dns_ip1.AssignFromIPLiteral("2606:4700:4700::1111"));
  EXPECT_TRUE(ProviderHasAddr("Cloudflare", dns_ip0));
  EXPECT_TRUE(ProviderHasAddr("Cloudflare", dns_ip1));
  // CleanBrowsingFamily upgradeable IP
  IPAddress dns_ip2;
  EXPECT_TRUE(dns_ip2.AssignFromIPLiteral("2a0d:2a00:2::"));
  EXPECT_TRUE(ProviderHasAddr("CleanBrowsingFamily", dns_ip2));
  // CleanBrowsingSecure upgradeable IP
  IPAddress dns_ip3(185, 228, 169, 9);
  EXPECT_TRUE(ProviderHasAddr("CleanBrowsingSecure", dns_ip3));
  // Non-upgradeable IP
  IPAddress dns_ip4(1, 2, 3, 4);

  config.nameservers = {
      IPEndPoint(dns_ip0, dns_protocol::kDefaultPort),
      IPEndPoint(dns_ip1, dns_protocol::kDefaultPort),
      IPEndPoint(dns_ip2, 54),
      IPEndPoint(dns_ip3, dns_protocol::kDefaultPort),
      IPEndPoint(dns_ip4, dns_protocol::kDefaultPort),
  };
  EXPECT_TRUE(config.IsValid());
  return config;
}

// Check that entries are written to the cache with the right NAK.
TEST_F(HostResolverManagerTest, NetworkAnonymizationKeyWriteToHostCache) {
  const SchemefulSite kSite1(GURL("https://origin1.test/"));
  const SchemefulSite kSite2(GURL("https://origin2.test/"));
  auto kNetworkAnonymizationKey1 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite1);
  auto kNetworkAnonymizationKey2 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite2);

  const char kFirstDnsResult[] = "192.168.1.42";
  const char kSecondDnsResult[] = "192.168.1.43";

  for (bool split_cache_by_network_anonymization_key : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (split_cache_by_network_anonymization_key) {
      feature_list.InitAndEnableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    }
    proc_->AddRuleForAllFamilies("just.testing", kFirstDnsResult);
    proc_->SignalMultiple(1u);

    // Resolve a host using kNetworkAnonymizationKey1.
    ResolveHostResponseHelper response1(resolver_->CreateRequest(
        HostPortPair("just.testing", 80), kNetworkAnonymizationKey1,
        NetLogWithSource(), std::nullopt, resolve_context_.get()));
    EXPECT_THAT(response1.result_error(), IsOk());
    EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected(kFirstDnsResult, 80)));
    EXPECT_THAT(
        response1.request()->GetEndpointResults(),
        testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
            testing::ElementsAre(CreateExpected(kFirstDnsResult, 80))))));
    EXPECT_FALSE(response1.request()->GetStaleInfo());
    EXPECT_EQ(1u, proc_->GetCaptureList().size());

    // If the host cache is being split by NetworkAnonymizationKeys, there
    // should be an entry in the HostCache with kNetworkAnonymizationKey1.
    // Otherwise, there should be an entry with the empty NAK.
    if (split_cache_by_network_anonymization_key) {
      EXPECT_TRUE(GetCacheHit(
          HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                         0 /* host_resolver_flags */, HostResolverSource::ANY,
                         kNetworkAnonymizationKey1)));

      EXPECT_FALSE(GetCacheHit(
          HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                         0 /* host_resolver_flags */, HostResolverSource::ANY,
                         NetworkAnonymizationKey())));
    } else {
      EXPECT_FALSE(GetCacheHit(
          HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                         0 /* host_resolver_flags */, HostResolverSource::ANY,
                         kNetworkAnonymizationKey1)));

      EXPECT_TRUE(GetCacheHit(
          HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                         0 /* host_resolver_flags */, HostResolverSource::ANY,
                         NetworkAnonymizationKey())));
    }

    // There should be no entry using kNetworkAnonymizationKey2 in either case.
    EXPECT_FALSE(GetCacheHit(HostCache::Key(
        "just.testing", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
        HostResolverSource::ANY, kNetworkAnonymizationKey2)));

    // A request using kNetworkAnonymizationKey2 should only be served out of
    // the cache of the cache if |split_cache_by_network_anonymization_key| is
    // false. If it's not served over the network, it is provided a different
    // result.
    if (split_cache_by_network_anonymization_key) {
      proc_->AddRuleForAllFamilies("just.testing", kSecondDnsResult);
      proc_->SignalMultiple(1u);
    }
    ResolveHostResponseHelper response2(resolver_->CreateRequest(
        HostPortPair("just.testing", 80), kNetworkAnonymizationKey2,
        NetLogWithSource(), std::nullopt, resolve_context_.get()));
    EXPECT_THAT(response2.result_error(), IsOk());
    if (split_cache_by_network_anonymization_key) {
      EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
                  testing::ElementsAre(CreateExpected(kSecondDnsResult, 80)));
      EXPECT_THAT(
          response2.request()->GetEndpointResults(),
          testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
              testing::ElementsAre(CreateExpected(kSecondDnsResult, 80))))));
      EXPECT_FALSE(response2.request()->GetStaleInfo());
      EXPECT_EQ(2u, proc_->GetCaptureList().size());
      EXPECT_TRUE(GetCacheHit(
          HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                         0 /* host_resolver_flags */, HostResolverSource::ANY,
                         kNetworkAnonymizationKey2)));
    } else {
      EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
                  testing::ElementsAre(CreateExpected(kFirstDnsResult, 80)));
      EXPECT_THAT(
          response2.request()->GetEndpointResults(),
          testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
              testing::ElementsAre(CreateExpected(kFirstDnsResult, 80))))));
      EXPECT_TRUE(response2.request()->GetStaleInfo());
      EXPECT_EQ(1u, proc_->GetCaptureList().size());
      EXPECT_FALSE(GetCacheHit(
          HostCache::Key("just.testing", DnsQueryType::UNSPECIFIED,
                         0 /* host_resolver_flags */, HostResolverSource::ANY,
                         kNetworkAnonymizationKey2)));
    }

    resolve_context_->host_cache()->clear();
    proc_->ClearCaptureList();
  }
}

// Check that entries are read to the cache with the right NAK.
TEST_F(HostResolverManagerTest, NetworkAnonymizationKeyReadFromHostCache) {
  const SchemefulSite kSite1(GURL("https://origin1.test/"));
  const SchemefulSite kSite2(GURL("https://origin2.test/"));
  auto kNetworkAnonymizationKey1 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite1);
  auto kNetworkAnonymizationKey2 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite2);

  struct CacheEntry {
    NetworkAnonymizationKey network_anonymization_key;
    const char* cached_ip_address;
  };

  const CacheEntry kCacheEntries[] = {
      {NetworkAnonymizationKey(), "192.168.1.42"},
      {kNetworkAnonymizationKey1, "192.168.1.43"},
      {kNetworkAnonymizationKey2, "192.168.1.44"},
  };

  // Add entries to cache for the empty NAK, NAK1, and NAK2. Only the
  // HostResolverManager obeys network state partitioning, so this is fine to do
  // regardless of the feature value.
  for (const auto& cache_entry : kCacheEntries) {
    HostCache::Key key("just.testing", DnsQueryType::UNSPECIFIED, 0,
                       HostResolverSource::ANY,
                       cache_entry.network_anonymization_key);
    IPAddress address;
    ASSERT_TRUE(address.AssignFromIPLiteral(cache_entry.cached_ip_address));
    HostCache::Entry entry = HostCache::Entry(
        OK, {{address, 80}}, /*aliases=*/{}, HostCache::Entry::SOURCE_UNKNOWN);
    resolve_context_->host_cache()->Set(key, entry, base::TimeTicks::Now(),
                                        base::Days(1));
  }

  for (bool split_cache_by_network_anonymization_key : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (split_cache_by_network_anonymization_key) {
      feature_list.InitAndEnableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    }

    // A request that uses kNetworkAnonymizationKey1 will return cache entry 1
    // if the NetworkAnonymizationKeys are being used, and cache entry 0
    // otherwise.
    ResolveHostResponseHelper response1(resolver_->CreateRequest(
        HostPortPair("just.testing", 80), kNetworkAnonymizationKey1,
        NetLogWithSource(), std::nullopt, resolve_context_.get()));
    EXPECT_THAT(response1.result_error(), IsOk());
    EXPECT_THAT(
        response1.request()->GetAddressResults()->endpoints(),
        testing::ElementsAre(CreateExpected(
            kCacheEntries[split_cache_by_network_anonymization_key ? 1 : 0]
                .cached_ip_address,
            80)));
    EXPECT_THAT(
        response1.request()->GetEndpointResults(),
        testing::Pointee(testing::ElementsAre(
            ExpectEndpointResult(testing::ElementsAre(CreateExpected(
                kCacheEntries[split_cache_by_network_anonymization_key ? 1 : 0]
                    .cached_ip_address,
                80))))));
    EXPECT_TRUE(response1.request()->GetStaleInfo());

    // A request that uses kNetworkAnonymizationKey2 will return cache entry 2
    // if the NetworkAnonymizationKeys are being used, and cache entry 0
    // otherwise.
    ResolveHostResponseHelper response2(resolver_->CreateRequest(
        HostPortPair("just.testing", 80), kNetworkAnonymizationKey2,
        NetLogWithSource(), std::nullopt, resolve_context_.get()));
    EXPECT_THAT(response2.result_error(), IsOk());
    EXPECT_THAT(
        response2.request()->GetAddressResults()->endpoints(),
        testing::ElementsAre(CreateExpected(
            kCacheEntries[split_cache_by_network_anonymization_key ? 2 : 0]
                .cached_ip_address,
            80)));
    EXPECT_THAT(
        response2.request()->GetEndpointResults(),
        testing::Pointee(testing::ElementsAre(
            ExpectEndpointResult(testing::ElementsAre(CreateExpected(
                kCacheEntries[split_cache_by_network_anonymization_key ? 2 : 0]
                    .cached_ip_address,
                80))))));
    EXPECT_TRUE(response2.request()->GetStaleInfo());
  }
}

// Test that two requests made with different NetworkAnonymizationKeys are not
// merged if network state partitioning is enabled.
TEST_F(HostResolverManagerTest, NetworkAnonymizationKeyTwoRequestsAtOnce) {
  const SchemefulSite kSite1(GURL("https://origin1.test/"));
  const SchemefulSite kSite2(GURL("https://origin2.test/"));
  auto kNetworkAnonymizationKey1 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite1);
  auto kNetworkAnonymizationKey2 =
      net::NetworkAnonymizationKey::CreateSameSite(kSite2);

  const char kDnsResult[] = "192.168.1.42";

  for (bool split_cache_by_network_anonymization_key : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    if (split_cache_by_network_anonymization_key) {
      feature_list.InitAndEnableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kPartitionConnectionsByNetworkIsolationKey);
    }
    proc_->AddRuleForAllFamilies("just.testing", kDnsResult);

    // Start resolving a host using kNetworkAnonymizationKey1.
    ResolveHostResponseHelper response1(resolver_->CreateRequest(
        HostPortPair("just.testing", 80), kNetworkAnonymizationKey1,
        NetLogWithSource(), std::nullopt, resolve_context_.get()));
    EXPECT_FALSE(response1.complete());

    // Start resolving the same host using kNetworkAnonymizationKey2.
    ResolveHostResponseHelper response2(resolver_->CreateRequest(
        HostPortPair("just.testing", 80), kNetworkAnonymizationKey2,
        NetLogWithSource(), std::nullopt, resolve_context_.get()));
    EXPECT_FALSE(response2.complete());

    // Wait for and complete the expected number of over-the-wire DNS
    // resolutions.
    if (split_cache_by_network_anonymization_key) {
      proc_->WaitFor(2);
      EXPECT_EQ(2u, proc_->GetCaptureList().size());
      proc_->SignalMultiple(2u);
    } else {
      proc_->WaitFor(1);
      EXPECT_EQ(1u, proc_->GetCaptureList().size());
      proc_->SignalMultiple(1u);
    }

    // Both requests should have completed successfully, with neither served out
    // of the cache.

    EXPECT_THAT(response1.result_error(), IsOk());
    EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected(kDnsResult, 80)));
    EXPECT_THAT(response1.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected(kDnsResult, 80))))));
    EXPECT_FALSE(response1.request()->GetStaleInfo());

    EXPECT_THAT(response2.result_error(), IsOk());
    EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected(kDnsResult, 80)));
    EXPECT_THAT(response2.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected(kDnsResult, 80))))));
    EXPECT_FALSE(response2.request()->GetStaleInfo());

    resolve_context_->host_cache()->clear();
    proc_->ClearCaptureList();
  }
}

// Test that two otherwise-identical requests with different ResolveContexts are
// not merged.
TEST_F(HostResolverManagerTest, ContextsNotMerged) {
  const char kDnsResult[] = "192.168.1.42";

  proc_->AddRuleForAllFamilies("just.testing", kDnsResult);

  // Start resolving a host using |resolve_context_|.
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(response1.complete());

  // Start resolving the same host using another ResolveContext and cache.
  ResolveContext resolve_context2(resolve_context_->url_request_context(),
                                  true /* enable_caching */);
  resolver_->RegisterResolveContext(&resolve_context2);
  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("just.testing", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, &resolve_context2));
  EXPECT_FALSE(response2.complete());
  EXPECT_EQ(2u, resolver_->num_jobs_for_testing());

  // Wait for and complete the 2 over-the-wire DNS resolutions.
  proc_->WaitFor(2);
  EXPECT_EQ(2u, proc_->GetCaptureList().size());
  proc_->SignalMultiple(2u);

  // Both requests should have completed successfully, with neither served out
  // of the cache.

  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected(kDnsResult, 80)));
  EXPECT_THAT(response1.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected(kDnsResult, 80))))));
  EXPECT_FALSE(response1.request()->GetStaleInfo());

  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected(kDnsResult, 80)));
  EXPECT_THAT(response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected(kDnsResult, 80))))));
  EXPECT_FALSE(response2.request()->GetStaleInfo());

  EXPECT_EQ(1u, resolve_context_->host_cache()->size());
  EXPECT_EQ(1u, resolve_context2.host_cache()->size());

  resolver_->DeregisterResolveContext(&resolve_context2);
}

// HostResolverManagerDnsTest ==================================================

HostResolverManagerDnsTest::HostResolverManagerDnsTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : HostResolverManagerTest(time_source),
      notifier_task_runner_(
          base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {
  auto config_service = std::make_unique<TestDnsConfigService>();
  config_service_ = config_service.get();
  notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
      notifier_task_runner_, std::move(config_service));
}

HostResolverManagerDnsTest::~HostResolverManagerDnsTest() = default;

void HostResolverManagerDnsTest::DestroyResolver() {
  mock_dns_client_ = nullptr;
  HostResolverManagerTest::DestroyResolver();
}

void HostResolverManagerDnsTest::SetDnsClient(
    std::unique_ptr<DnsClient> dns_client) {
  mock_dns_client_ = nullptr;
  resolver_->SetDnsClientForTesting(std::move(dns_client));
}

void HostResolverManagerDnsTest::TearDown() {
  HostResolverManagerTest::TearDown();
  InvalidateDnsConfig();

  // Ensure |notifier_| is fully cleaned up before test shutdown.
  notifier_.reset();
  notifier_task_runner_->RunUntilIdle();
}

HostResolver::ManagerOptions HostResolverManagerDnsTest::DefaultOptions() {
  HostResolver::ManagerOptions options =
      HostResolverManagerTest::DefaultOptions();
  options.insecure_dns_client_enabled = true;
  options.additional_types_via_insecure_dns_enabled = true;
  return options;
}

void HostResolverManagerDnsTest::CreateResolverWithOptionsAndParams(
    HostResolver::ManagerOptions options,
    const HostResolverSystemTask::Params& params,
    bool ipv6_reachable,
    bool is_async,
    bool ipv4_reachable) {
  DestroyResolver();

  resolver_ = std::make_unique<TestHostResolverManager>(
      options, notifier_.get(), nullptr /* net_log */, ipv6_reachable,
      ipv4_reachable, is_async);
  auto dns_client =
      std::make_unique<MockDnsClient>(DnsConfig(), CreateDefaultDnsRules());
  mock_dns_client_ = dns_client.get();
  resolver_->SetDnsClientForTesting(std::move(dns_client));
  resolver_->SetInsecureDnsClientEnabled(
      options.insecure_dns_client_enabled,
      options.additional_types_via_insecure_dns_enabled);
  resolver_->set_host_resolver_system_params_for_test(params);
  resolver_->RegisterResolveContext(resolve_context_.get());
}

void HostResolverManagerDnsTest::UseMockDnsClient(const DnsConfig& config,
                                                  MockDnsClientRuleList rules) {
  // HostResolver expects DnsConfig to get set after setting DnsClient, so
  // create first with an empty config and then update the config.
  auto dns_client =
      std::make_unique<MockDnsClient>(DnsConfig(), std::move(rules));
  mock_dns_client_ = dns_client.get();
  resolver_->SetDnsClientForTesting(std::move(dns_client));
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/true);
  if (!config.Equals(DnsConfig())) {
    ChangeDnsConfig(config);
  }
}

// static
MockDnsClientRuleList HostResolverManagerDnsTest::CreateDefaultDnsRules() {
  MockDnsClientRuleList rules;

  AddDnsRule(&rules, "nodomain", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kNoDomain, false /* delay */);
  AddDnsRule(&rules, "nodomain", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kNoDomain, false /* delay */);
  AddDnsRule(&rules, "nx", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kFail, false /* delay */);
  AddDnsRule(&rules, "nx", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kFail, false /* delay */);
  AddDnsRule(&rules, "ok", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "ok", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "4ok", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "4ok", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  AddDnsRule(&rules, "6ok", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  AddDnsRule(&rules, "6ok", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "4nx", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "4nx", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kFail, false /* delay */);
  AddDnsRule(&rules, "empty", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  AddDnsRule(&rules, "empty", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);

  AddDnsRule(&rules, "slow_nx", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kFail, true /* delay */);
  AddDnsRule(&rules, "slow_nx", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kFail, true /* delay */);

  AddDnsRule(&rules, "4slow_ok", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, true /* delay */);
  AddDnsRule(&rules, "4slow_ok", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "6slow_ok", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "6slow_ok", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, true /* delay */);
  AddDnsRule(&rules, "4slow_4ok", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, true /* delay */);
  AddDnsRule(&rules, "4slow_4ok", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  AddDnsRule(&rules, "4slow_4timeout", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kTimeout, true /* delay */);
  AddDnsRule(&rules, "4slow_4timeout", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "4slow_6timeout", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, true /* delay */);
  AddDnsRule(&rules, "4slow_6timeout", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kTimeout, false /* delay */);

  AddDnsRule(&rules, "4collision", dns_protocol::kTypeA,
             IPAddress(127, 0, 53, 53), false /* delay */);
  AddDnsRule(&rules, "4collision", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  AddDnsRule(&rules, "6collision", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  // This isn't the expected IP for collisions (but looks close to it).
  AddDnsRule(&rules, "6collision", dns_protocol::kTypeAAAA,
             IPAddress(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 0, 53, 53),
             false /* delay */);

  AddSecureDnsRule(&rules, "automatic_nodomain", dns_protocol::kTypeA,
                   MockDnsClientRule::ResultType::kNoDomain, false /* delay */);
  AddSecureDnsRule(&rules, "automatic_nodomain", dns_protocol::kTypeAAAA,
                   MockDnsClientRule::ResultType::kNoDomain, false /* delay */);
  AddDnsRule(&rules, "automatic_nodomain", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kNoDomain, false /* delay */);
  AddDnsRule(&rules, "automatic_nodomain", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kNoDomain, false /* delay */);
  AddSecureDnsRule(&rules, "automatic", dns_protocol::kTypeA,
                   MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddSecureDnsRule(&rules, "automatic", dns_protocol::kTypeAAAA,
                   MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "automatic", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "automatic", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "insecure_automatic", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddDnsRule(&rules, "insecure_automatic", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kOk, false /* delay */);

  AddSecureDnsRule(&rules, "secure", dns_protocol::kTypeA,
                   MockDnsClientRule::ResultType::kOk, false /* delay */);
  AddSecureDnsRule(&rules, "secure", dns_protocol::kTypeAAAA,
                   MockDnsClientRule::ResultType::kOk, false /* delay */);

  return rules;
}

// static
void HostResolverManagerDnsTest::AddDnsRule(
    MockDnsClientRuleList* rules,
    const std::string& prefix,
    uint16_t qtype,
    MockDnsClientRule::ResultType result_type,
    bool delay) {
  rules->emplace_back(prefix, qtype, false /* secure */,
                      MockDnsClientRule::Result(result_type), delay);
}

// static
void HostResolverManagerDnsTest::AddDnsRule(MockDnsClientRuleList* rules,
                                            const std::string& prefix,
                                            uint16_t qtype,
                                            const IPAddress& result_ip,
                                            bool delay) {
  rules->emplace_back(
      prefix, qtype, false /* secure */,
      MockDnsClientRule::Result(BuildTestDnsAddressResponse(prefix, result_ip)),
      delay);
}

// static
void HostResolverManagerDnsTest::AddDnsRule(MockDnsClientRuleList* rules,
                                            const std::string& prefix,
                                            uint16_t qtype,
                                            IPAddress result_ip,
                                            std::string cannonname,
                                            bool delay) {
  rules->emplace_back(
      prefix, qtype, false /* secure */,
      MockDnsClientRule::Result(BuildTestDnsAddressResponseWithCname(
          prefix, result_ip, std::move(cannonname))),
      delay);
}

// static
void HostResolverManagerDnsTest::AddDnsRule(MockDnsClientRuleList* rules,

                                            const std::string& prefix,
                                            uint16_t qtype,
                                            DnsResponse dns_test_response,
                                            bool delay) {
  rules->emplace_back(prefix, qtype, false /* secure */,
                      MockDnsClientRule::Result(std::move(dns_test_response)),
                      delay);
}

// static
void HostResolverManagerDnsTest::AddSecureDnsRule(
    MockDnsClientRuleList* rules,
    const std::string& prefix,
    uint16_t qtype,
    MockDnsClientRule::ResultType result_type,
    bool delay) {
  rules->emplace_back(prefix, qtype, true /* secure */,
                      MockDnsClientRule::Result(result_type), delay);
}

void HostResolverManagerDnsTest::ChangeDnsConfig(const DnsConfig& config) {
  DCHECK(config.IsValid());
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnHostsRead,
                     base::Unretained(config_service_), config.hosts));
  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::OnConfigRead,
                                base::Unretained(config_service_), config));

  notifier_task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
}

void HostResolverManagerDnsTest::InvalidateDnsConfig() {
  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::OnHostsRead,
                                base::Unretained(config_service_), DnsHosts()));
  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::InvalidateConfig,
                                base::Unretained(config_service_)));

  notifier_task_runner_->FastForwardBy(DnsConfigService::kInvalidationTimeout);
  base::RunLoop().RunUntilIdle();
}

void HostResolverManagerDnsTest::SetInitialDnsConfig(const DnsConfig& config) {
  InvalidateDnsConfig();
  ChangeDnsConfig(config);
}

void HostResolverManagerDnsTest::TriggerInsecureFailureCondition() {
  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.

  // Disable Secure DNS for these requests.
  HostResolver::ResolveHostParameters parameters;
  parameters.secure_dns_policy = SecureDnsPolicy::kDisable;

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  for (unsigned i = 0; i < maximum_insecure_dns_task_failures(); ++i) {
    // Use custom names to require separate Jobs.
    std::string hostname = base::StringPrintf("nx_%u", i);
    // Ensure fallback to HostResolverSystemTask succeeds.
    proc_->AddRuleForAllFamilies(hostname, "192.168.1.101");
    responses.emplace_back(
        std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
            HostPortPair(hostname, 80), NetworkAnonymizationKey(),
            NetLogWithSource(), parameters, resolve_context_.get())));
  }

  proc_->SignalMultiple(responses.size());

  for (const auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsOk());
  }

  ASSERT_FALSE(proc_->HasBlockedRequests());
}

TEST_F(HostResolverManagerDnsTest, FlushCacheOnDnsConfigChange) {
  proc_->SignalMultiple(2u);  // One before the flush, one after.

  // Resolve to populate the cache.
  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("host1", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(initial_response.result_error(), IsOk());
  EXPECT_EQ(1u, proc_->GetCaptureList().size());

  // Result expected to come from the cache.
  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("host1", 75), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsOk());
  EXPECT_EQ(1u, proc_->GetCaptureList().size());  // No expected increase.

  // Flush cache by triggering a DNS config change.
  ChangeDnsConfig(CreateValidDnsConfig());

  // Expect flushed from cache and therefore served from |proc_|.
  ResolveHostResponseHelper flushed_response(resolver_->CreateRequest(
      HostPortPair("host1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(flushed_response.result_error(), IsOk());
  EXPECT_EQ(2u, proc_->GetCaptureList().size());  // Expected increase.
}

TEST_F(HostResolverManagerDnsTest, DisableAndEnableInsecureDnsClient) {
  // Disable fallback to allow testing how requests are initially handled.
  set_allow_fallback_to_systemtask(false);

  ChangeDnsConfig(CreateValidDnsConfig());
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.2.47");
  proc_->SignalMultiple(1u);

  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled*/ false);
  ResolveHostResponseHelper response_system(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 1212), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_system.result_error(), IsOk());
  EXPECT_THAT(response_system.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.2.47", 1212)));
  EXPECT_THAT(
      response_system.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.2.47", 1212))))));

  resolver_->SetInsecureDnsClientEnabled(/*enabled*/ true,
                                         /*additional_dns_types_enabled=*/true);
  ResolveHostResponseHelper response_dns_client(resolver_->CreateRequest(
      HostPortPair("ok_fail", 1212), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_dns_client.result_error(), IsOk());
  EXPECT_THAT(response_dns_client.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("::1", 1212),
                                            CreateExpected("127.0.0.1", 1212)));
  EXPECT_THAT(
      response_dns_client.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::UnorderedElementsAre(CreateExpected("::1", 1212),
                                        CreateExpected("127.0.0.1", 1212))))));
}

TEST_F(HostResolverManagerDnsTest,
       UseHostResolverSystemTaskWhenPrivateDnsActive) {
  // Disable fallback to allow testing how requests are initially handled.
  set_allow_fallback_to_systemtask(false);
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.2.47");
  proc_->SignalMultiple(1u);

  DnsConfig config = CreateValidDnsConfig();
  config.dns_over_tls_active = true;
  ChangeDnsConfig(config);
  ResolveHostResponseHelper response_system(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 1212), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_system.result_error(), IsOk());
  EXPECT_THAT(response_system.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.2.47", 1212)));
  EXPECT_THAT(
      response_system.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("192.168.2.47", 1212))))));
}

// RFC 6761 localhost names should always resolve to loopback.
TEST_F(HostResolverManagerDnsTest, LocalhostLookup) {
  // Add a rule resolving localhost names to a non-loopback IP and test
  // that they still resolves to loopback.
  proc_->AddRuleForAllFamilies("foo.localhost", "192.168.1.42");
  proc_->AddRuleForAllFamilies("localhost", "192.168.1.42");
  proc_->AddRuleForAllFamilies("localhost.", "192.168.1.42");

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("foo.localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response0.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response1.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("localhost.", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response2.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// RFC 6761 localhost names should always resolve to loopback, even if a HOSTS
// file is active.
TEST_F(HostResolverManagerDnsTest, LocalhostLookupWithHosts) {
  DnsHosts hosts;
  hosts[DnsHostsKey("localhost", ADDRESS_FAMILY_IPV4)] =
      IPAddress({192, 168, 1, 1});
  hosts[DnsHostsKey("foo.localhost", ADDRESS_FAMILY_IPV4)] =
      IPAddress({192, 168, 1, 2});

  DnsConfig config = CreateValidDnsConfig();
  config.hosts = hosts;
  ChangeDnsConfig(config);

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response0.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("foo.localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response1.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// Test successful and fallback resolutions in HostResolverManager::DnsTask.
TEST_F(HostResolverManagerDnsTest, DnsTask) {
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  // Initially there is no config, so client should not be invoked.
  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("ok_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(initial_response.complete());

  proc_->SignalMultiple(1u);

  EXPECT_THAT(initial_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("ok_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("nx_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  proc_->SignalMultiple(4u);

  // Resolved by MockDnsClient.
  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response0.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  // Fallback to HostResolverSystemTask.
  EXPECT_THAT(response1.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.102", 80)));
  EXPECT_THAT(response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.102", 80))))));
}

TEST_F(HostResolverManagerDnsTest, DnsTaskWithScheme) {
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kWsScheme, "ok_fail", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  // Resolved by MockDnsClient.
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// Test successful and failing resolutions in HostResolverManager::DnsTask when
// fallback to HostResolverSystemTask is disabled.
TEST_F(HostResolverManagerDnsTest, NoFallbackToHostResolverSystemTask) {
  set_allow_fallback_to_systemtask(false);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  // Set empty DnsConfig.
  InvalidateDnsConfig();
  // Initially there is no config, so client should not be invoked.
  ResolveHostResponseHelper initial_response0(resolver_->CreateRequest(
      HostPortPair("ok_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper initial_response1(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(2u);

  EXPECT_THAT(initial_response0.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(initial_response1.result_error(), IsOk());
  EXPECT_THAT(initial_response1.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.102", 80)));
  EXPECT_THAT(initial_response1.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.102", 80))))));

  // Switch to a valid config.
  ChangeDnsConfig(CreateValidDnsConfig());
  // First request is resolved by MockDnsClient, others should fail due to
  // disabled fallback to HostResolverSystemTask.
  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("ok_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(6u);

  // Resolved by MockDnsClient.
  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response0.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  // Fallback to HostResolverSystemTask is disabled.
  EXPECT_THAT(response1.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

// Test behavior of OnDnsTaskFailure when Job is aborted.
TEST_F(HostResolverManagerDnsTest, OnDnsTaskFailureAbortedJob) {
  ChangeDnsConfig(CreateValidDnsConfig());
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("nx_abort", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  // Abort all jobs here.
  CreateResolver();
  proc_->SignalMultiple(1u);
  // Run to completion.
  base::RunLoop().RunUntilIdle();  // Notification happens async.
  // It shouldn't crash during OnDnsTaskFailure callbacks.
  EXPECT_FALSE(response.complete());

  // Repeat test with Fallback to HostResolverSystemTask disabled
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());
  ResolveHostResponseHelper no_fallback_response(resolver_->CreateRequest(
      HostPortPair("nx_abort", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  // Abort all jobs here.
  CreateResolver();
  proc_->SignalMultiple(2u);
  // Run to completion.
  base::RunLoop().RunUntilIdle();  // Notification happens async.
  // It shouldn't crash during OnDnsTaskFailure callbacks.
  EXPECT_FALSE(no_fallback_response.complete());
}

// Fallback to proc allowed with ANY source.
TEST_F(HostResolverManagerDnsTest, FallbackBySource_Any) {
  // Ensure fallback is otherwise allowed by resolver settings.
  set_allow_fallback_to_systemtask(true);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("nx_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(2u);

  EXPECT_THAT(response0.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.102", 80)));
  EXPECT_THAT(response1.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.102", 80))))));
}

// Fallback to proc not allowed with DNS source.
TEST_F(HostResolverManagerDnsTest, FallbackBySource_Dns) {
  // Ensure fallback is otherwise allowed by resolver settings.
  set_allow_fallback_to_systemtask(true);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("nx_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  // Nothing should reach |proc_| on success, but let failures through to fail
  // instead of hanging.
  proc_->SignalMultiple(2u);

  EXPECT_THAT(response0.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response1.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

// Fallback to proc on DnsClient change allowed with ANY source.
TEST_F(HostResolverManagerDnsTest, FallbackOnAbortBySource_Any) {
  // Ensure fallback is otherwise allowed by resolver settings.
  set_allow_fallback_to_systemtask(true);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("ok_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(2u);

  // Simulate the case when the preference or policy has disabled the insecure
  // DNS client causing AbortInsecureDnsTasks.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);

  // All requests should fallback to system resolver.
  EXPECT_THAT(response0.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.102", 80)));
  EXPECT_THAT(response1.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.102", 80))))));
}

// Fallback to system on DnsClient change not allowed with DNS source.
TEST_F(HostResolverManagerDnsTest, FallbackOnAbortBySource_Dns) {
  // Ensure fallback is otherwise allowed by resolver settings.
  set_allow_fallback_to_systemtask(true);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("ok_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  // Nothing should reach |proc_| on success, but let failures through to fail
  // instead of hanging.
  proc_->SignalMultiple(2u);

  // Simulate the case when the preference or policy has disabled the insecure
  // DNS client causing AbortInsecureDnsTasks.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);

  // No fallback expected.  All requests should fail.
  EXPECT_THAT(response0.result_error(), IsError(ERR_NETWORK_CHANGED));
  EXPECT_THAT(response1.result_error(), IsError(ERR_NETWORK_CHANGED));
}

// Insecure DnsClient change shouldn't affect secure DnsTasks.
TEST_F(HostResolverManagerDnsTest,
       DisableInsecureDnsClient_SecureDnsTasksUnaffected) {
  // Ensure fallback is otherwise allowed by resolver settings.
  set_allow_fallback_to_systemtask(true);

  proc_->AddRuleForAllFamilies("automatic", "192.168.1.102");
  // All other hostnames will fail in proc_.

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(),
      /* optional_parameters=*/std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(response_secure.complete());

  // Simulate the case when the preference or policy has disabled the insecure
  // DNS client causing AbortInsecureDnsTasks.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled*/ false);

  EXPECT_THAT(response_secure.result_error(), IsOk());
  EXPECT_THAT(response_secure.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_secure.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

TEST_F(HostResolverManagerDnsTest, DnsTaskUnspec) {
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies("4nx", "192.168.1.101");
  // All other hostnames will fail in proc_.

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("4ok", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("6ok", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("4nx", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));

  proc_->SignalMultiple(4u);

  for (auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsOk());
  }

  EXPECT_THAT(responses[0]->request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      responses[0]->request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(responses[1]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(responses[1]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(responses[2]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::1", 80)));
  EXPECT_THAT(responses[2]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::1", 80))))));
  EXPECT_THAT(responses[3]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.101", 80)));
  EXPECT_THAT(responses[3]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.101", 80))))));
}

TEST_F(HostResolverManagerDnsTest, NameCollisionIcann) {
  ChangeDnsConfig(CreateValidDnsConfig());

  // When the resolver returns an A record with 127.0.53.53 it should be
  // mapped to a special error.
  ResolveHostResponseHelper response_ipv4(resolver_->CreateRequest(
      HostPortPair("4collision", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_ipv4.result_error(), IsError(ERR_ICANN_NAME_COLLISION));
  EXPECT_THAT(response_ipv4.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response_ipv4.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // When the resolver returns an AAAA record with ::127.0.53.53 it should
  // work just like any other IP. (Despite having the same suffix, it is not
  // considered special)
  ResolveHostResponseHelper response_ipv6(resolver_->CreateRequest(
      HostPortPair("6collision", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_ipv6.result_error(), IsOk());
  EXPECT_THAT(response_ipv6.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::127.0.53.53", 80)));
  EXPECT_THAT(response_ipv6.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::127.0.53.53", 80))))));
}

TEST_F(HostResolverManagerDnsTest, ServeFromHosts) {
  // Initially, use empty HOSTS file.
  DnsConfig config = CreateValidDnsConfig();
  ChangeDnsConfig(config);

  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.
  proc_->SignalMultiple(1u);  // For the first request which misses.

  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("nx_ipv4", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(initial_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  IPAddress local_ipv4 = IPAddress::IPv4Localhost();
  IPAddress local_ipv6 = IPAddress::IPv6Localhost();

  DnsHosts hosts;
  hosts[DnsHostsKey("nx_ipv4", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  hosts[DnsHostsKey("nx_ipv6", ADDRESS_FAMILY_IPV6)] = local_ipv6;
  hosts[DnsHostsKey("nx_both", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  hosts[DnsHostsKey("nx_both", ADDRESS_FAMILY_IPV6)] = local_ipv6;

  // Update HOSTS file.
  config.hosts = hosts;
  ChangeDnsConfig(config);

  ResolveHostResponseHelper response_ipv4(resolver_->CreateRequest(
      HostPortPair("nx_ipv4", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_ipv4.result_error(), IsOk());
  EXPECT_THAT(response_ipv4.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(response_ipv4.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(response_ipv4.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));

  ResolveHostResponseHelper response_ipv6(resolver_->CreateRequest(
      HostPortPair("nx_ipv6", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_ipv6.result_error(), IsOk());
  EXPECT_THAT(response_ipv6.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::1", 80)));
  EXPECT_THAT(response_ipv6.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::1", 80))))));
  EXPECT_THAT(response_ipv6.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));

  ResolveHostResponseHelper response_both(resolver_->CreateRequest(
      HostPortPair("nx_both", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_both.result_error(), IsOk());
  EXPECT_THAT(response_both.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_both.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(response_both.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));

  // Requests with specified DNS query type.
  HostResolver::ResolveHostParameters parameters;

  parameters.dns_query_type = DnsQueryType::A;
  ResolveHostResponseHelper response_specified_ipv4(resolver_->CreateRequest(
      HostPortPair("nx_ipv4", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(response_specified_ipv4.result_error(), IsOk());
  EXPECT_THAT(
      response_specified_ipv4.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(response_specified_ipv4.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(response_specified_ipv4.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));

  parameters.dns_query_type = DnsQueryType::AAAA;
  ResolveHostResponseHelper response_specified_ipv6(resolver_->CreateRequest(
      HostPortPair("nx_ipv6", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(response_specified_ipv6.result_error(), IsOk());
  EXPECT_THAT(
      response_specified_ipv6.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(CreateExpected("::1", 80)));
  EXPECT_THAT(response_specified_ipv6.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::1", 80))))));
  EXPECT_THAT(response_specified_ipv6.request()->GetDnsAliasResults(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       SkipHostsWithUpcomingHostResolverSystemTask) {
  // Disable the DnsClient.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);

  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.
  proc_->SignalMultiple(1u);  // For the first request which misses.

  DnsConfig config = CreateValidDnsConfig();
  DnsHosts hosts;
  hosts[DnsHostsKey("hosts", ADDRESS_FAMILY_IPV4)] = IPAddress::IPv4Localhost();

  // Update HOSTS file.
  config.hosts = hosts;
  ChangeDnsConfig(config);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("hosts", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

// Test that hosts ending in ".local" or ".local." are resolved using the system
// resolver.
TEST_F(HostResolverManagerDnsTest, BypassDnsTask) {
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies(std::string(),
                               std::string());  // Default to failures.

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;

  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("ok.local", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("ok.local.", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("oklocal", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("oklocal.", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  proc_->SignalMultiple(5u);

  for (size_t i = 0; i < 2; ++i)
    EXPECT_THAT(responses[i]->result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  for (size_t i = 2; i < responses.size(); ++i)
    EXPECT_THAT(responses[i]->result_error(), IsOk());
}

#if BUILDFLAG(ENABLE_MDNS)
// Test that non-address queries for hosts ending in ".local" are resolved using
// the MDNS resolver.
TEST_F(HostResolverManagerDnsTest, BypassDnsToMdnsWithNonAddress) {
  // Ensure DNS task and system requests will fail.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "myhello.local", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      false /* delay */);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  proc_->AddRuleForAllFamilies(std::string(), std::string());

  auto socket_factory = std::make_unique<MockMDnsSocketFactory>();
  MockMDnsSocketFactory* socket_factory_ptr = socket_factory.get();
  resolver_->SetMdnsSocketFactoryForTesting(std::move(socket_factory));
  // 2 socket creations for every transaction.
  EXPECT_CALL(*socket_factory_ptr, OnSendTo(_)).Times(2);

  HostResolver::ResolveHostParameters dns_parameters;
  dns_parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("myhello.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), dns_parameters, resolve_context_.get()));

  socket_factory_ptr->SimulateReceive(kMdnsResponseTxt,
                                      sizeof(kMdnsResponseTxt));
  proc_->SignalMultiple(1u);

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetTextResults(),
              testing::Pointee(testing::ElementsAre("foo", "bar")));
}
#endif  // BUILDFLAG(ENABLE_MDNS)

// Test that DNS task is always used when explicitly requested as the source,
// even with a case that would normally bypass it eg hosts ending in ".local".
TEST_F(HostResolverManagerDnsTest, DnsNotBypassedWhenDnsSource) {
  // Ensure DNS task requests will succeed and system requests will fail.
  ChangeDnsConfig(CreateValidDnsConfig());
  proc_->AddRuleForAllFamilies(std::string(), std::string());

  HostResolver::ResolveHostParameters dns_parameters;
  dns_parameters.source = HostResolverSource::DNS;

  ResolveHostResponseHelper dns_response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      dns_parameters, resolve_context_.get()));
  ResolveHostResponseHelper dns_local_response(resolver_->CreateRequest(
      HostPortPair("ok.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), dns_parameters, resolve_context_.get()));
  ResolveHostResponseHelper normal_local_response(resolver_->CreateRequest(
      HostPortPair("ok.local", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  proc_->SignalMultiple(3u);

  EXPECT_THAT(dns_response.result_error(), IsOk());
  EXPECT_THAT(dns_local_response.result_error(), IsOk());
  EXPECT_THAT(normal_local_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverManagerDnsTest, SystemOnlyBypassesDnsTask) {
  // Ensure DNS task requests will succeed and system requests will fail.
  ChangeDnsConfig(CreateValidDnsConfig());
  proc_->AddRuleForAllFamilies(std::string(), std::string());

  ResolveHostResponseHelper dns_response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::SYSTEM;
  ResolveHostResponseHelper system_response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));

  proc_->SignalMultiple(2u);

  EXPECT_THAT(dns_response.result_error(), IsOk());
  EXPECT_THAT(system_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverManagerDnsTest,
       DisableInsecureDnsClientOnPersistentFailure) {
  ChangeDnsConfig(CreateValidDnsConfig());

  // Check that DnsTask works.
  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("ok_1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(initial_response.result_error(), IsOk());

  TriggerInsecureFailureCondition();

  // Insecure DnsTasks should be disabled by now unless explicitly requested via
  // |source|.
  ResolveHostResponseHelper fail_response(resolver_->CreateRequest(
      HostPortPair("ok_2", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::DNS;
  ResolveHostResponseHelper dns_response(resolver_->CreateRequest(
      HostPortPair("ok_2", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  proc_->SignalMultiple(2u);
  EXPECT_THAT(fail_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(dns_response.result_error(), IsOk());

  // Check that it is re-enabled after DNS change.
  ChangeDnsConfig(CreateValidDnsConfig());
  ResolveHostResponseHelper reenabled_response(resolver_->CreateRequest(
      HostPortPair("ok_3", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(reenabled_response.result_error(), IsOk());
}

TEST_F(HostResolverManagerDnsTest, SecureDnsWorksAfterInsecureFailure) {
  DnsConfig config = CreateValidDnsConfig();
  config.secure_dns_mode = SecureDnsMode::kSecure;
  ChangeDnsConfig(config);

  TriggerInsecureFailureCondition();

  // Secure DnsTasks should not be affected.
  ResolveHostResponseHelper secure_response(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      /* optional_parameters=*/std::nullopt, resolve_context_.get()));
  EXPECT_THAT(secure_response.result_error(), IsOk());
}

TEST_F(HostResolverManagerDnsTest, DontDisableDnsClientOnSporadicFailure) {
  ChangeDnsConfig(CreateValidDnsConfig());

  // |proc_| defaults to successes.

  // 20 failures interleaved with 20 successes.
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  for (unsigned i = 0; i < 40; ++i) {
    // Use custom names to require separate Jobs.
    std::string hostname = (i % 2) == 0 ? base::StringPrintf("nx_%u", i)
                                        : base::StringPrintf("ok_%u", i);
    responses.emplace_back(
        std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
            HostPortPair(hostname, 80), NetworkAnonymizationKey(),
            NetLogWithSource(), std::nullopt, resolve_context_.get())));
  }

  proc_->SignalMultiple(40u);

  for (const auto& response : responses)
    EXPECT_THAT(response->result_error(), IsOk());

  // Make |proc_| default to failures.
  proc_->AddRuleForAllFamilies(std::string(), std::string());

  // DnsTask should still be enabled.
  ResolveHostResponseHelper final_response(resolver_->CreateRequest(
      HostPortPair("ok_last", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(final_response.result_error(), IsOk());
}

void HostResolverManagerDnsTest::Ipv6UnreachableTest(bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    false /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 500), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());

  // Only expect IPv4 results.
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 500)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 500))))));
}

TEST_F(HostResolverManagerDnsTest, Ipv6UnreachableAsync) {
  Ipv6UnreachableTest(true);
}

TEST_F(HostResolverManagerDnsTest, Ipv6UnreachableSync) {
  Ipv6UnreachableTest(false);
}

void HostResolverManagerDnsTest::Ipv6UnreachableInvalidConfigTest(
    bool is_async) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    false /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */, is_async);

  proc_->AddRule("example.com", ADDRESS_FAMILY_UNSPECIFIED, "1.2.3.4,::5");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("example.com", 500), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("1.2.3.4", 500),
                                            CreateExpected("::5", 500)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::5", 500), CreateExpected("1.2.3.4", 500))))));
}
// Without a valid DnsConfig, assume IPv6 is needed and ignore prober.
TEST_F(HostResolverManagerDnsTest, Ipv6Unreachable_InvalidConfigAsync) {
  Ipv6UnreachableInvalidConfigTest(true);
}

TEST_F(HostResolverManagerDnsTest, Ipv6Unreachable_InvalidConfigSync) {
  Ipv6UnreachableInvalidConfigTest(false);
}

TEST_F(HostResolverManagerDnsTest, Ipv6Unreachable_UseLocalIpv6) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    false /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);

  DnsConfig config = CreateValidDnsConfig();
  config.use_local_ipv6 = true;
  ChangeDnsConfig(config);

  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("ok", 500), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 500),
                                            CreateExpected("::1", 500)));
  EXPECT_THAT(
      response1.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 500), CreateExpected("127.0.0.1", 500))))));

  // Set |use_local_ipv6| to false. Expect only IPv4 results.
  config.use_local_ipv6 = false;
  ChangeDnsConfig(config);

  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("ok", 500), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 500)));
  EXPECT_THAT(response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 500))))));
}

// Confirm that resolving "localhost" is unrestricted even if there are no
// global IPv6 address. See SystemHostResolverCall for rationale.
// Test both the DnsClient and system host resolver paths.
TEST_F(HostResolverManagerDnsTest, Ipv6Unreachable_Localhost) {
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    false /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);

  // Make request fail if we actually get to the system resolver.
  proc_->AddRuleForAllFamilies(std::string(), std::string());

  // Try without DnsClient.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);
  ResolveHostResponseHelper system_response(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(system_response.result_error(), IsOk());
  EXPECT_THAT(system_response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      system_response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  // With DnsClient
  UseMockDnsClient(CreateValidDnsConfig(), CreateDefaultDnsRules());
  ResolveHostResponseHelper builtin_response(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(builtin_response.result_error(), IsOk());
  EXPECT_THAT(builtin_response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      builtin_response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  // DnsClient configured without ipv6 (but ipv6 should still work for
  // localhost).
  DnsConfig config = CreateValidDnsConfig();
  config.use_local_ipv6 = false;
  ChangeDnsConfig(config);
  ResolveHostResponseHelper ipv6_disabled_response(resolver_->CreateRequest(
      HostPortPair("localhost", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(ipv6_disabled_response.result_error(), IsOk());
  EXPECT_THAT(
      ipv6_disabled_response.request()->GetAddressResults()->endpoints(),
      testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                    CreateExpected("::1", 80)));
  EXPECT_THAT(
      ipv6_disabled_response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// Test that IPv6 being unreachable only causes the AAAA query to be disabled,
// rather than querying only for A. See https://crbug.com/1272055.
TEST_F(HostResolverManagerDnsTest, Ipv6UnreachableOnlyDisablesAAAAQuery) {
  const std::string kName = "https.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsAliasRecord(kName, "alias.test")};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kUnexpected),
      /*delay=*/false);

  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    /*ipv6_reachable=*/false,
                                    /*check_ipv6_on_wifi=*/true);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(),
      /*optional_parameters=*/std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 443)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 443))))));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, SeparateJobsBySecureDnsMode) {
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "a", dns_protocol::kTypeA, true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      false /* delay */);
  rules.emplace_back(
      "a", dns_protocol::kTypeAAAA, true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      false /* delay */);
  rules.emplace_back(
      "a", dns_protocol::kTypeA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      true /* delay */);
  rules.emplace_back(
      "a", dns_protocol::kTypeAAAA, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      true /* delay */);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  // Create three requests. One with a DISABLE policy parameter, one with no
  // resolution parameters at all, and one with an ALLOW policy parameter
  // (which is a no-op).
  HostResolver::ResolveHostParameters parameters_disable_secure;
  parameters_disable_secure.secure_dns_policy = SecureDnsPolicy::kDisable;
  ResolveHostResponseHelper insecure_response(resolver_->CreateRequest(
      HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters_disable_secure, resolve_context_.get()));
  EXPECT_EQ(1u, resolver_->num_jobs_for_testing());

  ResolveHostResponseHelper automatic_response0(resolver_->CreateRequest(
      HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_EQ(2u, resolver_->num_jobs_for_testing());

  HostResolver::ResolveHostParameters parameters_allow_secure;
  parameters_allow_secure.secure_dns_policy = SecureDnsPolicy::kAllow;
  ResolveHostResponseHelper automatic_response1(resolver_->CreateRequest(
      HostPortPair("a", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters_allow_secure, resolve_context_.get()));
  // The AUTOMATIC mode requests should be joined into the same job.
  EXPECT_EQ(2u, resolver_->num_jobs_for_testing());

  // Automatic mode requests have completed.  Insecure request is still blocked.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(insecure_response.complete());
  EXPECT_TRUE(automatic_response0.complete());
  EXPECT_TRUE(automatic_response1.complete());
  EXPECT_THAT(automatic_response0.result_error(), IsOk());
  EXPECT_THAT(automatic_response1.result_error(), IsOk());

  // Complete insecure transaction.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_TRUE(insecure_response.complete());
  EXPECT_THAT(insecure_response.result_error(), IsOk());
}

// Cancel a request with a single DNS transaction active.
TEST_F(HostResolverManagerDnsTest, CancelWithOneTransactionActive) {
  // Disable ipv6 to ensure we'll only try a single transaction for the host.
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    false /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);
  DnsConfig config = CreateValidDnsConfig();
  config.use_local_ipv6 = false;
  ChangeDnsConfig(config);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());
  ASSERT_EQ(1u, num_running_dispatcher_jobs());

  response.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Dispatcher state checked in TearDown.
}

// Cancel a request with a single DNS transaction active and another pending.
TEST_F(HostResolverManagerDnsTest, CancelWithOneTransactionActiveOnePending) {
  CreateSerialResolver();
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  response.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Dispatcher state checked in TearDown.
}

// Cancel a request with two DNS transactions active.
TEST_F(HostResolverManagerDnsTest, CancelWithTwoTransactionsActive) {
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  response.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Dispatcher state checked in TearDown.
}

// Delete a resolver with some active requests and some queued requests.
TEST_F(HostResolverManagerDnsTest, DeleteWithActiveTransactions) {
  // At most 10 Jobs active at once.
  CreateResolverWithLimitsAndParams(10u, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);

  ChangeDnsConfig(CreateValidDnsConfig());

  // Add 12 DNS lookups (creating well more than 10 transaction).
  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  for (int i = 0; i < 12; ++i) {
    std::string hostname = base::StringPrintf("ok%i", i);
    responses.emplace_back(
        std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
            HostPortPair(hostname, 80), NetworkAnonymizationKey(),
            NetLogWithSource(), std::nullopt, resolve_context_.get())));
  }
  EXPECT_EQ(10u, num_running_dispatcher_jobs());

  DestroyResolver();

  base::RunLoop().RunUntilIdle();
  for (auto& response : responses) {
    EXPECT_FALSE(response->complete());
  }
}

TEST_F(HostResolverManagerDnsTest, DeleteWithSecureTransactions) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  DestroyResolver();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
}

TEST_F(HostResolverManagerDnsTest, DeleteWithCompletedRequests) {
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  DestroyResolver();

  // Completed requests should be unaffected by manager destruction.
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// Cancel a request with only the IPv6 transaction active.
TEST_F(HostResolverManagerDnsTest, CancelWithIPv6TransactionActive) {
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("6slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  // The IPv4 request should complete, the IPv6 request is still pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  response.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Dispatcher state checked in TearDown.
}

// Cancel a request with only the IPv4 transaction pending.
TEST_F(HostResolverManagerDnsTest, CancelWithIPv4TransactionPending) {
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  // The IPv6 request should complete, the IPv4 request is still pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  response.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
}

TEST_F(HostResolverManagerDnsTest, CancelWithAutomaticModeTransactionPending) {
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "secure_6slow_6nx_insecure_6slow_ok", dns_protocol::kTypeA,
      true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      false /* delay */);
  rules.emplace_back(
      "secure_6slow_6nx_insecure_6slow_ok", dns_protocol::kTypeAAAA,
      true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      true /* delay */);
  rules.emplace_back(
      "secure_6slow_6nx_insecure_6slow_ok", dns_protocol::kTypeA,
      false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      false /* delay */);
  rules.emplace_back(
      "secure_6slow_6nx_insecure_6slow_ok", dns_protocol::kTypeAAAA,
      false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      true /* delay */);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("secure_6slow_6nx_insecure_6slow_ok", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_EQ(0u, num_running_dispatcher_jobs());

  // The secure IPv4 request should complete, the secure IPv6 request is still
  // pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, num_running_dispatcher_jobs());

  response0.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response0.complete());
  EXPECT_EQ(0u, num_running_dispatcher_jobs());

  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("secure_6slow_6nx_insecure_6slow_ok", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_EQ(0u, num_running_dispatcher_jobs());

  // The secure IPv4 request should complete, the secure IPv6 request is still
  // pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, num_running_dispatcher_jobs());

  // Let the secure IPv6 request complete and start the insecure requests.
  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_EQ(2u, num_running_dispatcher_jobs());

  // The insecure IPv4 request should complete, the insecure IPv6 request is
  // still pending.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  response1.CancelRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response1.complete());

  // Dispatcher state checked in TearDown.
}

// Test cases where AAAA completes first.
TEST_F(HostResolverManagerDnsTest, AAAACompletesFirst) {
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("4slow_4ok", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("4slow_4timeout", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("4slow_6timeout", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(responses[0]->complete());
  EXPECT_FALSE(responses[1]->complete());
  EXPECT_FALSE(responses[2]->complete());
  // The IPv6 of request 3 should have failed and resulted in cancelling the
  // IPv4 request.
  EXPECT_THAT(responses[3]->result_error(), IsError(ERR_DNS_TIMED_OUT));
  EXPECT_EQ(3u, num_running_dispatcher_jobs());

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(responses[0]->result_error(), IsOk());
  EXPECT_THAT(responses[0]->request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      responses[0]->request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));

  EXPECT_THAT(responses[1]->result_error(), IsOk());
  EXPECT_THAT(responses[1]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(responses[1]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));

  EXPECT_THAT(responses[2]->result_error(), IsError(ERR_DNS_TIMED_OUT));
}

TEST_F(HostResolverManagerDnsTest, AAAACompletesFirst_AutomaticMode) {
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "secure_slow_nx_insecure_4slow_ok", dns_protocol::kTypeA,
      true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      true /* delay */);
  rules.emplace_back(
      "secure_slow_nx_insecure_4slow_ok", dns_protocol::kTypeAAAA,
      true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      true /* delay */);
  rules.emplace_back(
      "secure_slow_nx_insecure_4slow_ok", dns_protocol::kTypeA,
      false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      true /* delay */);
  rules.emplace_back(
      "secure_slow_nx_insecure_4slow_ok", dns_protocol::kTypeAAAA,
      false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("secure_slow_nx_insecure_4slow_ok", 80),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
  // Complete the secure transactions.
  mock_dns_client_->CompleteDelayedTransactions();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
  // Complete the insecure transactions.
  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("127.0.0.1", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80))))));
  HostCache::Key insecure_key =
      HostCache::Key("secure_slow_nx_insecure_4slow_ok",
                     DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
                     HostResolverSource::ANY, NetworkAnonymizationKey());
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(insecure_key);
  EXPECT_TRUE(!!cache_result);
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic) {
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.100");
  set_allow_fallback_to_systemtask(true);

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;

  // A successful DoH request should result in a secure cache entry.
  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_secure.result_error(), IsOk());
  EXPECT_FALSE(
      response_secure.request()->GetResolveErrorInfo().is_secure_network_error);
  EXPECT_THAT(response_secure.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_secure.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  HostCache::Key secure_key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);

  // A successful plaintext DNS request should result in an insecure cache
  // entry.
  ResolveHostResponseHelper response_insecure(resolver_->CreateRequest(
      HostPortPair("insecure_automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_insecure.result_error(), IsOk());
  EXPECT_FALSE(response_insecure.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(response_insecure.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_insecure.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  HostCache::Key insecure_key =
      HostCache::Key("insecure_automatic", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  cache_result = GetCacheHit(insecure_key);
  EXPECT_TRUE(!!cache_result);

  // Fallback to HostResolverSystemTask allowed in AUTOMATIC mode.
  ResolveHostResponseHelper response_system(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(1u);
  EXPECT_THAT(response_system.result_error(), IsOk());
  EXPECT_THAT(response_system.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.100", 80)));
  EXPECT_THAT(response_system.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.100", 80))))));
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_SecureCache) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  // Populate cache with a secure entry.
  HostCache::Key cached_secure_key =
      HostCache::Key("automatic_cached", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  cached_secure_key.secure = true;
  IPEndPoint kExpectedSecureIP = CreateExpected("192.168.1.102", 80);
  PopulateCache(cached_secure_key, kExpectedSecureIP);

  // The secure cache should be checked prior to any DoH request being sent.
  ResolveHostResponseHelper response_secure_cached(resolver_->CreateRequest(
      HostPortPair("automatic_cached", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_secure_cached.result_error(), IsOk());
  EXPECT_FALSE(response_secure_cached.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(
      response_secure_cached.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(kExpectedSecureIP));
  EXPECT_THAT(response_secure_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedSecureIP)))));
  EXPECT_FALSE(
      response_secure_cached.request()->GetStaleInfo().value().is_stale());
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_InsecureCache) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  // Populate cache with an insecure entry.
  HostCache::Key cached_insecure_key =
      HostCache::Key("insecure_automatic_cached", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  IPEndPoint kExpectedInsecureIP = CreateExpected("192.168.1.103", 80);
  PopulateCache(cached_insecure_key, kExpectedInsecureIP);

  // The insecure cache should be checked after DoH requests fail.
  ResolveHostResponseHelper response_insecure_cached(resolver_->CreateRequest(
      HostPortPair("insecure_automatic_cached", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_insecure_cached.result_error(), IsOk());
  EXPECT_FALSE(response_insecure_cached.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(
      response_insecure_cached.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(kExpectedInsecureIP));
  EXPECT_THAT(response_insecure_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedInsecureIP)))));
  EXPECT_FALSE(
      response_insecure_cached.request()->GetStaleInfo().value().is_stale());
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_Downgrade) {
  ChangeDnsConfig(CreateValidDnsConfig());
  // There is no DoH server available.
  DnsConfigOverrides overrides;
  overrides.dns_over_https_config.emplace();
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;

  // Populate cache with both secure and insecure entries.
  HostCache::Key cached_secure_key =
      HostCache::Key("automatic_cached", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  cached_secure_key.secure = true;
  IPEndPoint kExpectedSecureIP = CreateExpected("192.168.1.102", 80);
  PopulateCache(cached_secure_key, kExpectedSecureIP);
  HostCache::Key cached_insecure_key =
      HostCache::Key("insecure_automatic_cached", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  IPEndPoint kExpectedInsecureIP = CreateExpected("192.168.1.103", 80);
  PopulateCache(cached_insecure_key, kExpectedInsecureIP);

  // The secure cache should still be checked first.
  ResolveHostResponseHelper response_cached(resolver_->CreateRequest(
      HostPortPair("automatic_cached", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_cached.result_error(), IsOk());
  EXPECT_THAT(response_cached.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(kExpectedSecureIP));
  EXPECT_THAT(response_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedSecureIP)))));

  // The insecure cache should be checked before any insecure requests are sent.
  ResolveHostResponseHelper insecure_response_cached(resolver_->CreateRequest(
      HostPortPair("insecure_automatic_cached", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(insecure_response_cached.result_error(), IsOk());
  EXPECT_THAT(
      insecure_response_cached.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(kExpectedInsecureIP));
  EXPECT_THAT(insecure_response_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedInsecureIP)))));

  // The DnsConfig doesn't contain DoH servers so AUTOMATIC mode will be
  // downgraded to OFF. A successful plaintext DNS request should result in an
  // insecure cache entry.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  HostCache::Key key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  cache_result = GetCacheHit(key);
  EXPECT_TRUE(!!cache_result);
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_Unavailable) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);
  mock_dns_client_->SetForceDohServerAvailable(false);

  // DoH requests should be skipped when there are no available DoH servers
  // in automatic mode. The cached result should be in the insecure cache.
  ResolveHostResponseHelper response_automatic(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_automatic.result_error(), IsOk());
  EXPECT_FALSE(response_automatic.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(response_automatic.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_automatic.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  HostCache::Key secure_key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(secure_key);
  EXPECT_FALSE(!!cache_result);

  HostCache::Key insecure_key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  cache_result = GetCacheHit(insecure_key);
  EXPECT_TRUE(!!cache_result);
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_Unavailable_Fail) {
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);
  mock_dns_client_->SetForceDohServerAvailable(false);

  // Insecure requests that fail should not be cached.
  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_secure.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_FALSE(
      response_secure.request()->GetResolveErrorInfo().is_secure_network_error);

  HostCache::Key secure_key = HostCache::Key(
      "secure", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(secure_key);
  EXPECT_FALSE(!!cache_result);

  HostCache::Key insecure_key = HostCache::Key(
      "secure", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  cache_result = GetCacheHit(insecure_key);
  EXPECT_FALSE(!!cache_result);
}

// Test that DoH server availability is respected per-context.
TEST_F(HostResolverManagerDnsTest,
       SecureDnsMode_Automatic_UnavailableByContext) {
  // Create and register two separate contexts.
  auto request_context1 = CreateTestURLRequestContextBuilder()->Build();
  auto request_context2 = CreateTestURLRequestContextBuilder()->Build();
  ResolveContext resolve_context1(request_context1.get(),
                                  false /* enable_caching */);
  ResolveContext resolve_context2(request_context2.get(),
                                  false /* enable_caching */);
  resolver_->RegisterResolveContext(&resolve_context1);
  resolver_->RegisterResolveContext(&resolve_context2);

  // Configure the resolver and underlying mock to attempt a secure query iff
  // the context has marked a DoH server available and otherwise attempt a
  // non-secure query.
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);
  mock_dns_client_->SetForceDohServerAvailable(false);

  // Mark a DoH server successful only for |resolve_context2|. Note that this
  // must come after the resolver's configuration is set because this relies on
  // the specific configuration containing a DoH server.
  resolve_context2.RecordServerSuccess(0u /* server_index */,
                                       true /* is_doh_server */,
                                       mock_dns_client_->GetCurrentSession());

  // No available DoH servers for |resolve_context1|, so expect a non-secure
  // request. Non-secure requests for "secure" will fail with
  // ERR_NAME_NOT_RESOLVED.
  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, &resolve_context1));
  ASSERT_THAT(response_secure.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  // One available DoH server for |resolve_context2|, so expect a secure
  // request. Secure requests for "secure" will succeed.
  ResolveHostResponseHelper response_secure2(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, &resolve_context2));
  ASSERT_THAT(response_secure2.result_error(), IsOk());

  resolver_->DeregisterResolveContext(&resolve_context1);
  resolver_->DeregisterResolveContext(&resolve_context2);
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_Stale) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  // Populate cache with insecure entry.
  HostCache::Key cached_stale_key = HostCache::Key(
      "automatic_stale", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  IPEndPoint kExpectedStaleIP = CreateExpected("192.168.1.102", 80);
  PopulateCache(cached_stale_key, kExpectedStaleIP);
  MakeCacheStale();

  HostResolver::ResolveHostParameters stale_allowed_parameters;
  stale_allowed_parameters.cache_usage =
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  // The insecure cache should be checked before secure requests are made since
  // stale results are allowed.
  ResolveHostResponseHelper response_stale(resolver_->CreateRequest(
      HostPortPair("automatic_stale", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), stale_allowed_parameters, resolve_context_.get()));
  EXPECT_THAT(response_stale.result_error(), IsOk());
  EXPECT_FALSE(
      response_stale.request()->GetResolveErrorInfo().is_secure_network_error);
  EXPECT_THAT(response_stale.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(kExpectedStaleIP));
  EXPECT_THAT(response_stale.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedStaleIP)))));
  EXPECT_TRUE(response_stale.request()->GetStaleInfo()->is_stale());
}

TEST_F(HostResolverManagerDnsTest,
       SecureDnsMode_Automatic_InsecureAsyncDisabled) {
  proc_->AddRuleForAllFamilies("insecure_automatic", "192.168.1.100");
  ChangeDnsConfig(CreateValidDnsConfig());
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;

  // The secure part of the dns client should be enabled.
  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_secure.result_error(), IsOk());
  EXPECT_THAT(response_secure.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_secure.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  HostCache::Key secure_key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);

  // The insecure part of the dns client is disabled so insecure requests
  // should be skipped.
  ResolveHostResponseHelper response_insecure(resolver_->CreateRequest(
      HostPortPair("insecure_automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(1u);
  ASSERT_THAT(response_insecure.result_error(), IsOk());
  EXPECT_THAT(response_insecure.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.100", 80)));
  EXPECT_THAT(response_insecure.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.100", 80))))));
  HostCache::Key insecure_key =
      HostCache::Key("insecure_automatic", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  cache_result = GetCacheHit(insecure_key);
  EXPECT_TRUE(!!cache_result);

  HostCache::Key cached_insecure_key =
      HostCache::Key("insecure_automatic_cached", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  IPEndPoint kExpectedInsecureIP = CreateExpected("192.168.1.101", 80);
  PopulateCache(cached_insecure_key, kExpectedInsecureIP);

  // The insecure cache should still be checked even if the insecure part of
  // the dns client is disabled.
  ResolveHostResponseHelper response_insecure_cached(resolver_->CreateRequest(
      HostPortPair("insecure_automatic_cached", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_insecure_cached.result_error(), IsOk());
  EXPECT_THAT(
      response_insecure_cached.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(kExpectedInsecureIP));
  EXPECT_THAT(response_insecure_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedInsecureIP)))));
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Automatic_DotActive) {
  proc_->AddRuleForAllFamilies("insecure_automatic", "192.168.1.100");
  DnsConfig config = CreateValidDnsConfig();
  config.dns_over_tls_active = true;
  ChangeDnsConfig(config);
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;

  // The secure part of the dns client should be enabled.
  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_secure.result_error(), IsOk());
  EXPECT_THAT(response_secure.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response_secure.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  HostCache::Key secure_key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);

  // Insecure async requests should be skipped since the system resolver
  // requests will be secure.
  ResolveHostResponseHelper response_insecure(resolver_->CreateRequest(
      HostPortPair("insecure_automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(1u);
  ASSERT_THAT(response_insecure.result_error(), IsOk());
  EXPECT_FALSE(response_insecure.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(response_insecure.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.100", 80)));
  EXPECT_THAT(response_insecure.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.100", 80))))));
  HostCache::Key insecure_key =
      HostCache::Key("insecure_automatic", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  cache_result = GetCacheHit(insecure_key);
  EXPECT_TRUE(!!cache_result);

  HostCache::Key cached_insecure_key =
      HostCache::Key("insecure_automatic_cached", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  IPEndPoint kExpectedInsecureIP = CreateExpected("192.168.1.101", 80);
  PopulateCache(cached_insecure_key, kExpectedInsecureIP);

  // The insecure cache should still be checked.
  ResolveHostResponseHelper response_insecure_cached(resolver_->CreateRequest(
      HostPortPair("insecure_automatic_cached", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response_insecure_cached.result_error(), IsOk());
  EXPECT_FALSE(response_insecure_cached.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(
      response_insecure_cached.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(kExpectedInsecureIP));
  EXPECT_THAT(response_insecure_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedInsecureIP)))));
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Secure) {
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.100");
  set_allow_fallback_to_systemtask(true);

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;

  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_secure.result_error(), IsOk());
  EXPECT_FALSE(
      response_secure.request()->GetResolveErrorInfo().is_secure_network_error);
  HostCache::Key secure_key = HostCache::Key(
      "secure", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);

  ResolveHostResponseHelper response_insecure(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_insecure.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_TRUE(response_insecure.request()
                  ->GetResolveErrorInfo()
                  .is_secure_network_error);
  HostCache::Key insecure_key = HostCache::Key(
      "ok", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  cache_result = GetCacheHit(insecure_key);
  EXPECT_FALSE(!!cache_result);

  // Fallback to HostResolverSystemTask not allowed in SECURE mode.
  ResolveHostResponseHelper response_system(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(1u);
  EXPECT_THAT(response_system.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_TRUE(
      response_system.request()->GetResolveErrorInfo().is_secure_network_error);
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Secure_InsecureAsyncDisabled) {
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.100");
  set_allow_fallback_to_systemtask(true);
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;

  // The secure part of the dns client should be enabled.
  ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response_secure.result_error(), IsOk());
  HostCache::Key secure_key = HostCache::Key(
      "secure", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  secure_key.secure = true;
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Secure_Local_CacheMiss) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  // Populate cache with an insecure entry.
  HostCache::Key cached_insecure_key = HostCache::Key(
      "automatic", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  IPEndPoint kExpectedInsecureIP = CreateExpected("192.168.1.102", 80);
  PopulateCache(cached_insecure_key, kExpectedInsecureIP);

  // NONE query expected to complete synchronously with a cache miss since
  // the insecure cache should not be checked.
  ResolveHostResponseHelper cache_miss_request(resolver_->CreateRequest(
      HostPortPair("automatic", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), source_none_parameters, resolve_context_.get()));
  EXPECT_TRUE(cache_miss_request.complete());
  EXPECT_THAT(cache_miss_request.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_FALSE(cache_miss_request.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
  EXPECT_THAT(cache_miss_request.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cache_miss_request.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_FALSE(cache_miss_request.request()->GetStaleInfo());
}

TEST_F(HostResolverManagerDnsTest, SecureDnsMode_Secure_Local_CacheHit) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  HostResolver::ResolveHostParameters source_none_parameters;
  source_none_parameters.source = HostResolverSource::LOCAL_ONLY;

  // Populate cache with a secure entry.
  HostCache::Key cached_secure_key = HostCache::Key(
      "secure", DnsQueryType::UNSPECIFIED, 0 /* host_resolver_flags */,
      HostResolverSource::ANY, NetworkAnonymizationKey());
  cached_secure_key.secure = true;
  IPEndPoint kExpectedSecureIP = CreateExpected("192.168.1.103", 80);
  PopulateCache(cached_secure_key, kExpectedSecureIP);

  // NONE query expected to complete synchronously with a cache hit from the
  // secure cache.
  ResolveHostResponseHelper response_cached(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_TRUE(response_cached.complete());
  EXPECT_THAT(response_cached.result_error(), IsOk());
  EXPECT_FALSE(
      response_cached.request()->GetResolveErrorInfo().is_secure_network_error);
  EXPECT_THAT(response_cached.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(kExpectedSecureIP));
  EXPECT_THAT(response_cached.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(kExpectedSecureIP)))));
}

// On an IPv6 network, if we get A results and the AAAA response is SERVFAIL, we
// fail the whole DnsTask rather than proceeding with just the A results. In
// SECURE mode, fallback to the system resolver is disabled. See
// https://crbug.com/1292324.
TEST_F(HostResolverManagerDnsTest,
       SecureDnsModeIsSecureAndAAAAServfailCausesFailDespiteAResults) {
  constexpr char kName[] = "name.test";

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(
          MockDnsClientRule::ResultType::kOk,
          BuildTestDnsAddressResponse(kName, IPAddress(192, 168, 1, 103))),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/false);

  DnsConfig config = CreateValidDnsConfig();
  config.use_local_ipv6 = true;

  CreateResolver();
  UseMockDnsClient(config, std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(),
      /*optional_parameters=*/std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

// Test for a resolve with a transaction that takes longer than usual to
// complete. With the typical behavior of using fast timeouts, this is expected
// to timeout and fallback to the system resolver.
TEST_F(HostResolverManagerDnsTest, SlowResolve) {
  // Add a successful fallback result.
  proc_->AddRuleForAllFamilies("slow_succeed", "192.168.1.211");

  MockDnsClientRuleList rules = CreateDefaultDnsRules();
  AddDnsRule(&rules, "slow_fail", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddDnsRule(&rules, "slow_fail", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddDnsRule(&rules, "slow_succeed", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddDnsRule(&rules, "slow_succeed", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kSlow, false /* delay */);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("slow_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("slow_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  proc_->SignalMultiple(3u);

  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response0.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(response1.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.211", 80)));
  EXPECT_THAT(response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.211", 80))))));
}

// Test for a resolve with a secure transaction that takes longer than usual to
// complete. In automatic mode, because fallback to insecure is available, the
// secure transaction is expected to quickly timeout and fallback to insecure.
TEST_F(HostResolverManagerDnsTest, SlowSecureResolve_AutomaticMode) {
  set_allow_fallback_to_systemtask(false);

  MockDnsClientRuleList rules = CreateDefaultDnsRules();
  AddSecureDnsRule(&rules, "slow_fail", dns_protocol::kTypeA,
                   MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddSecureDnsRule(&rules, "slow_fail", dns_protocol::kTypeAAAA,
                   MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddSecureDnsRule(&rules, "slow_succeed", dns_protocol::kTypeA,
                   MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddSecureDnsRule(&rules, "slow_succeed", dns_protocol::kTypeAAAA,
                   MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddDnsRule(&rules, "slow_succeed", dns_protocol::kTypeA,
             IPAddress(111, 222, 112, 223), false /* delay */);
  AddDnsRule(&rules, "slow_succeed", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, false /* delay */);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("slow_fail", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response2(resolver_->CreateRequest(
      HostPortPair("slow_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response0.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
  EXPECT_THAT(response1.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response2.result_error(), IsOk());
  EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("111.222.112.223", 80)));
  EXPECT_THAT(
      response2.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("111.222.112.223", 80))))));
}

// Test for a resolve with a secure transaction that takes longer than usual to
// complete. In secure mode, because no fallback is available, this is expected
// to wait longer before timeout and complete successfully.
TEST_F(HostResolverManagerDnsTest, SlowSecureResolve_SecureMode) {
  MockDnsClientRuleList rules = CreateDefaultDnsRules();
  AddSecureDnsRule(&rules, "slow", dns_protocol::kTypeA,
                   MockDnsClientRule::ResultType::kSlow, false /* delay */);
  AddSecureDnsRule(&rules, "slow", dns_protocol::kTypeAAAA,
                   MockDnsClientRule::ResultType::kSlow, false /* delay */);
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("slow", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response0.result_error(), IsOk());
  EXPECT_THAT(response1.result_error(), IsOk());
}

// Test the case where only a single transaction slot is available.
TEST_F(HostResolverManagerDnsTest, SerialResolver) {
  CreateSerialResolver();
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(response.complete());
  EXPECT_EQ(1u, num_running_dispatcher_jobs());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(response.complete());
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// Test the case where subsequent transactions are handled on transaction
// completion when only part of a multi-transaction request could be initially
// started.
TEST_F(HostResolverManagerDnsTest, AAAAStartsAfterOtherJobFinishes) {
  CreateResolverWithLimitsAndParams(3u, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response0(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_EQ(2u, num_running_dispatcher_jobs());
  ResolveHostResponseHelper response1(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_EQ(3u, num_running_dispatcher_jobs());

  // Request 0's transactions should complete, starting Request 1's second
  // transaction, which should also complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, num_running_dispatcher_jobs());
  EXPECT_TRUE(response0.complete());
  EXPECT_FALSE(response1.complete());

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response1.result_error(), IsOk());
  EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
              testing::UnorderedElementsAre(CreateExpected("127.0.0.1", 80),
                                            CreateExpected("::1", 80)));
  EXPECT_THAT(
      response1.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(testing::UnorderedElementsAre(
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

// Tests the case that a Job with a single transaction receives an empty address
// list, triggering fallback to HostResolverSystemTask.
TEST_F(HostResolverManagerDnsTest, IPv4EmptyFallback) {
  // Disable ipv6 to ensure we'll only try a single transaction for the host.
  CreateResolverWithLimitsAndParams(kMaxJobs, DefaultParams(proc_),
                                    false /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);
  DnsConfig config = CreateValidDnsConfig();
  config.use_local_ipv6 = false;
  ChangeDnsConfig(config);

  proc_->AddRuleForAllFamilies("empty_fallback", "192.168.0.1",
                               HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("empty_fallback", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.0.1", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.0.1", 80))))));
}

// Tests the case that a Job with two transactions receives two empty address
// lists, triggering fallback to HostResolverSystemTask.
TEST_F(HostResolverManagerDnsTest, UnspecEmptyFallback) {
  ChangeDnsConfig(CreateValidDnsConfig());
  proc_->AddRuleForAllFamilies("empty_fallback", "192.168.0.1");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("empty_fallback", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.0.1", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.0.1", 80))))));
}

// Tests getting a new invalid DnsConfig while there are active DnsTasks.
TEST_F(HostResolverManagerDnsTest, InvalidDnsConfigWithPendingRequests) {
  // At most 3 jobs active at once.  This number is important, since we want
  // to make sure that aborting the first HostResolverManager::Job does not
  // trigger another DnsTransaction on the second Job when it releases its
  // second prioritized dispatcher slot.
  CreateResolverWithLimitsAndParams(3u, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);

  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies("slow_nx1", "192.168.0.1");
  proc_->AddRuleForAllFamilies("slow_nx2", "192.168.0.2");
  proc_->AddRuleForAllFamilies("ok", "192.168.0.3");

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  // First active job gets two slots.
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("slow_nx1", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  // Next job gets one slot, and waits on another.
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("slow_nx2", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));

  EXPECT_EQ(3u, num_running_dispatcher_jobs());
  for (auto& response : responses) {
    EXPECT_FALSE(response->complete());
  }

  // Clear DNS config. Fully in-progress, partially in-progress, and queued
  // requests should all be aborted.
  InvalidateDnsConfig();
  for (auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsError(ERR_NETWORK_CHANGED));
  }
}

// Test that initial DNS config read signals do not abort pending requests
// when using DnsClient.
TEST_F(HostResolverManagerDnsTest, DontAbortOnInitialDNSConfigRead) {
  // DnsClient is enabled, but there's no DnsConfig, so the request should start
  // using HostResolverSystemTask.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host1", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_FALSE(response.complete());

  EXPECT_TRUE(proc_->WaitFor(1u));
  // Send the initial config read signal, with a valid config.
  SetInitialDnsConfig(CreateValidDnsConfig());
  proc_->SignalAll();

  EXPECT_THAT(response.result_error(), IsOk());
}

// Tests the case that the insecure part of the DnsClient is automatically
// disabled due to failures while there are active DnsTasks.
TEST_F(HostResolverManagerDnsTest,
       AutomaticallyDisableInsecureDnsClientWithPendingRequests) {
  // Trying different limits is important for this test:  Different limits
  // result in different behavior when aborting in-progress DnsTasks.  Having
  // a DnsTask that has one job active and one in the queue when another job
  // occupying two slots has its DnsTask aborted is the case most likely to run
  // into problems.  Try limits between [1, 2 * # of non failure requests].
  for (size_t limit = 1u; limit < 10u; ++limit) {
    CreateResolverWithLimitsAndParams(limit, DefaultParams(proc_),
                                      true /* ipv6_reachable */,
                                      true /* check_ipv6_on_wifi */);

    // Set the resolver in automatic-secure mode.
    net::DnsConfig config = CreateValidDnsConfig();
    config.secure_dns_mode = SecureDnsMode::kAutomatic;
    ChangeDnsConfig(config);

    // Start with request parameters that disable Secure DNS.
    HostResolver::ResolveHostParameters parameters;
    parameters.secure_dns_policy = SecureDnsPolicy::kDisable;

    // Queue up enough failures to disable insecure DnsTasks.  These will all
    // fall back to HostResolverSystemTasks, and succeed there.
    std::vector<std::unique_ptr<ResolveHostResponseHelper>> failure_responses;
    for (unsigned i = 0u; i < maximum_insecure_dns_task_failures(); ++i) {
      std::string host = base::StringPrintf("nx%u", i);
      proc_->AddRuleForAllFamilies(host, "192.168.0.1");
      failure_responses.emplace_back(
          std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
              HostPortPair(host, 80), NetworkAnonymizationKey(),
              NetLogWithSource(), parameters, resolve_context_.get())));
      EXPECT_FALSE(failure_responses[i]->complete());
    }

    // These requests should all bypass insecure DnsTasks, due to the above
    // failures, so should end up using HostResolverSystemTasks.
    proc_->AddRuleForAllFamilies("slow_ok1", "192.168.0.2");
    ResolveHostResponseHelper response0(resolver_->CreateRequest(
        HostPortPair("slow_ok1", 80), NetworkAnonymizationKey(),
        NetLogWithSource(), parameters, resolve_context_.get()));
    EXPECT_FALSE(response0.complete());
    proc_->AddRuleForAllFamilies("slow_ok2", "192.168.0.3");
    ResolveHostResponseHelper response1(resolver_->CreateRequest(
        HostPortPair("slow_ok2", 80), NetworkAnonymizationKey(),
        NetLogWithSource(), parameters, resolve_context_.get()));
    EXPECT_FALSE(response1.complete());
    proc_->AddRuleForAllFamilies("slow_ok3", "192.168.0.4");
    ResolveHostResponseHelper response2(resolver_->CreateRequest(
        HostPortPair("slow_ok3", 80), NetworkAnonymizationKey(),
        NetLogWithSource(), parameters, resolve_context_.get()));
    EXPECT_FALSE(response2.complete());

    // Requests specifying DNS source cannot fallback to HostResolverSystemTask,
    // so they should be unaffected.
    parameters.source = HostResolverSource::DNS;
    ResolveHostResponseHelper response_dns(resolver_->CreateRequest(
        HostPortPair("6slow_ok", 80), NetworkAnonymizationKey(),
        NetLogWithSource(), parameters, resolve_context_.get()));
    EXPECT_FALSE(response_dns.complete());

    // Requests specifying SYSTEM source should be unaffected by disabling
    // DnsClient.
    proc_->AddRuleForAllFamilies("nx_ok", "192.168.0.5");
    parameters.source = HostResolverSource::SYSTEM;
    ResolveHostResponseHelper response_system(resolver_->CreateRequest(
        HostPortPair("nx_ok", 80), NetworkAnonymizationKey(),
        NetLogWithSource(), parameters, resolve_context_.get()));
    EXPECT_FALSE(response_system.complete());

    // Secure DnsTasks should not be affected.
    ResolveHostResponseHelper response_secure(resolver_->CreateRequest(
        HostPortPair("automatic", 80), NetworkAnonymizationKey(),
        NetLogWithSource(), /* optional_parameters=*/std::nullopt,
        resolve_context_.get()));
    EXPECT_FALSE(response_secure.complete());

    proc_->SignalMultiple(maximum_insecure_dns_task_failures() + 4);

    for (size_t i = 0u; i < maximum_insecure_dns_task_failures(); ++i) {
      EXPECT_THAT(failure_responses[i]->result_error(), IsOk());
      EXPECT_THAT(
          failure_responses[i]->request()->GetAddressResults()->endpoints(),
          testing::ElementsAre(CreateExpected("192.168.0.1", 80)));
      EXPECT_THAT(
          failure_responses[i]->request()->GetEndpointResults(),
          testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
              testing::ElementsAre(CreateExpected("192.168.0.1", 80))))));
    }

    EXPECT_THAT(response0.result_error(), IsOk());
    EXPECT_THAT(response0.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected("192.168.0.2", 80)));
    EXPECT_THAT(response0.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected("192.168.0.2", 80))))));
    EXPECT_THAT(response1.result_error(), IsOk());
    EXPECT_THAT(response1.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected("192.168.0.3", 80)));
    EXPECT_THAT(response1.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected("192.168.0.3", 80))))));
    EXPECT_THAT(response2.result_error(), IsOk());
    EXPECT_THAT(response2.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected("192.168.0.4", 80)));
    EXPECT_THAT(response2.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected("192.168.0.4", 80))))));

    mock_dns_client_->CompleteDelayedTransactions();
    EXPECT_THAT(response_dns.result_error(), IsOk());

    EXPECT_THAT(response_system.result_error(), IsOk());
    EXPECT_THAT(response_system.request()->GetAddressResults()->endpoints(),
                testing::ElementsAre(CreateExpected("192.168.0.5", 80)));
    EXPECT_THAT(response_system.request()->GetEndpointResults(),
                testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                    testing::ElementsAre(CreateExpected("192.168.0.5", 80))))));

    EXPECT_THAT(response_secure.result_error(), IsOk());
  }
}

// Tests a call to SetDnsClient while there are active DnsTasks.
TEST_F(HostResolverManagerDnsTest,
       ManuallyDisableDnsClientWithPendingRequests) {
  // At most 3 jobs active at once.  This number is important, since we want to
  // make sure that aborting the first HostResolverManager::Job does not trigger
  // another DnsTransaction on the second Job when it releases its second
  // prioritized dispatcher slot.
  CreateResolverWithLimitsAndParams(3u, DefaultParams(proc_),
                                    true /* ipv6_reachable */,
                                    true /* check_ipv6_on_wifi */);

  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRuleForAllFamilies("slow_ok1", "192.168.0.1");
  proc_->AddRuleForAllFamilies("slow_ok2", "192.168.0.2");
  proc_->AddRuleForAllFamilies("ok", "192.168.0.3");

  std::vector<std::unique_ptr<ResolveHostResponseHelper>> responses;
  // First active job gets two slots.
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("slow_ok1", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  EXPECT_FALSE(responses[0]->complete());
  // Next job gets one slot, and waits on another.
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("slow_ok2", 80), NetworkAnonymizationKey(),
          NetLogWithSource(), std::nullopt, resolve_context_.get())));
  EXPECT_FALSE(responses[1]->complete());
  // Next one is queued.
  responses.emplace_back(
      std::make_unique<ResolveHostResponseHelper>(resolver_->CreateRequest(
          HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
          std::nullopt, resolve_context_.get())));
  EXPECT_FALSE(responses[2]->complete());

  EXPECT_EQ(3u, num_running_dispatcher_jobs());

  // Clear DnsClient.  The two in-progress jobs should fall back to a
  // HostResolverSystemTask, and the next one should be started with a
  // HostResolverSystemTask.
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);

  // All three in-progress requests should now be running a
  // HostResolverSystemTask.
  EXPECT_EQ(3u, num_running_dispatcher_jobs());
  proc_->SignalMultiple(3u);

  for (auto& response : responses) {
    EXPECT_THAT(response->result_error(), IsOk());
  }
  EXPECT_THAT(responses[0]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.0.1", 80)));
  EXPECT_THAT(responses[0]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.0.1", 80))))));
  EXPECT_THAT(responses[1]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.0.2", 80)));
  EXPECT_THAT(responses[1]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.0.2", 80))))));
  EXPECT_THAT(responses[2]->request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.0.3", 80)));
  EXPECT_THAT(responses[2]->request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.0.3", 80))))));
}

// When explicitly requesting source=DNS, no fallback allowed, so doing so with
// DnsClient disabled should result in an error.
TEST_F(HostResolverManagerDnsTest, DnsCallsWithDisabledDnsClient) {
  ChangeDnsConfig(CreateValidDnsConfig());
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/false,
      /*additional_dns_types_enabled=*/false);

  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      params, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
}

TEST_F(HostResolverManagerDnsTest,
       DnsCallsWithDisabledDnsClient_DisabledAtConstruction) {
  HostResolver::ManagerOptions options = DefaultOptions();
  options.insecure_dns_client_enabled = false;
  CreateResolverWithOptionsAndParams(std::move(options), DefaultParams(proc_),
                                     true /* ipv6_reachable */);
  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      params, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
}

// Same as DnsClient disabled, requests with source=DNS and no usable DnsConfig
// should result in an error.
TEST_F(HostResolverManagerDnsTest, DnsCallsWithNoDnsConfig) {
  InvalidateDnsConfig();

  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      params, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
}

TEST_F(HostResolverManagerDnsTest, NoCheckIpv6OnWifi) {
  // CreateSerialResolver will destroy the current resolver_ which will attempt
  // to remove itself from the NetworkChangeNotifier. If this happens after a
  // new NetworkChangeNotifier is active, then it will not remove itself from
  // the old NetworkChangeNotifier which is a potential use-after-free.
  DestroyResolver();
  test::ScopedMockNetworkChangeNotifier notifier;
  // Serial resolver to guarantee order of resolutions.
  CreateSerialResolver(false /* check_ipv6_on_wifi */);

  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_WIFI);
  // Needed so IPv6 availability check isn't skipped.
  ChangeDnsConfig(CreateValidDnsConfig());

  proc_->AddRule("h1", ADDRESS_FAMILY_UNSPECIFIED, "::3");
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV4, "1.0.0.1");
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV4, "1.0.0.1",
                 HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6);
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV6, "::2");

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("h1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::A;
  ResolveHostResponseHelper v4_response(resolver_->CreateRequest(
      HostPortPair("h1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  parameters.dns_query_type = DnsQueryType::AAAA;
  ResolveHostResponseHelper v6_response(resolver_->CreateRequest(
      HostPortPair("h1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));

  proc_->SignalMultiple(3u);

  // Should revert to only IPV4 request.
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("1.0.0.1", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("1.0.0.1", 80))))));

  EXPECT_THAT(v4_response.result_error(), IsOk());
  EXPECT_THAT(v4_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("1.0.0.1", 80)));
  EXPECT_THAT(v4_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("1.0.0.1", 80))))));
  EXPECT_THAT(v6_response.result_error(), IsOk());
  EXPECT_THAT(v6_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::2", 80)));
  EXPECT_THAT(v6_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::2", 80))))));

  // Now repeat the test on non-wifi to check that IPv6 is used as normal
  // after the network changes.
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_4G);
  base::RunLoop().RunUntilIdle();  // Wait for NetworkChangeNotifier.

  ResolveHostResponseHelper no_wifi_response(resolver_->CreateRequest(
      HostPortPair("h1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  parameters.dns_query_type = DnsQueryType::A;
  ResolveHostResponseHelper no_wifi_v4_response(resolver_->CreateRequest(
      HostPortPair("h1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  parameters.dns_query_type = DnsQueryType::AAAA;
  ResolveHostResponseHelper no_wifi_v6_response(resolver_->CreateRequest(
      HostPortPair("h1", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));

  proc_->SignalMultiple(3u);

  // IPV6 should be available.
  EXPECT_THAT(no_wifi_response.result_error(), IsOk());
  EXPECT_THAT(no_wifi_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::3", 80)));
  EXPECT_THAT(no_wifi_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::3", 80))))));

  EXPECT_THAT(no_wifi_v4_response.result_error(), IsOk());
  EXPECT_THAT(no_wifi_v4_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("1.0.0.1", 80)));
  EXPECT_THAT(no_wifi_v4_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("1.0.0.1", 80))))));
  EXPECT_THAT(no_wifi_v6_response.result_error(), IsOk());
  EXPECT_THAT(no_wifi_v6_response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("::2", 80)));
  EXPECT_THAT(no_wifi_v6_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("::2", 80))))));
}

TEST_F(HostResolverManagerDnsTest, NotFoundTtl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  CreateResolver();
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  // NODATA
  ResolveHostResponseHelper no_data_response(resolver_->CreateRequest(
      HostPortPair("empty", 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_data_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(no_data_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(no_data_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "empty", kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalErrorResult(
          "empty", DnsQueryType::A, HostResolverInternalResult::Source::kDns,
          Optional(base::TimeTicks::Now() + base::Days(1)),
          Optional(base::Time::Now() + base::Days(1)), ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "empty", kNetworkAnonymizationKey, DnsQueryType::AAAA,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalErrorResult(
          "empty", DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns,
          Optional(base::TimeTicks::Now() + base::Days(1)),
          Optional(base::Time::Now() + base::Days(1)), ERR_NAME_NOT_RESOLVED)));

  // NXDOMAIN
  ResolveHostResponseHelper no_domain_response(resolver_->CreateRequest(
      HostPortPair("nodomain", 80), kNetworkAnonymizationKey,
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_domain_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(no_domain_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(no_domain_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "nodomain", kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalErrorResult(
          "nodomain", DnsQueryType::A, HostResolverInternalResult::Source::kDns,
          Optional(base::TimeTicks::Now() + base::Days(1)),
          Optional(base::Time::Now() + base::Days(1)), ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "nodomain", kNetworkAnonymizationKey, DnsQueryType::AAAA,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalErrorResult(
          "nodomain", DnsQueryType::AAAA,
          HostResolverInternalResult::Source::kDns,
          Optional(base::TimeTicks::Now() + base::Days(1)),
          Optional(base::Time::Now() + base::Days(1)), ERR_NAME_NOT_RESOLVED)));
}

TEST_F(HostResolverManagerDnsTest, NotFoundTtlWithHostCache) {
  base::test::ScopedFeatureList feature_list;
  DisableHostResolverCache(feature_list);

  CreateResolver();
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  // NODATA
  ResolveHostResponseHelper no_data_response(resolver_->CreateRequest(
      HostPortPair("empty", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_data_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(no_data_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(no_data_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  HostCache::Key key("empty", DnsQueryType::UNSPECIFIED, 0,
                     HostResolverSource::ANY, NetworkAnonymizationKey());
  HostCache::EntryStaleness staleness;
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      resolve_context_->host_cache()->Lookup(key, base::TimeTicks::Now(),
                                             false /* ignore_secure */);
  EXPECT_TRUE(!!cache_result);
  EXPECT_TRUE(cache_result->second.has_ttl());
  EXPECT_THAT(cache_result->second.ttl(), base::Seconds(86400));

  // NXDOMAIN
  ResolveHostResponseHelper no_domain_response(resolver_->CreateRequest(
      HostPortPair("nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_domain_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(no_domain_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(no_domain_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  HostCache::Key nxkey("nodomain", DnsQueryType::UNSPECIFIED, 0,
                       HostResolverSource::ANY, NetworkAnonymizationKey());
  cache_result = resolve_context_->host_cache()->Lookup(
      nxkey, base::TimeTicks::Now(), false /* ignore_secure */);
  EXPECT_TRUE(!!cache_result);
  EXPECT_TRUE(cache_result->second.has_ttl());
  EXPECT_THAT(cache_result->second.ttl(), base::Seconds(86400));
}

TEST_F(HostResolverManagerDnsTest, CachedError) {
  proc_->AddRuleForAllFamilies(std::string(),
                               "0.0.0.1");  // Default to failures.
  proc_->SignalMultiple(1u);

  CreateResolver();
  set_allow_fallback_to_systemtask(true);
  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters cache_only_parameters;
  cache_only_parameters.source = HostResolverSource::LOCAL_ONLY;

  // Expect cache initially empty.
  ResolveHostResponseHelper cache_miss_response0(resolver_->CreateRequest(
      HostPortPair("nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), cache_only_parameters, resolve_context_.get()));
  EXPECT_THAT(cache_miss_response0.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_FALSE(cache_miss_response0.request()->GetStaleInfo());

  // The cache should not be populate with an error because fallback to
  // HostResolverSystemTask was available.
  ResolveHostResponseHelper no_domain_response_with_fallback(
      resolver_->CreateRequest(HostPortPair("nodomain", 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_domain_response_with_fallback.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));

  // Expect cache still empty.
  ResolveHostResponseHelper cache_miss_response1(resolver_->CreateRequest(
      HostPortPair("nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), cache_only_parameters, resolve_context_.get()));
  EXPECT_THAT(cache_miss_response1.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_FALSE(cache_miss_response1.request()->GetStaleInfo());

  // Disable fallback to systemtask
  set_allow_fallback_to_systemtask(false);

  // Populate cache with an error.
  ResolveHostResponseHelper no_domain_response(resolver_->CreateRequest(
      HostPortPair("nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_domain_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));

  // Expect the error result can be resolved from the cache.
  ResolveHostResponseHelper cache_hit_response(resolver_->CreateRequest(
      HostPortPair("nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), cache_only_parameters, resolve_context_.get()));
  EXPECT_THAT(cache_hit_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_FALSE(cache_hit_response.request()->GetStaleInfo().value().is_stale());
}

TEST_F(HostResolverManagerDnsTest, CachedError_AutomaticMode) {
  CreateResolver();
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  // Switch to automatic mode.
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  HostCache::Key insecure_key =
      HostCache::Key("automatic_nodomain", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  HostCache::Key secure_key =
      HostCache::Key("automatic_nodomain", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  secure_key.secure = true;

  // Expect cache initially empty.
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
  cache_result = GetCacheHit(secure_key);
  EXPECT_FALSE(!!cache_result);
  cache_result = GetCacheHit(insecure_key);
  EXPECT_FALSE(!!cache_result);

  // Populate both secure and insecure caches with an error.
  ResolveHostResponseHelper no_domain_response(resolver_->CreateRequest(
      HostPortPair("automatic_nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_domain_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));

  // Expect both secure and insecure caches to have the error result.
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);
  cache_result = GetCacheHit(insecure_key);
  EXPECT_TRUE(!!cache_result);
}

TEST_F(HostResolverManagerDnsTest, CachedError_SecureMode) {
  CreateResolver();
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  // Switch to secure mode.
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  HostCache::Key insecure_key =
      HostCache::Key("automatic_nodomain", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  HostCache::Key secure_key =
      HostCache::Key("automatic_nodomain", DnsQueryType::UNSPECIFIED,
                     0 /* host_resolver_flags */, HostResolverSource::ANY,
                     NetworkAnonymizationKey());
  secure_key.secure = true;

  // Expect cache initially empty.
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
  cache_result = GetCacheHit(secure_key);
  EXPECT_FALSE(!!cache_result);
  cache_result = GetCacheHit(insecure_key);
  EXPECT_FALSE(!!cache_result);

  // Populate secure cache with an error.
  ResolveHostResponseHelper no_domain_response(resolver_->CreateRequest(
      HostPortPair("automatic_nodomain", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  EXPECT_THAT(no_domain_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));

  // Expect only the secure cache to have the error result.
  cache_result = GetCacheHit(secure_key);
  EXPECT_TRUE(!!cache_result);
  cache_result = GetCacheHit(insecure_key);
  EXPECT_FALSE(!!cache_result);
}

// Test that if one of A and AAAA completes successfully and the other fails,
// the failure is not cached.
TEST_F(HostResolverManagerDnsTest, TtlNotSharedBetweenQtypes) {
  CreateResolver();
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_4timeout", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt /* optional_parameters */,
      resolve_context_.get()));

  // Ensure success completes before the timeout result.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_TIMED_OUT));

  // Expect failure not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, CanonicalName) {
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "alias", dns_protocol::kTypeA, IPAddress::IPv4Localhost(),
             "canonical", false /* delay */);
  AddDnsRule(&rules, "alias", dns_protocol::kTypeAAAA,
             IPAddress::IPv6Localhost(), "canonical", false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("alias", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      params, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsOk());

  EXPECT_THAT(
      response.request()->GetDnsAliasResults(),
      testing::Pointee(testing::UnorderedElementsAre("canonical", "alias")));
}

TEST_F(HostResolverManagerDnsTest, CanonicalName_PreferV6) {
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "alias", dns_protocol::kTypeA, IPAddress::IPv4Localhost(),
             "wrong", false /* delay */);
  AddDnsRule(&rules, "alias", dns_protocol::kTypeAAAA,
             IPAddress::IPv6Localhost(), "correct", true /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("alias", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      params, resolve_context_.get()));
  ASSERT_FALSE(response.complete());
  base::RunLoop().RunUntilIdle();
  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsOk());

  // GetDnsAliasResults() includes all aliases from all families.
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(
                  testing::UnorderedElementsAre("correct", "alias", "wrong")));
}

TEST_F(HostResolverManagerDnsTest, CanonicalName_V4Only) {
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "alias", dns_protocol::kTypeA, IPAddress::IPv4Localhost(),
             "correct", false /* delay */);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  HostResolver::ResolveHostParameters params;
  params.dns_query_type = DnsQueryType::A;
  params.source = HostResolverSource::DNS;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("alias", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      params, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(
      response.request()->GetDnsAliasResults(),
      testing::Pointee(testing::UnorderedElementsAre("correct", "alias")));
}

// Test that responses containing CNAME records but no address results are fine
// and treated as normal NODATA responses.
TEST_F(HostResolverManagerDnsTest, CanonicalNameWithoutResults) {
  MockDnsClientRuleList rules;

  DnsResponse a_response =
      BuildTestDnsResponse("a.test", dns_protocol::kTypeA,
                           {BuildTestCnameRecord("c.test", "d.test"),
                            BuildTestCnameRecord("b.test", "c.test"),
                            BuildTestCnameRecord("a.test", "b.test")});
  AddDnsRule(&rules, "a.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);

  DnsResponse aaaa_response =
      BuildTestDnsResponse("a.test", dns_protocol::kTypeAAAA,
                           {BuildTestCnameRecord("c.test", "d.test"),
                            BuildTestCnameRecord("b.test", "c.test"),
                            BuildTestCnameRecord("a.test", "b.test")});
  AddDnsRule(&rules, "a.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("a.test", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      /*optional_parameters=*/std::nullopt, resolve_context_.get()));

  ASSERT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_FALSE(response.request()->GetDnsAliasResults());

  // Underlying error should be the typical no-results error
  // (ERR_NAME_NOT_RESOLVED), not anything more exotic like
  // ERR_DNS_MALFORMED_RESPONSE.
  EXPECT_EQ(response.request()->GetResolveErrorInfo().error,
            ERR_NAME_NOT_RESOLVED);
}

// Test that if the response for one address family contains CNAME records but
// no address results, it doesn't interfere with the other address family
// receiving address results (as would happen if such a response were
// incorrectly treated as a malformed response error).
TEST_F(HostResolverManagerDnsTest, CanonicalNameWithResultsForOnlyOneFamily) {
  MockDnsClientRuleList rules;

  DnsResponse a_response =
      BuildTestDnsResponse("a.test", dns_protocol::kTypeA,
                           {BuildTestCnameRecord("c.test", "d.test"),
                            BuildTestCnameRecord("b.test", "c.test"),
                            BuildTestCnameRecord("a.test", "b.test")});
  AddDnsRule(&rules, "a.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);

  DnsResponse aaaa_response = BuildTestDnsResponse(
      "a.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("d.test", IPAddress::IPv6Localhost()),
       BuildTestCnameRecord("c.test", "d.test"),
       BuildTestCnameRecord("b.test", "c.test"),
       BuildTestCnameRecord("a.test", "b.test")});
  AddDnsRule(&rules, "a.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("a.test", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      /*optional_parameters=*/std::nullopt, resolve_context_.get()));

  ASSERT_THAT(response.result_error(), IsOk());

  ASSERT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 80))))));
}

// Test that without specifying source, a request that would otherwise be
// handled by DNS is sent to the system resolver if cannonname is requested.
TEST_F(HostResolverManagerDnsTest, CanonicalNameForcesProc) {
  // Disable fallback to ensure system resolver is used directly, not via
  // fallback.
  set_allow_fallback_to_systemtask(false);

  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102",
                               HOST_RESOLVER_CANONNAME, "canonical");
  proc_->SignalMultiple(1u);

  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters params;
  params.include_canonical_name = true;
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("nx_succeed", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), params, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsOk());

  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::UnorderedElementsAre("canonical")));
}

TEST_F(HostResolverManagerDnsTest, DnsAliases) {
  MockDnsClientRuleList rules;

  DnsResponse expected_A_response = BuildTestDnsResponse(
      "first.test", dns_protocol::kTypeA,
      {BuildTestAddressRecord("fourth.test", IPAddress::IPv4Localhost()),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestCnameRecord("second.test", "third.test"),
       BuildTestCnameRecord("first.test", "second.test")});

  AddDnsRule(&rules, "first.test", dns_protocol::kTypeA,
             std::move(expected_A_response), false /* delay */);

  DnsResponse expected_AAAA_response = BuildTestDnsResponse(
      "first.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("fourth.test", IPAddress::IPv6Localhost()),
       BuildTestCnameRecord("third.test", "fourth.test"),
       BuildTestCnameRecord("second.test", "third.test"),
       BuildTestCnameRecord("first.test", "second.test")});

  AddDnsRule(&rules, "first.test", dns_protocol::kTypeAAAA,
             std::move(expected_AAAA_response), false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);
  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("first.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), params, resolve_context_.get()));

  ASSERT_THAT(response.result_error(), IsOk());
  ASSERT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetAddressResults()->dns_aliases(),
              testing::UnorderedElementsAre("fourth.test", "third.test",
                                            "second.test", "first.test"));

  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "fourth.test", "third.test", "second.test", "first.test")));
}

TEST_F(HostResolverManagerDnsTest, DnsAliasesAreFixedUp) {
  MockDnsClientRuleList rules;

  // Need to manually encode non-URL-canonical names because DNSDomainFromDot()
  // requires URL-canonical names.
  constexpr char kNonCanonicalName[] = "\005HOST2\004test\000";

  DnsResponse expected_A_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeA,
      {BuildTestAddressRecord("host2.test", IPAddress::IPv4Localhost()),
       BuildTestDnsRecord(
           "host.test", dns_protocol::kTypeCNAME,
           std::string(kNonCanonicalName, sizeof(kNonCanonicalName) - 1))});

  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA,
             std::move(expected_A_response), false /* delay */);

  DnsResponse expected_AAAA_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("host2.test", IPAddress::IPv6Localhost()),
       BuildTestDnsRecord(
           "host.test", dns_protocol::kTypeCNAME,
           std::string(kNonCanonicalName, sizeof(kNonCanonicalName) - 1))});

  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(expected_AAAA_response), false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);
  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), params, resolve_context_.get()));

  ASSERT_THAT(response.result_error(), IsOk());
  ASSERT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetAddressResults()->dns_aliases(),
              testing::UnorderedElementsAre("host2.test", "host.test"));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(
                  testing::UnorderedElementsAre("host2.test", "host.test")));
}

TEST_F(HostResolverManagerDnsTest, RejectsLocalhostAlias) {
  MockDnsClientRuleList rules;

  DnsResponse expected_A_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeA,
      {BuildTestAddressRecord("localhost", IPAddress::IPv4Localhost()),
       BuildTestCnameRecord("host.test", "localhost")});

  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA,
             std::move(expected_A_response), false /* delay */);

  DnsResponse expected_AAAA_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("localhost", IPAddress::IPv6Localhost()),
       BuildTestCnameRecord("host.test", "localhost")});

  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(expected_AAAA_response), false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);
  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), params, resolve_context_.get()));

  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
}

TEST_F(HostResolverManagerDnsTest, NoAdditionalDnsAliases) {
  MockDnsClientRuleList rules;

  AddDnsRule(&rules, "first.test", dns_protocol::kTypeA,
             IPAddress::IPv4Localhost(), false /* delay */);

  AddDnsRule(&rules, "first.test", dns_protocol::kTypeAAAA,
             IPAddress::IPv6Localhost(), false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);
  HostResolver::ResolveHostParameters params;
  params.source = HostResolverSource::DNS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("first.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), params, resolve_context_.get()));

  ASSERT_THAT(response.result_error(), IsOk());
  ASSERT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetAddressResults()->dns_aliases(),
              testing::ElementsAre("first.test"));
  EXPECT_THAT(response.request()->GetDnsAliasResults(),
              testing::Pointee(testing::UnorderedElementsAre("first.test")));
}

TEST_F(HostResolverManagerTest, ResolveLocalHostname) {
  std::vector<IPEndPoint> addresses;

  TestBothLoopbackIPs("localhost");
  TestBothLoopbackIPs("localhoST");
  TestBothLoopbackIPs("localhost.");
  TestBothLoopbackIPs("localhoST.");
  TestBothLoopbackIPs("foo.localhost");
  TestBothLoopbackIPs("foo.localhOSt");
  TestBothLoopbackIPs("foo.localhost.");
  TestBothLoopbackIPs("foo.localhOSt.");

  // Legacy localhost names.
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomain", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomAIn", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomain.", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomAIn.", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhoST6", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6.", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6.localdomain6", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6.localdomain6.", &addresses));

  EXPECT_FALSE(ResolveLocalHostname("127.0.0.1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:0:0:0:1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhostx", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.x", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localdomain", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localdomain.x", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6x", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomain6", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6.localdomain", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("127.0.0.1.1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname(".127.0.0.255", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::2", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1:1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:1:0:0:1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1:1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:0:0:0:0:1", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localhost.com", &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localhoste", &addresses));
}

TEST_F(HostResolverManagerDnsTest, AddDnsOverHttpsServerAfterConfig) {
  DestroyResolver();
  test::ScopedMockNetworkChangeNotifier notifier;
  CreateSerialResolver();  // To guarantee order of resolutions.
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_WIFI);
  ChangeDnsConfig(CreateValidDnsConfig());

  std::string server("https://dnsserver.example.net/dns-query{?dns}");
  DnsConfigOverrides overrides;
  overrides.dns_over_https_config = *DnsOverHttpsConfig::FromString(server);
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);
  const auto* config = mock_dns_client_->GetEffectiveConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(overrides.dns_over_https_config, config->doh_config);
  EXPECT_EQ(SecureDnsMode::kAutomatic, config->secure_dns_mode);
}

TEST_F(HostResolverManagerDnsTest, AddDnsOverHttpsServerBeforeConfig) {
  DestroyResolver();
  test::ScopedMockNetworkChangeNotifier notifier;
  CreateSerialResolver();  // To guarantee order of resolutions.
  std::string server("https://dnsserver.example.net/dns-query{?dns}");
  DnsConfigOverrides overrides;
  overrides.dns_over_https_config = *DnsOverHttpsConfig::FromString(server);
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_WIFI);
  ChangeDnsConfig(CreateValidDnsConfig());

  const auto* config = mock_dns_client_->GetEffectiveConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(overrides.dns_over_https_config, config->doh_config);
  EXPECT_EQ(SecureDnsMode::kAutomatic, config->secure_dns_mode);
}

TEST_F(HostResolverManagerDnsTest, AddDnsOverHttpsServerBeforeClient) {
  DestroyResolver();
  test::ScopedMockNetworkChangeNotifier notifier;
  CreateSerialResolver();  // To guarantee order of resolutions.
  std::string server("https://dnsserver.example.net/dns-query{?dns}");
  DnsConfigOverrides overrides;
  overrides.dns_over_https_config = *DnsOverHttpsConfig::FromString(server);
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_WIFI);
  ChangeDnsConfig(CreateValidDnsConfig());

  const auto* config = mock_dns_client_->GetEffectiveConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(overrides.dns_over_https_config, config->doh_config);
  EXPECT_EQ(SecureDnsMode::kAutomatic, config->secure_dns_mode);
}

TEST_F(HostResolverManagerDnsTest, AddDnsOverHttpsServerAndThenRemove) {
  DestroyResolver();
  test::ScopedMockNetworkChangeNotifier notifier;
  CreateSerialResolver();  // To guarantee order of resolutions.
  std::string server("https://dns.example.com/");
  DnsConfigOverrides overrides;
  overrides.dns_over_https_config = *DnsOverHttpsConfig::FromString(server);
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_WIFI);
  DnsConfig network_dns_config = CreateValidDnsConfig();
  network_dns_config.doh_config = {};
  ChangeDnsConfig(network_dns_config);

  const auto* config = mock_dns_client_->GetEffectiveConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(overrides.dns_over_https_config, config->doh_config);
  EXPECT_EQ(SecureDnsMode::kAutomatic, config->secure_dns_mode);

  resolver_->SetDnsConfigOverrides(DnsConfigOverrides());
  config = mock_dns_client_->GetEffectiveConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(0u, config->doh_config.servers().size());
  EXPECT_EQ(SecureDnsMode::kOff, config->secure_dns_mode);
}

// Basic test socket factory that allows creation of UDP sockets, but those
// sockets are mocks with no data and are not expected to be usable.
class AlwaysFailSocketFactory : public MockClientSocketFactory {
 public:
  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    return std::make_unique<MockUDPClientSocket>();
  }
};

class TestDnsObserver : public NetworkChangeNotifier::DNSObserver {
 public:
  void OnDNSChanged() override { ++dns_changed_calls_; }

  int dns_changed_calls() const { return dns_changed_calls_; }

 private:
  int dns_changed_calls_ = 0;
};

// Built-in client and config overrides not available on iOS.
#if !BUILDFLAG(IS_IOS)
TEST_F(HostResolverManagerDnsTest, SetDnsConfigOverrides) {
  test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  TestDnsObserver config_observer;
  NetworkChangeNotifier::AddDNSObserver(&config_observer);

  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  DnsConfig original_config = CreateValidDnsConfig();
  original_config.hosts = {
      {DnsHostsKey("host", ADDRESS_FAMILY_IPV4), IPAddress(192, 168, 1, 1)}};
  ChangeDnsConfig(original_config);

  // Confirm pre-override state.
  ASSERT_EQ(original_config, *client_ptr->GetEffectiveConfig());

  DnsConfigOverrides overrides;
  const std::vector<IPEndPoint> nameservers = {
      CreateExpected("192.168.0.1", 92)};
  overrides.nameservers = nameservers;
  overrides.dns_over_tls_active = true;
  const std::string dns_over_tls_hostname = "dns.example.com";
  overrides.dns_over_tls_hostname = dns_over_tls_hostname;
  const std::vector<std::string> search = {"str"};
  overrides.search = search;
  overrides.append_to_multi_label_name = false;
  const int ndots = 5;
  overrides.ndots = ndots;
  const base::TimeDelta fallback_period = base::Seconds(10);
  overrides.fallback_period = fallback_period;
  const int attempts = 20;
  overrides.attempts = attempts;
  const int doh_attempts = 19;
  overrides.doh_attempts = doh_attempts;
  overrides.rotate = true;
  overrides.use_local_ipv6 = true;
  auto doh_config = *DnsOverHttpsConfig::FromString("https://dns.example.com/");
  overrides.dns_over_https_config = doh_config;
  const SecureDnsMode secure_dns_mode = SecureDnsMode::kSecure;
  overrides.secure_dns_mode = secure_dns_mode;
  overrides.allow_dns_over_https_upgrade = true;
  overrides.clear_hosts = true;

  // This test is expected to test overriding all fields.
  EXPECT_TRUE(overrides.OverridesEverything());

  EXPECT_EQ(0, config_observer.dns_changed_calls());

  resolver_->SetDnsConfigOverrides(overrides);

  const DnsConfig* overridden_config = client_ptr->GetEffectiveConfig();
  ASSERT_TRUE(overridden_config);
  EXPECT_EQ(nameservers, overridden_config->nameservers);
  EXPECT_TRUE(overridden_config->dns_over_tls_active);
  EXPECT_EQ(dns_over_tls_hostname, overridden_config->dns_over_tls_hostname);
  EXPECT_EQ(search, overridden_config->search);
  EXPECT_FALSE(overridden_config->append_to_multi_label_name);
  EXPECT_EQ(ndots, overridden_config->ndots);
  EXPECT_EQ(fallback_period, overridden_config->fallback_period);
  EXPECT_EQ(attempts, overridden_config->attempts);
  EXPECT_EQ(doh_attempts, overridden_config->doh_attempts);
  EXPECT_TRUE(overridden_config->rotate);
  EXPECT_TRUE(overridden_config->use_local_ipv6);
  EXPECT_EQ(doh_config, overridden_config->doh_config);
  EXPECT_EQ(secure_dns_mode, overridden_config->secure_dns_mode);
  EXPECT_TRUE(overridden_config->allow_dns_over_https_upgrade);
  EXPECT_THAT(overridden_config->hosts, testing::IsEmpty());

  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(1, config_observer.dns_changed_calls());

  NetworkChangeNotifier::RemoveDNSObserver(&config_observer);
}

TEST_F(HostResolverManagerDnsTest,
       SetDnsConfigOverrides_OverrideEverythingCreation) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  // Confirm pre-override state.
  ASSERT_EQ(original_config, *client_ptr->GetEffectiveConfig());
  ASSERT_FALSE(original_config.Equals(DnsConfig()));

  DnsConfigOverrides overrides =
      DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  EXPECT_TRUE(overrides.OverridesEverything());

  // Ensure config is valid by setting a nameserver.
  std::vector<IPEndPoint> nameservers = {CreateExpected("1.2.3.4", 50)};
  overrides.nameservers = nameservers;
  EXPECT_TRUE(overrides.OverridesEverything());

  resolver_->SetDnsConfigOverrides(overrides);

  DnsConfig expected;
  expected.nameservers = nameservers;
  EXPECT_THAT(client_ptr->GetEffectiveConfig(), testing::Pointee(expected));
}

TEST_F(HostResolverManagerDnsTest, SetDnsConfigOverrides_PartialOverride) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  // Confirm pre-override state.
  ASSERT_EQ(original_config, *client_ptr->GetEffectiveConfig());

  DnsConfigOverrides overrides;
  const std::vector<IPEndPoint> nameservers = {
      CreateExpected("192.168.0.2", 192)};
  overrides.nameservers = nameservers;
  overrides.rotate = true;
  EXPECT_FALSE(overrides.OverridesEverything());

  resolver_->SetDnsConfigOverrides(overrides);

  const DnsConfig* overridden_config = client_ptr->GetEffectiveConfig();
  ASSERT_TRUE(overridden_config);
  EXPECT_EQ(nameservers, overridden_config->nameservers);
  EXPECT_EQ(original_config.search, overridden_config->search);
  EXPECT_EQ(original_config.hosts, overridden_config->hosts);
  EXPECT_TRUE(overridden_config->append_to_multi_label_name);
  EXPECT_EQ(original_config.ndots, overridden_config->ndots);
  EXPECT_EQ(original_config.fallback_period,
            overridden_config->fallback_period);
  EXPECT_EQ(original_config.attempts, overridden_config->attempts);
  EXPECT_TRUE(overridden_config->rotate);
  EXPECT_FALSE(overridden_config->use_local_ipv6);
  EXPECT_EQ(original_config.doh_config, overridden_config->doh_config);
  EXPECT_EQ(original_config.secure_dns_mode,
            overridden_config->secure_dns_mode);
}

// Test that overridden configs are reapplied over a changed underlying system
// config.
TEST_F(HostResolverManagerDnsTest, SetDnsConfigOverrides_NewConfig) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  // Confirm pre-override state.
  ASSERT_EQ(original_config, *client_ptr->GetEffectiveConfig());

  DnsConfigOverrides overrides;
  const std::vector<IPEndPoint> nameservers = {
      CreateExpected("192.168.0.2", 192)};
  overrides.nameservers = nameservers;

  resolver_->SetDnsConfigOverrides(overrides);
  ASSERT_TRUE(client_ptr->GetEffectiveConfig());
  ASSERT_EQ(nameservers, client_ptr->GetEffectiveConfig()->nameservers);

  DnsConfig new_config = original_config;
  new_config.attempts = 103;
  ASSERT_NE(nameservers, new_config.nameservers);
  ChangeDnsConfig(new_config);

  const DnsConfig* overridden_config = client_ptr->GetEffectiveConfig();
  ASSERT_TRUE(overridden_config);
  EXPECT_EQ(nameservers, overridden_config->nameservers);
  EXPECT_EQ(new_config.attempts, overridden_config->attempts);
}

TEST_F(HostResolverManagerDnsTest, SetDnsConfigOverrides_ClearOverrides) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  DnsConfigOverrides overrides;
  overrides.attempts = 245;
  resolver_->SetDnsConfigOverrides(overrides);

  ASSERT_THAT(client_ptr->GetEffectiveConfig(),
              testing::Not(testing::Pointee(original_config)));

  resolver_->SetDnsConfigOverrides(DnsConfigOverrides());
  EXPECT_THAT(client_ptr->GetEffectiveConfig(),
              testing::Pointee(original_config));
}

TEST_F(HostResolverManagerDnsTest, SetDnsConfigOverrides_NoChange) {
  test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  TestDnsObserver config_observer;
  NetworkChangeNotifier::AddDNSObserver(&config_observer);

  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  // Confirm pre-override state.
  ASSERT_EQ(original_config, *client_ptr->GetEffectiveConfig());

  DnsConfigOverrides overrides;
  overrides.nameservers = original_config.nameservers;

  EXPECT_EQ(0, config_observer.dns_changed_calls());

  resolver_->SetDnsConfigOverrides(overrides);
  EXPECT_THAT(client_ptr->GetEffectiveConfig(),
              testing::Pointee(original_config));

  base::RunLoop().RunUntilIdle();  // Notifications are async.
  EXPECT_EQ(0,
            config_observer.dns_changed_calls());  // No expected notification

  NetworkChangeNotifier::RemoveDNSObserver(&config_observer);
}

// No effect or notifications expected using partial overrides without a base
// system config.
TEST_F(HostResolverManagerDnsTest, NoBaseConfig_PartialOverrides) {
  test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  TestDnsObserver config_observer;
  NetworkChangeNotifier::AddDNSObserver(&config_observer);

  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  client_ptr->SetSystemConfig(std::nullopt);

  DnsConfigOverrides overrides;
  overrides.nameservers.emplace({CreateExpected("192.168.0.3", 193)});
  resolver_->SetDnsConfigOverrides(overrides);
  base::RunLoop().RunUntilIdle();  // Potential notifications are async.

  EXPECT_FALSE(client_ptr->GetEffectiveConfig());
  EXPECT_EQ(0, config_observer.dns_changed_calls());

  NetworkChangeNotifier::RemoveDNSObserver(&config_observer);
}

TEST_F(HostResolverManagerDnsTest, NoBaseConfig_OverridesEverything) {
  test::ScopedMockNetworkChangeNotifier mock_network_change_notifier;
  TestDnsObserver config_observer;
  NetworkChangeNotifier::AddDNSObserver(&config_observer);

  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  client_ptr->SetSystemConfig(std::nullopt);

  DnsConfigOverrides overrides =
      DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  const std::vector<IPEndPoint> nameservers = {
      CreateExpected("192.168.0.4", 194)};
  overrides.nameservers = nameservers;
  resolver_->SetDnsConfigOverrides(overrides);
  base::RunLoop().RunUntilIdle();  // Notifications are async.

  DnsConfig expected;
  expected.nameservers = nameservers;

  EXPECT_THAT(client_ptr->GetEffectiveConfig(), testing::Pointee(expected));
  EXPECT_EQ(1, config_observer.dns_changed_calls());

  NetworkChangeNotifier::RemoveDNSObserver(&config_observer);
}

TEST_F(HostResolverManagerDnsTest, DohMapping) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  ChangeDnsConfig(original_config);

  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  auto expected_doh_config = *DnsOverHttpsConfig::FromTemplatesForTesting(
      {"https://chrome.cloudflare-dns.com/dns-query",
       "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
       "https://doh.cleanbrowsing.org/doh/security-filter{?dns}"});
  EXPECT_EQ(expected_doh_config, fetched_config->doh_config);
}

TEST_F(HostResolverManagerDnsTest, DohMappingDisabled) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  original_config.allow_dns_over_https_upgrade = false;
  ChangeDnsConfig(original_config);

  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  EXPECT_THAT(fetched_config->doh_config.servers(), IsEmpty());
}

TEST_F(HostResolverManagerDnsTest, DohMappingModeIneligibleForUpgrade) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  original_config.secure_dns_mode = SecureDnsMode::kSecure;
  ChangeDnsConfig(original_config);

  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  EXPECT_THAT(fetched_config->doh_config.servers(), IsEmpty());
}

TEST_F(HostResolverManagerDnsTest,
       DohMappingUnhandledOptionsIneligibleForUpgrade) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  original_config.unhandled_options = true;
  ChangeDnsConfig(original_config);

  EXPECT_FALSE(client_ptr->GetEffectiveConfig());
}

TEST_F(HostResolverManagerDnsTest, DohMappingWithExclusion) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          GetDohProviderEntryForTesting("CleanBrowsingSecure").feature.get(),
          GetDohProviderEntryForTesting("Cloudflare").feature.get()});

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  ChangeDnsConfig(original_config);

  // A DoH upgrade should be attempted on the DNS servers in the config, but
  // only for permitted providers.
  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  auto expected_doh_config = *DnsOverHttpsConfig::FromString(
      "https://doh.cleanbrowsing.org/doh/family-filter{?dns}");
  EXPECT_EQ(expected_doh_config, fetched_config->doh_config);
}

TEST_F(HostResolverManagerDnsTest, DohMappingIgnoredIfTemplateSpecified) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  ChangeDnsConfig(original_config);

  // If the overrides contains DoH servers, no DoH upgrade should be attempted.
  DnsConfigOverrides overrides;
  const auto dns_over_https_config_override =
      *DnsOverHttpsConfig::FromString("https://doh.server.override.com/");
  overrides.dns_over_https_config = dns_over_https_config_override;
  resolver_->SetDnsConfigOverrides(overrides);
  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  EXPECT_EQ(dns_over_https_config_override, fetched_config->doh_config);
}

TEST_F(HostResolverManagerDnsTest,
       DohMappingUnhandledOptionsAndTemplateSpecified) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  original_config.unhandled_options = true;
  ChangeDnsConfig(original_config);

  // If the overrides contains DoH servers, no DoH upgrade should be attempted.
  DnsConfigOverrides overrides;
  const auto dns_over_https_config_override =
      *DnsOverHttpsConfig::FromString("https://doh.server.override.com/");
  overrides.dns_over_https_config = dns_over_https_config_override;
  resolver_->SetDnsConfigOverrides(overrides);
  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_TRUE(fetched_config->nameservers.empty());
  EXPECT_FALSE(client_ptr->CanUseInsecureDnsTransactions());
  EXPECT_EQ(dns_over_https_config_override, fetched_config->doh_config);
  EXPECT_TRUE(client_ptr->CanUseSecureDnsTransactions());
}

TEST_F(HostResolverManagerDnsTest, DohMappingWithAutomaticDot) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  original_config.dns_over_tls_active = true;
  ChangeDnsConfig(original_config);

  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  auto expected_doh_config = *DnsOverHttpsConfig::FromTemplatesForTesting(
      {"https://chrome.cloudflare-dns.com/dns-query",
       "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
       "https://doh.cleanbrowsing.org/doh/security-filter{?dns}"});
  EXPECT_EQ(expected_doh_config, fetched_config->doh_config);
}

TEST_F(HostResolverManagerDnsTest, DohMappingWithStrictDot) {
  // Use a real DnsClient to test config-handling behavior.
  AlwaysFailSocketFactory socket_factory;
  auto client = DnsClient::CreateClient(nullptr /* net_log */);
  DnsClient* client_ptr = client.get();
  SetDnsClient(std::move(client));

  // Create a DnsConfig containing IP addresses associated with Cloudflare,
  // SafeBrowsing family filter, SafeBrowsing security filter, and other IPs
  // not associated with hardcoded DoH services.
  DnsConfig original_config = CreateUpgradableDnsConfig();
  original_config.secure_dns_mode = SecureDnsMode::kAutomatic;
  original_config.dns_over_tls_active = true;

  // Google DoT hostname
  original_config.dns_over_tls_hostname = "dns.google";
  ChangeDnsConfig(original_config);
  const DnsConfig* fetched_config = client_ptr->GetEffectiveConfig();
  EXPECT_EQ(original_config.nameservers, fetched_config->nameservers);
  auto expected_doh_config =
      *DnsOverHttpsConfig::FromString("https://dns.google/dns-query{?dns}");
  EXPECT_EQ(expected_doh_config, fetched_config->doh_config);
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(HostResolverManagerDnsTest, FlushCacheOnDnsConfigOverridesChange) {
  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters local_source_parameters;
  local_source_parameters.source = HostResolverSource::LOCAL_ONLY;

  // Populate cache.
  ResolveHostResponseHelper initial_response(resolver_->CreateRequest(
      HostPortPair("ok", 70), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(initial_response.result_error(), IsOk());

  // Confirm result now cached.
  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("ok", 75), NetworkAnonymizationKey(), NetLogWithSource(),
      local_source_parameters, resolve_context_.get()));
  ASSERT_THAT(cached_response.result_error(), IsOk());
  ASSERT_TRUE(cached_response.request()->GetStaleInfo());

  // Flush cache by triggering a DnsConfigOverrides change.
  DnsConfigOverrides overrides;
  overrides.attempts = 4;
  resolver_->SetDnsConfigOverrides(overrides);

  // Expect no longer cached
  ResolveHostResponseHelper flushed_response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      local_source_parameters, resolve_context_.get()));
  EXPECT_THAT(flushed_response.result_error(), IsError(ERR_DNS_CACHE_MISS));
}

TEST_F(HostResolverManagerDnsTest,
       FlushContextSessionDataOnDnsConfigOverridesChange) {
  ChangeDnsConfig(CreateValidDnsConfig());

  DnsSession* session_before = mock_dns_client_->GetCurrentSession();
  resolve_context_->RecordServerSuccess(
      0u /* server_index */, true /* is_doh_server */, session_before);
  ASSERT_TRUE(resolve_context_->GetDohServerAvailability(0u, session_before));

  // Flush data by triggering a DnsConfigOverrides change.
  DnsConfigOverrides overrides;
  overrides.attempts = 4;
  resolver_->SetDnsConfigOverrides(overrides);

  DnsSession* session_after = mock_dns_client_->GetCurrentSession();
  EXPECT_NE(session_before, session_after);

  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(0u, session_after));

  // Confirm new session is in use.
  resolve_context_->RecordServerSuccess(
      0u /* server_index */, true /* is_doh_server */, session_after);
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(0u, session_after));
}

// Test that even when using config overrides, a change to the base system
// config cancels pending requests.
TEST_F(HostResolverManagerDnsTest, CancellationOnBaseConfigChange) {
  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  DnsConfigOverrides overrides;
  overrides.nameservers.emplace({CreateExpected("123.123.123.123", 80)});
  ASSERT_FALSE(overrides.OverridesEverything());
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());

  DnsConfig new_config = original_config;
  new_config.attempts = 103;
  ChangeDnsConfig(new_config);

  EXPECT_THAT(response.result_error(), IsError(ERR_NETWORK_CHANGED));
}

// Test that when all configuration is overridden, system configuration changes
// do not cancel requests.
TEST_F(HostResolverManagerDnsTest,
       CancellationOnBaseConfigChange_OverridesEverything) {
  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  DnsConfigOverrides overrides =
      DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  overrides.nameservers.emplace({CreateExpected("123.123.123.123", 80)});
  ASSERT_TRUE(overrides.OverridesEverything());
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());

  DnsConfig new_config = original_config;
  new_config.attempts = 103;
  ChangeDnsConfig(new_config);

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsOk());
}

// Test that in-progress queries are cancelled on applying new DNS config
// overrides, same as receiving a new DnsConfig from the system.
TEST_F(HostResolverManagerDnsTest, CancelQueriesOnSettingOverrides) {
  ChangeDnsConfig(CreateValidDnsConfig());
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());

  DnsConfigOverrides overrides;
  overrides.attempts = 123;
  resolver_->SetDnsConfigOverrides(overrides);

  EXPECT_THAT(response.result_error(), IsError(ERR_NETWORK_CHANGED));
}

// Queries should not be cancelled if equal overrides are set.
TEST_F(HostResolverManagerDnsTest,
       CancelQueriesOnSettingOverrides_SameOverrides) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.attempts = 123;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());

  resolver_->SetDnsConfigOverrides(overrides);

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsOk());
}

// Test that in-progress queries are cancelled on clearing DNS config overrides,
// same as receiving a new DnsConfig from the system.
TEST_F(HostResolverManagerDnsTest, CancelQueriesOnClearingOverrides) {
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.attempts = 123;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());

  resolver_->SetDnsConfigOverrides(DnsConfigOverrides());

  EXPECT_THAT(response.result_error(), IsError(ERR_NETWORK_CHANGED));
}

// Queries should not be cancelled on clearing overrides if there were not any
// overrides.
TEST_F(HostResolverManagerDnsTest,
       CancelQueriesOnClearingOverrides_NoOverrides) {
  ChangeDnsConfig(CreateValidDnsConfig());
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("4slow_ok", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));
  ASSERT_FALSE(response.complete());

  resolver_->SetDnsConfigOverrides(DnsConfigOverrides());

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsOk());
}

TEST_F(HostResolverManagerDnsTest,
       FlushContextSessionDataOnSystemConfigChange) {
  DnsConfig original_config = CreateValidDnsConfig();
  ChangeDnsConfig(original_config);

  DnsSession* session_before = mock_dns_client_->GetCurrentSession();
  resolve_context_->RecordServerSuccess(
      0u /* server_index */, true /* is_doh_server */, session_before);
  ASSERT_TRUE(resolve_context_->GetDohServerAvailability(0u, session_before));

  // Flush data by triggering a config change.
  DnsConfig new_config = original_config;
  new_config.attempts = 103;
  ChangeDnsConfig(new_config);

  DnsSession* session_after = mock_dns_client_->GetCurrentSession();
  EXPECT_NE(session_before, session_after);

  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(0u, session_after));

  // Confirm new session is in use.
  resolve_context_->RecordServerSuccess(
      0u /* server_index */, true /* is_doh_server */, session_after);
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(0u, session_after));
}

TEST_F(HostResolverManagerDnsTest, TxtQuery) {
  // Simulate two separate DNS records, each with multiple strings.
  std::vector<std::string> foo_records = {"foo1", "foo2", "foo3"};
  std::vector<std::string> bar_records = {"bar1", "bar2"};
  std::vector<std::vector<std::string>> text_records = {foo_records,
                                                        bar_records};

  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeTXT, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsTextResponse(
                         "host", std::move(text_records))),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Order between separate DNS records is undefined, but each record should
  // stay in order as that order may be meaningful.
  ASSERT_THAT(response.request()->GetTextResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "foo1", "foo2", "foo3", "bar1", "bar2")));
  const std::vector<std::string>* results =
      response.request()->GetTextResults();
  EXPECT_NE(results->end(), base::ranges::search(*results, foo_records));
  EXPECT_NE(results->end(), base::ranges::search(*results, bar_records));

  // Expect result to be cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 1u);
  parameters.source = HostResolverSource::LOCAL_ONLY;
  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsOk());
  ASSERT_THAT(cached_response.request()->GetTextResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "foo1", "foo2", "foo3", "bar1", "bar2")));
  results = cached_response.request()->GetTextResults();
  EXPECT_NE(results->end(), base::ranges::search(*results, foo_records));
  EXPECT_NE(results->end(), base::ranges::search(*results, bar_records));
}

TEST_F(HostResolverManagerDnsTest, TxtQueryRejectsIpLiteral) {
  MockDnsClientRuleList rules;

  // Entry that would resolve if DNS is mistakenly queried to ensure that does
  // not happen.
  rules.emplace_back("8.8.8.8", dns_protocol::kTypeTXT, /*secure=*/false,
                     MockDnsClientRule::Result(
                         BuildTestDnsTextResponse("8.8.8.8", {{"text"}})),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("8.8.8.8", 108), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Test that TXT records can be extracted from a response that also contains
// unrecognized record types.
TEST_F(HostResolverManagerDnsTest, TxtQuery_MixedWithUnrecognizedType) {
  std::vector<std::string> text_strings = {"foo"};

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          "host", dns_protocol::kTypeTXT,
          {BuildTestDnsRecord("host", 3u /* type */, "fake rdata 1"),
           BuildTestTextRecord("host", std::move(text_strings)),
           BuildTestDnsRecord("host", 3u /* type */, "fake rdata 2")})),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  EXPECT_THAT(response.request()->GetTextResults(),
              testing::Pointee(testing::ElementsAre("foo")));
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_InvalidConfig) {
  set_allow_fallback_to_systemtask(false);
  // Set empty DnsConfig.
  InvalidateDnsConfig();

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_NonexistentDomain) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kNoDomain),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result to be cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 1u);
  parameters.source = HostResolverSource::LOCAL_ONLY;
  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(cached_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_Failure) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_Timeout) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kTimeout),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_TIMED_OUT));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_Empty) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result to be cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 1u);
  parameters.source = HostResolverSource::LOCAL_ONLY;
  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(cached_response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(cached_response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_Malformed) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_MismatchedName) {
  std::vector<std::vector<std::string>> text_records = {{"text"}};
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeTXT, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsTextResponse(
                         "host", std::move(text_records), "not.host")),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, TxtQuery_WrongType) {
  // Respond to a TXT query with an A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeTXT, false /* secure */,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          "host", dns_protocol::kTypeTXT,
          {BuildTestAddressRecord("host", IPAddress(1, 2, 3, 4))})),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  // Responses for the wrong type should be ignored.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest,
       TxtInsecureQueryDisallowedWhenAdditionalTypesDisallowed) {
  const std::string kName = "txt.test";

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kOff;
  resolver_->SetDnsConfigOverrides(overrides);
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/false);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kName, 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  // No non-local work is done, so ERR_DNS_CACHE_MISS is the result.
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Same as TxtQuery except we specify DNS HostResolverSource instead of relying
// on automatic determination.  Expect same results since DNS should be what we
// automatically determine, but some slightly different logic paths are
// involved.
TEST_F(HostResolverManagerDnsTest, TxtDnsQuery) {
  // Simulate two separate DNS records, each with multiple strings.
  std::vector<std::string> foo_records = {"foo1", "foo2", "foo3"};
  std::vector<std::string> bar_records = {"bar1", "bar2"};
  std::vector<std::vector<std::string>> text_records = {foo_records,
                                                        bar_records};

  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeTXT, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsTextResponse(
                         "host", std::move(text_records))),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::DNS;
  parameters.dns_query_type = DnsQueryType::TXT;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Order between separate DNS records is undefined, but each record should
  // stay in order as that order may be meaningful.
  ASSERT_THAT(response.request()->GetTextResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "foo1", "foo2", "foo3", "bar1", "bar2")));
  const std::vector<std::string>* results =
      response.request()->GetTextResults();
  EXPECT_NE(results->end(), base::ranges::search(*results, foo_records));
  EXPECT_NE(results->end(), base::ranges::search(*results, bar_records));

  // Expect result to be cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 1u);
  ResolveHostResponseHelper cached_response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(cached_response.result_error(), IsOk());
  EXPECT_TRUE(cached_response.request()->GetStaleInfo());
  ASSERT_THAT(cached_response.request()->GetTextResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  "foo1", "foo2", "foo3", "bar1", "bar2")));
  results = cached_response.request()->GetTextResults();
  EXPECT_NE(results->end(), base::ranges::search(*results, foo_records));
  EXPECT_NE(results->end(), base::ranges::search(*results, bar_records));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery) {
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypePTR, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsPointerResponse(
                         "host", {"foo.com", "bar.com"})),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Order between separate records is undefined.
  EXPECT_THAT(response.request()->GetHostnameResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  HostPortPair("foo.com", 108), HostPortPair("bar.com", 108))));
}

TEST_F(HostResolverManagerDnsTest, PtrQueryRejectsIpLiteral) {
  MockDnsClientRuleList rules;

  // Entry that would resolve if DNS is mistakenly queried to ensure that does
  // not happen.
  rules.emplace_back("8.8.8.8", dns_protocol::kTypePTR, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsPointerResponse(
                         "8.8.8.8", {"foo.com", "bar.com"})),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("8.8.8.8", 108), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQueryHandlesReverseIpLookup) {
  const char kHostname[] = "8.8.8.8.in-addr.arpa";

  MockDnsClientRuleList rules;
  rules.emplace_back(kHostname, dns_protocol::kTypePTR, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsPointerResponse(
                         kHostname, {"dns.google.test", "foo.test"})),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHostname, 108), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Order between separate records is undefined.
  EXPECT_THAT(response.request()->GetHostnameResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  HostPortPair("dns.google.test", 108),
                  HostPortPair("foo.test", 108))));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_NonexistentDomain) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypePTR, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kNoDomain),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_Failure) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypePTR, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_Timeout) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypePTR, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kTimeout),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_TIMED_OUT));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_Empty) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypePTR, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_Malformed) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypePTR, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_MismatchedName) {
  std::vector<std::string> ptr_records = {{"foo.com"}};
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypePTR, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsPointerResponse(
                         "host", std::move(ptr_records), "not.host")),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, PtrQuery_WrongType) {
  // Respond to a TXT query with an A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypePTR, false /* secure */,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          "host", dns_protocol::kTypePTR,
          {BuildTestAddressRecord("host", IPAddress(1, 2, 3, 4))})),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  // Responses for the wrong type should be ignored.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       PtrInsecureQueryDisallowedWhenAdditionalTypesDisallowed) {
  const std::string kName = "ptr.test";

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kOff;
  resolver_->SetDnsConfigOverrides(overrides);
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/false);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kName, 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  // No non-local work is done, so ERR_DNS_CACHE_MISS is the result.
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Same as PtrQuery except we specify DNS HostResolverSource instead of relying
// on automatic determination.  Expect same results since DNS should be what we
// automatically determine, but some slightly different logic paths are
// involved.
TEST_F(HostResolverManagerDnsTest, PtrDnsQuery) {
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypePTR, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsPointerResponse(
                         "host", {"foo.com", "bar.com"})),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::DNS;
  parameters.dns_query_type = DnsQueryType::PTR;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Order between separate records is undefined.
  EXPECT_THAT(response.request()->GetHostnameResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  HostPortPair("foo.com", 108), HostPortPair("bar.com", 108))));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery) {
  const TestServiceRecord kRecord1 = {2, 3, 1223, "foo.com"};
  const TestServiceRecord kRecord2 = {5, 10, 80, "bar.com"};
  const TestServiceRecord kRecord3 = {5, 1, 5, "google.com"};
  const TestServiceRecord kRecord4 = {2, 100, 12345, "chromium.org"};
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeSRV, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsServiceResponse(
                         "host", {kRecord1, kRecord2, kRecord3, kRecord4})),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect ordered by priority, and random within a priority.
  const std::vector<HostPortPair>* results =
      response.request()->GetHostnameResults();
  ASSERT_THAT(
      results,
      testing::Pointee(testing::UnorderedElementsAre(
          HostPortPair("foo.com", 1223), HostPortPair("bar.com", 80),
          HostPortPair("google.com", 5), HostPortPair("chromium.org", 12345))));
  auto priority2 =
      std::vector<HostPortPair>(results->begin(), results->begin() + 2);
  EXPECT_THAT(priority2, testing::UnorderedElementsAre(
                             HostPortPair("foo.com", 1223),
                             HostPortPair("chromium.org", 12345)));
  auto priority5 =
      std::vector<HostPortPair>(results->begin() + 2, results->end());
  EXPECT_THAT(priority5,
              testing::UnorderedElementsAre(HostPortPair("bar.com", 80),
                                            HostPortPair("google.com", 5)));
}

TEST_F(HostResolverManagerDnsTest, SrvQueryRejectsIpLiteral) {
  MockDnsClientRuleList rules;

  // Entry that would resolve if DNS is mistakenly queried to ensure that does
  // not happen.
  rules.emplace_back("8.8.8.8", dns_protocol::kTypeSRV, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsServiceResponse(
                         "8.8.8.8", {{/*priority=*/4, /*weight=*/0, /*port=*/90,
                                      /*target=*/"google.test"}})),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("8.8.8.8", 108), NetworkAnonymizationKey(),
      NetLogWithSource(), parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// 0-weight services are allowed. Ensure that we can handle such records,
// especially the case where all entries have weight 0.
TEST_F(HostResolverManagerDnsTest, SrvQuery_ZeroWeight) {
  const TestServiceRecord kRecord1 = {5, 0, 80, "bar.com"};
  const TestServiceRecord kRecord2 = {5, 0, 5, "google.com"};
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeSRV, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsServiceResponse(
                         "host", {kRecord1, kRecord2})),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect ordered by priority, and random within a priority.
  EXPECT_THAT(response.request()->GetHostnameResults(),
              testing::Pointee(testing::UnorderedElementsAre(
                  HostPortPair("bar.com", 80), HostPortPair("google.com", 5))));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_NonexistentDomain) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeSRV, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kNoDomain),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_Failure) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeSRV, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_Timeout) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeSRV, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kTimeout),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_TIMED_OUT));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_Empty) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeSRV, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_Malformed) {
  // Setup fallback to confirm it is not used for non-address results.
  set_allow_fallback_to_systemtask(true);
  proc_->AddRuleForAllFamilies("host", "192.168.1.102");
  proc_->SignalMultiple(1u);

  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeSRV, false /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_MismatchedName) {
  std::vector<TestServiceRecord> srv_records = {{1, 2, 3, "foo.com"}};
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeSRV, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsServiceResponse(
                         "host", std::move(srv_records), "not.host")),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, SrvQuery_WrongType) {
  // Respond to a SRV query with an A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      "host", dns_protocol::kTypeSRV, false /* secure */,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          "host", dns_protocol::kTypeSRV,
          {BuildTestAddressRecord("host", IPAddress(1, 2, 3, 4))})),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  // Responses for the wrong type should be ignored.
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       SrvInsecureQueryDisallowedWhenAdditionalTypesDisallowed) {
  const std::string kName = "srv.test";

  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kOff;
  resolver_->SetDnsConfigOverrides(overrides);
  resolver_->SetInsecureDnsClientEnabled(
      /*enabled=*/true,
      /*additional_dns_types_enabled=*/false);

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kName, 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  // No non-local work is done, so ERR_DNS_CACHE_MISS is the result.
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_CACHE_MISS));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Same as SrvQuery except we specify DNS HostResolverSource instead of relying
// on automatic determination.  Expect same results since DNS should be what we
// automatically determine, but some slightly different logic paths are
// involved.
TEST_F(HostResolverManagerDnsTest, SrvDnsQuery) {
  const TestServiceRecord kRecord1 = {2, 3, 1223, "foo.com"};
  const TestServiceRecord kRecord2 = {5, 10, 80, "bar.com"};
  const TestServiceRecord kRecord3 = {5, 1, 5, "google.com"};
  const TestServiceRecord kRecord4 = {2, 100, 12345, "chromium.org"};
  MockDnsClientRuleList rules;
  rules.emplace_back("host", dns_protocol::kTypeSRV, false /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsServiceResponse(
                         "host", {kRecord1, kRecord2, kRecord3, kRecord4})),
                     false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.source = HostResolverSource::DNS;
  parameters.dns_query_type = DnsQueryType::SRV;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host", 108), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect ordered by priority, and random within a priority.
  const std::vector<HostPortPair>* results =
      response.request()->GetHostnameResults();
  ASSERT_THAT(
      results,
      testing::Pointee(testing::UnorderedElementsAre(
          HostPortPair("foo.com", 1223), HostPortPair("bar.com", 80),
          HostPortPair("google.com", 5), HostPortPair("chromium.org", 12345))));
  auto priority2 =
      std::vector<HostPortPair>(results->begin(), results->begin() + 2);
  EXPECT_THAT(priority2, testing::UnorderedElementsAre(
                             HostPortPair("foo.com", 1223),
                             HostPortPair("chromium.org", 12345)));
  auto priority5 =
      std::vector<HostPortPair>(results->begin() + 2, results->end());
  EXPECT_THAT(priority5,
              testing::UnorderedElementsAre(HostPortPair("bar.com", 80),
                                            HostPortPair("google.com", 5)));
}

TEST_F(HostResolverManagerDnsTest, HttpsQuery) {
  const std::string kName = "https.test";

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::HTTPS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), parameters,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsQueryForNonStandardPort) {
  const std::string kName = "https.test";
  const std::string kExpectedQueryName = "_1111._https." + kName;

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsServiceRecord(
      kExpectedQueryName, /*priority=*/1, /*service_name=*/kName,
      /*params=*/{})};
  rules.emplace_back(
      kExpectedQueryName, dns_protocol::kTypeHttps,
      /*secure=*/false,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          kExpectedQueryName, dns_protocol::kTypeHttps, records)),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::HTTPS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 1111),
      NetworkAnonymizationKey(), NetLogWithSource(), parameters,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsQueryForHttpUpgrade) {
  const std::string kName = "https.test";

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::HTTPS;

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Test that HTTPS requests for an http host with port 443 will result in a
// transaction hostname without prepending port and scheme, despite not having
// the default port for an http host. The request host ("http://https.test:443")
// will be mapped to the equivalent https upgrade host
// ("https://https.test:443") at port 443, which is the default port for an
// https host, so port and scheme are not prefixed.
TEST_F(HostResolverManagerDnsTest, HttpsQueryForHttpUpgradeFromHttpsPort) {
  const std::string kName = "https.test";

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::HTTPS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), parameters,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsQueryForHttpUpgradeWithNonStandardPort) {
  const std::string kName = "https.test";
  const std::string kExpectedQueryName = "_1111._https." + kName;

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsServiceRecord(
      kExpectedQueryName, /*priority=*/1, /*service_name=*/kName,
      /*params=*/{})};
  rules.emplace_back(
      kExpectedQueryName, dns_protocol::kTypeHttps,
      /*secure=*/false,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          kExpectedQueryName, dns_protocol::kTypeHttps, records)),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = DnsQueryType::HTTPS;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpScheme, kName, 1111),
      NetworkAnonymizationKey(), NetLogWithSource(), parameters,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQuery) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features(features::kUseDnsHttpsSvcb);

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::IsEmpty(), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQueryWithNonstandardPort) {
  const char kName[] = "name.test";
  const char kExpectedHttpsQueryName[] = "_108._https.name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsServiceRecord(
      kExpectedHttpsQueryName, /*priority=*/1, /*service_name=*/kName,
      /*params=*/{})};
  rules.emplace_back(
      kExpectedHttpsQueryName, dns_protocol::kTypeHttps,
      /*secure=*/true,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          kExpectedHttpsQueryName, dns_protocol::kTypeHttps, records)),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 108),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::IsEmpty(), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryWithNonstandardPortAndDefaultServiceName) {
  const char kName[] = "name.test";
  const char kExpectedHttpsQueryName[] = "_108._https.name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsServiceRecord(
      kExpectedHttpsQueryName, /*priority=*/1, /*service_name=*/".",
      /*params=*/{})};
  rules.emplace_back(
      kExpectedHttpsQueryName, dns_protocol::kTypeHttps,
      /*secure=*/true,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          kExpectedHttpsQueryName, dns_protocol::kTypeHttps, records)),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 108),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  // Expect only A/AAAA results without metadata because the HTTPS service
  // target name matches the port-prefixed name which does not match the A/AAAA
  // name and is thus not supported due to requiring followup queries.
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQueryWithAlpnAndEch) {
  const char kName[] = "name.test";
  const uint8_t kEch[] = "ECH is neato!";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsServiceRecord(
      kName, /*priority=*/8, /*service_name=*/".",
      /*params=*/
      {BuildTestHttpsServiceAlpnParam({"foo1", "foo2"}),
       BuildTestHttpsServiceEchConfigParam(kEch)})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::UnorderedElementsAre(
                      "foo1", "foo2", dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::ElementsAreArray(kEch), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQueryWithNonMatchingPort) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/8, /*service_name=*/".",
                                  /*params=*/
                                  {BuildTestHttpsServicePortParam(3000)})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQueryWithMatchingPort) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/8, /*service_name=*/".",
                                  /*params=*/
                                  {BuildTestHttpsServicePortParam(443)})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::IsEmpty(), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQueryWithoutAddresses) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);

  // Will fall back to insecure due to lack of addresses.
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);

  // Will fall back to system resolver due to lack of addresses.
  proc_->AddRuleForAllFamilies("just.testing", "");
  proc_->SignalMultiple(1u);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  // No address results overrides overall result.
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No results maintained when overall error is ERR_NAME_NOT_RESOLVED (and also
  // because of the fallback to system resolver).
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, HttpsQueriedInAddressQueryButNoResults) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

// For a response where DnsTransaction can at least do its basic parsing and
// return a DnsResponse object to HostResolverManager.  See
// `UnparsableHttpsInAddressRequestIsFatal` for a response so unparsable that
// DnsTransaction couldn't do that.
TEST_F(HostResolverManagerDnsTest,
       MalformedHttpsInResponseInAddressRequestIsIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       MalformedHttpsRdataInAddressRequestIsIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, /*answers=*/
                         {BuildTestDnsRecord(kName, dns_protocol::kTypeHttps,
                                             /*rdata=*/"malformed rdata")})),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       FailedHttpsInAddressRequestIsFatalWhenFeatureEnabled) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest,
       FailedHttpsInAddressRequestIgnoredWhenFeatureDisabled) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "false"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(
    HostResolverManagerDnsTest,
    FailedHttpsInAddressRequestAfterAddressFailureIsFatalWhenFeatureEnabled) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  // Delay HTTPS result to ensure it comes after A failure.
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/false);
  // Delay AAAA result to ensure it is cancelled after A failure.
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kUnexpected),
      /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
  mock_dns_client_->CompleteDelayedTransactions();

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(
    HostResolverManagerDnsTest,
    FailedHttpsInAddressRequestAfterAddressFailureIgnoredWhenFeatureDisabled) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "false"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  // Delay HTTPS result to ensure it is cancelled after AAAA failure.
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kUnexpected),
      /*delay=*/true);
  // Delay A result to ensure it is cancelled after AAAA failure.
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kUnexpected),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/false);

  // Expect fall back to insecure due to AAAA failure.
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  base::RunLoop().RunUntilIdle();
  // Unnecessary to complete delayed transactions because they should be
  // cancelled after first failure (AAAA).
  EXPECT_TRUE(response.complete());

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_TRUE(response.request()->GetEndpointResults());
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_TRUE(response.request()->GetExperimentalResultsForTesting());
}

TEST_F(HostResolverManagerDnsTest, TimeoutHttpsInAddressRequestIsFatal) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kTimeout),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_TIMED_OUT));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, ServfailHttpsInAddressRequestIsFatal) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(
          MockDnsClientRule::ResultType::kFail,
          BuildTestDnsResponse(kName, dns_protocol::kTypeHttps, /*answers=*/{},
                               /*authority=*/{}, /*additional=*/{},
                               dns_protocol::kRcodeSERVFAIL),
          ERR_DNS_SERVER_FAILED),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

// For a response so malformed that DnsTransaction can't do its basic parsing to
// determine an RCODE and return a DnsResponse object to HostResolverManager.
// Essentially equivalent to a network error. See
// `MalformedHttpsInResponseInAddressRequestIsFatal` for a malformed response
// that can at least send a DnsResponse to HostResolverManager.
TEST_F(HostResolverManagerDnsTest, UnparsableHttpsInAddressRequestIsFatal) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(
                         MockDnsClientRule::ResultType::kFail,
                         /*response=*/std::nullopt, ERR_DNS_MALFORMED_RESPONSE),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));

  // Expect result not cached.
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, RefusedHttpsInAddressRequestIsIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       // Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(
          MockDnsClientRule::ResultType::kFail,
          BuildTestDnsResponse(kName, dns_protocol::kTypeHttps, /*answers=*/{},
                               /*authority=*/{}, /*additional=*/{},
                               dns_protocol::kRcodeREFUSED),
          ERR_DNS_SERVER_FAILED),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest, HttpsInAddressQueryForWssScheme) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kWssScheme, kName, 443),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::IsEmpty(), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, NoHttpsInAddressQueryWithoutScheme) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  // Should not be queried.
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kUnexpected),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kName, 443), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, NoHttpsInAddressQueryForNonHttpScheme) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  // Should not be queried.
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kUnexpected),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kFtpScheme, kName, 443),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryForHttpSchemeWhenUpgradeEnabled) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryForHttpSchemeWhenUpgradeEnabledWithAliasRecord) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsAliasRecord(kName, "alias.test")};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(
    HostResolverManagerDnsTest,
    HttpsInAddressQueryForHttpSchemeWhenUpgradeEnabledWithIncompatibleServiceRecord) {
  const char kName[] = "name.test";
  const uint16_t kMadeUpParam = 65300;  // From the private-use block.

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsServiceRecord(
      kName, /*priority=*/1, /*service_name=*/".",
      /*params=*/
      {BuildTestHttpsServiceMandatoryParam({kMadeUpParam}),
       {kMadeUpParam, "foo"}})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));

  // Expect incompatible HTTPS record to have no effect on results (except
  // `GetExperimentalResultsForTesting()` which returns the record
  // compatibility).
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_TRUE(response.request()->GetEndpointResults());
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              Pointee(Not(Contains(true))));
}

// Even if no addresses are received for a request, finding an HTTPS record
// should still force an HTTP->HTTPS upgrade.
TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryForHttpSchemeWhenUpgradeEnabledWithoutAddresses) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kEmpty),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, HttpsInSecureModeAddressQuery) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsAliasRecord(kName, "alias.test")};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_TRUE(response.request()->GetEndpointResults());
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInSecureModeAddressQueryForHttpScheme) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, HttpsInInsecureAddressQuery) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::IsEmpty(), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

TEST_F(HostResolverManagerDnsTest, HttpsInInsecureAddressQueryForHttpScheme) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kName, 80),
                               NetworkAnonymizationKey(), NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));
  EXPECT_THAT(response.request()->GetAddressResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, FailedHttpsInInsecureAddressRequestIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       TimeoutHttpsInInsecureAddressRequestIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kTimeout),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       ServfailHttpsInInsecureAddressRequestIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/false,
      MockDnsClientRule::Result(
          MockDnsClientRule::ResultType::kFail,
          BuildTestDnsResponse(kName, dns_protocol::kTypeHttps, /*answers=*/{},
                               /*authority=*/{}, /*additional=*/{},
                               dns_protocol::kRcodeSERVFAIL),
          ERR_DNS_SERVER_FAILED),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       UnparsableHttpsInInsecureAddressRequestIgnored) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(
                         MockDnsClientRule::ResultType::kFail,
                         /*response=*/std::nullopt, ERR_DNS_MALFORMED_RESPONSE),
                     /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

// Test that when additional HTTPS timeout Feature params are disabled, the task
// does not timeout until the transactions themselves timeout.
TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryWaitsWithoutAdditionalTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Disable timeouts.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  rules.emplace_back(
      kName, dns_protocol::kTypeHttps, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kTimeout),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait an absurd amount of time (1 hour) and expect the request to not
  // complete because it is waiting on the transaction, where the mock is
  // delaying completion.
  FastForwardBy(base::Hours(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::IsEmpty()));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInSecureAddressQueryWithOnlyMinTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "30m"},
       // Set a Secure absolute timeout of 10 minutes via the "min" param.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "10m"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait until 1 second before expected timeout.
  FastForwardBy(base::Minutes(10) - base::Seconds(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Exceed expected timeout.
  FastForwardBy(base::Seconds(2));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInSecureAddressQueryWithOnlyMaxTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "30m"},
       // Set a Secure absolute timeout of 10 minutes via the "max" param.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "10m"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait until 1 second before expected timeout.
  FastForwardBy(base::Minutes(10) - base::Seconds(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Exceed expected timeout.
  FastForwardBy(base::Seconds(2));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInSecureAddressQueryWithRelativeTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "30m"},
       // Set a Secure relative timeout of 10%.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "10"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Complete final address transaction after 100 seconds total.
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(
      mock_dns_client_->CompleteOneDelayedTransactionOfType(DnsQueryType::A));
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(mock_dns_client_->CompleteOneDelayedTransactionOfType(
      DnsQueryType::AAAA));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect timeout at additional 10 seconds.
  FastForwardBy(base::Seconds(9));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  FastForwardBy(base::Seconds(2));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInSecureAddressQueryWithMaxTimeoutFirst) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       // Set a Secure max timeout of 30s and a relative timeout of 100%.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "30s"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "100"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "10s"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Complete final address transaction after 4 minutes total.
  FastForwardBy(base::Minutes(2));
  ASSERT_TRUE(
      mock_dns_client_->CompleteOneDelayedTransactionOfType(DnsQueryType::A));
  FastForwardBy(base::Minutes(2));
  ASSERT_TRUE(mock_dns_client_->CompleteOneDelayedTransactionOfType(
      DnsQueryType::AAAA));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait until 1 second before expected timeout (from the max timeout).
  FastForwardBy(base::Seconds(29));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Exceed expected timeout.
  FastForwardBy(base::Seconds(2));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryWithRelativeTimeoutFirst) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       // Set a Secure max timeout of 20 minutes and a relative timeout of 10%.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "20m"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "10"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "1s"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Complete final address transaction after 100 seconds total.
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(
      mock_dns_client_->CompleteOneDelayedTransactionOfType(DnsQueryType::A));
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(mock_dns_client_->CompleteOneDelayedTransactionOfType(
      DnsQueryType::AAAA));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect timeout at additional 10 seconds (from the relative timeout).
  FastForwardBy(base::Seconds(9));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  FastForwardBy(base::Seconds(2));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryWithRelativeTimeoutShorterThanMinTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {{"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       // Set a Secure min timeout of 1 minute and a relative timeout of 10%.
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "20m"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "10"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "1m"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Complete final address transaction after 100 seconds total.
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(
      mock_dns_client_->CompleteOneDelayedTransactionOfType(DnsQueryType::A));
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(mock_dns_client_->CompleteOneDelayedTransactionOfType(
      DnsQueryType::AAAA));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect timeout at additional 1 minute (from the min timeout).
  FastForwardBy(base::Minutes(1) - base::Seconds(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  FastForwardBy(base::Seconds(2));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInInsecureAddressQueryWithOnlyMinTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Set an Insecure absolute timeout of 10 minutes via the "min" param.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "10m"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait until 1 second before expected timeout.
  FastForwardBy(base::Minutes(10) - base::Seconds(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Exceed expected timeout.
  FastForwardBy(base::Seconds(2));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInInsecureAddressQueryWithOnlyMaxTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Set an Insecure absolute timeout of 10 minutes via the "max" param.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "10m"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait until 1 second before expected timeout.
  FastForwardBy(base::Minutes(10) - base::Seconds(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Exceed expected timeout.
  FastForwardBy(base::Seconds(2));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest,
       HttpsInInsecureAddressQueryWithRelativeTimeout) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Set an Insecure relative timeout of 10%.
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "10"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Complete final address transaction after 100 seconds total.
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(
      mock_dns_client_->CompleteOneDelayedTransactionOfType(DnsQueryType::A));
  FastForwardBy(base::Seconds(50));
  ASSERT_TRUE(mock_dns_client_->CompleteOneDelayedTransactionOfType(
      DnsQueryType::AAAA));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect timeout at additional 10 seconds.
  FastForwardBy(base::Seconds(9));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  FastForwardBy(base::Seconds(2));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

// Test that HTTPS timeouts are not used when fatal for the request.
TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryWaitsWithoutTimeoutIfFatal) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Set timeouts but also enforce secure responses.
       {"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "20m"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/true,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/true,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kAutomatic;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait an absurd amount of time (1 hour) and expect the request to not
  // complete because it is waiting on the transaction, where the mock is
  // delaying completion.
  FastForwardBy(base::Hours(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(
          ExpectEndpointResult(
              testing::SizeIs(2),
              ExpectConnectionEndpointMetadata(
                  testing::ElementsAre(dns_protocol::kHttpsServiceDefaultAlpn),
                  testing::IsEmpty(), kName)),
          ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              testing::Pointee(testing::ElementsAre(true)));
}

// Test that HTTPS timeouts are always respected for insecure requests.
TEST_F(HostResolverManagerDnsTest,
       HttpsInAddressQueryAlwaysRespectsTimeoutsForInsecure) {
  const char kName[] = "name.test";

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kUseDnsHttpsSvcb,
      {// Set timeouts but also enforce secure responses.
       {"UseDnsHttpsSvcbEnforceSecureResponse", "true"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbInsecureExtraTimeMin", "20m"},
       {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
       {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestHttpsServiceRecord(kName, /*priority=*/1, /*service_name=*/".",
                                  /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeHttps, /*secure=*/false,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeHttps, records)),
                     /*delay=*/true);
  rules.emplace_back(
      kName, dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      url::SchemeHostPort(url::kHttpsScheme, kName, 443),
      NetworkAnonymizationKey(), NetLogWithSource(), std::nullopt,
      resolve_context_.get()));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Wait until 1s before expected timeout.
  FastForwardBy(base::Minutes(20) - base::Seconds(1));
  RunUntilIdle();
  EXPECT_FALSE(response.complete());

  FastForwardBy(base::Seconds(2));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // No experimental results if transaction did not complete.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, UnsolicitedHttps) {
  const char kName[] = "unsolicited.test";

  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {
      BuildTestAddressRecord(kName, IPAddress(1, 2, 3, 4))};
  std::vector<DnsResourceRecord> additional = {BuildTestHttpsServiceRecord(
      kName, /*priority=*/1, /*service_name=*/".", /*params=*/{})};
  rules.emplace_back(kName, dns_protocol::kTypeA, true /* secure */,
                     MockDnsClientRule::Result(BuildTestDnsResponse(
                         kName, dns_protocol::kTypeA, records,
                         {} /* authority */, additional)),
                     false /* delay */);
  rules.emplace_back(
      kName, dns_protocol::kTypeAAAA, true /* secure */,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      false /* delay */);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kName, 108), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_TRUE(response.request()->GetAddressResults());
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::SizeIs(2)))));
  EXPECT_THAT(response.request()->GetTextResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  EXPECT_THAT(response.request()->GetHostnameResults(),
              AnyOf(nullptr, Pointee(IsEmpty())));
  // Unsolicited records not included in results.
  EXPECT_THAT(response.request()->GetExperimentalResultsForTesting(),
              AnyOf(nullptr, Pointee(IsEmpty())));
}

TEST_F(HostResolverManagerDnsTest, DohProbeRequest) {
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_FALSE(mock_dns_client_->factory()->doh_probes_running());

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver_->CreateDohProbeRequest(resolve_context_.get());
  EXPECT_THAT(request->Start(), IsError(ERR_IO_PENDING));

  EXPECT_TRUE(mock_dns_client_->factory()->doh_probes_running());

  request.reset();

  EXPECT_FALSE(mock_dns_client_->factory()->doh_probes_running());
}

TEST_F(HostResolverManagerDnsTest, DohProbeRequest_BeforeConfig) {
  InvalidateDnsConfig();

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver_->CreateDohProbeRequest(resolve_context_.get());
  EXPECT_THAT(request->Start(), IsError(ERR_IO_PENDING));
  EXPECT_FALSE(mock_dns_client_->factory()->doh_probes_running());

  ChangeDnsConfig(CreateValidDnsConfig());
  EXPECT_TRUE(mock_dns_client_->factory()->doh_probes_running());
}

TEST_F(HostResolverManagerDnsTest, DohProbeRequest_InvalidateConfig) {
  ChangeDnsConfig(CreateValidDnsConfig());

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver_->CreateDohProbeRequest(resolve_context_.get());
  EXPECT_THAT(request->Start(), IsError(ERR_IO_PENDING));
  ASSERT_TRUE(mock_dns_client_->factory()->doh_probes_running());

  InvalidateDnsConfig();

  EXPECT_FALSE(mock_dns_client_->factory()->doh_probes_running());
}

TEST_F(HostResolverManagerDnsTest, DohProbeRequest_RestartOnConnectionChange) {
  DestroyResolver();
  test::ScopedMockNetworkChangeNotifier notifier;
  CreateSerialResolver();
  notifier.mock_network_change_notifier()->SetConnectionType(
      NetworkChangeNotifier::CONNECTION_NONE);
  ChangeDnsConfig(CreateValidDnsConfig());

  std::unique_ptr<HostResolver::ProbeRequest> request =
      resolver_->CreateDohProbeRequest(resolve_context_.get());
  EXPECT_THAT(request->Start(), IsError(ERR_IO_PENDING));
  EXPECT_TRUE(mock_dns_client_->factory()->doh_probes_running());
  mock_dns_client_->factory()->CompleteDohProbeRuners();
  ASSERT_FALSE(mock_dns_client_->factory()->doh_probes_running());

  notifier.mock_network_change_notifier()->SetConnectionTypeAndNotifyObservers(
      NetworkChangeNotifier::CONNECTION_NONE);

  EXPECT_TRUE(mock_dns_client_->factory()->doh_probes_running());
}

TEST_F(HostResolverManagerDnsTest, MultipleDohProbeRequests) {
  ChangeDnsConfig(CreateValidDnsConfig());

  EXPECT_FALSE(mock_dns_client_->factory()->doh_probes_running());

  std::unique_ptr<HostResolver::ProbeRequest> request1 =
      resolver_->CreateDohProbeRequest(resolve_context_.get());
  EXPECT_THAT(request1->Start(), IsError(ERR_IO_PENDING));
  std::unique_ptr<HostResolver::ProbeRequest> request2 =
      resolver_->CreateDohProbeRequest(resolve_context_.get());
  EXPECT_THAT(request2->Start(), IsError(ERR_IO_PENDING));

  EXPECT_TRUE(mock_dns_client_->factory()->doh_probes_running());

  request1.reset();
  EXPECT_TRUE(mock_dns_client_->factory()->doh_probes_running());

  request2.reset();
  EXPECT_FALSE(mock_dns_client_->factory()->doh_probes_running());
}

// Test that a newly-registered ResolveContext is immediately usable with a DNS
// configuration loaded before the context registration.
TEST_F(HostResolverManagerDnsTest,
       NewlyRegisteredContext_ConfigBeforeRegistration) {
  ResolveContext context(nullptr /* url_request_context */,
                         true /* enable_caching */);
  set_allow_fallback_to_systemtask(false);
  ChangeDnsConfig(CreateValidDnsConfig());
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ASSERT_TRUE(mock_dns_client_->GetCurrentSession());

  resolver_->RegisterResolveContext(&context);
  EXPECT_EQ(context.current_session_for_testing(),
            mock_dns_client_->GetCurrentSession());

  // Test a SECURE-mode DoH request with SetForceDohServerAvailable(false).
  // Should only succeed if a DoH server is marked available in the
  // ResolveContext. MockDnsClient skips most other interaction with
  // ResolveContext.
  mock_dns_client_->SetForceDohServerAvailable(false);
  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              mock_dns_client_->GetCurrentSession());
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, &context));
  EXPECT_THAT(response.result_error(), IsOk());

  resolver_->DeregisterResolveContext(&context);
}

// Test interaction with a ResolveContext registered before a DNS config is
// ready.
TEST_F(HostResolverManagerDnsTest,
       NewlyRegisteredContext_NoConfigAtRegistration) {
  ResolveContext context(nullptr /* url_request_context */,
                         true /* enable_caching */);
  set_allow_fallback_to_systemtask(false);
  InvalidateDnsConfig();
  DnsConfigOverrides overrides;
  overrides.secure_dns_mode = SecureDnsMode::kSecure;
  resolver_->SetDnsConfigOverrides(overrides);

  ASSERT_FALSE(mock_dns_client_->GetCurrentSession());

  // Register context before loading a DNS config.
  resolver_->RegisterResolveContext(&context);
  EXPECT_FALSE(context.current_session_for_testing());

  // Load DNS config and expect the session to be loaded into the ResolveContext
  ChangeDnsConfig(CreateValidDnsConfig());
  ASSERT_TRUE(mock_dns_client_->GetCurrentSession());
  EXPECT_EQ(context.current_session_for_testing(),
            mock_dns_client_->GetCurrentSession());

  // Test a SECURE-mode DoH request with SetForceDohServerAvailable(false).
  // Should only succeed if a DoH server is marked available in the
  // ResolveContext. MockDnsClient skips most other interaction with
  // ResolveContext.
  mock_dns_client_->SetForceDohServerAvailable(false);
  context.RecordServerSuccess(0u /* server_index */, true /* is_doh_server */,
                              mock_dns_client_->GetCurrentSession());
  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("secure", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      std::nullopt, &context));
  EXPECT_THAT(response.result_error(), IsOk());

  resolver_->DeregisterResolveContext(&context);
}

// `HostResolver::ResolveHostParameters::avoid_multicast_resolution` not
// currently supported to do anything except with the system resolver. So with
// DnsTask, expect it to be ignored.
TEST_F(HostResolverManagerDnsTest, AvoidMulticastIgnoredWithDnsTask) {
  ChangeDnsConfig(CreateValidDnsConfig());

  HostResolver::ResolveHostParameters parameters;
  parameters.avoid_multicast_resolution = true;

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), NetworkAnonymizationKey(), NetLogWithSource(),
      parameters, resolve_context_.get()));
  EXPECT_THAT(response.result_error(), IsOk());
}

class MockAddressSorter : public AddressSorter {
 public:
  MOCK_METHOD(void,
              Sort,
              (const std::vector<IPEndPoint>& endpoints, CallbackType callback),
              (const, override));

  void ExpectCall(const std::vector<IPEndPoint>& expected,
                  std::vector<IPEndPoint> sorted) {
    EXPECT_CALL(*this, Sort(expected, _))
        .WillOnce([sorted](const std::vector<IPEndPoint>& endpoints,
                           AddressSorter::CallbackType callback) {
          std::move(callback).Run(true, std::move(sorted));
        });
  }

  void ExpectCallAndFailSort(const std::vector<IPEndPoint>& expected) {
    EXPECT_CALL(*this, Sort(expected, _))
        .WillOnce([](const std::vector<IPEndPoint>& endpoints,
                     AddressSorter::CallbackType callback) {
          std::move(callback).Run(false, {});
        });
  }
};

TEST_F(HostResolverManagerDnsTest, ResultsAreSorted) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  // Expect sorter to be separately called with A and AAAA results. For the
  // AAAA, sort to reversed order.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCall(
      {CreateExpected("::1", 0), CreateExpected("2001:4860:4860::8888", 0)},
      {CreateExpected("2001:4860:4860::8888", 0), CreateExpected("::1", 0)});
  sorter->ExpectCall({CreateExpected("127.0.0.1", 0)},
                     {CreateExpected("127.0.0.1", 0)});

  DnsResponse a_response =
      BuildTestDnsAddressResponse("host.test", IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv6Localhost()),
       BuildTestAddressRecord(
           "host.test",
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());

  // Expect results in the order given by the sorter (with AAAA results before A
  // results).
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(
          testing::ElementsAre(ExpectEndpointResult(testing::ElementsAre(
              CreateExpected("2001:4860:4860::8888", 80),
              CreateExpected("::1", 80), CreateExpected("127.0.0.1", 80))))));
}

TEST_F(HostResolverManagerDnsTest, ResultsAreSortedWithHostCache) {
  base::test::ScopedFeatureList feature_list;
  DisableHostResolverCache(feature_list);

  // When using HostCache, expect sorter to be called once for all address
  // results together (AAAA before A).
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCall(
      {CreateExpected("::1", 0), CreateExpected("2001:4860:4860::8888", 0),
       CreateExpected("127.0.0.1", 0)},
      {CreateExpected("2001:4860:4860::8888", 0),
       CreateExpected("127.0.0.1", 0), CreateExpected("::1", 0)});

  DnsResponse a_response =
      BuildTestDnsAddressResponse("host.test", IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv6Localhost()),
       BuildTestAddressRecord(
           "host.test",
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());

  // Expect results in the order given by the sorter.
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(
          testing::ElementsAre(ExpectEndpointResult(testing::ElementsAre(
              CreateExpected("2001:4860:4860::8888", 80),
              CreateExpected("127.0.0.1", 80), CreateExpected("::1", 80))))));
}

TEST_F(HostResolverManagerDnsTest, Ipv4OnlyResultsAreSorted) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  // Sort to reversed order.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCall(
      {CreateExpected("127.0.0.1", 0), CreateExpected("127.0.0.2", 0)},
      {CreateExpected("127.0.0.2", 0), CreateExpected("127.0.0.1", 0)});

  DnsResponse a_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv4Localhost()),
       BuildTestAddressRecord("host.test",
                              IPAddress::FromIPLiteral("127.0.0.2").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());

  // Expect results in the order given by the sorter.
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.2", 80),
                                       CreateExpected("127.0.0.1", 80))))));
}

TEST_F(HostResolverManagerDnsTest, Ipv4OnlyResultsNotSortedWithHostCache) {
  base::test::ScopedFeatureList feature_list;
  DisableHostResolverCache(feature_list);

  // When using HostCache, expect no sort calls for IPv4-only results.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();

  DnsResponse a_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv4Localhost()),
       BuildTestAddressRecord("host.test",
                              IPAddress::FromIPLiteral("127.0.0.2").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());

  // Expect results in original unsorted order.
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("127.0.0.1", 80),
                                       CreateExpected("127.0.0.2", 80))))));
}

TEST_F(HostResolverManagerDnsTest, EmptyResultsNotSorted) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  // Expect no calls to sorter for empty results.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();

  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kEmpty,
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

TEST_F(HostResolverManagerDnsTest, EmptyResultsNotSortedWithHostCache) {
  base::test::ScopedFeatureList feature_list;
  DisableHostResolverCache(feature_list);

  // Expect no calls to sorter for empty results.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();

  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA,
             MockDnsClientRule::ResultType::kEmpty,
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));
}

// Test for when AddressSorter removes all results.
TEST_F(HostResolverManagerDnsTest, ResultsSortedAsUnreachable) {
  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  // Set up sorter to return result with no addresses.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCall(
      {CreateExpected("::1", 0), CreateExpected("2001:4860:4860::8888", 0)},
      {});
  sorter->ExpectCall({CreateExpected("127.0.0.1", 0)}, {});

  DnsResponse a_response =
      BuildTestDnsAddressResponse("host.test", IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv6Localhost()),
       BuildTestAddressRecord(
           "host.test",
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  ASSERT_FALSE(!!GetCacheHit(HostCache::Key(
      "host.test", DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, NetworkAnonymizationKey())));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  // Expect error is cached (because pre-sort results had a TTL).
  EXPECT_TRUE(!!GetCacheHit(HostCache::Key(
      "host.test", DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, NetworkAnonymizationKey())));
}

// Test for when AddressSorter removes all results.
TEST_F(HostResolverManagerDnsTest, ResultsSortedAsUnreachableWithHostCache) {
  base::test::ScopedFeatureList feature_list;
  DisableHostResolverCache(feature_list);

  // Set up sorter to return result with no addresses.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCall(
      {CreateExpected("::1", 0), CreateExpected("2001:4860:4860::8888", 0),
       CreateExpected("127.0.0.1", 0)},
      {});

  DnsResponse a_response =
      BuildTestDnsAddressResponse("host.test", IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv6Localhost()),
       BuildTestAddressRecord(
           "host.test",
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  ASSERT_FALSE(!!GetCacheHit(HostCache::Key(
      "host.test", DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, NetworkAnonymizationKey())));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  // Expect error is cached (because pre-sort results had a TTL).
  EXPECT_TRUE(!!GetCacheHit(HostCache::Key(
      "host.test", DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, NetworkAnonymizationKey())));
}

TEST_F(HostResolverManagerDnsTest, SortFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(10);

  // Fail the AAAA sort. Don't expect resolver to even attempt to sort A.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCallAndFailSort(
      {CreateExpected("::1", 0), CreateExpected("2001:4860:4860::8888", 0)});

  DnsResponse a_response = BuildTestDnsAddressResponse(
      std::string(kHost), IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord(std::string(kHost), IPAddress::IPv6Localhost(),
                              kMinTtl),
       BuildTestAddressRecord(
           std::string(kHost),
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value(),
           base::Minutes(15))});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeA,
             std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_SORT_ERROR));

  // Expect error is cached with same TTL as results that failed to sort.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::A, HostResolverSource::DNS,
      /*secure=*/false));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalErrorResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kUnknown,
                  Optional(base::TimeTicks::Now() + kMinTtl),
                  Optional(base::Time::Now() + kMinTtl), ERR_DNS_SORT_ERROR)));
}

// Test for if a transaction sort fails after another transaction has already
// succeeded.
TEST_F(HostResolverManagerDnsTest, PartialSortFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";
  constexpr base::TimeDelta kMinTtl = base::Minutes(3);

  // Successfully sort A. Fail to sort AAAA.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCall({IPEndPoint(IPAddress::IPv4Localhost(), 0)},
                     {IPEndPoint(IPAddress::IPv4Localhost(), 0)});
  sorter->ExpectCallAndFailSort({IPEndPoint(IPAddress::IPv6Localhost(), 0),
                                 CreateExpected("2001:4860:4860::8888", 0)});

  DnsResponse a_response = BuildTestDnsAddressResponse(
      std::string(kHost), IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord(std::string(kHost), IPAddress::IPv6Localhost(),
                              kMinTtl),
       BuildTestAddressRecord(
           std::string(kHost),
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value(),
           base::Minutes(7))});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeA,
             std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/true);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect the successful A result to be cached immediately on receipt.
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::A,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::A,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv4Localhost(), 0)))));
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA));

  mock_dns_client_->CompleteDelayedTransactions();
  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_SORT_ERROR));

  // Expect error is cached with same TTL as results that failed to sort.
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::A,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::A,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv4Localhost(), 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalErrorResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kUnknown,
                  Optional(base::TimeTicks::Now() + kMinTtl),
                  Optional(base::Time::Now() + kMinTtl), ERR_DNS_SORT_ERROR)));
}

TEST_F(HostResolverManagerDnsTest, SortFailureWithHostCache) {
  base::test::ScopedFeatureList feature_list;
  DisableHostResolverCache(feature_list);

  // Fail the sort.
  auto sorter = std::make_unique<testing::StrictMock<MockAddressSorter>>();
  sorter->ExpectCallAndFailSort({CreateExpected("::1", 0),
                                 CreateExpected("2001:4860:4860::8888", 0),
                                 CreateExpected("127.0.0.1", 0)});

  DnsResponse a_response =
      BuildTestDnsAddressResponse("host.test", IPAddress::IPv4Localhost());
  DnsResponse aaaa_response = BuildTestDnsResponse(
      "host.test", dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord("host.test", IPAddress::IPv6Localhost()),
       BuildTestAddressRecord(
           "host.test",
           IPAddress::FromIPLiteral("2001:4860:4860::8888").value())});
  MockDnsClientRuleList rules;
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeA, std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, "host.test", dns_protocol::kTypeAAAA,
             std::move(aaaa_response), /*delay=*/false);

  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client_->SetAddressSorterForTesting(std::move(sorter));
  set_allow_fallback_to_systemtask(false);

  ASSERT_FALSE(!!GetCacheHit(HostCache::Key(
      "host.test", DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, NetworkAnonymizationKey())));

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("host.test", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsError(ERR_DNS_SORT_ERROR));

  // Expect error is cached (because pre-sort results had a TTL).
  EXPECT_TRUE(!!GetCacheHit(HostCache::Key(
      "host.test", DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, NetworkAnonymizationKey())));
}

TEST_F(HostResolverManagerDnsTest, HostResolverCacheContainsTransactions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUseHostResolverCache,
                            features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{});

  ChangeDnsConfig(CreateValidDnsConfig());

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("ok", 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsOk());

  // Expect separate transactions to be separately cached.
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "ok", kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalDataResult(
          "ok", DnsQueryType::A, HostResolverInternalResult::Source::kDns, _, _,
          ElementsAre(IPEndPoint(IPAddress::IPv4Localhost(), 0)))));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "ok", kNetworkAnonymizationKey, DnsQueryType::AAAA,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalDataResult(
          "ok", DnsQueryType::AAAA, HostResolverInternalResult::Source::kDns, _,
          _, ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));
}

TEST_F(HostResolverManagerDnsTest, HostResolverCacheContainsAliasChains) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUseHostResolverCache,
                            features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  MockDnsClientRuleList rules;
  DnsResponse a_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeA,
      {BuildTestCnameRecord(std::string(kHost), "alias1.test"),
       BuildTestCnameRecord("alias1.test", "alias2.test"),
       BuildTestAddressRecord("alias2.test", IPAddress::IPv4Localhost())});
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeA,
             std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsOk());

  // Expect each alias link and the result to be separately cached with the
  // aliases cached under the original query type.
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          kHost, kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          std::string(kHost), DnsQueryType::A,
          HostResolverInternalResult::Source::kDns, _, _, "alias1.test")));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "alias1.test", kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          "alias1.test", DnsQueryType::A,
          HostResolverInternalResult::Source::kDns, _, _, "alias2.test")));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "alias2.test", kNetworkAnonymizationKey, DnsQueryType::A,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  "alias2.test", DnsQueryType::A,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv4Localhost(), 0)))));
}

TEST_F(HostResolverManagerDnsTest,
       HostResolverCacheContainsAliasChainsWithErrors) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUseHostResolverCache,
                            features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";
  constexpr base::TimeDelta kTtl = base::Minutes(30);

  MockDnsClientRuleList rules;
  DnsResponse a_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeA,
      /*answers=*/
      {BuildTestCnameRecord(std::string(kHost), "alias1.test"),
       BuildTestCnameRecord("alias1.test", "alias2.test")},
      /*authority=*/
      {BuildTestDnsRecord("authority.test", dns_protocol::kTypeSOA,
                          "fake rdata", kTtl)});
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeA,
             std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  // Expect each alias link and the result error to be separately cached with
  // the aliases cached under the original query type.
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          kHost, kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          std::string(kHost), DnsQueryType::A,
          HostResolverInternalResult::Source::kDns, _, _, "alias1.test")));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "alias1.test", kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          "alias1.test", DnsQueryType::A,
          HostResolverInternalResult::Source::kDns, _, _, "alias2.test")));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  "alias2.test", kNetworkAnonymizationKey, DnsQueryType::A,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalErrorResult(
                  "alias2.test", DnsQueryType::A,
                  HostResolverInternalResult::Source::kDns,
                  Optional(base::TimeTicks::Now() + kTtl),
                  Optional(base::Time::Now() + kTtl), ERR_NAME_NOT_RESOLVED)));
}

TEST_F(HostResolverManagerDnsTest,
       HostResolverCacheContainsAliasChainsWithNoTtlErrors) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kUseHostResolverCache,
                            features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  MockDnsClientRuleList rules;
  // No SOA authority record, so NODATA error is not cacheable.
  DnsResponse a_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeA,
      /*answers=*/
      {BuildTestCnameRecord(std::string(kHost), "alias1.test"),
       BuildTestCnameRecord("alias1.test", "alias2.test")});
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeA,
             std::move(a_response),
             /*delay=*/false);
  AddDnsRule(&rules, std::string(kHost), dns_protocol::kTypeAAAA,
             MockDnsClientRule::ResultType::kEmpty, /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsError(ERR_NAME_NOT_RESOLVED));

  // Expect each alias link to be separately cached under the original query
  // type. No cache entry for the NODATA error because there was no SOA record
  // to contain the NODATA TTL.
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          kHost, kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          std::string(kHost), DnsQueryType::A,
          HostResolverInternalResult::Source::kDns, _, _, "alias1.test")));
  EXPECT_THAT(
      resolve_context_->host_resolver_cache()->Lookup(
          "alias1.test", kNetworkAnonymizationKey, DnsQueryType::A,
          HostResolverSource::DNS, /*secure=*/false),
      Pointee(ExpectHostResolverInternalAliasResult(
          "alias1.test", DnsQueryType::A,
          HostResolverInternalResult::Source::kDns, _, _, "alias2.test")));
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      "alias2.test", kNetworkAnonymizationKey, DnsQueryType::A,
      HostResolverSource::DNS, /*secure=*/false));
}

TEST_F(HostResolverManagerDnsTest, NetworkErrorsNotSavedInHostCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{features::kUseHostResolverCache});

  constexpr std::string_view kHost = "host.test";

  // Network failures for all result types.
  MockDnsClientRuleList rules;
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
                     MockDnsClientRule::Result(
                         MockDnsClientRule::ResultType::kFail,
                         /*response=*/std::nullopt, ERR_CONNECTION_REFUSED),
                     /*delay=*/false);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail,
                                /*response=*/std::nullopt,
                                ERR_CONNECTION_REFUSED),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsError(ERR_CONNECTION_REFUSED));

  // Expect result not cached because network errors have no TTL.
  EXPECT_FALSE(GetCacheHit(HostCache::Key(
      std::string(kHost), DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, kNetworkAnonymizationKey)));
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

// Test for if a DNS transaction fails with network error after another
// transaction has already succeeded.
TEST_F(HostResolverManagerDnsTest, PartialNetworkErrorsNotSavedInHostCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{features::kUseHostResolverCache});

  constexpr std::string_view kHost = "host.test";

  // Return a successful AAAA response before a delayed failure A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
                     MockDnsClientRule::Result(
                         MockDnsClientRule::ResultType::kFail,
                         /*response=*/std::nullopt, ERR_CONNECTION_REFUSED),
                     /*delay=*/true);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsError(ERR_CONNECTION_REFUSED));

  // Even if some transactions have already received results successfully, a
  // network failure means the entire request fails and nothing should be cached
  // to the HostCache.
  EXPECT_FALSE(GetCacheHit(HostCache::Key(
      std::string(kHost), DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, kNetworkAnonymizationKey)));
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest, NetworkErrorsNotSavedInHostResolverCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  // Network failures for all result types.
  MockDnsClientRuleList rules;
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
                     MockDnsClientRule::Result(
                         MockDnsClientRule::ResultType::kFail,
                         /*response=*/std::nullopt, ERR_CONNECTION_REFUSED),
                     /*delay=*/false);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kFail,
                                /*response=*/std::nullopt,
                                ERR_CONNECTION_REFUSED),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsError(ERR_CONNECTION_REFUSED));

  // Expect result not cached because network errors have no TTL.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey));
}

// Test for if a DNS transaction fails with network error after another
// transaction has already succeeded.
TEST_F(HostResolverManagerDnsTest,
       PartialNetworkErrorsNotSavedInHostResolverCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  // Return a successful AAAA response before a delayed failure A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
                     MockDnsClientRule::Result(
                         MockDnsClientRule::ResultType::kFail,
                         /*response=*/std::nullopt, ERR_CONNECTION_REFUSED),
                     /*delay=*/true);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect AAAA result to be cached immediately on receipt.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::A));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));

  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsError(ERR_CONNECTION_REFUSED));

  // Expect same cache contents, as network errors are not cacheable.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::A));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));
}

TEST_F(HostResolverManagerDnsTest, MalformedResponsesNotSavedInHostCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{features::kUseHostResolverCache});

  constexpr std::string_view kHost = "host.test";

  // Malformed responses for all result types.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/false);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));

  // Expect result not cached because malformed responses have no TTL.
  EXPECT_FALSE(GetCacheHit(HostCache::Key(
      std::string(kHost), DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, kNetworkAnonymizationKey)));
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

// Test for if a DNS transaction fails with a malformed response after another
// transaction has already succeeded.
TEST_F(HostResolverManagerDnsTest,
       PartialMalformedResponsesNotSavedInHostCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{features::kUseHostResolverCache});

  constexpr std::string_view kHost = "host.test";

  // Return a successful AAAA response before a delayed failure A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/true);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));

  // Even if some transactions have already received results successfully, a
  // malformed response means the entire request fails and nothing should be
  // cached to the HostCache.
  EXPECT_FALSE(GetCacheHit(HostCache::Key(
      std::string(kHost), DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
      HostResolverSource::ANY, kNetworkAnonymizationKey)));
  EXPECT_EQ(resolve_context_->host_cache()->size(), 0u);
}

TEST_F(HostResolverManagerDnsTest,
       MalformedResponsesNotSavedInHostResolverCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  // Network failures for all result types.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/false);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));

  // Expect result not cached because malformed responses have no TTL.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey));
}

// Test for if a DNS transaction fails with malformed response after another
// transaction has already succeeded.
TEST_F(HostResolverManagerDnsTest,
       PartialMalformedResponsesNotSavedInHostResolverCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  // Return a successful AAAA response before a delayed failure A response.
  MockDnsClientRuleList rules;
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kMalformed),
      /*delay=*/true);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair(kHost, 80), kNetworkAnonymizationKey, NetLogWithSource(),
      std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect the successful AAAA result to be cached immediately on receipt.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::A));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));

  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_MALFORMED_RESPONSE));

  // Expect same cache contents, as malformed responses are not cacheable.
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::A));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));
}

TEST_F(HostResolverManagerDnsTest, HttpToHttpsUpgradeSavedInHostCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey},
      /*disabled_features=*/{features::kUseHostResolverCache});

  constexpr std::string_view kHost = "host.test";

  // Return successful A/AAAA responses before HTTPS to ensure they are not
  // cached.
  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsAliasRecord(
      std::string(kHost), "alias.test", /*ttl=*/base::Days(20))};
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeHttps, /*secure=*/false,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          std::string(kHost), dns_protocol::kTypeHttps, records)),
      /*delay=*/true);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kHost, 80),
                               kNetworkAnonymizationKey, NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());
  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));

  // Even if some transactions have already received results successfully, an
  // HTTPS record means the entire request fails and the upgrade failure should
  // be cached for the TTL from the HTTPS response.
  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(
          HostCache::Key(url::SchemeHostPort(url::kHttpScheme, kHost, 80),
                         DnsQueryType::UNSPECIFIED, /*host_resolver_flags=*/0,
                         HostResolverSource::ANY, kNetworkAnonymizationKey));
  ASSERT_TRUE(cache_result);
  ASSERT_TRUE(cache_result->second.has_ttl());
  EXPECT_EQ(cache_result->second.ttl(), base::Days(20));
}

// Test cache behavior for when an HTTPS response indicating http->https upgrade
// is received after successful address responses.
TEST_F(HostResolverManagerDnsTest,
       HttpToHttpsUpgradeAfterAddressesSavedInHostResolverCache) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::
                                kPartitionConnectionsByNetworkIsolationKey,
                            features::kUseHostResolverCache},
      /*disabled_features=*/{});

  constexpr std::string_view kHost = "host.test";

  // Return successful A/AAAA responses before HTTPS.
  MockDnsClientRuleList rules;
  std::vector<DnsResourceRecord> records = {BuildTestHttpsAliasRecord(
      std::string(kHost), "alias.test", /*ttl=*/base::Days(20))};
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeHttps, /*secure=*/false,
      MockDnsClientRule::Result(BuildTestDnsResponse(
          std::string(kHost), dns_protocol::kTypeHttps, records)),
      /*delay=*/true);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  rules.emplace_back(
      std::string(kHost), dns_protocol::kTypeAAAA, /*secure=*/false,
      MockDnsClientRule::Result(MockDnsClientRule::ResultType::kOk),
      /*delay=*/false);
  CreateResolver();
  UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
  set_allow_fallback_to_systemtask(false);

  const SchemefulSite kSite(GURL("https://site.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);

  ResolveHostResponseHelper response(
      resolver_->CreateRequest(url::SchemeHostPort(url::kHttpScheme, kHost, 80),
                               kNetworkAnonymizationKey, NetLogWithSource(),
                               std::nullopt, resolve_context_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(response.complete());

  // Expect successful address responses to be cached immediately on receipt.
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::A,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::A,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv4Localhost(), 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));
  EXPECT_FALSE(resolve_context_->host_resolver_cache()->Lookup(
      kHost, kNetworkAnonymizationKey, DnsQueryType::HTTPS,
      HostResolverSource::DNS, /*secure=*/false));

  mock_dns_client_->CompleteDelayedTransactions();
  ASSERT_THAT(response.result_error(), IsError(ERR_DNS_NAME_HTTPS_ONLY));

  // All responses cached, including the full metadata result because it is
  // still a usable result when requested for https://.
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::A,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::A,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv4Localhost(), 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::AAAA,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalDataResult(
                  std::string(kHost), DnsQueryType::AAAA,
                  HostResolverInternalResult::Source::kDns, _, _,
                  ElementsAre(IPEndPoint(IPAddress::IPv6Localhost(), 0)))));
  EXPECT_THAT(resolve_context_->host_resolver_cache()->Lookup(
                  kHost, kNetworkAnonymizationKey, DnsQueryType::HTTPS,
                  HostResolverSource::DNS, /*secure=*/false),
              Pointee(ExpectHostResolverInternalMetadataResult(
                  std::string(kHost), DnsQueryType::HTTPS,
                  HostResolverInternalResult::Source::kDns,
                  Optional(base::TimeTicks::Now() + base::Days(20)),
                  Optional(base::Time::Now() + base::Days(20)))));
}

class HostResolverManagerBootstrapTest : public HostResolverManagerDnsTest {
 protected:
  using MockResult = MockDnsClientRule::ResultType;

  void SetUp() override {
    // The request host scheme and port are only preserved if the SVCB feature
    // is enabled.
    features.InitAndEnableFeatureWithParameters(
        features::kUseDnsHttpsSvcb,
        {// Disable timeouts.
         {"UseDnsHttpsSvcbInsecureExtraTimeMax", "0"},
         {"UseDnsHttpsSvcbInsecureExtraTimePercent", "0"},
         {"UseDnsHttpsSvcbInsecureExtraTimeMin", "0"},
         {"UseDnsHttpsSvcbSecureExtraTimeMax", "0"},
         {"UseDnsHttpsSvcbSecureExtraTimePercent", "0"},
         {"UseDnsHttpsSvcbSecureExtraTimeMin", "0"}});

    HostResolverManagerDnsTest::SetUp();

    // MockHostResolverProc only returns failure if there is at least one
    // non-matching rule.
    proc_->AddRuleForAllFamilies("other_name", {});
    proc_->SignalMultiple(1u);  // Allow up to one proc query.
  }

  const NetworkAnonymizationKey kAnonymizationKey;
  const url::SchemeHostPort kEndpoint =
      url::SchemeHostPort(url::kHttpsScheme, "bootstrap", 443);
  const std::vector<IPEndPoint> kCacheAddrs = {
      {{0x20, 0x01, 0x0d, 0xb1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, 0},
      {{192, 0, 2, 1}, 0}};
  const std::vector<IPEndPoint> kBootstrapAddrs = {
      {{0x20, 0x01, 0x0d, 0xb1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}, 0},
      {{192, 0, 2, 2}, 0}};
  // The mock DNS client always returns localhost.
  const std::vector<IPEndPoint> kRemoteAddrs = {
      {IPAddress::IPv6Localhost(), 0},
      {IPAddress::IPv4Localhost(), 0}};

  static HostResolver::ResolveHostParameters bootstrap_params() {
    HostResolver::ResolveHostParameters params;
    params.secure_dns_policy = SecureDnsPolicy::kBootstrap;
    return params;
  }

  void ConfigureMockDns(MockResult insecure_result, MockResult secure_result) {
    MockDnsClientRuleList rules;
    AddDnsRule(&rules, kEndpoint.host(), dns_protocol::kTypeA, insecure_result,
               /*delay=*/false);
    AddDnsRule(&rules, kEndpoint.host(), dns_protocol::kTypeAAAA,
               insecure_result, /*delay=*/false);
    AddSecureDnsRule(&rules, kEndpoint.host(), dns_protocol::kTypeA,
                     secure_result, /*delay=*/false);
    AddSecureDnsRule(&rules, kEndpoint.host(), dns_protocol::kTypeAAAA,
                     secure_result, /*delay=*/false);
    UseMockDnsClient(CreateValidDnsConfig(), std::move(rules));
    mock_dns_client_->set_preset_endpoint(kEndpoint);
  }

  HostCache::Key MakeCacheKey(bool secure) {
    HostCache::Key cache_key(kEndpoint, DnsQueryType::UNSPECIFIED, 0,
                             HostResolverSource::ANY, kAnonymizationKey);
    cache_key.secure = secure;
    return cache_key;
  }

  void PopulateCache(bool secure) {
    constexpr base::TimeDelta kTtl = base::Seconds(3600);
    HostCache::Entry entry(OK, kCacheAddrs, /*aliases=*/{},
                           HostCache::Entry::SOURCE_DNS, kTtl);
    resolve_context_->host_cache()->Set(MakeCacheKey(secure), std::move(entry),
                                        GetMockTickClock()->NowTicks(), kTtl);
  }

  base::test::ScopedFeatureList features;
};

std::vector<IPAddress> IPAddresses(const std::vector<IPEndPoint>& endpoints) {
  return base::ToVector(endpoints, &IPEndPoint::address);
}

std::vector<IPAddress> IPAddresses(const AddressList& addresses) {
  return IPAddresses(addresses.endpoints());
}

MATCHER_P(AddressesMatch, expected, "Matches addresses between AddressLists") {
  return testing::Matches(testing::UnorderedElementsAreArray(
      IPAddresses(expected)))(IPAddresses(arg));
}

TEST_F(HostResolverManagerBootstrapTest, BlankSlate) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kOk,
                   /*secure_result=*/MockResult::kUnexpected);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_FALSE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kRemoteAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kRemoteAddrs)))));
}

TEST_F(HostResolverManagerBootstrapTest, InsecureCacheEntry) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kUnexpected);
  PopulateCache(/*secure=*/false);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kCacheAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kCacheAddrs)))));
}

TEST_F(HostResolverManagerBootstrapTest, SecureCacheEntry) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kUnexpected);
  PopulateCache(/*secure=*/true);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kCacheAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kCacheAddrs)))));
}

TEST_F(HostResolverManagerBootstrapTest, OnlyBootstrap) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kOk);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kBootstrapAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kBootstrapAddrs)))));

  // Run the followup query.
  RunUntilIdle();

  // Confirm that the remote addresses are now in the secure cache.
  const auto* secure_result = resolve_context_->host_cache()->Lookup(
      MakeCacheKey(/*secure=*/true), GetMockTickClock()->NowTicks());
  ASSERT_THAT(secure_result, testing::NotNull());
  EXPECT_THAT(
      secure_result->second.GetEndpoints(),
      testing::ElementsAre(ExpectEndpointResult(AddressesMatch(kRemoteAddrs))));
}

// The insecure cache is ignored, so the results are identical to
// OnlyBootstrap.
TEST_F(HostResolverManagerBootstrapTest, BootstrapAndInsecureCache) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kOk);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);
  PopulateCache(/*secure=*/false);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kBootstrapAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kBootstrapAddrs)))));

  // Run the followup query.
  RunUntilIdle();

  // Confirm that the remote addresses are now in the secure cache.
  const auto* secure_result = resolve_context_->host_cache()->Lookup(
      MakeCacheKey(/*secure=*/true), GetMockTickClock()->NowTicks());
  ASSERT_THAT(secure_result, testing::NotNull());
  EXPECT_THAT(
      secure_result->second.GetEndpoints(),
      testing::ElementsAre(ExpectEndpointResult(AddressesMatch(kRemoteAddrs))));
}

// The bootstrap addrs are ignored, so the results are identical to
// SecureCacheEntry.
TEST_F(HostResolverManagerBootstrapTest, BootstrapAndSecureCacheEntry) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kUnexpected);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);
  PopulateCache(/*secure=*/true);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kCacheAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kCacheAddrs)))));
}

TEST_F(HostResolverManagerBootstrapTest, BlankSlateFailure) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kFail,
                   /*secure_result=*/MockResult::kUnexpected);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_FALSE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(),
              IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_FALSE(bootstrap_response.request()
                   ->GetResolveErrorInfo()
                   .is_secure_network_error);
}

TEST_F(HostResolverManagerBootstrapTest, BootstrapFollowupFailure) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kFail);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);

  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response.complete());
  EXPECT_THAT(bootstrap_response.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kBootstrapAddrs)));
  EXPECT_THAT(bootstrap_response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kBootstrapAddrs)))));

  // Run the followup query.
  RunUntilIdle();

  // Confirm that the secure cache remains empty.
  const auto* secure_result = resolve_context_->host_cache()->Lookup(
      MakeCacheKey(/*secure=*/true), GetMockTickClock()->NowTicks());
  EXPECT_THAT(secure_result, testing::IsNull());
}

TEST_F(HostResolverManagerBootstrapTest, ContextClose) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kOk);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);

  // Trigger a followup request.
  ResolveHostResponseHelper bootstrap_response(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  // Deregistering the resolve context should clean up the pending followup job.
  EXPECT_EQ(1u, resolver_->num_jobs_for_testing());
  resolver_->DeregisterResolveContext(resolve_context_.get());
  EXPECT_EQ(0u, resolver_->num_jobs_for_testing());

  mock_dns_client_ = nullptr;
  resolver_ = nullptr;  // Avoid duplicate Deregister in TearDown.
}

// Equivalent to OnlyBootstrap + BootstrapAndSecureCacheEntry
TEST_F(HostResolverManagerBootstrapTest, BootstrapAfterFollowup) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kOk);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);

  // Run bootstrap and its followup query.
  ResolveHostResponseHelper bootstrap_response1(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));
  RunUntilIdle();

  // The remote addresses are now in the secure cache.
  // Rerun bootstrap, which reads the secure cache results.
  ResolveHostResponseHelper bootstrap_response2(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response2.complete());
  EXPECT_THAT(bootstrap_response2.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response2.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kRemoteAddrs)));
  EXPECT_THAT(bootstrap_response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kRemoteAddrs)))));
}

TEST_F(HostResolverManagerBootstrapTest, BootstrapFollowupFailureTwice) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kFail);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);

  // Run the bootstrap query and the followup, which will fail.
  ResolveHostResponseHelper bootstrap_response1(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));
  RunUntilIdle();

  // Reissue the bootstrap query.
  ResolveHostResponseHelper bootstrap_response2(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response2.complete());
  EXPECT_THAT(bootstrap_response2.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response2.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kBootstrapAddrs)));
  EXPECT_THAT(bootstrap_response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kBootstrapAddrs)))));

  // Run the followup query again.
  RunUntilIdle();

  // Confirm that the secure cache remains empty.
  const auto* secure_result = resolve_context_->host_cache()->Lookup(
      MakeCacheKey(/*secure=*/true), GetMockTickClock()->NowTicks());
  EXPECT_THAT(secure_result, testing::IsNull());
}

TEST_F(HostResolverManagerBootstrapTest, OnlyBootstrapTwice) {
  ConfigureMockDns(/*insecure_result=*/MockResult::kUnexpected,
                   /*secure_result=*/MockResult::kOk);
  mock_dns_client_->set_preset_addrs(kBootstrapAddrs);

  ResolveHostResponseHelper bootstrap_response1(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response1.complete());
  EXPECT_THAT(bootstrap_response1.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response1.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kBootstrapAddrs)));
  EXPECT_THAT(bootstrap_response1.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kBootstrapAddrs)))));

  ResolveHostResponseHelper bootstrap_response2(
      resolver_->CreateRequest(kEndpoint, kAnonymizationKey, NetLogWithSource(),
                               bootstrap_params(), resolve_context_.get()));

  EXPECT_TRUE(bootstrap_response2.complete());
  EXPECT_THAT(bootstrap_response2.result_error(), IsOk());
  EXPECT_THAT(bootstrap_response2.request()->GetAddressResults(),
              testing::Pointee(AddressesMatch(kBootstrapAddrs)));
  EXPECT_THAT(bootstrap_response2.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(AddressesMatch(kBootstrapAddrs)))));

  // Run the followup query.
  RunUntilIdle();

  // Confirm that the remote addresses are now in the secure cache.
  const auto* secure_result = resolve_context_->host_cache()->Lookup(
      MakeCacheKey(/*secure=*/true), GetMockTickClock()->NowTicks());
  ASSERT_THAT(secure_result, testing::NotNull());
  EXPECT_THAT(
      secure_result->second.GetEndpoints(),
      testing::ElementsAre(ExpectEndpointResult(AddressesMatch(kRemoteAddrs))));
}

void HostResolverManagerTest::IPv4AddressLiteralInIPv6OnlyNetworkTest(
    bool is_async) {
  HostResolver::ManagerOptions options = DefaultOptions();
  CreateResolverWithOptionsAndParams(std::move(options), DefaultParams(proc_),
                                     true /* ipv6_reachable */, is_async,
                                     false /* ipv4_reachable */);
  proc_->AddRule("ipv4only.arpa", ADDRESS_FAMILY_IPV6,
                 "64:ff9b::c000:aa,64:ff9b::c000:ab,2001:db8:43::c000:aa,"
                 "2001:db8:43::c000:ab");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("192.168.1.42", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.top_level_result_error(), IsOk());
  EXPECT_THAT(
      response.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(CreateExpected("64:ff9b::c0a8:12a", 80),
                           CreateExpected("2001:db8:43::c0a8:12a", 80)));
  EXPECT_THAT(
      response.request()->GetEndpointResults(),
      testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
          testing::ElementsAre(CreateExpected("64:ff9b::c0a8:12a", 80),
                               CreateExpected("2001:db8:43::c0a8:12a", 80))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());

  ASSERT_TRUE(!proc_->GetCaptureList().empty());
  EXPECT_EQ("ipv4only.arpa", proc_->GetCaptureList()[0].hostname);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(HostCache::Key(
          "ipv4only.arpa", DnsQueryType::AAAA, 0 /* host_resolver_flags */,
          HostResolverSource::ANY, NetworkAnonymizationKey()));
  EXPECT_TRUE(cache_result);
}

TEST_F(HostResolverManagerTest, IPv4AddressLiteralInIPv6OnlyNetworkAsync) {
  IPv4AddressLiteralInIPv6OnlyNetworkTest(true);
}

TEST_F(HostResolverManagerTest, IPv4AddressLiteralInIPv6OnlyNetworkSync) {
  IPv4AddressLiteralInIPv6OnlyNetworkTest(false);
}

void HostResolverManagerTest::IPv4AddressLiteralInIPv6OnlyNetworkPort443Test(
    bool is_async) {
  HostResolver::ManagerOptions options = DefaultOptions();
  CreateResolverWithOptionsAndParams(std::move(options), DefaultParams(proc_),
                                     true /* ipv6_reachable */, is_async,
                                     false /* ipv4_reachable */);
  proc_->AddRule("ipv4only.arpa", ADDRESS_FAMILY_IPV6,
                 "64:ff9b::c000:aa,64:ff9b::c000:ab,2001:db8:43::c000:aa,"
                 "2001:db8:43::c000:ab");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("192.168.1.42", 443), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.top_level_result_error(), IsOk());
  EXPECT_THAT(
      response.request()->GetAddressResults()->endpoints(),
      testing::ElementsAre(CreateExpected("64:ff9b::c0a8:12a", 443),
                           CreateExpected("2001:db8:43::c0a8:12a", 443)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(
                  ExpectEndpointResult(testing::ElementsAre(
                      CreateExpected("64:ff9b::c0a8:12a", 443),
                      CreateExpected("2001:db8:43::c0a8:12a", 443))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());

  ASSERT_TRUE(!proc_->GetCaptureList().empty());
  EXPECT_EQ("ipv4only.arpa", proc_->GetCaptureList()[0].hostname);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      GetCacheHit(HostCache::Key(
          "ipv4only.arpa", DnsQueryType::AAAA, 0 /* host_resolver_flags */,
          HostResolverSource::ANY, NetworkAnonymizationKey()));
  EXPECT_TRUE(cache_result);
}

TEST_F(HostResolverManagerTest,
       IPv4AddressLiteralInIPv6OnlyNetworkPort443Async) {
  IPv4AddressLiteralInIPv6OnlyNetworkPort443Test(true);
}

TEST_F(HostResolverManagerTest,
       IPv4AddressLiteralInIPv6OnlyNetworkPort443Sync) {
  IPv4AddressLiteralInIPv6OnlyNetworkPort443Test(false);
}

void HostResolverManagerTest::IPv4AddressLiteralInIPv6OnlyNetworkNoDns64Test(
    bool is_async) {
  HostResolver::ManagerOptions options = DefaultOptions();
  CreateResolverWithOptionsAndParams(std::move(options), DefaultParams(proc_),
                                     true /* ipv6_reachable */, is_async,
                                     false /* ipv4_reachable */);
  proc_->AddRule("ipv4only.arpa", ADDRESS_FAMILY_IPV6, std::string());
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("192.168.1.42", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.top_level_result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}

TEST_F(HostResolverManagerTest,
       IPv4AddressLiteralInIPv6OnlyNetworkNoDns64Async) {
  IPv4AddressLiteralInIPv6OnlyNetworkNoDns64Test(true);
}

TEST_F(HostResolverManagerTest,
       IPv4AddressLiteralInIPv6OnlyNetworkNoDns64Sync) {
  IPv4AddressLiteralInIPv6OnlyNetworkNoDns64Test(false);
}

void HostResolverManagerTest::IPv4AddressLiteralInIPv6OnlyNetworkBadAddressTest(
    bool is_async) {
  HostResolver::ManagerOptions options = DefaultOptions();
  CreateResolverWithOptionsAndParams(std::move(options), DefaultParams(proc_),
                                     true /* ipv6_reachable */, is_async,
                                     false /* ipv4_reachable */);
  proc_->AddRule("ipv4only.arpa", ADDRESS_FAMILY_IPV6, "2001:db8::1");
  proc_->SignalMultiple(1u);

  ResolveHostResponseHelper response(resolver_->CreateRequest(
      HostPortPair("192.168.1.42", 80), NetworkAnonymizationKey(),
      NetLogWithSource(), std::nullopt, resolve_context_.get()));

  EXPECT_THAT(response.result_error(), IsOk());
  EXPECT_THAT(response.top_level_result_error(), IsOk());
  EXPECT_THAT(response.request()->GetAddressResults()->endpoints(),
              testing::ElementsAre(CreateExpected("192.168.1.42", 80)));
  EXPECT_THAT(response.request()->GetEndpointResults(),
              testing::Pointee(testing::ElementsAre(ExpectEndpointResult(
                  testing::ElementsAre(CreateExpected("192.168.1.42", 80))))));
  EXPECT_FALSE(response.request()->GetStaleInfo());
}
// Test when DNS returns bad IPv6 address of ipv4only.arpa., and the
// IPv4 address of ipv4only.arpa is not contained in the IPv6 address.
TEST_F(HostResolverManagerTest,
       IPv4AddressLiteralInIPv6OnlyNetworkBadAddressAsync) {
  IPv4AddressLiteralInIPv6OnlyNetworkBadAddressTest(true);
}

TEST_F(HostResolverManagerTest,
       IPv4AddressLiteralInIPv6OnlyNetworkBadAddressSync) {
  IPv4AddressLiteralInIPv6OnlyNetworkBadAddressTest(false);
}

}  // namespace net
