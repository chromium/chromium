// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_HOST_RESOLVER_H_
#define NET_DNS_MOCK_HOST_RESOLVER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/public/dns_query_type.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class HostCache;
class HostPortPair;
class IPEndPoint;
class RuleBasedHostResolverProc;
class URLRequestContext;

// Fills |*addrlist| with a socket address for |host_list| which should be a
// comma-separated list of IPv4 or IPv6 literal(s) without enclosing brackets.
// If |canonical_name| is non-empty it is used as the DNS canonical name for
// the host. Returns OK on success, ERR_UNEXPECTED otherwise.
int ParseAddressList(const std::string& host_list,
                     const std::string& canonical_name,
                     AddressList* addrlist);

// In most cases, it is important that unit tests avoid relying on making actual
// DNS queries since the resulting tests can be flaky, especially if the network
// is unreliable for some reason.  To simplify writing tests that avoid making
// actual DNS queries, pass a MockHostResolver as the HostResolver dependency.
// The socket addresses returned can be configured using the
// RuleBasedHostResolverProc:
//
//   host_resolver->rules()->AddRule("foo.com", "1.2.3.4");
//   host_resolver->rules()->AddRule("bar.com", "2.3.4.5");
//
// The above rules define a static mapping from hostnames to IP address
// literals.  The first parameter to AddRule specifies a host pattern to match
// against, and the second parameter indicates what value should be used to
// replace the given hostname.  So, the following is also supported:
//
//   host_mapper->AddRule("*.com", "127.0.0.1");
//
// Replacement doesn't have to be string representing an IP address. It can
// re-map one hostname to another as well.
//
// By default, MockHostResolvers include a single rule that maps all hosts to
// 127.0.0.1.
//
// Separate rules are used for separate HostResolverSource (eg
// HostResolverSource::SYSTEM for requests that should only be resolved using
// the system resolver).  Use rules_map() to access the separate rules if tests
// involve requests specifying sources:
//
//    host_resolver->rules_map()[HostResolverSource::DNS]->AddRule("foo.com",
//                                                                 "1.2.3.4");

