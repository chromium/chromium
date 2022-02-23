// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/immediate_crash.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_results.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/resolve_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

class FailingRequestImpl : public HostResolver::ResolveHostRequest,
                           public HostResolver::ProbeRequest {
 public:
  explicit FailingRequestImpl(int error) : error_(error) {}

  FailingRequestImpl(const FailingRequestImpl&) = delete;
  FailingRequestImpl& operator=(const FailingRequestImpl&) = delete;

  ~FailingRequestImpl() override = default;

  int Start(CompletionOnceCallback callback) override { return error_; }
  int Start() override { return error_; }

  AddressList* GetAddressResults() const override { return nullptr; }

  std::vector<HostResolverEndpointResult>* GetEndpointResults() const override {
    return nullptr;
  }

  const absl::optional<std::vector<std::string>>& GetTextResults()
      const override {
    static const base::NoDestructor<absl::optional<std::vector<std::string>>>
        nullopt_result;
    return *nullopt_result;
  }

  const absl::optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    static const base::NoDestructor<absl::optional<std::vector<HostPortPair>>>
        nullopt_result;
    return *nullopt_result;
  }

  const std::set<std::string>* GetDnsAliasResults() const override {
    return nullptr;
  }

  ResolveErrorInfo GetResolveErrorInfo() const override {
    return ResolveErrorInfo(error_);
  }

  const absl::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    static const absl::optional<HostCache::EntryStaleness> nullopt_result;
    return nullopt_result;
  }

 private:
  const int error_;
};

bool EndpointResultIsNonProtocol(const HostResolverEndpointResult& result) {
  return result.metadata.supported_protocol_alpns.empty();
}

}  // namespace

const std::vector<bool>*
HostResolver::ResolveHostRequest::GetExperimentalResultsForTesting() const {
  IMMEDIATE_CRASH();
}

const size_t HostResolver::ManagerOptions::kDefaultRetryAttempts =
    static_cast<size_t>(-1);

std::unique_ptr<HostResolver> HostResolver::Factory::CreateResolver(
    HostResolverManager* manager,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  return HostResolver::CreateResolver(manager, host_mapping_rules,
                                      enable_caching);
}

std::unique_ptr<HostResolver> HostResolver::Factory::CreateStandaloneResolver(
    NetLog* net_log,
    const ManagerOptions& options,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  return HostResolver::CreateStandaloneResolver(
      net_log, options, host_mapping_rules, enable_caching);
}

HostResolver::ResolveHostParameters::ResolveHostParameters() = default;

HostResolver::ResolveHostParameters::ResolveHostParameters(
    const ResolveHostParameters& other) = default;

HostResolver::~HostResolver() = default;

std::unique_ptr<HostResolver::ProbeRequest>
HostResolver::CreateDohProbeRequest() {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<HostResolver::MdnsListener> HostResolver::CreateMdnsListener(
    const HostPortPair& host,
    DnsQueryType query_type) {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
  return nullptr;
}

HostCache* HostResolver::GetHostCache() {
  return nullptr;
}

base::Value HostResolver::GetDnsConfigAsValue() const {
  return base::Value(base::Value::Type::DICTIONARY);
}

void HostResolver::SetRequestContext(URLRequestContext* request_context) {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
}

HostResolverManager* HostResolver::GetManagerForTesting() {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
  return nullptr;
}

const URLRequestContext* HostResolver::GetContextForTesting() const {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED();
  return nullptr;
}

// static
std::unique_ptr<HostResolver> HostResolver::CreateResolver(
    HostResolverManager* manager,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  DCHECK(manager);

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, enable_caching);

  auto resolver = std::make_unique<ContextHostResolver>(
      manager, std::move(resolve_context));

  if (host_mapping_rules.empty())
    return resolver;
  auto remapped_resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver));
  remapped_resolver->SetRulesFromString(host_mapping_rules);
  return remapped_resolver;
}

