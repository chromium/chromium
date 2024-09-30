// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_HOST_RESOLVER_H_
#define NET_DNS_MOCK_HOST_RESOLVER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class HostCache;
class IPEndPoint;
class URLRequestContext;

// Fills `ip_endpoints` with a socket address for `host_list` which should be a
// comma-separated list of IPv4 or IPv6 literal(s) without enclosing brackets.
int ParseAddressList(std::string_view host_list,
                     std::vector<net::IPEndPoint>* ip_endpoints);

// In most cases, it is important that unit tests avoid relying on making actual
// DNS queries since the resulting tests can be flaky, especially if the network
// is unreliable for some reason.  To simplify writing tests that avoid making
// actual DNS queries, pass a MockHostResolver as the HostResolver dependency.
// The socket addresses returned can be configured using the
// MockHostResolverBase::RuleResolver:
//
//   host_resolver->rules()->AddRule("foo.com", "1.2.3.4");
//   host_resolver->rules()->AddRule("bar.com", "2.3.4.5");
//
// The above rules define a static mapping from hostnames to IP address
// literals.  The first parameter to AddRule specifies a host pattern to match
// against, and the second parameter indicates what IP address should be used to
// replace the given hostname.  So, the following is also supported:
//
//   host_mapper->AddRule("*.com", "127.0.0.1");
//
// For more advanced matching, the first parameter may be replaced with a
// MockHostResolverBase::RuleResolver::RuleKey. For more advanced responses, the
// second parameter may be replaced with a
// MockHostResolverBase::RuleResolver::RuleResultOrError.
//
// MockHostResolvers may optionally be created with a default result:
//
//   MockHostResolver(ERR_NAME_NOT_RESOLVED);
//   MockHostResolver(AddressList(ip_endpoint));
//   MockHostResolver(MockHostResolverBase::RuleResolver::GetLocalhostResult());
//
// If no default result is given, every resolve request must match a configured
// rule, otherwise DCHECKs will fire.

// Base class shared by MockHostResolver and MockCachingHostResolver.
class MockHostResolverBase : public HostResolver {
 private:
  class RequestBase;
  class RequestImpl;
  class ServiceEndpointRequestImpl;
  class ProbeRequestImpl;
  class MdnsListenerImpl;

 public:
  class RuleResolver {
   public:
    struct RuleKey {
      struct WildcardScheme : absl::monostate {};
      struct NoScheme : absl::monostate {};
      using Scheme = std::string;

      RuleKey();
      ~RuleKey();

      RuleKey(const RuleKey&);
      RuleKey& operator=(const RuleKey&);
      RuleKey(RuleKey&&);
      RuleKey& operator=(RuleKey&&);

      auto GetTuple() const {
        return std::tie(scheme, hostname_pattern, port, query_type,
                        query_source);
      }

      bool operator<(const RuleKey& other) const {
        return GetTuple() < other.GetTuple();
      }

      // If `WildcardScheme`, scheme is wildcard and any query will match,
      // whether made with url::SchemeHostPort or HostPortPair. If `NoScheme`,
      // queries will only match if made using HostPortPair. Else, queries will
      // only match if made using url::SchemeHostPort with matching scheme
      // value.
      absl::variant<WildcardScheme, NoScheme, Scheme> scheme = WildcardScheme();

      // Pattern matched via `base::MatchPattern()`.
      std::string hostname_pattern = "*";

      // `nullopt` represents wildcard and all queries will match.
      std::optional<uint16_t> port;
      std::optional<DnsQueryType> query_type;
      std::optional<HostResolverSource> query_source;
    };

    struct RuleResult {
      RuleResult();
      explicit RuleResult(
          std::vector<HostResolverEndpointResult> endpoints,
          std::set<std::string> aliases = std::set<std::string>());

      ~RuleResult();

      RuleResult(const RuleResult&);
      RuleResult& operator=(const RuleResult&);
      RuleResult(RuleResult&&);
      RuleResult& operator=(RuleResult&&);

      std::vector<HostResolverEndpointResult> endpoints;
      std::set<std::string> aliases;
    };

    using ErrorResult = Error;
    using RuleResultOrError = absl::variant<RuleResult, ErrorResult>;

