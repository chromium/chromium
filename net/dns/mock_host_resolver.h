// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_HOST_RESOLVER_H_
#define NET_DNS_MOCK_HOST_RESOLVER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_source.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class HostCache;
class RuleBasedHostResolverProc;

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
  class LegacyRequestImpl;

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
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;
  int Resolve(const RequestInfo& info,
              RequestPriority priority,
              AddressList* addresses,
              CompletionOnceCallback callback,
              std::unique_ptr<Request>* request,
              const NetLogWithSource& net_log) override;
  int ResolveFromCache(const RequestInfo& info,
                       AddressList* addresses,
                       const NetLogWithSource& net_log) override;
  int ResolveStaleFromCache(const RequestInfo& info,
                            AddressList* addresses,
                            HostCache::EntryStaleness* stale_info,
                            const NetLogWithSource& source_net_log) override;
  HostCache* GetHostCache() override;
  bool HasCached(base::StringPiece hostname,
                 HostCache::Entry::Source* source_out,
                 HostCache::EntryStaleness* stale_out) const override;
  void SetDnsConfigOverrides(const DnsConfigOverrides& overrides) override {}

  // Detach cancelled request.
  void DetachRequest(size_t id);

  // Resolves all pending requests. It is only valid to invoke this if
  // set_ondemand_mode was set before. The requests are resolved asynchronously,
  // after this call returns.
  void ResolveAllPending();

  // Returns true if there are pending requests that can be resolved by invoking
  // ResolveAllPending().
  bool has_pending_requests() const { return !requests_.empty(); }

  // The number of times that Resolve() has been called.
  size_t num_resolve() const {
    return num_resolve_;
  }

  // The number of times that ResolveFromCache() has been called.
  size_t num_resolve_from_cache() const {
    return num_resolve_from_cache_;
  }

  // Returns the RequestPriority of the last call to Resolve() (or
  // DEFAULT_PRIORITY if Resolve() hasn't been called yet).
  RequestPriority last_request_priority() const {
    return last_request_priority_;
  }

  void set_tick_clock(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 protected:
  explicit MockHostResolverBase(bool use_caching);

 private:
  typedef std::map<size_t, RequestImpl*> RequestMap;

  // Handle resolution for |request|. Expected to be called only the RequestImpl
  // object itself.
  int Resolve(RequestImpl* request);

  // Resolve as IP or from |cache_| return cached error or
  // DNS_CACHE_MISS if failed.
  int ResolveFromIPLiteralOrCache(
      const HostPortPair& host,
      AddressFamily requested_address_family,
      HostResolverFlags flags,
      HostResolverSource source,
      bool allow_cache,
      AddressList* addresses,
      HostCache::EntryStaleness* stale_info = nullptr);
  // Resolve via |proc_|.
  int ResolveProc(const HostPortPair& host,
                  AddressFamily requested_address_family,
                  HostResolverFlags flags,
                  HostResolverSource source,
                  AddressList* addresses);
  // Resolve request stored in |requests_|. Pass rv to callback.
  void ResolveNow(size_t id);

  RequestPriority last_request_priority_;
  bool synchronous_mode_;
  bool ondemand_mode_;
  std::map<HostResolverSource, scoped_refptr<RuleBasedHostResolverProc>>
      rules_map_;
  std::unique_ptr<HostCache> cache_;
  RequestMap requests_;
  size_t next_request_id_;

  size_t num_resolve_;
  size_t num_resolve_from_cache_;

  const base::TickClock* tick_clock_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MockHostResolverBase);
};

class MockHostResolver : public MockHostResolverBase {
 public:
  MockHostResolver() : MockHostResolverBase(false /*use_caching*/) {}
  ~MockHostResolver() override {}
};

// Same as MockHostResolver, except internally it uses a host-cache.
//
// Note that tests are advised to use MockHostResolver instead, since it is
// more predictable. (MockHostResolver also can be put into synchronous
// operation mode in case that is what you needed from the caching version).
class MockCachingHostResolver : public MockHostResolverBase {
 public:
  MockCachingHostResolver() : MockHostResolverBase(true /*use_caching*/) {}
  ~MockCachingHostResolver() override {}
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

// HangingHostResolver never completes its |Resolve| request.
class HangingHostResolver : public HostResolver {
 public:
  HangingHostResolver();
  ~HangingHostResolver() override;
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;
  int Resolve(const RequestInfo& info,
              RequestPriority priority,
              AddressList* addresses,
              CompletionOnceCallback callback,
              std::unique_ptr<Request>* out_req,
              const NetLogWithSource& net_log) override;
  int ResolveFromCache(const RequestInfo& info,
                       AddressList* addresses,
                       const NetLogWithSource& net_log) override;
  int ResolveStaleFromCache(const RequestInfo& info,
                            AddressList* addresses,
                            HostCache::EntryStaleness* stale_info,
                            const NetLogWithSource& source_net_log) override;
  bool HasCached(base::StringPiece hostname,
                 HostCache::Entry::Source* source_out,
                 HostCache::EntryStaleness* stale_out) const override;

  // Use to detect cancellations since there's otherwise no externally-visible
  // differentiation between a cancelled and a hung task.
  int num_cancellations() const { return num_cancellations_; }

 private:
  class RequestImpl;

  int num_cancellations_ = 0;
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
