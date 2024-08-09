// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_host_resolver.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// Cache size for the MockCachingHostResolver.
const unsigned kMaxCacheEntries = 100;
// TTL for the successful resolutions. Failures are not cached.
const unsigned kCacheEntryTTLSeconds = 60;

absl::variant<url::SchemeHostPort, std::string> GetCacheHost(
    const HostResolver::Host& endpoint) {
  if (endpoint.HasScheme()) {
    return endpoint.AsSchemeHostPort();
  }

  return endpoint.GetHostname();
}

std::optional<HostCache::Entry> CreateCacheEntry(
    std::string_view canonical_name,
    const std::vector<HostResolverEndpointResult>& endpoint_results,
    const std::set<std::string>& aliases) {
  std::optional<std::vector<net::IPEndPoint>> ip_endpoints;
  std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
      endpoint_metadatas;
  for (const auto& endpoint_result : endpoint_results) {
    if (!ip_endpoints) {
      ip_endpoints = endpoint_result.ip_endpoints;
    } else {
      // TODO(crbug.com/40203587): Support caching different IP endpoints
      // resutls.
      CHECK(*ip_endpoints == endpoint_result.ip_endpoints)
          << "Currently caching MockHostResolver only supports same IP "
             "endpoints results.";
    }

    if (!endpoint_result.metadata.supported_protocol_alpns.empty()) {
      endpoint_metadatas.emplace(/*priority=*/1, endpoint_result.metadata);
    }
  }
  DCHECK(ip_endpoints);
  auto endpoint_entry = HostCache::Entry(OK, *ip_endpoints, aliases,
                                         HostCache::Entry::SOURCE_UNKNOWN);
  endpoint_entry.set_canonical_names(std::set{std::string(canonical_name)});
  if (endpoint_metadatas.empty()) {
    return endpoint_entry;
  }
  return HostCache::Entry::MergeEntries(
      HostCache::Entry(OK, std::move(endpoint_metadatas),
                       HostCache::Entry::SOURCE_UNKNOWN),
      endpoint_entry);
}
}  // namespace

