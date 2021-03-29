// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_host_resolver.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/host_cache.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// Cache size for the MockCachingHostResolver.
const unsigned kMaxCacheEntries = 100;
// TTL for the successful resolutions. Failures are not cached.
const unsigned kCacheEntryTTLSeconds = 60;

RuleBasedHostResolverProc* CreateMockHostResolverProc(bool add_catchall) {
  if (add_catchall) {
    return CreateCatchAllHostResolverProc();
  }

  return new RuleBasedHostResolverProc(/*previous=*/nullptr,
                                       /*allow_fallback=*/false);
}

}  // namespace

int ParseAddressList(const std::string& host_list,
                     const std::vector<std::string>& dns_aliases,
                     AddressList* addrlist) {
  *addrlist = AddressList();
  addrlist->SetDnsAliases(dns_aliases);
  for (const base::StringPiece& address : base::SplitStringPiece(
           host_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(address)) {
      LOG(WARNING) << "Not a supported IP literal: " << address;
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
              const NetworkIsolationKey& network_isolation_key,
              const base::Optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<MockHostResolverBase> resolver)
      : request_host_(request_host),
        network_isolation_key_(network_isolation_key),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        priority_(parameters_.initial_priority),
        host_resolver_flags_(ParametersToHostResolverFlags(parameters_)),
        resolve_error_info_(ResolveErrorInfo(ERR_IO_PENDING)),
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

  void DetachFromResolver() {
    id_ = 0;
    resolver_ = nullptr;
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

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<std::string>>>
        nullopt_result;
    return *nullopt_result;
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    DCHECK(complete_);
    static const base::NoDestructor<base::Optional<std::vector<HostPortPair>>>
        nullopt_result;
    return *nullopt_result;
  }

  const base::Optional<std::vector<std::string>>& GetDnsAliasResults()
      const override {
    DCHECK(complete_);
    return sanitized_dns_alias_results_;
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    DCHECK(complete_);
    return resolve_error_info_;
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    DCHECK(complete_);
    return staleness_;
  }

  void ChangeRequestPriority(RequestPriority priority) override {
    priority_ = priority;
  }

  void SetError(int error) {
    // Should only be called before request is marked completed.
    DCHECK(!complete_);
    resolve_error_info_ = ResolveErrorInfo(error);
  }

  void SetAddressResults(const AddressList& address_results,
                         base::Optional<HostCache::EntryStaleness> staleness) {
    // Should only be called at most once and before request is marked
    // completed.
    DCHECK(!complete_);
    DCHECK(!address_results_);
    DCHECK(!parameters_.is_speculative);

    address_results_ = address_results;
    sanitized_dns_alias_results_ =
        dns_alias_utility::SanitizeDnsAliases(address_results_->dns_aliases());
    staleness_ = std::move(staleness);
  }

  void OnAsyncCompleted(size_t id, int error) {
    DCHECK_EQ(id_, id);
    id_ = 0;

    // Check that error information has been set and that the top-level error
    // code is valid.
    DCHECK(resolve_error_info_.error != ERR_IO_PENDING);
    DCHECK(error == OK || error == ERR_NAME_NOT_RESOLVED);

    DCHECK(!complete_);
    complete_ = true;

    DCHECK(callback_);
    std::move(callback_).Run(error);
  }

  const HostPortPair& request_host() const { return request_host_; }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  const ResolveHostParameters& parameters() const { return parameters_; }

  int host_resolver_flags() const { return host_resolver_flags_; }

  size_t id() { return id_; }

  RequestPriority priority() const { return priority_; }

  void set_id(size_t id) {
    DCHECK_GT(id, 0u);
    DCHECK_EQ(0u, id_);

    id_ = id;
  }

  bool complete() { return complete_; }

 private:
  const HostPortPair request_host_;
  const NetworkIsolationKey network_isolation_key_;
  const ResolveHostParameters parameters_;
  RequestPriority priority_;
  int host_resolver_flags_;

  base::Optional<AddressList> address_results_;
  base::Optional<std::vector<std::string>> sanitized_dns_alias_results_;
  base::Optional<HostCache::EntryStaleness> staleness_;
  ResolveErrorInfo resolve_error_info_;

  // Used while stored with the resolver for async resolution.  Otherwise 0.
  size_t id_;

  CompletionOnceCallback callback_;
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
  bool complete_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

class MockHostResolverBase::ProbeRequestImpl
    : public HostResolver::ProbeRequest {
 public:
  explicit ProbeRequestImpl(base::WeakPtr<MockHostResolverBase> resolver)
      : resolver_(std::move(resolver)) {}

  ProbeRequestImpl(const ProbeRequestImpl&) = delete;
  ProbeRequestImpl& operator=(const ProbeRequestImpl&) = delete;

  ~ProbeRequestImpl() override {
    if (resolver_ && resolver_->doh_probe_request_ == this)
      resolver_->doh_probe_request_ = nullptr;
  }

  int Start() override {
    DCHECK(resolver_);
    DCHECK(!resolver_->doh_probe_request_);

    resolver_->doh_probe_request_ = this;

    return ERR_IO_PENDING;
  }

 private:
  base::WeakPtr<MockHostResolverBase> resolver_;
};

class MockHostResolverBase::MdnsListenerImpl
    : public HostResolver::MdnsListener {
 public:
  MdnsListenerImpl(const HostPortPair& host,
                   DnsQueryType query_type,
                   base::WeakPtr<MockHostResolverBase> resolver)
      : host_(host),
        query_type_(query_type),
        delegate_(nullptr),
        resolver_(resolver) {
    DCHECK_NE(DnsQueryType::UNSPECIFIED, query_type_);
    DCHECK(resolver_);
  }

  ~MdnsListenerImpl() override {
    if (resolver_)
      resolver_->RemoveCancelledListener(this);
  }

  int Start(Delegate* delegate) override {
    DCHECK(delegate);
    DCHECK(!delegate_);
    DCHECK(resolver_);

    delegate_ = delegate;
    resolver_->AddListener(this);

    return OK;
  }

  void TriggerAddressResult(Delegate::UpdateType update_type,
                            IPEndPoint address) {
    delegate_->OnAddressResult(update_type, query_type_, std::move(address));
  }

  void TriggerTextResult(Delegate::UpdateType update_type,
                         std::vector<std::string> text_records) {
    delegate_->OnTextResult(update_type, query_type_, std::move(text_records));
  }

  void TriggerHostnameResult(Delegate::UpdateType update_type,
                             HostPortPair host) {
    delegate_->OnHostnameResult(update_type, query_type_, std::move(host));
  }

  void TriggerUnhandledResult(Delegate::UpdateType update_type) {
    delegate_->OnUnhandledResult(update_type, query_type_);
  }

  const HostPortPair& host() const { return host_; }
  DnsQueryType query_type() const { return query_type_; }

 private:
  const HostPortPair host_;
  const DnsQueryType query_type_;

  Delegate* delegate_;

  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding listener objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
};

MockHostResolverBase::~MockHostResolverBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Sanity check that pending requests are always cleaned up, by waiting for
  // completion, manually cancelling, or calling OnShutdown().
  DCHECK(requests_.empty());
}

void MockHostResolverBase::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Cancel all pending requests.
  for (auto& request : requests_) {
    request.second->DetachFromResolver();
  }
  requests_.clear();

  // Prevent future requests by clearing resolution rules and the cache.
  rules_map_.clear();
  cache_ = nullptr;

  doh_probe_request_ = nullptr;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MockHostResolverBase::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(host, network_isolation_key,
                                       optional_parameters, AsWeakPtr());
}

