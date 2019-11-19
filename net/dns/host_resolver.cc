// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
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
#include "net/dns/mapped_host_resolver.h"

namespace net {

namespace {

class FailingRequestImpl : public HostResolver::ResolveHostRequest,
                           public HostResolver::ProbeRequest {
 public:
  explicit FailingRequestImpl(int error) : error_(error) {}
  ~FailingRequestImpl() override = default;

  int Start(CompletionOnceCallback callback) override { return error_; }
  int Start() override { return error_; }

  const base::Optional<AddressList>& GetAddressResults() const override {
    static base::NoDestructor<base::Optional<AddressList>> nullopt_result;
    return *nullopt_result;
  }

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    static const base::NoDestructor<base::Optional<std::vector<std::string>>>
        nullopt_result;
    return *nullopt_result;
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    static const base::NoDestructor<base::Optional<std::vector<HostPortPair>>>
        nullopt_result;
    return *nullopt_result;
  }

  const base::Optional<EsniContent>& GetEsniResults() const override {
    static const base::NoDestructor<base::Optional<EsniContent>> nullopt_result;
    return *nullopt_result;
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    static const base::NoDestructor<base::Optional<HostCache::EntryStaleness>>
        nullopt_result;
    return *nullopt_result;
  }

 private:
  const int error_;

  DISALLOW_COPY_AND_ASSIGN(FailingRequestImpl);
};

}  // namespace

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

std::unique_ptr<HostResolver::ResolveHostRequest> HostResolver::CreateRequest(
    const HostPortPair& host,
    const NetLogWithSource& net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  return CreateRequest(host, NetworkIsolationKey(), net_log,
                       optional_parameters);
}

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

std::unique_ptr<base::Value> HostResolver::GetDnsConfigAsValue() const {
  return nullptr;
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

  auto cache = enable_caching ? HostCache::CreateDefaultCache() : nullptr;
  auto resolver =
      std::make_unique<ContextHostResolver>(manager, std::move(cache));

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
    base::Optional<ManagerOptions> options,
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
    base::Optional<ManagerOptions> options,
    bool enable_caching) {
  auto cache = enable_caching ? HostCache::CreateDefaultCache() : nullptr;

  return std::make_unique<ContextHostResolver>(
      std::make_unique<HostResolverManager>(
          std::move(options).value_or(ManagerOptions()),
          NetworkChangeNotifier::GetSystemDnsConfigNotifier(), net_log),
      std::move(cache));
}

// static
AddressFamily HostResolver::DnsQueryTypeToAddressFamily(
    DnsQueryType dns_query_type) {
  switch (dns_query_type) {
    case DnsQueryType::UNSPECIFIED:
      return ADDRESS_FAMILY_UNSPECIFIED;
    case DnsQueryType::A:
      return ADDRESS_FAMILY_IPV4;
    case DnsQueryType::AAAA:
      return ADDRESS_FAMILY_IPV6;
    default:
      // |dns_query_type| should be an address type (A or AAAA) or UNSPECIFIED.
      NOTREACHED();
      return ADDRESS_FAMILY_UNSPECIFIED;
  }
}

// static
HostResolverFlags HostResolver::ParametersToHostResolverFlags(
    const ResolveHostParameters& parameters) {
  HostResolverFlags flags = 0;
  if (parameters.source == HostResolverSource::SYSTEM)
    flags |= HOST_RESOLVER_SYSTEM_ONLY;
  if (parameters.include_canonical_name)
    flags |= HOST_RESOLVER_CANONNAME;
  if (parameters.loopback_only)
    flags |= HOST_RESOLVER_LOOPBACK_ONLY;
  return flags;
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