int ParseAddressList(std::string_view host_list,
                     std::vector<net::IPEndPoint>* ip_endpoints) {
  ip_endpoints->clear();
  for (std::string_view address : base::SplitStringPiece(
           host_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    IPAddress ip_address;
    if (!ip_address.AssignFromIPLiteral(address)) {
      LOG(WARNING) << "Not a supported IP literal: " << address;
      return ERR_UNEXPECTED;
    }
    ip_endpoints->push_back(IPEndPoint(ip_address, 0));
  }
  return OK;
}

// Base class for
// MockHostResolverBase::{RequestImpl,ServiceEndpointRequestImpl}.
class MockHostResolverBase::RequestBase {
 public:
  RequestBase(Host request_endpoint,
              const NetworkAnonymizationKey& network_anonymization_key,
              const std::optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<MockHostResolverBase> resolver)
      : request_endpoint_(std::move(request_endpoint)),
        network_anonymization_key_(network_anonymization_key),
        parameters_(optional_parameters ? optional_parameters.value()
                                        : ResolveHostParameters()),
        priority_(parameters_.initial_priority),
        host_resolver_flags_(ParametersToHostResolverFlags(parameters_)),
        resolve_error_info_(ResolveErrorInfo(ERR_IO_PENDING)),
        resolver_(resolver) {}

  RequestBase(const RequestBase&) = delete;
  RequestBase& operator=(const RequestBase&) = delete;

  virtual ~RequestBase() {
    if (id_ > 0) {
      if (resolver_) {
        resolver_->DetachRequest(id_);
      }
      id_ = 0;
      resolver_ = nullptr;
    }
  }

  void DetachFromResolver() {
    id_ = 0;
    resolver_ = nullptr;
  }

  void SetError(int error) {
    // Should only be called before request is marked completed.
    DCHECK(!complete_);
    resolve_error_info_ = ResolveErrorInfo(error);
  }

  // Sets `endpoint_results_`, `fixed_up_dns_alias_results_`,
  // `address_results_` and `staleness_` after fixing them up.
  // Also sets `error` to OK.
  void SetEndpointResults(
      std::vector<HostResolverEndpointResult> endpoint_results,
      std::set<std::string> aliases,
      std::optional<HostCache::EntryStaleness> staleness) {
    DCHECK(!complete_);
    DCHECK(!endpoint_results_);
    DCHECK(!parameters_.is_speculative);

    endpoint_results_ = std::move(endpoint_results);
    for (auto& result : *endpoint_results_) {
      result.ip_endpoints = FixupEndPoints(result.ip_endpoints);
    }

    fixed_up_dns_alias_results_ = FixupAliases(aliases);

    // `HostResolver` implementations are expected to provide an `AddressList`
    // result whenever `HostResolverEndpointResult` is also available.
    address_results_ = EndpointResultToAddressList(
        *endpoint_results_, *fixed_up_dns_alias_results_);

    staleness_ = std::move(staleness);

    SetError(OK);
    SetEndpointResultsInternal();
  }

  void OnAsyncCompleted(size_t id, int error) {
    DCHECK_EQ(id_, id);
    id_ = 0;

    // Check that error information has been set and that the top-level error
    // code is valid.
    DCHECK(resolve_error_info_.error != ERR_IO_PENDING);
    DCHECK(error == OK || error == ERR_NAME_NOT_RESOLVED ||
           error == ERR_DNS_NAME_HTTPS_ONLY);

    DCHECK(!complete_);
    complete_ = true;

    DCHECK(callback_);
    std::move(callback_).Run(error);
  }

  const Host& request_endpoint() const { return request_endpoint_; }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
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

  // Similar get GetAddressResults() and GetResolveErrorInfo(), but only exposed
  // through the HostResolver::ResolveHostRequest interface, and don't have the
  // DCHECKs that `complete_` is true.
  const std::optional<AddressList>& address_results() const {
    return address_results_;
  }
  ResolveErrorInfo resolve_error_info() const { return resolve_error_info_; }

 protected:
  std::vector<IPEndPoint> FixupEndPoints(
      const std::vector<IPEndPoint>& endpoints) {
    std::vector<IPEndPoint> corrected;
    for (const IPEndPoint& endpoint : endpoints) {
      DCHECK_NE(endpoint.GetFamily(), ADDRESS_FAMILY_UNSPECIFIED);
      if (parameters_.dns_query_type == DnsQueryType::UNSPECIFIED ||
          parameters_.dns_query_type ==
              AddressFamilyToDnsQueryType(endpoint.GetFamily())) {
        if (endpoint.port() == 0) {
          corrected.emplace_back(endpoint.address(),
                                 request_endpoint_.GetPort());
        } else {
          corrected.push_back(endpoint);
        }
      }
    }
    return corrected;
  }

  std::set<std::string> FixupAliases(const std::set<std::string> aliases) {
    if (aliases.empty()) {
      return std::set<std::string>{
          std::string(request_endpoint_.GetHostnameWithoutBrackets())};
    }
    return aliases;
  }

  // Helper method of SetEndpointResults() for subclass specific logic.
  virtual void SetEndpointResultsInternal() {}

  const Host request_endpoint_;
  const NetworkAnonymizationKey network_anonymization_key_;
  const ResolveHostParameters parameters_;
  RequestPriority priority_;
  int host_resolver_flags_;

  std::optional<AddressList> address_results_;
  std::optional<std::vector<HostResolverEndpointResult>> endpoint_results_;
  std::optional<std::set<std::string>> fixed_up_dns_alias_results_;
  std::optional<HostCache::EntryStaleness> staleness_;
  ResolveErrorInfo resolve_error_info_;

  // Used while stored with the resolver for async resolution.  Otherwise 0.
  size_t id_ = 0;

  CompletionOnceCallback callback_;
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
  bool complete_ = false;
};

class MockHostResolverBase::RequestImpl
    : public RequestBase,
      public HostResolver::ResolveHostRequest {
 public:
  RequestImpl(Host request_endpoint,
              const NetworkAnonymizationKey& network_anonymization_key,
              const std::optional<ResolveHostParameters>& optional_parameters,
              base::WeakPtr<MockHostResolverBase> resolver)
      : RequestBase(std::move(request_endpoint),
                    network_anonymization_key,
                    optional_parameters,
                    std::move(resolver)) {}

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override = default;

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

  const AddressList* GetAddressResults() const override {
    DCHECK(complete_);
    return base::OptionalToPtr(address_results_);
  }

  const std::vector<HostResolverEndpointResult>* GetEndpointResults()
      const override {
    DCHECK(complete_);
    return base::OptionalToPtr(endpoint_results_);
  }

  const std::vector<std::string>* GetTextResults() const override {
    DCHECK(complete_);
    static const base::NoDestructor<std::vector<std::string>> empty_result;
    return empty_result.get();
  }

  const std::vector<HostPortPair>* GetHostnameResults() const override {
    DCHECK(complete_);
    static const base::NoDestructor<std::vector<HostPortPair>> empty_result;
    return empty_result.get();
  }

  const std::set<std::string>* GetDnsAliasResults() const override {
    DCHECK(complete_);
    return base::OptionalToPtr(fixed_up_dns_alias_results_);
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    DCHECK(complete_);
    return resolve_error_info_;
  }

  const std::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    DCHECK(complete_);
    return staleness_;
  }

  void ChangeRequestPriority(RequestPriority priority) override {
    priority_ = priority;
  }
};

class MockHostResolverBase::ServiceEndpointRequestImpl
    : public RequestBase,
      public HostResolver::ServiceEndpointRequest {
 public:
  ServiceEndpointRequestImpl(
      Host request_endpoint,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::optional<ResolveHostParameters>& optional_parameters,
      base::WeakPtr<MockHostResolverBase> resolver)
      : RequestBase(std::move(request_endpoint),
                    network_anonymization_key,
                    optional_parameters,
                    std::move(resolver)) {}

  ServiceEndpointRequestImpl(const ServiceEndpointRequestImpl&) = delete;
  ServiceEndpointRequestImpl& operator=(const ServiceEndpointRequestImpl&) =
      delete;

  ~ServiceEndpointRequestImpl() override = default;

  // HostResolver::ServiceEndpointRequest implementations:
  int Start(Delegate* delegate) override {
    CHECK(delegate);
    CHECK(!delegate_);
    CHECK_EQ(id_, 0u);
    CHECK(!complete_);
    CHECK(resolver_);

    int rv = resolver_->Resolve(this);
    DCHECK(!complete_);
    if (rv == ERR_IO_PENDING) {
      CHECK_GT(id_, 0u);
      delegate_ = delegate;
      callback_ = base::BindOnce(
          &ServiceEndpointRequestImpl::NotifyDelegateOfCompletion,
          weak_ptr_factory_.GetWeakPtr());
    } else {
      CHECK_EQ(id_, 0u);
      complete_ = true;
    }

    return rv;
  }

  const std::vector<ServiceEndpoint>& GetEndpointResults() override {
    return service_endpoint_results_;
  }

  const std::set<std::string>& GetDnsAliasResults() override {
    if (fixed_up_dns_alias_results_.has_value()) {
      return *fixed_up_dns_alias_results_;
    }
    static const base::NoDestructor<std::set<std::string>> kEmptyDnsAliases;
    return *kEmptyDnsAliases.get();
  }

  bool EndpointsCryptoReady() override { return true; }

  ResolveErrorInfo GetResolveErrorInfo() override {
    return resolve_error_info_;
  }

  void ChangeRequestPriority(RequestPriority priority) override {
    priority_ = priority;
  }

 private:
  void SetEndpointResultsInternal() override {
    if (!endpoint_results_.has_value()) {
      return;
    }

    std::vector<ServiceEndpoint> service_endpoints;
    for (const auto& endpoint : *endpoint_results_) {
      std::vector<IPEndPoint> ipv4_endpoints;
      std::vector<IPEndPoint> ipv6_endpoints;
      for (const auto& ip_endpoint : endpoint.ip_endpoints) {
        if (ip_endpoint.address().IsIPv6()) {
          ipv6_endpoints.emplace_back(ip_endpoint);
        } else {
          ipv4_endpoints.emplace_back(ip_endpoint);
        }
      }
      service_endpoints.emplace_back(std::move(ipv4_endpoints),
                                     std::move(ipv6_endpoints),
                                     endpoint.metadata);
    }

    service_endpoint_results_ = std::move(service_endpoints);
  }

  void NotifyDelegateOfCompletion(int rv) {
    CHECK(delegate_);
    CHECK_NE(rv, ERR_IO_PENDING);
    delegate_.ExtractAsDangling()->OnServiceEndpointRequestFinished(rv);
  }

  raw_ptr<Delegate> delegate_;
  std::vector<ServiceEndpoint> service_endpoint_results_;

  base::WeakPtrFactory<ServiceEndpointRequestImpl> weak_ptr_factory_{this};
};