std::unique_ptr<HostResolver::ProbeRequest>
MockHostResolverBase::CreateDohProbeRequest() {
  return std::make_unique<ProbeRequestImpl>(AsWeakPtr());
}

std::unique_ptr<HostResolver::MdnsListener>
MockHostResolverBase::CreateMdnsListener(const HostPortPair& host,
                                         DnsQueryType query_type) {
  return std::make_unique<MdnsListenerImpl>(host, query_type, AsWeakPtr());
}

HostCache* MockHostResolverBase::GetHostCache() {
  return cache_.get();
}

int MockHostResolverBase::LoadIntoCache(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cache_);

  ResolveHostParameters parameters =
      optional_parameters.value_or(ResolveHostParameters());

  AddressList addresses;
  base::Optional<HostCache::EntryStaleness> stale_info;
  int rv = ResolveFromIPLiteralOrCache(
      host, network_isolation_key, parameters.dns_query_type,
      ParametersToHostResolverFlags(parameters), parameters.source,
      parameters.cache_usage, &addresses, &stale_info);
  if (rv != ERR_DNS_CACHE_MISS) {
    // Request already in cache (or IP literal). No need to load it.
    return rv;
  }

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!IsValidDNSDomain(host.host()))
    return ERR_NAME_NOT_RESOLVED;

  return ResolveProc(host, network_isolation_key,
                     DnsQueryTypeToAddressFamily(parameters.dns_query_type),
                     ParametersToHostResolverFlags(parameters),
                     parameters.source, &addresses);
}

