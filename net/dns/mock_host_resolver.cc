// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_host_resolver.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/host_cache.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// Cache size for the MockCachingHostResolver.
const unsigned kMaxCacheEntries = 100;
// TTL for the successful resolutions. Failures are not cached.
const unsigned kCacheEntryTTLSeconds = 60;

}  // namespace

int ParseAddressList(const std::string& host_list,
                     const std::string& canonical_name,
                     AddressList* addrlist) {
  *addrlist = AddressList();
  addrlist->set_canonical_name(canonical_name);
  for (const base::StringPiece& address : base::SplitStringPiece(
           host_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(address)) {
      LOG(WARNING) << "Not a supported IP literal: " << address.as_string();
      return ERR_UNEXPECTED;
    }
    addrlist->push_back(IPEndPoint(ip_address, 0));
  }
  return OK;
}

class MockHostResolverBase::RequestImpl
    : public HostResolver::ResolveHostRequest {
 public:
  RequestImpl(const HostPortPair& request_host,
              const base::Optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<MockHostResolverBase> resolver)
      : request_host_(request_host),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        host_resolver_flags_(ParametersToHostResolverFlags(parameters_)),
        id_(0),
        resolver_(resolver),
        complete_(false) {}

  ~RequestImpl() override {
    if (id_ > 0) {
      if (resolver_)
        resolver_->DetachRequest(id_);
      id_ = 0;
      resolver_ = nullptr;
    }
  }

  int Start(CompletionOnceCallback callback) override {
    DCHECK(callback);
    // Start() may only be called once per request.
    DCHECK_EQ(0u, id_);
    DCHECK(!complete_);
    DCHECK(!callback_);
    // Parent HostResolver must still be alive to call Start().
    DCHECK(resolver_);

    int rv = resolver_->Resolve(this);
    DCHECK(!complete_);
    if (rv == ERR_IO_PENDING) {
      DCHECK_GT(id_, 0u);
      callback_ = std::move(callback);
    } else {
      DCHECK_EQ(0u, id_);
      complete_ = true;
    }

    return rv;
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    DCHECK(complete_);
    return address_results_;
  }

  void set_address_results(const AddressList& address_results) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!address_results_);
    DCHECK(!parameters_.is_speculative);

    address_results_ = address_results;
  }

  void OnAsyncCompleted(size_t id, int error) {
    DCHECK_EQ(id_, id);
    id_ = 0;

    DCHECK(!complete_);
    complete_ = true;

    DCHECK(callback_);
    std::move(callback_).Run(error);
  }

  const HostPortPair& request_host() const { return request_host_; }

  const ResolveHostParameters& parameters() const { return parameters_; }

  int host_resolver_flags() const { return host_resolver_flags_; }

  size_t id() { return id_; }

  void set_id(size_t id) {
    DCHECK_GT(id, 0u);
    DCHECK_EQ(0u, id_);

    id_ = id;
  }

  bool complete() { return complete_; }

 private:
  const HostPortPair request_host_;
  const ResolveHostParameters parameters_;
  int host_resolver_flags_;

  base::Optional<AddressList> address_results_;

  // Used while stored with the resolver for async resolution.  Otherwise 0.
  size_t id_;

  CompletionOnceCallback callback_;
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
  bool complete_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

class MockHostResolverBase::LegacyRequestImpl : public HostResolver::Request {
 public:
  explicit LegacyRequestImpl(std::unique_ptr<RequestImpl> inner_request)
      : inner_request_(std::move(inner_request)) {
    DCHECK_EQ(0u, inner_request_->id());
    DCHECK(!inner_request_->complete());
  }

  ~LegacyRequestImpl() override {}

  void ChangeRequestPriority(RequestPriority priority) override {}

  int Start() {
    return inner_request_->Start(base::BindOnce(
        &LegacyRequestImpl::LegacyApiCallback, base::Unretained(this)));
  }

  void AssignCallback(CompletionOnceCallback callback,
                      AddressList* addresses_result_ptr) {
    DCHECK(callback);
    DCHECK(addresses_result_ptr);
    DCHECK_GT(inner_request_->id(), 0u);
    DCHECK(!inner_request_->complete());

    callback_ = std::move(callback);
    addresses_result_ptr_ = addresses_result_ptr;
  }

  const RequestImpl& inner_request() const { return *inner_request_; }