class MockHostResolverBase::ProbeRequestImpl
    : public HostResolver::ProbeRequest {
 public:
  explicit ProbeRequestImpl(base::WeakPtr<MockHostResolverBase> resolver)
      : resolver_(std::move(resolver)) {}

  ProbeRequestImpl(const ProbeRequestImpl&) = delete;
  ProbeRequestImpl& operator=(const ProbeRequestImpl&) = delete;

  ~ProbeRequestImpl() override {
    if (resolver_) {
      resolver_->state_->ClearDohProbeRequestIfMatching(this);
    }
  }

  int Start() override {
    DCHECK(resolver_);
    resolver_->state_->set_doh_probe_request(this);

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
      : host_(host), query_type_(query_type), resolver_(resolver) {
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

  void TriggerAddressResult(MdnsListenerUpdateType update_type,
                            IPEndPoint address) {
    delegate_->OnAddressResult(update_type, query_type_, std::move(address));
  }

  void TriggerTextResult(MdnsListenerUpdateType update_type,
                         std::vector<std::string> text_records) {
    delegate_->OnTextResult(update_type, query_type_, std::move(text_records));
  }

  void TriggerHostnameResult(MdnsListenerUpdateType update_type,
                             HostPortPair host) {
    delegate_->OnHostnameResult(update_type, query_type_, std::move(host));
  }

  void TriggerUnhandledResult(MdnsListenerUpdateType update_type) {
    delegate_->OnUnhandledResult(update_type, query_type_);
  }

  const HostPortPair& host() const { return host_; }
  DnsQueryType query_type() const { return query_type_; }

 private:
  const HostPortPair host_;
  const DnsQueryType query_type_;

  raw_ptr<Delegate> delegate_ = nullptr;

  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding listener objects.
  base::WeakPtr<MockHostResolverBase> resolver_;
};

MockHostResolverBase::RuleResolver::RuleKey::RuleKey() = default;

MockHostResolverBase::RuleResolver::RuleKey::~RuleKey() = default;

MockHostResolverBase::RuleResolver::RuleKey::RuleKey(const RuleKey&) = default;

MockHostResolverBase::RuleResolver::RuleKey&
MockHostResolverBase::RuleResolver::RuleKey::operator=(const RuleKey&) =
    default;

MockHostResolverBase::RuleResolver::RuleKey::RuleKey(RuleKey&&) = default;

MockHostResolverBase::RuleResolver::RuleKey&
MockHostResolverBase::RuleResolver::RuleKey::operator=(RuleKey&&) = default;

MockHostResolverBase::RuleResolver::RuleResult::RuleResult() = default;

MockHostResolverBase::RuleResolver::RuleResult::RuleResult(
    std::vector<HostResolverEndpointResult> endpoints,
    std::set<std::string> aliases)
    : endpoints(std::move(endpoints)), aliases(std::move(aliases)) {}

MockHostResolverBase::RuleResolver::RuleResult::~RuleResult() = default;

MockHostResolverBase::RuleResolver::RuleResult::RuleResult(const RuleResult&) =
    default;

MockHostResolverBase::RuleResolver::RuleResult&
MockHostResolverBase::RuleResolver::RuleResult::operator=(const RuleResult&) =
    default;

MockHostResolverBase::RuleResolver::RuleResult::RuleResult(RuleResult&&) =
    default;

MockHostResolverBase::RuleResolver::RuleResult&
MockHostResolverBase::RuleResolver::RuleResult::operator=(RuleResult&&) =
    default;

MockHostResolverBase::RuleResolver::RuleResolver(
    std::optional<RuleResultOrError> default_result)
    : default_result_(std::move(default_result)) {}

MockHostResolverBase::RuleResolver::~RuleResolver() = default;

MockHostResolverBase::RuleResolver::RuleResolver(const RuleResolver&) = default;

MockHostResolverBase::RuleResolver&
MockHostResolverBase::RuleResolver::operator=(const RuleResolver&) = default;

MockHostResolverBase::RuleResolver::RuleResolver(RuleResolver&&) = default;

MockHostResolverBase::RuleResolver&
MockHostResolverBase::RuleResolver::operator=(RuleResolver&&) = default;

const MockHostResolverBase::RuleResolver::RuleResultOrError&
MockHostResolverBase::RuleResolver::Resolve(
    const Host& request_endpoint,
    DnsQueryTypeSet request_types,
    HostResolverSource request_source) const {
  for (const auto& rule : rules_) {
    const RuleKey& key = rule.first;
    const RuleResultOrError& result = rule.second;

    if (absl::holds_alternative<RuleKey::NoScheme>(key.scheme) &&
        request_endpoint.HasScheme()) {
      continue;
    }

    if (key.port.has_value() &&
        key.port.value() != request_endpoint.GetPort()) {
      continue;
    }

    DCHECK(!key.query_type.has_value() ||
           key.query_type.value() != DnsQueryType::UNSPECIFIED);
    if (key.query_type.has_value() &&
        !request_types.Has(key.query_type.value())) {
      continue;
    }

    if (key.query_source.has_value() &&
        request_source != key.query_source.value()) {
      continue;
    }

    if (absl::holds_alternative<RuleKey::Scheme>(key.scheme) &&
        (!request_endpoint.HasScheme() ||
         request_endpoint.GetScheme() !=
             absl::get<RuleKey::Scheme>(key.scheme))) {
      continue;
    }

    if (!base::MatchPattern(request_endpoint.GetHostnameWithoutBrackets(),
                            key.hostname_pattern)) {
      continue;
    }

    return result;
  }

  if (default_result_)
    return default_result_.value();

  NOTREACHED_IN_MIGRATION() << "Request " << request_endpoint.GetHostname()
                            << " did not match any MockHostResolver rules.";
  static const RuleResultOrError kUnexpected = ERR_UNEXPECTED;
  return kUnexpected;
}

void MockHostResolverBase::RuleResolver::ClearRules() {
  rules_.clear();
}

// static
MockHostResolverBase::RuleResolver::RuleResultOrError
MockHostResolverBase::RuleResolver::GetLocalhostResult() {
  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), /*port=*/0)};
  return RuleResult(std::vector{endpoint});
}