void MockHostResolverBase::ResolveAllPending() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ondemand_mode_);
  for (auto i = requests_.begin(); i != requests_.end(); ++i) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MockHostResolverBase::ResolveNow,
                                  AsWeakPtr(), i->first));
  }
}

size_t MockHostResolverBase::last_id() {
  if (requests_.empty())
    return 0;
  return requests_.rbegin()->first;
}

void MockHostResolverBase::ResolveNow(size_t id) {
  auto it = requests_.find(id);
  if (it == requests_.end())
    return;  // was canceled

  RequestImpl* req = it->second;
  requests_.erase(it);

  AddressList addresses;
  int error = ResolveProc(
      req->request_host(), req->network_isolation_key(),
      DnsQueryTypeToAddressFamily(req->parameters().dns_query_type),
      req->host_resolver_flags(), req->parameters().source, &addresses);
  req->SetError(error);
  if (error == OK && !req->parameters().is_speculative)
    req->SetAddressResults(addresses, base::nullopt);
  req->OnAsyncCompleted(id, SquashErrorCode(error));
}

void MockHostResolverBase::DetachRequest(size_t id) {
  auto it = requests_.find(id);
  CHECK(it != requests_.end());
  requests_.erase(it);
}

const std::string& MockHostResolverBase::request_host(size_t id) {
  DCHECK(request(id));
  return request(id)->request_host().host();
}

RequestPriority MockHostResolverBase::request_priority(size_t id) {
  DCHECK(request(id));
  return request(id)->priority();
}

const NetworkIsolationKey& MockHostResolverBase::request_network_isolation_key(
    size_t id) {
  DCHECK(request(id));
  return request(id)->network_isolation_key();
}

void MockHostResolverBase::ResolveOnlyRequestNow() {
  DCHECK_EQ(1u, requests_.size());
  ResolveNow(requests_.begin()->first);
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListener::Delegate::UpdateType update_type,
    const IPEndPoint& address_result) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerAddressResult(update_type, address_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListener::Delegate::UpdateType update_type,
    const std::vector<std::string>& text_result) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerTextResult(update_type, text_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListener::Delegate::UpdateType update_type,
    const HostPortPair& host_result) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerHostnameResult(update_type, host_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListener::Delegate::UpdateType update_type) {
  for (auto* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerUnhandledResult(update_type);
  }
}

MockHostResolverBase::RequestImpl* MockHostResolverBase::request(size_t id) {
  RequestMap::iterator request = requests_.find(id);
  DCHECK(request != requests_.end());
  return (*request).second;
}

// start id from 1 to distinguish from NULL RequestHandle
MockHostResolverBase::MockHostResolverBase(bool use_caching,
                                           int cache_invalidation_num,
                                           bool require_matching_rule)
    : last_request_priority_(DEFAULT_PRIORITY),
      last_secure_dns_mode_override_(base::nullopt),
      synchronous_mode_(false),
      ondemand_mode_(false),
      initial_cache_invalidation_num_(cache_invalidation_num),
      next_request_id_(1),
      num_resolve_(0),
      num_resolve_from_cache_(0),
      num_non_local_resolves_(0),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  rules_map_[HostResolverSource::ANY] =
      CreateMockHostResolverProc(/*add_catchall=*/!require_matching_rule);
  rules_map_[HostResolverSource::SYSTEM] =
      CreateMockHostResolverProc(/*add_catchall=*/!require_matching_rule);
  rules_map_[HostResolverSource::DNS] =
      CreateMockHostResolverProc(/*add_catchall=*/!require_matching_rule);
  rules_map_[HostResolverSource::MULTICAST_DNS] =
      CreateMockHostResolverProc(/*add_catchall=*/!require_matching_rule);

  if (use_caching)
    cache_.reset(new HostCache(kMaxCacheEntries));
  else
    DCHECK_GE(0, cache_invalidation_num);
}