    // If `default_result` is nullopt, every resolve must match an added rule.
    explicit RuleResolver(
        std::optional<RuleResultOrError> default_result = std::nullopt);
    ~RuleResolver();

    RuleResolver(const RuleResolver&);
    RuleResolver& operator=(const RuleResolver&);
    RuleResolver(RuleResolver&&);
    RuleResolver& operator=(RuleResolver&&);

    const RuleResultOrError& Resolve(const Host& request_endpoint,
                                     DnsQueryTypeSet request_types,
                                     HostResolverSource request_source) const;

    void ClearRules();

    static RuleResultOrError GetLocalhostResult();

    void AddRule(RuleKey key, RuleResultOrError result);
    void AddRule(RuleKey key, std::string_view ip_literal);

    void AddRule(std::string_view hostname_pattern, RuleResultOrError result);
    void AddRule(std::string_view hostname_pattern,
                 std::string_view ip_literal);

    void AddRule(std::string_view hostname_pattern, Error error);

    // Legacy rule creation. Only for compatibility with tests written for use
    // with RuleBasedHostResolverProc. New code should use the AddRule() calls
    // above.
    void AddIPLiteralRule(std::string_view hostname_pattern,
                          std::string_view ip_literal,
                          std::string_view canonical_name);
    void AddIPLiteralRuleWithDnsAliases(std::string_view hostname_pattern,
                                        std::string_view ip_literal,
                                        std::vector<std::string> dns_aliases);
    void AddIPLiteralRuleWithDnsAliases(std::string_view hostname_pattern,
                                        std::string_view ip_literal,
                                        std::set<std::string> dns_aliases);
    void AddSimulatedFailure(std::string_view hostname_pattern);
    void AddSimulatedTimeoutFailure(std::string_view hostname_pattern);
    void AddRuleWithFlags(std::string_view host_pattern,
                          std::string_view ip_literal,
                          HostResolverFlags flags,
                          std::vector<std::string> dns_aliases = {});

   private:
    std::map<RuleKey, RuleResultOrError> rules_;
    std::optional<RuleResultOrError> default_result_;
  };

  using RequestMap = std::map<size_t, raw_ptr<RequestBase, CtnExperimental>>;

  // A set of states in MockHostResolver. This is used to observe the internal
  // state variables after destructing a MockHostResolver.
  class State : public base::RefCounted<State> {
   public:
    State();

    bool has_pending_requests() const { return !requests_.empty(); }
    bool IsDohProbeRunning() const { return !!doh_probe_request_; }
    size_t num_resolve() const { return num_resolve_; }
    size_t num_resolve_from_cache() const { return num_resolve_from_cache_; }
    size_t num_non_local_resolves() const { return num_non_local_resolves_; }

    RequestMap& mutable_requests() { return requests_; }
    void IncrementNumResolve() { ++num_resolve_; }
    void IncrementNumResolveFromCache() { ++num_resolve_from_cache_; }
    void IncrementNumNonLocalResolves() { ++num_non_local_resolves_; }
    void ClearDohProbeRequest() { doh_probe_request_ = nullptr; }
    void ClearDohProbeRequestIfMatching(ProbeRequestImpl* request) {
      if (request == doh_probe_request_) {
        doh_probe_request_ = nullptr;
      }
    }
    void set_doh_probe_request(ProbeRequestImpl* request) {
      DCHECK(request);
      DCHECK(!doh_probe_request_);
      doh_probe_request_ = request;
    }

   private:
    friend class RefCounted<State>;

    ~State();

    // Maintain non-owning pointers to outstanding requests and listeners to
    // allow completing/notifying them. The objects are owned by callers, and
    // should be removed from |this| on destruction by calling DetachRequest()
    // or RemoveCancelledListener().
    RequestMap requests_;
    raw_ptr<ProbeRequestImpl> doh_probe_request_ = nullptr;
    size_t num_resolve_ = 0;
    size_t num_resolve_from_cache_ = 0;
    size_t num_non_local_resolves_ = 0;
  };

  MockHostResolverBase(const MockHostResolverBase&) = delete;
  MockHostResolverBase& operator=(const MockHostResolverBase&) = delete;

  ~MockHostResolverBase() override;

  RuleResolver* rules() { return &rule_resolver_; }