void MockHostResolverBase::RuleResolver::AddRule(RuleKey key,
                                                 RuleResultOrError result) {
  // Literals are always resolved to themselves by MockHostResolverBase,
  // consequently we do not support remapping them.
  IPAddress ip_address;
  DCHECK(!ip_address.AssignFromIPLiteral(key.hostname_pattern));

  CHECK(rules_.emplace(std::move(key), std::move(result)).second)
      << "Duplicate rule key";
}

void MockHostResolverBase::RuleResolver::AddRule(RuleKey key,
                                                 std::string_view ip_literal) {
  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.emplace_back();
  CHECK_EQ(ParseAddressList(ip_literal, &endpoints[0].ip_endpoints), OK);
  AddRule(std::move(key), RuleResult(std::move(endpoints)));
}

void MockHostResolverBase::RuleResolver::AddRule(
    std::string_view hostname_pattern,
    RuleResultOrError result) {
  RuleKey key;
  key.hostname_pattern = std::string(hostname_pattern);
  AddRule(std::move(key), std::move(result));
}

void MockHostResolverBase::RuleResolver::AddRule(
    std::string_view hostname_pattern,
    std::string_view ip_literal) {
  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.emplace_back();
  CHECK_EQ(ParseAddressList(ip_literal, &endpoints[0].ip_endpoints), OK);
  AddRule(hostname_pattern, RuleResult(std::move(endpoints)));
}

void MockHostResolverBase::RuleResolver::AddRule(
    std::string_view hostname_pattern,
    Error error) {
  RuleKey key;
  key.hostname_pattern = std::string(hostname_pattern);

  AddRule(std::move(key), error);
}

void MockHostResolverBase::RuleResolver::AddIPLiteralRule(
    std::string_view hostname_pattern,
    std::string_view ip_literal,
    std::string_view canonical_name) {
  RuleKey key;
  key.hostname_pattern = std::string(hostname_pattern);

  std::set<std::string> aliases;
  if (!canonical_name.empty())
    aliases.emplace(canonical_name);

  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.emplace_back();
  CHECK_EQ(ParseAddressList(ip_literal, &endpoints[0].ip_endpoints), OK);
  AddRule(std::move(key), RuleResult(std::move(endpoints), std::move(aliases)));
}

void MockHostResolverBase::RuleResolver::AddIPLiteralRuleWithDnsAliases(
    std::string_view hostname_pattern,
    std::string_view ip_literal,
    std::vector<std::string> dns_aliases) {
  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.emplace_back();
  CHECK_EQ(ParseAddressList(ip_literal, &endpoints[0].ip_endpoints), OK);
  AddRule(hostname_pattern,
          RuleResult(
              std::move(endpoints),
              std::set<std::string>(dns_aliases.begin(), dns_aliases.end())));
}

void MockHostResolverBase::RuleResolver::AddIPLiteralRuleWithDnsAliases(
    std::string_view hostname_pattern,
    std::string_view ip_literal,
    std::set<std::string> dns_aliases) {
  std::vector<std::string> aliases_vector;
  base::ranges::move(dns_aliases, std::back_inserter(aliases_vector));

  AddIPLiteralRuleWithDnsAliases(hostname_pattern, ip_literal,
                                 std::move(aliases_vector));
}

void MockHostResolverBase::RuleResolver::AddSimulatedFailure(
    std::string_view hostname_pattern) {
  AddRule(hostname_pattern, ERR_NAME_NOT_RESOLVED);
}

void MockHostResolverBase::RuleResolver::AddSimulatedTimeoutFailure(
    std::string_view hostname_pattern) {
  AddRule(hostname_pattern, ERR_DNS_TIMED_OUT);
}