 private:
  void LegacyApiCallback(int error) {
    // Must call AssignCallback() before async results.
    DCHECK(callback_);

    if (error == OK && !inner_request_->parameters().is_speculative) {
      // Legacy API does not allow non-address results (eg TXT), so AddressList
      // is always expected to be present on OK.
      DCHECK(inner_request_->GetAddressResults());
      *addresses_result_ptr_ = inner_request_->GetAddressResults().value();
    }
    addresses_result_ptr_ = nullptr;
    std::move(callback_).Run(error);
  }

  const std::unique_ptr<RequestImpl> inner_request_;

  CompletionOnceCallback callback_;
  // This is a caller-provided pointer and should not be used once |callback_|
  // is invoked.
  AddressList* addresses_result_ptr_;

  DISALLOW_COPY_AND_ASSIGN(LegacyRequestImpl);
};

MockHostResolverBase::~MockHostResolverBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(requests_.empty());
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MockHostResolverBase::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(host, optional_parameters, AsWeakPtr());
}

int MockHostResolverBase::Resolve(const RequestInfo& info,
                                  RequestPriority priority,
                                  AddressList* addresses,
                                  CompletionOnceCallback callback,
                                  std::unique_ptr<Request>* out_request,
                                  const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(out_request);

  auto request = std::make_unique<RequestImpl>(
      info.host_port_pair(), RequestInfoToResolveHostParameters(info, priority),
      AsWeakPtr());
  auto wrapped_request =
      std::make_unique<LegacyRequestImpl>(std::move(request));

  int rv = wrapped_request->Start();

  if (rv == OK) {
    DCHECK(wrapped_request->inner_request().GetAddressResults());
    *addresses = wrapped_request->inner_request().GetAddressResults().value();
  } else if (rv == ERR_IO_PENDING) {
    wrapped_request->AssignCallback(std::move(callback), addresses);
    *out_request = std::move(wrapped_request);
  }

  return rv;
}

int MockHostResolverBase::ResolveFromCache(const RequestInfo& info,
                                           AddressList* addresses,
                                           const NetLogWithSource& net_log) {
  num_resolve_from_cache_++;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  next_request_id_++;
  int rv = ResolveFromIPLiteralOrCache(
      info.host_port_pair(), info.address_family(), info.host_resolver_flags(),
      HostResolverSource::ANY, info.allow_cached_response(), addresses);
  return rv;
}

int MockHostResolverBase::ResolveStaleFromCache(
    const RequestInfo& info,
    AddressList* addresses,
    HostCache::EntryStaleness* stale_info,
    const NetLogWithSource& net_log) {
  num_resolve_from_cache_++;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  next_request_id_++;
  int rv = ResolveFromIPLiteralOrCache(
      info.host_port_pair(), info.address_family(), info.host_resolver_flags(),
      HostResolverSource::ANY, info.allow_cached_response(), addresses,
      stale_info);
  return rv;
}

void MockHostResolverBase::DetachRequest(size_t id) {
  auto it = requests_.find(id);
  CHECK(it != requests_.end());
  requests_.erase(it);
}

HostCache* MockHostResolverBase::GetHostCache() {
  return cache_.get();
}

bool MockHostResolverBase::HasCached(
    base::StringPiece hostname,
    HostCache::Entry::Source* source_out,
    HostCache::EntryStaleness* stale_out) const {
  if (!cache_)
    return false;

  return cache_->HasEntry(hostname, source_out, stale_out);
}

void MockHostResolverBase::ResolveAllPending() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ondemand_mode_);
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MockHostResolverBase::ResolveNow, AsWeakPtr(), i->first));
  }
}

// start id from 1 to distinguish from NULL RequestHandle
MockHostResolverBase::MockHostResolverBase(bool use_caching)
    : last_request_priority_(DEFAULT_PRIORITY),
      synchronous_mode_(false),
      ondemand_mode_(false),
      next_request_id_(1),
      num_resolve_(0),
      num_resolve_from_cache_(0),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  rules_map_[HostResolverSource::ANY] = CreateCatchAllHostResolverProc();
  rules_map_[HostResolverSource::SYSTEM] = CreateCatchAllHostResolverProc();
  rules_map_[HostResolverSource::DNS] = CreateCatchAllHostResolverProc();
  rules_map_[HostResolverSource::MULTICAST_DNS] =
      CreateCatchAllHostResolverProc();

  if (use_caching) {
    cache_.reset(new HostCache(kMaxCacheEntries));
  }
}