  scoped_refptr<const State> state() const { return state_; }

  // Controls whether resolutions complete synchronously or asynchronously.
  void set_synchronous_mode(bool is_synchronous) {
    synchronous_mode_ = is_synchronous;
  }

  // Asynchronous requests are automatically resolved by default.
  // If set_ondemand_mode() is set then Resolve() returns IO_PENDING and
  // ResolveAllPending() must be explicitly invoked to resolve all requests
  // that are pending.
  void set_ondemand_mode(bool is_ondemand) {
    ondemand_mode_ = is_ondemand;
  }

  // HostResolver methods:
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;
  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;
  std::unique_ptr<ProbeRequest> CreateDohProbeRequest() override;
  std::unique_ptr<MdnsListener> CreateMdnsListener(
      const HostPortPair& host,
      DnsQueryType query_type) override;
  HostCache* GetHostCache() override;
  void SetRequestContext(URLRequestContext* request_context) override {}

  // Preloads the cache with what would currently be the result of a request
  // with the given parameters. Returns the net error of the cached result.
  int LoadIntoCache(
      absl::variant<url::SchemeHostPort, HostPortPair> endpoint,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::optional<ResolveHostParameters>& optional_parameters);
  int LoadIntoCache(
      const Host& endpoint,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::optional<ResolveHostParameters>& optional_parameters);

  // Returns true if there are pending requests that can be resolved by invoking
  // ResolveAllPending().
  bool has_pending_requests() const { return state_->has_pending_requests(); }

  // Resolves all pending requests. It is only valid to invoke this if
  // set_ondemand_mode was set before. The requests are resolved asynchronously,
  // after this call returns.
  void ResolveAllPending();

  // Each request is assigned an ID when started and stored with the resolver
  // for async resolution, starting with 1. IDs are not reused. Once a request
  // completes, it is destroyed, and can no longer be accessed.

  // Returns the ID of the most recently started still-active request. Zero if
  // no requests are currently active.
  size_t last_id();

  // Resolve request stored in |requests_|. Pass rv to callback.
  void ResolveNow(size_t id);

  // Detach cancelled request.
  void DetachRequest(size_t id);

  // Returns the hostname of the request with the given id.
  std::string_view request_host(size_t id);

  // Returns the priority of the request with the given id.
  RequestPriority request_priority(size_t id);

  // Returns NetworkAnonymizationKey of the request with the given id.
  const NetworkAnonymizationKey& request_network_anonymization_key(size_t id);

  // Like ResolveNow, but doesn't take an ID. DCHECKs if there's more than one
  // pending request.
  void ResolveOnlyRequestNow();

  // The number of times that Resolve() has been called.
  size_t num_resolve() const { return state_->num_resolve(); }

  // The number of times that ResolveFromCache() has been called.
  size_t num_resolve_from_cache() const {
    return state_->num_resolve_from_cache();
  }

  // The number of times resolve was attempted non-locally.
  size_t num_non_local_resolves() const {
    return state_->num_non_local_resolves();
  }

  // Returns the RequestPriority of the last call to Resolve() (or
  // DEFAULT_PRIORITY if Resolve() hasn't been called yet).
  RequestPriority last_request_priority() const {
    return last_request_priority_;
  }

  // Returns the NetworkAnonymizationKey passed in to the last call to Resolve()
  // (or std::nullopt if Resolve() hasn't been called yet).
  const std::optional<NetworkAnonymizationKey>&
  last_request_network_anonymization_key() {
    return last_request_network_anonymization_key_;
  }

  // Returns the SecureDnsPolicy of the last call to Resolve() (or
  // std::nullopt if Resolve() hasn't been called yet).
  SecureDnsPolicy last_secure_dns_policy() const {
    return last_secure_dns_policy_;
  }