void MockHostResolverBase::RuleResolver::AddRuleWithFlags(
    std::string_view host_pattern,
    std::string_view ip_literal,
    HostResolverFlags /*flags*/,
    std::vector<std::string> dns_aliases) {
  std::vector<HostResolverEndpointResult> endpoints;
  endpoints.emplace_back();
  CHECK_EQ(ParseAddressList(ip_literal, &endpoints[0].ip_endpoints), OK);
  AddRule(host_pattern, RuleResult(std::move(endpoints),
                                   std::set<std::string>(dns_aliases.begin(),
                                                         dns_aliases.end())));
}

MockHostResolverBase::State::State() = default;
MockHostResolverBase::State::~State() = default;

MockHostResolverBase::~MockHostResolverBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Sanity check that pending requests are always cleaned up, by waiting for
  // completion, manually cancelling, or calling OnShutdown().
  DCHECK(!state_->has_pending_requests());
}

void MockHostResolverBase::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Cancel all pending requests.
  for (auto& request : state_->mutable_requests()) {
    request.second->DetachFromResolver();
  }
  state_->mutable_requests().clear();

  // Prevent future requests by clearing resolution rules and the cache.
  rule_resolver_.ClearRules();
  cache_ = nullptr;

  state_->ClearDohProbeRequest();
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MockHostResolverBase::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  return std::make_unique<RequestImpl>(
      Host(std::move(host)), network_anonymization_key, optional_parameters,
      weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::ResolveHostRequest>
MockHostResolverBase::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    const NetLogWithSource& source_net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  return std::make_unique<RequestImpl>(Host(host), network_anonymization_key,
                                       optional_parameters,
                                       weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
MockHostResolverBase::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  return std::make_unique<ServiceEndpointRequestImpl>(
      std::move(host), network_anonymization_key, parameters,
      weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::ProbeRequest>
MockHostResolverBase::CreateDohProbeRequest() {
  return std::make_unique<ProbeRequestImpl>(weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::MdnsListener>
MockHostResolverBase::CreateMdnsListener(const HostPortPair& host,
                                         DnsQueryType query_type) {
  return std::make_unique<MdnsListenerImpl>(host, query_type,
                                            weak_ptr_factory_.GetWeakPtr());
}

HostCache* MockHostResolverBase::GetHostCache() {
  return cache_.get();
}

int MockHostResolverBase::LoadIntoCache(
    absl::variant<url::SchemeHostPort, HostPortPair> endpoint,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  return LoadIntoCache(Host(std::move(endpoint)), network_anonymization_key,
                       optional_parameters);
}

int MockHostResolverBase::LoadIntoCache(
    const Host& endpoint,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cache_);

  ResolveHostParameters parameters =
      optional_parameters.value_or(ResolveHostParameters());

  std::vector<HostResolverEndpointResult> endpoints;
  std::set<std::string> aliases;
  std::optional<HostCache::EntryStaleness> stale_info;
  int rv = ResolveFromIPLiteralOrCache(
      endpoint, network_anonymization_key, parameters.dns_query_type,
      ParametersToHostResolverFlags(parameters), parameters.source,
      parameters.cache_usage, &endpoints, &aliases, &stale_info);
  if (rv != ERR_DNS_CACHE_MISS) {
    // Request already in cache (or IP literal). No need to load it.
    return rv;
  }

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!dns_names_util::IsValidDnsName(endpoint.GetHostnameWithoutBrackets()))
    return ERR_NAME_NOT_RESOLVED;

  RequestImpl request(endpoint, network_anonymization_key, optional_parameters,
                      weak_ptr_factory_.GetWeakPtr());
  return DoSynchronousResolution(request);
}

void MockHostResolverBase::ResolveAllPending() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ondemand_mode_);
  for (auto& [id, request] : state_->mutable_requests()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockHostResolverBase::ResolveNow,
                                  weak_ptr_factory_.GetWeakPtr(), id));
  }
}

size_t MockHostResolverBase::last_id() {
  if (!has_pending_requests())
    return 0;
  return state_->mutable_requests().rbegin()->first;
}

void MockHostResolverBase::ResolveNow(size_t id) {
  auto it = state_->mutable_requests().find(id);
  if (it == state_->mutable_requests().end())
    return;  // was canceled

  RequestBase* req = it->second;
  state_->mutable_requests().erase(it);

  int error = DoSynchronousResolution(*req);
  req->OnAsyncCompleted(id, error);
}

void MockHostResolverBase::DetachRequest(size_t id) {
  auto it = state_->mutable_requests().find(id);
  CHECK(it != state_->mutable_requests().end());
  state_->mutable_requests().erase(it);
}

std::string_view MockHostResolverBase::request_host(size_t id) {
  DCHECK(request(id));
  return request(id)->request_endpoint().GetHostnameWithoutBrackets();
}

RequestPriority MockHostResolverBase::request_priority(size_t id) {
  DCHECK(request(id));
  return request(id)->priority();
}

const NetworkAnonymizationKey&
MockHostResolverBase::request_network_anonymization_key(size_t id) {
  DCHECK(request(id));
  return request(id)->network_anonymization_key();
}