int MockHostResolverBase::Resolve(RequestImpl* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  last_request_priority_ = request->parameters().initial_priority;
  num_resolve_++;
  AddressList addresses;
  int rv = ResolveFromIPLiteralOrCache(
      request->request_host(),
      DnsQueryTypeToAddressFamily(request->parameters().dns_query_type),
      request->host_resolver_flags(), request->parameters().source,
      request->parameters().allow_cached_response, &addresses);
  if (rv == OK && !request->parameters().is_speculative)
    request->set_address_results(addresses);
  if (rv != ERR_DNS_CACHE_MISS)
    return rv;

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!IsValidDNSDomain(request->request_host().host()))
    return ERR_NAME_NOT_RESOLVED;

  if (synchronous_mode_) {
    int rv = ResolveProc(
        request->request_host(),
        DnsQueryTypeToAddressFamily(request->parameters().dns_query_type),
        request->host_resolver_flags(), request->parameters().source,
        &addresses);
    if (rv == OK && !request->parameters().is_speculative)
      request->set_address_results(addresses);
    return rv;
  }

  // Store the request for asynchronous resolution
  size_t id = next_request_id_++;
  request->set_id(id);
  requests_[id] = request;

  if (!ondemand_mode_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockHostResolverBase::ResolveNow, AsWeakPtr(), id));
  }

  return ERR_IO_PENDING;
}

int MockHostResolverBase::ResolveFromIPLiteralOrCache(
    const HostPortPair& host,
    AddressFamily requested_address_family,
    HostResolverFlags flags,
    HostResolverSource source,
    bool allow_cache,
    AddressList* addresses,
    HostCache::EntryStaleness* stale_info) {
  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(host.host())) {
    // This matches the behavior HostResolverImpl.
    if (requested_address_family != ADDRESS_FAMILY_UNSPECIFIED &&
        requested_address_family != GetAddressFamily(ip_address)) {
      return ERR_NAME_NOT_RESOLVED;
    }

    *addresses = AddressList::CreateFromIPAddress(ip_address, host.port());
    if (flags & HOST_RESOLVER_CANONNAME)
      addresses->SetDefaultCanonicalName();
    return OK;
  }
  int rv = ERR_DNS_CACHE_MISS;
  if (cache_.get() && allow_cache) {
    HostCache::Key key(host.host(), requested_address_family, flags, source);
    const HostCache::Entry* entry;
    if (stale_info)
      entry = cache_->LookupStale(key, tick_clock_->NowTicks(), stale_info);
    else
      entry = cache_->Lookup(key, tick_clock_->NowTicks());
    if (entry) {
      rv = entry->error();
      if (rv == OK)
        *addresses = AddressList::CopyWithPort(entry->addresses(), host.port());
    }
  }
  return rv;
}

int MockHostResolverBase::ResolveProc(const HostPortPair& host,
                                      AddressFamily requested_address_family,
                                      HostResolverFlags flags,
                                      HostResolverSource source,
                                      AddressList* addresses) {
  DCHECK(rules_map_.find(source) != rules_map_.end());

  AddressList addr;
  int rv = rules_map_[source]->Resolve(host.host(), requested_address_family,
                                       flags, &addr, nullptr);
  if (cache_.get()) {
    HostCache::Key key(host.host(), requested_address_family, flags, source);
    // Storing a failure with TTL 0 so that it overwrites previous value.
    base::TimeDelta ttl;
    if (rv == OK)
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);
    cache_->Set(key,
                HostCache::Entry(rv, addr, HostCache::Entry::SOURCE_UNKNOWN),
                tick_clock_->NowTicks(), ttl);
  }
  if (rv == OK)
    *addresses = AddressList::CopyWithPort(addr, host.port());
  return rv;
}

void MockHostResolverBase::ResolveNow(size_t id) {
  auto it = requests_.find(id);
  if (it == requests_.end())
    return;  // was canceled

  RequestImpl* req = it->second;
  requests_.erase(it);

  AddressList addresses;
  int error = ResolveProc(
      req->request_host(),
      DnsQueryTypeToAddressFamily(req->parameters().dns_query_type),
      req->host_resolver_flags(), req->parameters().source, &addresses);
  if (error == OK && !req->parameters().is_speculative)
    req->set_address_results(addresses);
  req->OnAsyncCompleted(id, error);
}

//-----------------------------------------------------------------------------

RuleBasedHostResolverProc::Rule::Rule(ResolverType resolver_type,
                                      const std::string& host_pattern,
                                      AddressFamily address_family,
                                      HostResolverFlags host_resolver_flags,
                                      const std::string& replacement,
                                      const std::string& canonical_name,
                                      int latency_ms)
    : resolver_type(resolver_type),
      host_pattern(host_pattern),
      address_family(address_family),
      host_resolver_flags(host_resolver_flags),
      replacement(replacement),
      canonical_name(canonical_name),
      latency_ms(latency_ms) {}