// Base class shared by MockHostResolver and MockCachingHostResolver.
class MockHostResolverBase
    : public HostResolver,
      public base::SupportsWeakPtr<MockHostResolverBase> {
 private:
  class RequestImpl;
  class ProbeRequestImpl;
  class MdnsListenerImpl;

 public:
  ~MockHostResolverBase() override;

  RuleBasedHostResolverProc* rules() {
    return rules_map_[HostResolverSource::ANY].get();
  }
  void set_rules(RuleBasedHostResolverProc* rules) {
    rules_map_[HostResolverSource::ANY] = rules;
  }
  std::map<HostResolverSource, scoped_refptr<RuleBasedHostResolverProc>>
  rules_map() {
    return rules_map_;
  }

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
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;
  std::unique_ptr<ProbeRequest> CreateDohProbeRequest() override;
  std::unique_ptr<MdnsListener> CreateMdnsListener(
      const HostPortPair& host,
      DnsQueryType query_type) override;
  HostCache* GetHostCache() override;
  void SetRequestContext(URLRequestContext* request_context) override {}

  // Preloads the cache with what would currently be the result of a request
  // with the given parameters. Returns the net error of the cached result.
  int LoadIntoCache(
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      const base::Optional<ResolveHostParameters>& optional_parameters);

  // Returns true if there are pending requests that can be resolved by invoking
  // ResolveAllPending().
  bool has_pending_requests() const { return !requests_.empty(); }

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
  const std::string& request_host(size_t id);

  // Returns the priority of the request with the given id.
  RequestPriority request_priority(size_t id);

  // Returns NetworkIsolationKey of the request with the given id.
  const NetworkIsolationKey& request_network_isolation_key(size_t id);

  // Like ResolveNow, but doesn't take an ID. DCHECKs if there's more than one
  // pending request.
  void ResolveOnlyRequestNow();

  // The number of times that Resolve() has been called.
  size_t num_resolve() const {
    return num_resolve_;
  }

  // The number of times that ResolveFromCache() has been called.
  size_t num_resolve_from_cache() const {
    return num_resolve_from_cache_;
  }

  // The number of times resolve was attempted non-locally.
  size_t num_non_local_resolves() const { return num_non_local_resolves_; }

  // Returns the RequestPriority of the last call to Resolve() (or
  // DEFAULT_PRIORITY if Resolve() hasn't been called yet).
  RequestPriority last_request_priority() const {
    return last_request_priority_;
  }

  // Returns the NetworkIsolationKey passed in to the last call to Resolve() (or
  // base::nullopt if Resolve() hasn't been called yet).
  const base::Optional<NetworkIsolationKey>&
  last_request_network_isolation_key() {
    return last_request_network_isolation_key_;
  }

  // Returns the SecureDnsMode override of the last call to Resolve() (or
  // base::nullopt if Resolve() hasn't been called yet).
  const base::Optional<DnsConfig::SecureDnsMode>&
  last_secure_dns_mode_override() const {
    return last_secure_dns_mode_override_;
  }

  bool IsDohProbeRunning() const { return !!doh_probe_request_; }

  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListener::Delegate::UpdateType update_type,
                            const IPEndPoint& address_result);
  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListener::Delegate::UpdateType update_type,
                            const std::vector<std::string>& text_result);
  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListener::Delegate::UpdateType update_type,
                            const HostPortPair& host_result);
  void TriggerMdnsListeners(const HostPortPair& host,
                            DnsQueryType query_type,
                            MdnsListener::Delegate::UpdateType update_type);

  void set_tick_clock(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  friend class MockHostResolver;
  friend class MockCachingHostResolver;
  friend class MockHostResolverFactory;

  typedef std::map<size_t, RequestImpl*> RequestMap;

  // Returns the request with the given id.
  RequestImpl* request(size_t id);

  // If > 0, |cache_invalidation_num| is the number of times a cached entry can
  // be read before it invalidates itself. Useful to force cache expiration
  // scenarios.
  explicit MockHostResolverBase(bool use_caching, int cache_invalidation_num);

  // Handle resolution for |request|. Expected to be called only the RequestImpl
  // object itself.
  int Resolve(RequestImpl* request);

  // Resolve as IP or from |cache_| return cached error or
  // DNS_CACHE_MISS if failed.
  int ResolveFromIPLiteralOrCache(
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      DnsQueryType dns_query_type,
      HostResolverFlags flags,
      HostResolverSource source,
      HostResolver::ResolveHostParameters::CacheUsage cache_usage,
      AddressList* addresses,
      base::Optional<HostCache::EntryStaleness>* stale_info);
  // Resolve via |proc_|.
  int ResolveProc(const HostPortPair& host,
                  const NetworkIsolationKey& network_isolation_key,
                  AddressFamily requested_address_family,
                  HostResolverFlags flags,
                  HostResolverSource source,
                  AddressList* addresses);

  void AddListener(MdnsListenerImpl* listener);
  void RemoveCancelledListener(MdnsListenerImpl* listener);

  RequestPriority last_request_priority_;
  base::Optional<NetworkIsolationKey> last_request_network_isolation_key_;
  base::Optional<DnsConfig::SecureDnsMode> last_secure_dns_mode_override_;
  bool synchronous_mode_;
  bool ondemand_mode_;
  std::map<HostResolverSource, scoped_refptr<RuleBasedHostResolverProc>>
      rules_map_;
  std::unique_ptr<HostCache> cache_;

  const int initial_cache_invalidation_num_;
  std::map<HostCache::Key, int> cache_invalidation_nums_;

  // Maintain non-owning pointers to outstanding requests and listeners to allow
  // completing/notifying them. The objects are owned by callers, and should be
  // removed from |this| on destruction by calling DetachRequest() or
  // RemoveCancelledListener().
  RequestMap requests_;
  size_t next_request_id_;
  ProbeRequestImpl* doh_probe_request_ = nullptr;
  std::set<MdnsListenerImpl*> listeners_;

  size_t num_resolve_;
  size_t num_resolve_from_cache_;
  size_t num_non_local_resolves_;

  const base::TickClock* tick_clock_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MockHostResolverBase);
};

class MockHostResolver : public MockHostResolverBase {
 public:
  MockHostResolver()
      : MockHostResolverBase(false /*use_caching*/,
                             0 /* cache_invalidation_num */) {}
  ~MockHostResolver() override {}
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
  explicit MockCachingHostResolver(int cache_invalidation_num = 0)
      : MockHostResolverBase(true /*use_caching*/, cache_invalidation_num) {}
  ~MockCachingHostResolver() override {}
};

// Factory that will always create and return Mock(Caching)HostResolvers.
//
// The default behavior is to create a non-caching mock, even if the tested code
// requests caching enabled (via the |enable_caching| parameter in the creation
// methods). A caching mock will only be created if both |use_caching| is set on
// factory construction and |enable_caching| is set in the creation method.
class MockHostResolverFactory : public HostResolver::Factory {
 public:
  MockHostResolverFactory(
      scoped_refptr<RuleBasedHostResolverProc> rules = nullptr,
      bool use_caching = false,
      int cache_invalidation_num = 0);
  ~MockHostResolverFactory() override;