void MockHostResolverBase::ResolveOnlyRequestNow() {
  DCHECK_EQ(1u, state_->mutable_requests().size());
  ResolveNow(state_->mutable_requests().begin()->first);
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type,
    const IPEndPoint& address_result) {
  for (MdnsListenerImpl* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerAddressResult(update_type, address_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type,
    const std::vector<std::string>& text_result) {
  for (MdnsListenerImpl* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerTextResult(update_type, text_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type,
    const HostPortPair& host_result) {
  for (MdnsListenerImpl* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerHostnameResult(update_type, host_result);
  }
}

void MockHostResolverBase::TriggerMdnsListeners(
    const HostPortPair& host,
    DnsQueryType query_type,
    MdnsListenerUpdateType update_type) {
  for (MdnsListenerImpl* listener : listeners_) {
    if (listener->host() == host && listener->query_type() == query_type)
      listener->TriggerUnhandledResult(update_type);
  }
}

MockHostResolverBase::RequestBase* MockHostResolverBase::request(size_t id) {
  RequestMap::iterator request = state_->mutable_requests().find(id);
  CHECK(request != state_->mutable_requests().end());
  CHECK_EQ(request->second->id(), id);
  return (*request).second;
}

// start id from 1 to distinguish from NULL RequestHandle
MockHostResolverBase::MockHostResolverBase(bool use_caching,
                                           int cache_invalidation_num,
                                           RuleResolver rule_resolver)
    : rule_resolver_(std::move(rule_resolver)),
      initial_cache_invalidation_num_(cache_invalidation_num),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      state_(base::MakeRefCounted<State>()) {
  if (use_caching)
    cache_ = std::make_unique<HostCache>(kMaxCacheEntries);
  else
    DCHECK_GE(0, cache_invalidation_num);
}

int MockHostResolverBase::Resolve(RequestBase* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  last_request_priority_ = request->parameters().initial_priority;
  last_request_network_anonymization_key_ =
      request->network_anonymization_key();
  last_secure_dns_policy_ = request->parameters().secure_dns_policy;
  state_->IncrementNumResolve();
  std::vector<HostResolverEndpointResult> endpoints;
  std::set<std::string> aliases;
  std::optional<HostCache::EntryStaleness> stale_info;
  // TODO(crbug.com/40203587): Allow caching `ConnectionEndpoint` results.
  int rv = ResolveFromIPLiteralOrCache(
      request->request_endpoint(), request->network_anonymization_key(),
      request->parameters().dns_query_type, request->host_resolver_flags(),
      request->parameters().source, request->parameters().cache_usage,
      &endpoints, &aliases, &stale_info);

  if (rv == OK && !request->parameters().is_speculative) {
    request->SetEndpointResults(std::move(endpoints), std::move(aliases),
                                std::move(stale_info));
  } else {
    request->SetError(rv);
  }

  if (rv != ERR_DNS_CACHE_MISS ||
      request->parameters().source == HostResolverSource::LOCAL_ONLY) {
    return SquashErrorCode(rv);
  }

  // Just like the real resolver, refuse to do anything with invalid
  // hostnames.
  if (!dns_names_util::IsValidDnsName(
          request->request_endpoint().GetHostnameWithoutBrackets())) {
    request->SetError(ERR_NAME_NOT_RESOLVED);
    return ERR_NAME_NOT_RESOLVED;
  }

  if (synchronous_mode_)
    return DoSynchronousResolution(*request);

  // Store the request for asynchronous resolution
  size_t id = next_request_id_++;
  request->set_id(id);
  state_->mutable_requests()[id] = request;

  if (!ondemand_mode_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockHostResolverBase::ResolveNow,
                                  weak_ptr_factory_.GetWeakPtr(), id));
  }

  return ERR_IO_PENDING;
}

int MockHostResolverBase::ResolveFromIPLiteralOrCache(
    const Host& endpoint,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType dns_query_type,
    HostResolverFlags flags,
    HostResolverSource source,
    HostResolver::ResolveHostParameters::CacheUsage cache_usage,
    std::vector<HostResolverEndpointResult>* out_endpoints,
    std::set<std::string>* out_aliases,
    std::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_endpoints);
  DCHECK(out_aliases);
  DCHECK(out_stale_info);
  out_endpoints->clear();
  out_aliases->clear();
  *out_stale_info = std::nullopt;

  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(endpoint.GetHostnameWithoutBrackets())) {
    const DnsQueryType desired_address_query =
        AddressFamilyToDnsQueryType(GetAddressFamily(ip_address));
    DCHECK_NE(desired_address_query, DnsQueryType::UNSPECIFIED);

    // This matches the behavior HostResolverImpl.
    if (dns_query_type != DnsQueryType::UNSPECIFIED &&
        dns_query_type != desired_address_query) {
      return ERR_NAME_NOT_RESOLVED;
    }

    *out_endpoints = std::vector<HostResolverEndpointResult>(1);
    (*out_endpoints)[0].ip_endpoints.emplace_back(ip_address,
                                                  endpoint.GetPort());
    if (flags & HOST_RESOLVER_CANONNAME)
      *out_aliases = {ip_address.ToString()};
    return OK;
  }

  std::vector<IPEndPoint> localhost_endpoints;
  // Immediately resolve any "localhost" or recognized similar names.
  if (IsAddressType(dns_query_type) &&
      ResolveLocalHostname(endpoint.GetHostnameWithoutBrackets(),
                           &localhost_endpoints)) {
    *out_endpoints = std::vector<HostResolverEndpointResult>(1);
    (*out_endpoints)[0].ip_endpoints = localhost_endpoints;
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
    HostCache::Key key(GetCacheHost(endpoint), dns_query_type, flags,
                       effective_source, network_anonymization_key);
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
        *out_endpoints = cache_result->second.GetEndpoints();

        *out_aliases = cache_result->second.aliases();
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

int MockHostResolverBase::DoSynchronousResolution(RequestBase& request) {
  state_->IncrementNumNonLocalResolves();

  const RuleResolver::RuleResultOrError& result = rule_resolver_.Resolve(
      request.request_endpoint(), {request.parameters().dns_query_type},
      request.parameters().source);

  int error = ERR_UNEXPECTED;
  std::optional<HostCache::Entry> cache_entry;
  if (absl::holds_alternative<RuleResolver::RuleResult>(result)) {
    const auto& rule_result = absl::get<RuleResolver::RuleResult>(result);
    const auto& endpoint_results = rule_result.endpoints;
    const auto& aliases = rule_result.aliases;
    request.SetEndpointResults(endpoint_results, aliases,
                               /*staleness=*/std::nullopt);
    // TODO(crbug.com/40203587): Change `error` on empty results?
    error = OK;
    if (cache_.get()) {
      cache_entry = CreateCacheEntry(request.request_endpoint().GetHostname(),
                                     endpoint_results, aliases);
    }
  } else {
    DCHECK(absl::holds_alternative<RuleResolver::ErrorResult>(result));
    error = absl::get<RuleResolver::ErrorResult>(result);
    request.SetError(error);
    if (cache_.get()) {
      cache_entry.emplace(error, HostCache::Entry::SOURCE_UNKNOWN);
    }
  }
  if (cache_.get() && cache_entry.has_value()) {
    HostCache::Key key(
        GetCacheHost(request.request_endpoint()),
        request.parameters().dns_query_type, request.host_resolver_flags(),
        request.parameters().source, request.network_anonymization_key());
    // Storing a failure with TTL 0 so that it overwrites previous value.
    base::TimeDelta ttl;
    if (error == OK) {
      ttl = base::Seconds(kCacheEntryTTLSeconds);
      if (initial_cache_invalidation_num_ > 0)
        cache_invalidation_nums_[key] = initial_cache_invalidation_num_;
    }
    cache_->Set(key, cache_entry.value(), tick_clock_->NowTicks(), ttl);
  }

  return SquashErrorCode(error);
}

void MockHostResolverBase::AddListener(MdnsListenerImpl* listener) {
  listeners_.insert(listener);
}

void MockHostResolverBase::RemoveCancelledListener(MdnsListenerImpl* listener) {
  listeners_.erase(listener);
}

MockHostResolverFactory::MockHostResolverFactory(
    MockHostResolverBase::RuleResolver rules,
    bool use_caching,
    int cache_invalidation_num)
    : rules_(std::move(rules)),
      use_caching_(use_caching),
      cache_invalidation_num_(cache_invalidation_num) {}

MockHostResolverFactory::~MockHostResolverFactory() = default;

std::unique_ptr<HostResolver> MockHostResolverFactory::CreateResolver(
    HostResolverManager* manager,
    std::string_view host_mapping_rules,
    bool enable_caching) {
  DCHECK(host_mapping_rules.empty());

  // Explicit new to access private constructor.
  auto resolver = base::WrapUnique(new MockHostResolverBase(
      enable_caching && use_caching_, cache_invalidation_num_, rules_));
  return resolver;
}

std::unique_ptr<HostResolver> MockHostResolverFactory::CreateStandaloneResolver(
    NetLog* net_log,
    const HostResolver::ManagerOptions& options,
    std::string_view host_mapping_rules,
    bool enable_caching) {
  return CreateResolver(nullptr, host_mapping_rules, enable_caching);
}

//-----------------------------------------------------------------------------

RuleBasedHostResolverProc::Rule::Rule(ResolverType resolver_type,
                                      std::string_view host_pattern,
                                      AddressFamily address_family,
                                      HostResolverFlags host_resolver_flags,
                                      std::string_view replacement,
                                      std::vector<std::string> dns_aliases,
                                      int latency_ms)
    : resolver_type(resolver_type),
      host_pattern(host_pattern),
      address_family(address_family),
      host_resolver_flags(host_resolver_flags),
      replacement(replacement),
      dns_aliases(std::move(dns_aliases)),
      latency_ms(latency_ms) {
  DCHECK(this->dns_aliases != std::vector<std::string>({""}));
}

RuleBasedHostResolverProc::Rule::Rule(const Rule& other) = default;

RuleBasedHostResolverProc::Rule::~Rule() = default;

RuleBasedHostResolverProc::RuleBasedHostResolverProc(
    scoped_refptr<HostResolverProc> previous,
    bool allow_fallback)
    : HostResolverProc(std::move(previous), allow_fallback) {}

void RuleBasedHostResolverProc::AddRule(std::string_view host_pattern,
                                        std::string_view replacement) {
  AddRuleForAddressFamily(host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
                          replacement);
}

void RuleBasedHostResolverProc::AddRuleForAddressFamily(
    std::string_view host_pattern,
    AddressFamily address_family,
    std::string_view replacement) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, address_family, flags,
            replacement, {} /* dns_aliases */, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddRuleWithFlags(
    std::string_view host_pattern,
    std::string_view replacement,
    HostResolverFlags flags,
    std::vector<std::string> dns_aliases) {
  DCHECK(!replacement.empty());
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, std::move(dns_aliases), 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRule(
    std::string_view host_pattern,
    std::string_view ip_literal,
    std::string_view canonical_name) {
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
    std::string_view host_pattern,
    std::string_view ip_literal,
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
    std::string_view host_pattern,
    std::string_view replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, /*dns_aliases=*/{}, latency_ms);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    std::string_view host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), /*dns_aliases=*/{}, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    std::string_view host_pattern,
    HostResolverFlags flags) {
  Rule rule(Rule::kResolverTypeFail, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, std::string(), /*dns_aliases=*/{}, 0);
  AddRuleInternal(rule);
}

void RuleBasedHostResolverProc::AddSimulatedTimeoutFailure(
    std::string_view host_pattern,
    HostResolverFlags flags) {
  Rule rule(Rule::kResolverTypeFailTimeout, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, std::string(),
            /*dns_aliases=*/{}, 0);
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

size_t RuleBasedHostResolverProc::NumResolvesForHostPattern(
    std::string_view host_pattern) {
  base::AutoLock lock(rule_lock_);
  return num_resolves_per_host_pattern_[host_pattern];
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
      num_resolves_per_host_pattern_[r->host_pattern]++;

      if (r->latency_ms != 0) {
        base::PlatformThread::Sleep(base::Milliseconds(r->latency_ms));
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
          EnsureSystemHostResolverCallReady();
          return SystemHostResolverCall(effective_host, address_family,
                                        host_resolver_flags, addrlist,
                                        os_error);
        case Rule::kResolverTypeIPLiteral: {
          AddressList raw_addr_list;
          std::vector<std::string> aliases;
          aliases = (!r->dns_aliases.empty())
                        ? r->dns_aliases
                        : std::vector<std::string>({host});
          std::vector<net::IPEndPoint> ip_endpoints;
          int result = ParseAddressList(effective_host, &ip_endpoints);
          // Filter out addresses with the wrong family.
          *addrlist = AddressList();
          for (const auto& address : ip_endpoints) {
            if (address_family == ADDRESS_FAMILY_UNSPECIFIED ||
                address_family == address.GetFamily()) {
              addrlist->push_back(address);
            }
          }
          addrlist->SetDnsAliases(aliases);

          if (result == OK && addrlist->empty())
            return ERR_NAME_NOT_RESOLVED;
          return result;
        }
        default:
          NOTREACHED_IN_MIGRATION();
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
  // * CHECK that replacement is empty (empty domain names mean use a direct
  //   lookup) or a valid DNS name (which includes IP addresses).
  // * If the replacement is an IP address, switch to an IP literal rule.
  if (fixed_rule.resolver_type == Rule::kResolverTypeSystem) {
    CHECK(fixed_rule.replacement.empty() ||
          dns_names_util::IsValidDnsName(fixed_rule.replacement));

    IPAddress ip_address;
    bool valid_address = ip_address.AssignFromIPLiteral(fixed_rule.replacement);
    if (valid_address) {
      fixed_rule.resolver_type = Rule::kResolverTypeIPLiteral;
    }
  }

  CHECK(modifications_allowed_);
  base::AutoLock lock(rule_lock_);
  rules_.push_back(fixed_rule);
}

scoped_refptr<RuleBasedHostResolverProc> CreateCatchAllHostResolverProc() {
  auto catchall =
      base::MakeRefCounted<RuleBasedHostResolverProc>(/*previous=*/nullptr,
                                                      /*allow_fallback=*/false);
  // Note that IPv6 lookups fail.
  catchall->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  // Next add a rules-based layer that the test controls.
  return base::MakeRefCounted<RuleBasedHostResolverProc>(
      std::move(catchall), /*allow_fallback=*/false);
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

  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;

  ~RequestImpl() override {
    if (is_running_ && resolver_)
      resolver_->state_->IncrementNumCancellations();
  }

  int Start(CompletionOnceCallback callback) override { return Start(); }

  int Start() override {
    DCHECK(resolver_);
    is_running_ = true;
    return ERR_IO_PENDING;
  }

  const AddressList* GetAddressResults() const override {
    base::ImmediateCrash();
  }

  const std::vector<HostResolverEndpointResult>* GetEndpointResults()
      const override {
    base::ImmediateCrash();
  }

  const std::vector<std::string>* GetTextResults() const override {
    base::ImmediateCrash();
  }

  const std::vector<HostPortPair>* GetHostnameResults() const override {
    base::ImmediateCrash();
  }

  const std::set<std::string>* GetDnsAliasResults() const override {
    base::ImmediateCrash();
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    base::ImmediateCrash();
  }

  const std::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    base::ImmediateCrash();
  }

  void ChangeRequestPriority(RequestPriority priority) override {}

 private:
  // Use a WeakPtr as the resolver may be destroyed while there are still
  // outstanding request objects.
  base::WeakPtr<HangingHostResolver> resolver_;
  bool is_running_ = false;
};

HangingHostResolver::State::State() = default;
HangingHostResolver::State::~State() = default;

HangingHostResolver::HangingHostResolver()
    : state_(base::MakeRefCounted<State>()) {}

HangingHostResolver::~HangingHostResolver() = default;

void HangingHostResolver::OnShutdown() {
  shutting_down_ = true;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HangingHostResolver::CreateRequest(
    url::SchemeHostPort host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  // TODO(crbug.com/40181080): Propagate scheme and make affect behavior.
  return CreateRequest(HostPortPair::FromSchemeHostPort(host),
                       network_anonymization_key, net_log, optional_parameters);
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HangingHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkAnonymizationKey& network_anonymization_key,
    const NetLogWithSource& source_net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  last_host_ = host;
  last_network_anonymization_key_ = network_anonymization_key;

  if (shutting_down_)
    return CreateFailingRequest(ERR_CONTEXT_SHUT_DOWN);

  if (optional_parameters &&
      optional_parameters.value().source == HostResolverSource::LOCAL_ONLY) {
    return CreateFailingRequest(ERR_DNS_CACHE_MISS);
  }

  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
HangingHostResolver::CreateServiceEndpointRequest(
    Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<HostResolver::ProbeRequest>
HangingHostResolver::CreateDohProbeRequest() {
  if (shutting_down_)
    return CreateFailingProbeRequest(ERR_CONTEXT_SHUT_DOWN);

  return std::make_unique<RequestImpl>(weak_ptr_factory_.GetWeakPtr());
}

void HangingHostResolver::SetRequestContext(
    URLRequestContext* url_request_context) {}

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
  current_proc_->SetLastProc(previous_proc_);
}

}  // namespace net