  bool IsDohProbeRunning() const { return state_->IsDohProbeRunning(); }

  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListenerUpdateType update_type,
                            const IPEndPoint& address_result);
  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListenerUpdateType update_type,
                            const std::vector<std::string>& text_result);
  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListenerUpdateType update_type,
                            const HostPortPair& host_result);
  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListenerUpdateType update_type);

  void set_tick_clock(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  friend class MockHostResolver;
  friend class MockCachingHostResolver;
  friend class MockHostResolverFactory;

  // Returns the request with the given id.
  RequestBase* request(size_t id);

  // If > 0, |cache_invalidation_num| is the number of times a cached entry can
  // be read before it invalidates itself. Useful to force cache expiration
  // scenarios.
  MockHostResolverBase(bool use_caching,
                       int cache_invalidation_num,
                       RuleResolver rule_resolver);

  // Handle resolution for |request|. Expected to be called only the RequestBase
  // object itself.
  int Resolve(RequestBase* request);

  // Resolve as IP or from |cache_| return cached error or
  // DNS_CACHE_MISS if failed.
  int ResolveFromIPLiteralOrCache(
      const Host& endpoint,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType dns_query_type,
      HostResolverFlags flags,
      HostResolverSource source,
      HostResolver::ResolveHostParameters::CacheUsage cache_usage,
      std::vector<HostResolverEndpointResult>* out_endpoints,
      std::set<std::string>* out_aliases,
      std::optional<HostCache::EntryStaleness>* out_stale_info);
  int DoSynchronousResolution(RequestBase& request);

  void AddListener(MdnsListenerImpl* listener);
  void RemoveCancelledListener(MdnsListenerImpl* listener);

  RequestPriority last_request_priority_ = DEFAULT_PRIORITY;
  std::optional<NetworkAnonymizationKey>
      last_request_network_anonymization_key_;
  SecureDnsPolicy last_secure_dns_policy_ = SecureDnsPolicy::kAllow;
  bool synchronous_mode_ = false;
  bool ondemand_mode_ = false;
  RuleResolver rule_resolver_;
  std::unique_ptr<HostCache> cache_;

  const int initial_cache_invalidation_num_;
  std::map<HostCache::Key, int> cache_invalidation_nums_;

  std::set<raw_ptr<MdnsListenerImpl, SetExperimental>> listeners_;

  size_t next_request_id_ = 1;

  raw_ptr<const base::TickClock> tick_clock_;

  scoped_refptr<State> state_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<MockHostResolverBase> weak_ptr_factory_{this};
};

class MockHostResolver : public MockHostResolverBase {
 public:
  explicit MockHostResolver(std::optional<RuleResolver::RuleResultOrError>
                                default_result = std::nullopt)
      : MockHostResolverBase(/*use_caching=*/false,
                             /*cache_invalidation_num=*/0,
                             RuleResolver(std::move(default_result))) {}
  ~MockHostResolver() override = default;
};

// Same as MockHostResolver, except internally it uses a host-cache.
//
// Note that tests are advised to use MockHostResolver instead, since it is
// more predictable. (MockHostResolver also can be put into synchronous
// operation mode in case that is what you needed from the caching version).
class MockCachingHostResolver : public MockHostResolverBase {
 public:
  // If > 0, |cache_invalidation_num| is the number of times a cached entry can
  // be read before it invalidates itself. Useful to force cache expiration
  // scenarios.
  explicit MockCachingHostResolver(
      int cache_invalidation_num = 0,
      std::optional<RuleResolver::RuleResultOrError> default_result =
          std::nullopt)
      : MockHostResolverBase(/*use_caching=*/true,
                             cache_invalidation_num,
                             RuleResolver(std::move(default_result))) {}
  ~MockCachingHostResolver() override = default;
};

// Factory that will always create and return Mock(Caching)HostResolvers.
//
// The default behavior is to create a non-caching mock, even if the tested code
// requests caching enabled (via the |enable_caching| parameter in the creation
// methods). A caching mock will only be created if both |use_caching| is set on
// factory construction and |enable_caching| is set in the creation method.
class MockHostResolverFactory : public HostResolver::Factory {
 public:
  explicit MockHostResolverFactory(MockHostResolverBase::RuleResolver rules =
                                       MockHostResolverBase::RuleResolver(),
                                   bool use_caching = false,
                                   int cache_invalidation_num = 0);

  MockHostResolverFactory(const MockHostResolverFactory&) = delete;
  MockHostResolverFactory& operator=(const MockHostResolverFactory&) = delete;

  ~MockHostResolverFactory() override;