RuleBasedHostResolverProc::Rule::Rule(const Rule& other) = default;

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous)
    : HostResolverProc(previous), modifications_allowed_(true) {}

void RuleBasedHostResolverProc::AddRule(const std::string& host_pattern,
                                        const std::string& replacement) {
  AddRuleForAddressFamily(host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
                          replacement);
}

void RuleBasedHostResolverProc::AddRuleForAddressFamily(
    const std::string& host_pattern,
    AddressFamily address_family,
    const std::string& replacement) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
                            HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, address_family, flags,
            replacement, std::string(), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithFlags(
    const std::string& host_pattern,
    const std::string& replacement,
    HostResolverFlags flags,
    const std::string& canonical_name) {
  DCHECK(!replacement.empty());
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, canonical_name, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRule(
    const std::string& host_pattern,
    const std::string& ip_literal,
    const std::string& canonical_name) {
  // Literals are always resolved to themselves by HostResolverImpl,
  // consequently we do not support remapping them.
  IPAddress ip_address;
  DCHECK(!ip_address.AssignFromIPLiteral(host_pattern));
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
                            HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  if (!canonical_name.empty())
    flags |= HOST_RESOLVER_CANONNAME;

  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal, canonical_name, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
                            HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, std::string(), latency_ms);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
                            HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), std::string(), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
                            HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeFail, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), std::string(), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::ClearRules() {
  CHECK(modifications_allowed_);
  base::AutoLock lock(rule_lock_);
  rules_.clear();
}

void RuleBasedHostResolverProc::DisableModifications() {
  modifications_allowed_ = false;
}

RuleBasedHostResolverProc::RuleList RuleBasedHostResolverProc::GetRules() {
  RuleList rv;
  {
    base::AutoLock lock(rule_lock_);
    rv = rules_;
  }
  return rv;
}

int RuleBasedHostResolverProc::Resolve(const std::string& host,
                                       AddressFamily address_family,
                                       HostResolverFlags host_resolver_flags,
                                       AddressList* addrlist,
                                       int* os_error) {
  base::AutoLock lock(rule_lock_);
  RuleList::iterator r;
  for (r = rules_.begin(); r != rules_.end(); ++r) {
    bool matches_address_family =
        r->address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        r->address_family == address_family;
    // Ignore HOST_RESOLVER_SYSTEM_ONLY, since it should have no impact on
    // whether a rule matches.
    HostResolverFlags flags = host_resolver_flags & ~HOST_RESOLVER_SYSTEM_ONLY;
    // Flags match if all of the bitflags in host_resolver_flags are enabled
    // in the rule's host_resolver_flags. However, the rule may have additional
    // flags specified, in which case the flags should still be considered a
    // match.
    bool matches_flags = (r->host_resolver_flags & flags) == flags;
    if (matches_flags && matches_address_family &&
        base::MatchPattern(host, r->host_pattern)) {
      if (r->latency_ms != 0) {
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(r->latency_ms));
      }

      // Remap to a new host.
      const std::string& effective_host =
          r->replacement.empty() ? host : r->replacement;

      // Apply the resolving function to the remapped hostname.
      switch (r->resolver_type) {
        case Rule::kResolverTypeFail:
          return ERR_NAME_NOT_RESOLVED;
        case Rule::kResolverTypeSystem:
#if defined(OS_WIN)
          EnsureWinsockInit();
#endif
          return SystemHostResolverCall(effective_host, address_family,
                                        host_resolver_flags, addrlist,
                                        os_error);
        case Rule::kResolverTypeIPLiteral: {
          AddressList raw_addr_list;
          int result = ParseAddressList(
              effective_host,
              !r->canonical_name.empty() ? r->canonical_name : host,
              &raw_addr_list);
          // Filter out addresses with the wrong family.
          *addrlist = AddressList();
          for (const auto& address : raw_addr_list) {
            if (address_family == ADDRESS_FAMILY_UNSPECIFIED ||
                address_family == address.GetFamily()) {
              addrlist->push_back(address);
            }
          }
          addrlist->set_canonical_name(raw_addr_list.canonical_name());

          if (result == OK && addrlist->empty())
            return ERR_NAME_NOT_RESOLVED;
          return result;
        }
        default:
          NOTREACHED();
          return ERR_UNEXPECTED;
      }
    }
  }
  return ResolveUsingPrevious(host, address_family, host_resolver_flags,
                              addrlist, os_error);
}