int MockHostResolverBase::Resolve(RequestImpl* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  last_request_priority_ = request->parameters().initial_priority;
  last_request_network_isolation_key_ = request->network_isolation_key();
  last_secure_dns_mode_override_ =
      request->parameters().secure_dns_mode_override;
  num_resolve_++;
  AddressList addresses;
  base::Optional<HostCache::EntryStaleness> stale_info;
  int rv = ResolveFromIPLiteralOrCache(
      request->request_host(), request->network_isolation_key(),
      request->parameters().dns_query_type, request->host_resolver_flags(),
      request->parameters().source, request->parameters().cache_usage,
      &addresses, &stale_info);

  request->SetError(rv);
  if (rv == OK && !request->parameters().is_speculative)
    request->SetAddressResults(addresses, std::move(stale_info));
  if (rv != ERR_DNS_CACHE_MISS ||
      request->parameters().source == HostResolverSource::LOCAL_ONLY) {
    return SquashErrorCode(rv);
  }

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!IsValidDNSDomain(request->request_host().host())) {
    request->SetError(ERR_NAME_NOT_RESOLVED);
    return ERR_NAME_NOT_RESOLVED;
  }

  if (synchronous_mode_) {
    int rv = ResolveProc(
        request->request_host(), request->network_isolation_key(),
        DnsQueryTypeToAddressFamily(request->parameters().dns_query_type),
        request->host_resolver_flags(), request->parameters().source,
        &addresses);

    request->SetError(rv);
    if (rv == OK && !request->parameters().is_speculative)
      request->SetAddressResults(addresses, base::nullopt);
    return SquashErrorCode(rv);
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
    const NetworkIsolationKey& network_isolation_key,
    DnsQueryType dns_query_type,
    HostResolverFlags flags,
    HostResolverSource source,
    HostResolver::ResolveHostParameters::CacheUsage cache_usage,
    AddressList* addresses,
    base::Optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(addresses);
  DCHECK(out_stale_info);
  *out_stale_info = base::nullopt;

  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(host.host())) {
    // This matches the behavior HostResolverImpl.
    if (dns_query_type != DnsQueryType::UNSPECIFIED &&
        dns_query_type !=
            AddressFamilyToDnsQueryType(GetAddressFamily(ip_address))) {
      return ERR_NAME_NOT_RESOLVED;
    }

    *addresses = AddressList::CreateFromIPAddress(ip_address, host.port());
    if (flags & HOST_RESOLVER_CANONNAME)
      addresses->SetDefaultCanonicalName();
    return OK;
  }
  int rv = ERR_DNS_CACHE_MISS;
  bool cache_allowed =
      cache_usage == HostResolver::ResolveHostParameters::CacheUsage::ALLOWED ||
      cache_usage ==
          HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  if (cache_.get() && cache_allowed) {
    // Local-only requests search the cache for non-local-only results.
    HostResolverSource effective_source =
        source == HostResolverSource::LOCAL_ONLY ? HostResolverSource::ANY
                                                 : source;
    HostCache::Key key(host.host(), dns_query_type, flags, effective_source,
                       network_isolation_key);
    const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
    HostCache::EntryStaleness stale_info = HostCache::kNotStale;
    if (cache_usage ==
        HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED) {
      cache_result = cache_->LookupStale(key, tick_clock_->NowTicks(),
                                         &stale_info, true /* ignore_secure */);
    } else {
      cache_result = cache_->Lookup(key, tick_clock_->NowTicks(),
                                    true /* ignore_secure */);
    }
    if (cache_result) {
      rv = cache_result->second.error();
      if (rv == OK) {
        *addresses = AddressList::CopyWithPort(
            cache_result->second.addresses().value(), host.port());
        *out_stale_info = std::move(stale_info);
      }

      auto cache_invalidation_iterator = cache_invalidation_nums_.find(key);
      if (cache_invalidation_iterator != cache_invalidation_nums_.end()) {
        DCHECK_LE(1, cache_invalidation_iterator->second);
        cache_invalidation_iterator->second--;
        if (cache_invalidation_iterator->second == 0) {
          HostCache::Entry new_entry(cache_result->second);
          cache_->Set(key, new_entry, tick_clock_->NowTicks(),
                      base::TimeDelta());
          cache_invalidation_nums_.erase(cache_invalidation_iterator);
        }
      }
    }
  }
  return rv;
}