  std::unique_ptr<HostResolver> CreateResolver(
      HostResolverManager* manager,
      std::string_view host_mapping_rules,
      bool enable_caching) override;
  std::unique_ptr<HostResolver> CreateStandaloneResolver(
      NetLog* net_log,
      const HostResolver::ManagerOptions& options,
      std::string_view host_mapping_rules,
      bool enable_caching) override;

 private:
  const MockHostResolverBase::RuleResolver rules_;
  const bool use_caching_;
  const int cache_invalidation_num_;
};

// RuleBasedHostResolverProc applies a set of rules to map a host string to
// a replacement host string. It then uses the system host resolver to return
// a socket address. Generally the replacement should be an IPv4 literal so
// there is no network dependency.
//
// RuleBasedHostResolverProc is thread-safe, to a limited degree. Rules can be
// added or removed on any thread.
class RuleBasedHostResolverProc : public HostResolverProc {
 public:
  // If `allow_fallback` is false, no Proc fallback is allowed except to
  // `previous`.
  explicit RuleBasedHostResolverProc(scoped_refptr<HostResolverProc> previous,
                                     bool allow_fallback = true);

  // Any hostname matching the given pattern will be replaced with the given
  // |ip_literal|.
  void AddRule(std::string_view host_pattern, std::string_view ip_literal);

  // Same as AddRule(), but further restricts to |address_family|.
  void AddRuleForAddressFamily(std::string_view host_pattern,
                               AddressFamily address_family,
                               std::string_view ip_literal);

  void AddRuleWithFlags(std::string_view host_pattern,
                        std::string_view ip_literal,
                        HostResolverFlags flags,
                        std::vector<std::string> dns_aliases = {});

  // Same as AddRule(), but the replacement is expected to be an IPv4 or IPv6
  // literal. This can be used in place of AddRule() to bypass the system's
  // host resolver (the address list will be constructed manually).
  // If |canonical_name| is non-empty, it is copied to the resulting AddressList
  // but does not impact DNS resolution.
  // |ip_literal| can be a single IP address like "192.168.1.1" or a comma
  // separated list of IP addresses, like "::1,192:168.1.2".
  void AddIPLiteralRule(std::string_view host_pattern,
                        std::string_view ip_literal,
                        std::string_view canonical_name);

  // Same as AddIPLiteralRule, but with a parameter allowing multiple DNS
  // aliases, such as CNAME aliases, instead of only the canonical name. While
  // a simulation using HostResolverProc to obtain more than a single DNS alias
  // is currently unrealistic, this capability is useful for clients of
  // MockHostResolver who need to be able to obtain aliases and can be
  // agnostic about how the host resolution took place, as the alternative,
  // MockDnsClient, is not currently hooked up to MockHostResolver.
  void AddIPLiteralRuleWithDnsAliases(std::string_view host_pattern,
                                      std::string_view ip_literal,
                                      std::vector<std::string> dns_aliases);

  void AddRuleWithLatency(std::string_view host_pattern,
                          std::string_view replacement,
                          int latency_ms);

  // Make sure that |host| will not be re-mapped or even processed by underlying
  // host resolver procedures. It can also be a pattern.
  void AllowDirectLookup(std::string_view host);

  // Simulate a lookup failure for |host| (it also can be a pattern).
  void AddSimulatedFailure(
      std::string_view host,
      HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY);

  // Simulate a lookup timeout failure for |host| (it also can be a pattern).
  void AddSimulatedTimeoutFailure(
      std::string_view host,
      HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY);

  // Deletes all the rules that have been added.
  void ClearRules();

  // Causes method calls that add or delete rules to assert.
  // TODO(jam): once this class isn't used by tests that use an out of process
  // network service, remove this method and make Rule private.
  void DisableModifications();

  // HostResolverProc methods:
  int Resolve(const std::string& host,
              AddressFamily address_family,
              HostResolverFlags host_resolver_flags,
              AddressList* addrlist,
              int* os_error) override;

  struct Rule {
    // TODO(crbug.com/40822747) Deduplicate this enum's definition.
    enum ResolverType {
      kResolverTypeFail,
      kResolverTypeFailTimeout,
      // TODO(mmenke): Is it really reasonable for a "mock" host resolver to
      // fall back to the system resolver?
      kResolverTypeSystem,
      kResolverTypeIPLiteral,
    };