RuleBasedHostResolverProc::~RuleBasedHostResolverProc() = default;

void RuleBasedHostResolverProc::AddRuleInternal(const Rule& rule) {
  Rule fixed_rule = rule;
  // SystemResolverProc expects valid DNS addresses.
  // So for kResolverTypeSystem rules:
  // * If the replacement is an IP address, switch to an IP literal rule.
  // * If it's a non-empty invalid domain name, switch to a fail rule (Empty
  // domain names mean use a direct lookup).
  if (fixed_rule.resolver_type == Rule::kResolverTypeSystem) {
    IPAddress ip_address;
    bool valid_address = ip_address.AssignFromIPLiteral(fixed_rule.replacement);
    if (valid_address) {
      fixed_rule.resolver_type = Rule::kResolverTypeIPLiteral;
    } else if (!fixed_rule.replacement.empty() &&
               !IsValidDNSDomain(fixed_rule.replacement)) {
      // TODO(mmenke): Can this be replaced with a DCHECK instead?
      fixed_rule.resolver_type = Rule::kResolverTypeFail;
    }
  }

  CHECK(modifications_allowed_);
  base::AutoLock lock(rule_lock_);
  rules_.push_back(fixed_rule);
}

RuleBasedHostResolverProc* CreateCatchAllHostResolverProc() {
  RuleBasedHostResolverProc* catchall = new RuleBasedHostResolverProc(NULL);
  // Note that IPv6 lookups fail.
  catchall->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  // Next add a rules-based layer the use controls.
  return new RuleBasedHostResolverProc(catchall);
}

//-----------------------------------------------------------------------------

// Implementation of both the Request and ResolveHostRequest interfaces. Both
// can be implemented from the same class as this implementation does not really
// do anything except track cancellations, which for both interfaces is when the
// request is destroyed after being started.
class HangingHostResolver::RequestImpl
    : public HostResolver::Request,
      public HostResolver::ResolveHostRequest {
 public:
  RequestImpl(base::WeakPtr<HangingHostResolver> resolver, bool is_running)
      : resolver_(resolver), is_running_(is_running) {}

  ~RequestImpl() override {
    if (is_running_ && resolver_)
      resolver_->num_cancellations_++;
  }

  int Start(CompletionOnceCallback callback) override {
    DCHECK(resolver_);
    is_running_ = true;
    return ERR_IO_PENDING;
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    IMMEDIATE_CRASH();
  }

  void ChangeRequestPriority(RequestPriority priority) override {}

 private:
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<HangingHostResolver> resolver_;
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

HangingHostResolver::HangingHostResolver() = default;

HangingHostResolver::~HangingHostResolver() = default;

std::unique_ptr<HostResolver::ResolveHostRequest>
HangingHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr(),
                                       false /* started */);
}

int HangingHostResolver::Resolve(const RequestInfo& info,
                                 RequestPriority priority,
                                 AddressList* addresses,
                                 CompletionOnceCallback callback,
                                 std::unique_ptr<Request>* request,
                                 const NetLogWithSource& net_log) {
  *request = std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr(),
                                           true /* started */);
  return ERR_IO_PENDING;
}

int HangingHostResolver::ResolveFromCache(const RequestInfo& info,
                                          AddressList* addresses,
                                          const NetLogWithSource& net_log) {
  return ERR_DNS_CACHE_MISS;
}

int HangingHostResolver::ResolveStaleFromCache(
    const RequestInfo& info,
    AddressList* addresses,
    HostCache::EntryStaleness* stale_info,
    const NetLogWithSource& net_log) {
  return ERR_DNS_CACHE_MISS;
}

bool HangingHostResolver::HasCached(
    base::StringPiece hostname,
    HostCache::Entry::Source* source_out,
    HostCache::EntryStaleness* stale_out) const {
  return false;
}

//-----------------------------------------------------------------------------

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc() = default;

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc(
    HostResolverProc* proc) {
  Init(proc);
}

ScopedDefaultHostResolverProc::~ScopedDefaultHostResolverProc() {
  HostResolverProc* old_proc =
      HostResolverProc::SetDefault(previous_proc_.get());
  // The lifetimes of multiple instances must be nested.
  CHECK_EQ(old_proc, current_proc_.get());
}

void ScopedDefaultHostResolverProc::Init(HostResolverProc* proc) {
  current_proc_ = proc;
  previous_proc_ = HostResolverProc::SetDefault(current_proc_.get());
  current_proc_->SetLastProc(previous_proc_.get());
}

}  // namespace net