int MockHostResolverBase::ResolveProc(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    AddressFamily requested_address_family,
    HostResolverFlags flags,
    HostResolverSource source,
    AddressList* addresses) {
  DCHECK(rules_map_.find(source) != rules_map_.end());
  ++num_non_local_resolves_;

  AddressList addr;
  int rv = rules_map_[source]->Resolve(host.host(), requested_address_family,
                                       flags, &addr, nullptr);
  if (cache_.get()) {
    HostCache::Key key(host.host(),
                       AddressFamilyToDnsQueryType(requested_address_family),
                       flags, source, network_isolation_key);
    // Storing a failure with TTL 0 so that it overwrites previous value.
    base::TimeDelta ttl;
    if (rv == OK) {
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);
      if (initial_cache_invalidation_num_ > 0)
        cache_invalidation_nums_[key] = initial_cache_invalidation_num_;
    }
    cache_->Set(key,
                HostCache::Entry(rv, addr, HostCache::Entry::SOURCE_UNKNOWN),
                tick_clock_->NowTicks(), ttl);
  }
  if (rv == OK)
    *addresses = AddressList::CopyWithPort(addr, host.port());
  return rv;
}

void MockHostResolverBase::AddListener(MdnsListenerImpl* listener) {
  listeners_.insert(listener);
}

void MockHostResolverBase::RemoveCancelledListener(MdnsListenerImpl* listener) {
  listeners_.erase(listener);
}

MockHostResolverFactory::MockHostResolverFactory(
    scoped_refptr<RuleBasedHostResolverProc> rules,
    bool use_caching,
    int cache_invalidation_num)
    : rules_(std::move(rules)),
      use_caching_(use_caching),
      cache_invalidation_num_(cache_invalidation_num) {}

MockHostResolverFactory::~MockHostResolverFactory() = default;

std::unique_ptr<HostResolver> MockHostResolverFactory::CreateResolver(
    HostResolverManager* manager,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  DCHECK(host_mapping_rules.empty());

  // Explicit new to access private constructor.
  auto resolver = base::WrapUnique(new MockHostResolverBase(
      enable_caching && use_caching_, cache_invalidation_num_,
      /*require_matching_rule=*/true));
  if (rules_)
    resolver->set_rules(rules_.get());
  return resolver;
}

std::unique_ptr<HostResolver> MockHostResolverFactory::CreateStandaloneResolver(
    NetLog* net_log,
    const HostResolver::ManagerOptions& options,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  return CreateResolver(nullptr, host_mapping_rules, enable_caching);
}

//-----------------------------------------------------------------------------

RuleBasedHostResolverProc::Rule::Rule(ResolverType resolver_type,
                                      const std::string& host_pattern,
                                      AddressFamily address_family,
                                      HostResolverFlags host_resolver_flags,
                                      const std::string& replacement,
                                      std::vector<std::string> dns_aliases,
                                      int latency_ms)
    : resolver_type(resolver_type),
      host_pattern(host_pattern),
      address_family(address_family),
      host_resolver_flags(host_resolver_flags),
      replacement(replacement),
      dns_aliases(std::move(dns_aliases)),
      latency_ms(latency_ms) {
  DCHECK(dns_aliases != std::vector<std::string>({""}));
}

RuleBasedHostResolverProc::Rule::Rule(const Rule& other) = default;