// static
std::unique_ptr<HostResolver> HostResolver::CreateStandaloneResolver(
    NetLog* net_log,
    absl::optional<ManagerOptions> options,
    base::StringPiece host_mapping_rules,
    bool enable_caching) {
  std::unique_ptr<ContextHostResolver> resolver =
      CreateStandaloneContextResolver(net_log, std::move(options),
                                      enable_caching);

  if (host_mapping_rules.empty())
    return resolver;
  auto remapped_resolver =
      std::make_unique<MappedHostResolver>(std::move(resolver));
  remapped_resolver->SetRulesFromString(host_mapping_rules);
  return remapped_resolver;
}

// static
std::unique_ptr<ContextHostResolver>
HostResolver::CreateStandaloneContextResolver(
    NetLog* net_log,
    absl::optional<ManagerOptions> options,
    bool enable_caching) {
  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /* url_request_context */, enable_caching);

  return std::make_unique<ContextHostResolver>(
      std::make_unique<HostResolverManager>(
          std::move(options).value_or(ManagerOptions()),
          NetworkChangeNotifier::GetSystemDnsConfigNotifier(), net_log),
      std::move(resolve_context));
}

// static
AddressFamily HostResolver::DnsQueryTypeSetToAddressFamily(
    DnsQueryTypeSet dns_query_types) {
  DCHECK(HasAddressType(dns_query_types));
  // If the set of query types contains A and AAAA, defer the choice of address
  // family. Otherwise, pick the corresponding address family.
  if (dns_query_types.HasAll({DnsQueryType::A, DnsQueryType::AAAA}))
    return ADDRESS_FAMILY_UNSPECIFIED;
  if (dns_query_types.Has(DnsQueryType::AAAA))
    return ADDRESS_FAMILY_IPV6;
  DCHECK(dns_query_types.Has(DnsQueryType::A));
  return ADDRESS_FAMILY_IPV4;
}

// static
HostResolverFlags HostResolver::ParametersToHostResolverFlags(
    const ResolveHostParameters& parameters) {
  HostResolverFlags flags = 0;
  if (parameters.include_canonical_name)
    flags |= HOST_RESOLVER_CANONNAME;
  if (parameters.loopback_only)
    flags |= HOST_RESOLVER_LOOPBACK_ONLY;
  if (parameters.avoid_multicast_resolution)
    flags |= HOST_RESOLVER_AVOID_MULTICAST;
  return flags;
}

// static
int HostResolver::SquashErrorCode(int error) {
  // TODO(crbug.com/1043281): Consider squashing ERR_INTERNET_DISCONNECTED.
  if (error == OK || error == ERR_IO_PENDING ||
      error == ERR_INTERNET_DISCONNECTED || error == ERR_NAME_NOT_RESOLVED ||
      error == ERR_DNS_NAME_HTTPS_ONLY) {
    return error;
  } else {
    return ERR_NAME_NOT_RESOLVED;
  }
}

// static
std::vector<HostResolverEndpointResult>
HostResolver::AddressListToEndpointResults(const AddressList& address_list) {
  HostResolverEndpointResult connection_endpoint;
  connection_endpoint.ip_endpoints = address_list.endpoints();

  std::vector<HostResolverEndpointResult> list;
  list.push_back(std::move(connection_endpoint));
  return list;
}

// static
AddressList HostResolver::EndpointResultToAddressList(
    const std::vector<HostResolverEndpointResult>& endpoints,
    const std::set<std::string>& aliases) {
  AddressList list;

  auto non_protocol_endpoint =
      base::ranges::find_if(endpoints, &EndpointResultIsNonProtocol);
  if (non_protocol_endpoint == endpoints.end())
    return list;

  list.endpoints() = non_protocol_endpoint->ip_endpoints;

  std::vector<std::string> aliases_vector(aliases.begin(), aliases.end());
  list.SetDnsAliases(std::move(aliases_vector));

  return list;
}

HostResolver::HostResolver() = default;

// static
std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolver::CreateFailingRequest(int error) {
  return std::make_unique<FailingRequestImpl>(error);
}

// static
std::unique_ptr<HostResolver::ProbeRequest>
HostResolver::CreateFailingProbeRequest(int error) {
  return std::make_unique<FailingRequestImpl>(error);
}

}  // namespace net