  std::unique_ptr<HostResolver> CreateResolver(
      HostResolverManager* manager,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override;
  std::unique_ptr<HostResolver> CreateStandaloneResolver(
      NetLog* net_log,
      const HostResolver::ManagerOptions& options,
      base::StringPiece host_mapping_rules,
      bool enable_caching) override;

 private:
  const scoped_refptr<RuleBasedHostResolverProc> rules_;
  const bool use_caching_;
  const int cache_invalidation_num_;

  DISALLOW_COPY_AND_ASSIGN(MockHostResolverFactory);
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
  explicit RuleBasedHostResolverProc(HostResolverProc* previous);

  // Any hostname matching the given pattern will be replaced with the given
  // |ip_literal|.
  void AddRule(const std::string& host_pattern, const std::string& ip_literal);

  // Same as AddRule(), but further restricts to |address_family|.
  void AddRuleForAddressFamily(const std::string& host_pattern,
                               AddressFamily address_family,
                               const std::string& ip_literal);

  void AddRuleWithFlags(const std::string& host_pattern,
                        const std::string& ip_literal,
                        HostResolverFlags flags,
                        const std::string& canonical_name = "");

  // Same as AddRule(), but the replacement is expected to be an IPv4 or IPv6
  // literal. This can be used in place of AddRule() to bypass the system's
  // host resolver (the address list will be constructed manually).
  // If |canonical_name| is non-empty, it is copied to the resulting AddressList
  // but does not impact DNS resolution.
  // |ip_literal| can be a single IP address like "192.168.1.1" or a comma
  // separated list of IP addresses, like "::1,192:168.1.2".
  void AddIPLiteralRule(const std::string& host_pattern,
                        const std::string& ip_literal,
                        const std::string& canonical_name);

  void AddRuleWithLatency(const std::string& host_pattern,
                          const std::string& replacement,
                          int latency_ms);

  // Make sure that |host| will not be re-mapped or even processed by underlying
  // host resolver procedures. It can also be a pattern.
  void AllowDirectLookup(const std::string& host);

  // Simulate a lookup failure for |host| (it also can be a pattern).
  void AddSimulatedFailure(const std::string& host);

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
    enum ResolverType {
      kResolverTypeFail,
      // TODO(mmenke): Is it really reasonable for a "mock" host resolver to
      // fall back to the system resolver?
      kResolverTypeSystem,
      kResolverTypeIPLiteral,
    };

    Rule(ResolverType resolver_type,
         const std::string& host_pattern,
         AddressFamily address_family,
         HostResolverFlags host_resolver_flags,
         const std::string& replacement,
         const std::string& canonical_name,
         int latency_ms);
    Rule(const Rule& other);

    ResolverType resolver_type;
    std::string host_pattern;
    AddressFamily address_family;
    HostResolverFlags host_resolver_flags;
    std::string replacement;
    std::string canonical_name;
    int latency_ms;  // In milliseconds.
  };

  typedef std::list<Rule> RuleList;

  RuleList GetRules();

 private:
  ~RuleBasedHostResolverProc() override;

  void AddRuleInternal(const Rule& rule);

  RuleList rules_;

  // Must be obtained before writing to or reading from |rules_|.
  base::Lock rule_lock_;

  // Whether changes are allowed.
  bool modifications_allowed_;
};

// Create rules that map all requests to localhost.
RuleBasedHostResolverProc* CreateCatchAllHostResolverProc();

// HangingHostResolver never completes its |Resolve| request. As LOCAL_ONLY
// requests are not allowed to complete asynchronously, they will always result
// in |ERR_DNS_CACHE_MISS|.
class HangingHostResolver : public HostResolver {
 public:
  HangingHostResolver();
  ~HangingHostResolver() override;
  void OnShutdown() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;

  std::unique_ptr<ProbeRequest> CreateDohProbeRequest() override;

  // Use to detect cancellations since there's otherwise no externally-visible
  // differentiation between a cancelled and a hung task.
  int num_cancellations() const { return num_cancellations_; }

  // Return the corresponding values passed to the most recent call to
  // CreateRequest()
  const HostPortPair& last_host() const { return last_host_; }
  const NetworkIsolationKey& last_network_isolation_key() const {
    return last_network_isolation_key_;
  }

 private:
  class RequestImpl;
  class ProbeRequestImpl;

  HostPortPair last_host_;
  NetworkIsolationKey last_network_isolation_key_;

  int num_cancellations_ = 0;
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