RuleBasedHostResolverProc::Rule::~Rule() = default;

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous,
                                                     bool allow_fallback)
    : HostResolverProc(previous, allow_fallback),
      modifications_allowed_(true) {}

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
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, address_family, flags,
            replacement, {} /* dns_aliases */, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithFlags(
    const std::string& host_pattern,
    const std::string& replacement,
    HostResolverFlags flags,
    std::vector<std::string> dns_aliases) {
  DCHECK(!replacement.empty());
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, std::move(dns_aliases), 0);
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
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  std::vector<std::string> aliases;
  if (!canonical_name.empty()) {
    flags |= HOST_RESOLVER_CANONNAME;
    aliases.emplace_back(canonical_name);
  }

  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal, std::move(aliases),
            0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRuleWithDnsAliases(
    const std::string& host_pattern,
    const std::string& ip_literal,
    std::vector<std::string> dns_aliases) {
  // Literals are always resolved to themselves by HostResolverImpl,
  // consequently we do not support remapping them.
  IPAddress ip_address;
  DCHECK(!ip_address.AssignFromIPLiteral(host_pattern));
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  if (!dns_aliases.empty())
    flags |= HOST_RESOLVER_CANONNAME;

  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal,
            std::move(dns_aliases), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, {} /* dns_aliases */, latency_ms);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), {} /* dns_aliases */, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    const std::string& host_pattern,
    HostResolverFlags flags) {
  Rule rule(Rule::kResolverTypeFail, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), {} /* dns_aliases */, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedTimeoutFailure(
    const std::string& host_pattern,
    HostResolverFlags flags) {
  Rule rule(Rule::kResolverTypeFailTimeout, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, std::string(),
            {} /* dns_aliases */, 0);
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
    // Ignore HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6, since it should
    // have no impact on whether a rule matches.
    HostResolverFlags flags =
        host_resolver_flags & ~HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
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
        case Rule::kResolverTypeFailTimeout:
          return ERR_DNS_TIMED_OUT;
        case Rule::kResolverTypeSystem:
#if defined(OS_WIN)
          EnsureWinsockInit();
#endif
          return SystemHostResolverCall(effective_host, address_family,
                                        host_resolver_flags, addrlist,
                                        os_error);
        case Rule::kResolverTypeIPLiteral: {
          AddressList raw_addr_list;
          std::vector<std::string> aliases;
          aliases = (!r->dns_aliases.empty())
                        ? r->dns_aliases
                        : std::vector<std::string>({host});
          int result =
              ParseAddressList(effective_host, aliases, &raw_addr_list);
          // Filter out addresses with the wrong family.
          *addrlist = AddressList();
          for (const auto& address : raw_addr_list) {
            if (address_family == ADDRESS_FAMILY_UNSPECIFIED ||
                address_family == address.GetFamily()) {
              addrlist->push_back(address);
            }
          }
          addrlist->SetDnsAliases(raw_addr_list.dns_aliases());

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
  RuleBasedHostResolverProc* catchall =
      new RuleBasedHostResolverProc(/*previous=*/nullptr,
                                    /*allow_fallback=*/false);
  // Note that IPv6 lookups fail.
  catchall->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  // Next add a rules-based layer that the test controls.
  return new RuleBasedHostResolverProc(catchall, /*allow_fallback=*/false);
}

//-----------------------------------------------------------------------------

// Implementation of ResolveHostRequest that tracks cancellations when the
// request is destroyed after being started.
class HangingHostResolver::RequestImpl
    : public HostResolver::ResolveHostRequest,
      public HostResolver::ProbeRequest {
 public:
  explicit RequestImpl(base::WeakPtr<HangingHostResolver> resolver)
      : resolver_(resolver) {}

  ~RequestImpl() override {
    if (is_running_ && resolver_)
      resolver_->num_cancellations_++;
  }

  int Start(CompletionOnceCallback callback) override { return Start(); }

  int Start() override {
    DCHECK(resolver_);
    is_running_ = true;
    return ERR_IO_PENDING;
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    IMMEDIATE_CRASH();
  }

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    IMMEDIATE_CRASH();
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    IMMEDIATE_CRASH();
  }

  const base::Optional<std::vector<std::string>>& GetDnsAliasResults()
      const override {
    IMMEDIATE_CRASH();
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    IMMEDIATE_CRASH();
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    IMMEDIATE_CRASH();
  }

  void ChangeRequestPriority(RequestPriority priority) override {}

 private:
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<HangingHostResolver> resolver_;
  bool is_running_ = false;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

HangingHostResolver::HangingHostResolver() = default;

HangingHostResolver::~HangingHostResolver() = default;

void HangingHostResolver::OnShutdown() {
  shutting_down_ = true;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HangingHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  last_host_ = host;
  last_network_isolation_key_ = network_isolation_key;

  if (shutting_down_)
    return CreateFailingRequest(ERR_CONTEXT_SHUT_DOWN);

  if (optional_parameters &&
      optional_parameters.value().source == HostResolverSource::LOCAL_ONLY) {
    return CreateFailingRequest(ERR_DNS_CACHE_MISS);
  }

  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::ProbeRequest>
HangingHostResolver::CreateDohProbeRequest() {
  if (shutting_down_)
    return CreateFailingProbeRequest(ERR_CONTEXT_SHUT_DOWN);

  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr());
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