    Rule(ResolverType resolver_type,
         std::string_view host_pattern,
         AddressFamily address_family,
         HostResolverFlags host_resolver_flags,
         std::string_view replacement,
         std::vector<std::string> dns_aliases,
         int latency_ms);
    Rule(const Rule& other);
    ~Rule();

    ResolverType resolver_type;
    std::string host_pattern;
    AddressFamily address_family;
    HostResolverFlags host_resolver_flags;
    std::string replacement;
    std::vector<std::string> dns_aliases;
    int latency_ms;  // In milliseconds.
  };

  typedef std::list<Rule> RuleList;

  RuleList GetRules();

  // Returns the number of calls to Resolve() where |host| matched
  // |host_pattern|.
  size_t NumResolvesForHostPattern(std::string_view host_pattern);

 private:
  ~RuleBasedHostResolverProc() override;

  void AddRuleInternal(const Rule& rule);

  RuleList rules_ GUARDED_BY(rule_lock_);

  // Tracks the number of calls to Resolve() where |host| matches a rule's host
  // pattern.
  std::map<std::string_view, size_t> num_resolves_per_host_pattern_
      GUARDED_BY(rule_lock_);

  // Must be obtained before writing to or reading from |rules_|.
  base::Lock rule_lock_;

  // Whether changes are allowed.
  bool modifications_allowed_ = true;
};

// Create rules that map all requests to localhost.
scoped_refptr<RuleBasedHostResolverProc> CreateCatchAllHostResolverProc();

// HangingHostResolver never completes its |Resolve| request. As LOCAL_ONLY
// requests are not allowed to complete asynchronously, they will always result
// in |ERR_DNS_CACHE_MISS|.
class HangingHostResolver : public HostResolver {
 public:
  // A set of states in HangingHostResolver. This is used to observe the
  // internal state variables after destructing a MockHostResolver.
  class State : public base::RefCounted<State> {
   public:
    State();

    int num_cancellations() const { return num_cancellations_; }

    void IncrementNumCancellations() { ++num_cancellations_; }

   private:
    friend class RefCounted<State>;

    ~State();

    int num_cancellations_ = 0;
  };

  HangingHostResolver();
  ~HangingHostResolver() override;
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) override;
  std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) override;

  std::unique_ptr<ProbeRequest> CreateDohProbeRequest() override;

  void SetRequestContext(URLRequestContext* url_request_context) override;

  // Use to detect cancellations since there's otherwise no externally-visible
  // differentiation between a cancelled and a hung task.
  int num_cancellations() const { return state_->num_cancellations(); }

  // Return the corresponding values passed to the most recent call to
  // CreateRequest()
  const HostPortPair& last_host() const { return last_host_; }
  const NetworkAnonymizationKey& last_network_anonymization_key() const {
    return last_network_anonymization_key_;
  }

  const scoped_refptr<const State> state() const { return state_; }

 private:
  class RequestImpl;
  class ProbeRequestImpl;

  HostPortPair last_host_;
  NetworkAnonymizationKey last_network_anonymization_key_;

  scoped_refptr<State> state_;
  bool shutting_down_ = false;
  base::WeakPtrFactory<HangingHostResolver> weak_ptr_factory_{this};
};

// This class sets the default HostResolverProc for a particular scope.  The
// chain of resolver procs starting at |proc| is placed in front of any existing
// default resolver proc(s).  This means that if multiple
// ScopedDefaultHostResolverProcs are declared, then resolving will start with
// the procs given to the last-allocated one, then fall back to the procs given
// to the previously-allocated one, and so forth.
//
// NOTE: Only use this as a catch-all safety net. Individual tests should use
// MockHostResolver.
class ScopedDefaultHostResolverProc {
 public:
  ScopedDefaultHostResolverProc();
  explicit ScopedDefaultHostResolverProc(HostResolverProc* proc);

  ~ScopedDefaultHostResolverProc();

  void Init(HostResolverProc* proc);

 private:
  scoped_refptr<HostResolverProc> current_proc_;
  scoped_refptr<HostResolverProc> previous_proc_;
};

}  // namespace net

#endif  // NET_DNS_MOCK_HOST_RESOLVER_H_
