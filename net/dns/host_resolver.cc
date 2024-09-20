// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time_delta_from_string.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/resolve_context.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_library.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace net {

namespace {

// The experiment settings of features::kUseDnsHttpsSvcb. See the comments in
// net/base/features.h for more details.
const char kUseDnsHttpsSvcbEnable[] = "enable";
const char kUseDnsHttpsSvcbInsecureExtraTimeMax[] = "insecure_extra_time_max";
const char kUseDnsHttpsSvcbInsecureExtraTimePercent[] =
    "insecure_extra_time_percent";
const char kUseDnsHttpsSvcbInsecureExtraTimeMin[] = "insecure_extra_time_min";
const char kUseDnsHttpsSvcbSecureExtraTimeMax[] = "secure_extra_time_max";
const char kUseDnsHttpsSvcbSecureExtraTimePercent[] =
    "secure_extra_time_percent";
const char kUseDnsHttpsSvcbSecureExtraTimeMin[] = "secure_extra_time_min";

// An implementation of HostResolver::{ResolveHost,Probe}Request that always
// fails immediately.
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

  const std::vector<std::string>* GetTextResults() const override {
    return nullptr;
  }

  const std::vector<HostPortPair>* GetHostnameResults() const override {
    return nullptr;
  }

  const std::set<std::string>* GetDnsAliasResults() const override {
    return nullptr;
  }

  ResolveErrorInfo GetResolveErrorInfo() const override {
    return ResolveErrorInfo(error_);
  }

  const std::optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    static const std::optional<HostCache::EntryStaleness> nullopt_result;
    return nullopt_result;
  }

 private:
  const int error_;
};

// Similar to FailingRequestImpl, but for ServiceEndpointRequest.
class FailingServiceEndpointRequestImpl
    : public HostResolver::ServiceEndpointRequest {
 public:
  explicit FailingServiceEndpointRequestImpl(int error) : error_(error) {}

  FailingServiceEndpointRequestImpl(const FailingServiceEndpointRequestImpl&) =
      delete;
  FailingServiceEndpointRequestImpl& operator=(
      const FailingServiceEndpointRequestImpl&) = delete;

  ~FailingServiceEndpointRequestImpl() override = default;

  int Start(Delegate* delegate) override { return error_; }

  const std::vector<ServiceEndpoint>& GetEndpointResults() override {
    static const base::NoDestructor<std::vector<ServiceEndpoint>> kEmptyResult;
    return *kEmptyResult.get();
  }

  const std::set<std::string>& GetDnsAliasResults() override {
    static const base::NoDestructor<std::set<std::string>> kEmptyResult;
    return *kEmptyResult.get();
  }

  bool EndpointsCryptoReady() override { return false; }

  ResolveErrorInfo GetResolveErrorInfo() override {
    return ResolveErrorInfo(error_);
  }

  void ChangeRequestPriority(RequestPriority priority) override {}

 private:
  const int error_;
};

bool EndpointResultIsNonProtocol(const HostResolverEndpointResult& result) {
  return result.metadata.supported_protocol_alpns.empty();
}

void GetTimeDeltaFromDictString(const base::Value::Dict& args,
                                std::string_view key,
                                base::TimeDelta* out) {
  const std::string* value_string = args.FindString(key);
  if (!value_string)
    return;
  *out = base::TimeDeltaFromString(*value_string).value_or(*out);
}

}  // namespace

HostResolver::Host::Host(absl::variant<url::SchemeHostPort, HostPortPair> host)
    : host_(std::move(host)) {
#if DCHECK_IS_ON()
  if (absl::holds_alternative<url::SchemeHostPort>(host_)) {
    DCHECK(absl::get<url::SchemeHostPort>(host_).IsValid());
  } else {
    DCHECK(absl::holds_alternative<HostPortPair>(host_));
    DCHECK(!absl::get<HostPortPair>(host_).IsEmpty());
  }
#endif  // DCHECK_IS_ON()
}

HostResolver::Host::~Host() = default;

HostResolver::Host::Host(const Host&) = default;

HostResolver::Host& HostResolver::Host::operator=(const Host&) = default;

HostResolver::Host::Host(Host&&) = default;

HostResolver::Host& HostResolver::Host::operator=(Host&&) = default;

bool HostResolver::Host::HasScheme() const {
  return absl::holds_alternative<url::SchemeHostPort>(host_);
}

const std::string& HostResolver::Host::GetScheme() const {
  DCHECK(absl::holds_alternative<url::SchemeHostPort>(host_));
  return absl::get<url::SchemeHostPort>(host_).scheme();
}

std::string HostResolver::Host::GetHostname() const {
  if (absl::holds_alternative<url::SchemeHostPort>(host_)) {
    return absl::get<url::SchemeHostPort>(host_).host();
  } else {
    DCHECK(absl::holds_alternative<HostPortPair>(host_));
    return absl::get<HostPortPair>(host_).HostForURL();
  }
}

std::string_view HostResolver::Host::GetHostnameWithoutBrackets() const {
  if (absl::holds_alternative<url::SchemeHostPort>(host_)) {
    std::string_view hostname = absl::get<url::SchemeHostPort>(host_).host();
    if (hostname.size() > 2 && hostname.front() == '[' &&
        hostname.back() == ']') {
      return hostname.substr(1, hostname.size() - 2);
    } else {
      return hostname;
    }
  } else {
    DCHECK(absl::holds_alternative<HostPortPair>(host_));
    return absl::get<HostPortPair>(host_).host();
  }
}

uint16_t HostResolver::Host::GetPort() const {
  if (absl::holds_alternative<url::SchemeHostPort>(host_)) {
    return absl::get<url::SchemeHostPort>(host_).port();
  } else {
    DCHECK(absl::holds_alternative<HostPortPair>(host_));
    return absl::get<HostPortPair>(host_).port();
  }
}

std::string HostResolver::Host::ToString() const {
  if (absl::holds_alternative<url::SchemeHostPort>(host_)) {
    return absl::get<url::SchemeHostPort>(host_).Serialize();
  } else {
    DCHECK(absl::holds_alternative<HostPortPair>(host_));
    return absl::get<HostPortPair>(host_).ToString();
  }
}

const url::SchemeHostPort& HostResolver::Host::AsSchemeHostPort() const {
  const url::SchemeHostPort* scheme_host_port =
      absl::get_if<url::SchemeHostPort>(&host_);
  DCHECK(scheme_host_port);
  return *scheme_host_port;
}

HostResolver::HttpsSvcbOptions::HttpsSvcbOptions() = default;

HostResolver::HttpsSvcbOptions::HttpsSvcbOptions(
    const HttpsSvcbOptions& other) = default;
HostResolver::HttpsSvcbOptions::HttpsSvcbOptions(HttpsSvcbOptions&& other) =
    default;

HostResolver::HttpsSvcbOptions::~HttpsSvcbOptions() = default;

// static
HostResolver::HttpsSvcbOptions HostResolver::HttpsSvcbOptions::FromDict(
    const base::Value::Dict& dict) {
  net::HostResolver::HttpsSvcbOptions options;
  options.enable =
      dict.FindBool(kUseDnsHttpsSvcbEnable).value_or(options.enable);
  GetTimeDeltaFromDictString(dict, kUseDnsHttpsSvcbInsecureExtraTimeMax,
                             &options.insecure_extra_time_max);

  options.insecure_extra_time_percent =
      dict.FindInt(kUseDnsHttpsSvcbInsecureExtraTimePercent)
          .value_or(options.insecure_extra_time_percent);
  GetTimeDeltaFromDictString(dict, kUseDnsHttpsSvcbInsecureExtraTimeMin,
                             &options.insecure_extra_time_min);

  GetTimeDeltaFromDictString(dict, kUseDnsHttpsSvcbSecureExtraTimeMax,
                             &options.secure_extra_time_max);

  options.secure_extra_time_percent =
      dict.FindInt(kUseDnsHttpsSvcbSecureExtraTimePercent)
          .value_or(options.secure_extra_time_percent);
  GetTimeDeltaFromDictString(dict, kUseDnsHttpsSvcbSecureExtraTimeMin,
                             &options.secure_extra_time_min);

  return options;
}

// static
HostResolver::HttpsSvcbOptions HostResolver::HttpsSvcbOptions::FromFeatures() {
  net::HostResolver::HttpsSvcbOptions options;
  options.enable = base::FeatureList::IsEnabled(features::kUseDnsHttpsSvcb);
  options.insecure_extra_time_max =
      features::kUseDnsHttpsSvcbInsecureExtraTimeMax.Get();
  options.insecure_extra_time_percent =
      features::kUseDnsHttpsSvcbInsecureExtraTimePercent.Get();
  options.insecure_extra_time_min =
      features::kUseDnsHttpsSvcbInsecureExtraTimeMin.Get();
  options.secure_extra_time_max =
      features::kUseDnsHttpsSvcbSecureExtraTimeMax.Get();
  options.secure_extra_time_percent =
      features::kUseDnsHttpsSvcbSecureExtraTimePercent.Get();
  options.secure_extra_time_min =
      features::kUseDnsHttpsSvcbSecureExtraTimeMin.Get();
  return options;
}

HostResolver::ManagerOptions::ManagerOptions() = default;

HostResolver::ManagerOptions::ManagerOptions(const ManagerOptions& other) =
    default;
HostResolver::ManagerOptions::ManagerOptions(ManagerOptions&& other) = default;

HostResolver::ManagerOptions::~ManagerOptions() = default;

const std::vector<bool>*
HostResolver::ResolveHostRequest::GetExperimentalResultsForTesting() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<HostResolver> HostResolver::Factory::CreateResolver(
    HostResolverManager* manager,
    std::string_view host_mapping_rules,
    bool enable_caching) {
  return HostResolver::CreateResolver(manager, host_mapping_rules,
                                      enable_caching);
}

std::unique_ptr<HostResolver> HostResolver::Factory::CreateStandaloneResolver(
    NetLog* net_log,
    const ManagerOptions& options,
    std::string_view host_mapping_rules,
    bool enable_caching) {
  return HostResolver::CreateStandaloneResolver(
      net_log, options, host_mapping_rules, enable_caching);
}

HostResolver::ResolveHostParameters::ResolveHostParameters() = default;

HostResolver::~HostResolver() = default;

std::unique_ptr<HostResolver::ProbeRequest>
HostResolver::CreateDohProbeRequest() {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<HostResolver::MdnsListener> HostResolver::CreateMdnsListener(
    const HostPortPair& host,
    DnsQueryType query_type) {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

HostCache* HostResolver::GetHostCache() {
  return nullptr;
}

base::Value::Dict HostResolver::GetDnsConfigAsValue() const {
  return base::Value::Dict();
}

void HostResolver::SetRequestContext(URLRequestContext* request_context) {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED_IN_MIGRATION();
}

HostResolverManager* HostResolver::GetManagerForTesting() {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const URLRequestContext* HostResolver::GetContextForTesting() const {
  // Should be overridden in any HostResolver implementation where this method
  // may be called.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

handles::NetworkHandle HostResolver::GetTargetNetworkForTesting() const {
  return handles::kInvalidNetworkHandle;
}

// static
std::unique_ptr<HostResolver> HostResolver::CreateResolver(
    HostResolverManager* manager,
    std::string_view host_mapping_rules,
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
    std::optional<ManagerOptions> options,
    std::string_view host_mapping_rules,
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
    std::optional<ManagerOptions> options,
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
std::unique_ptr<HostResolver>
HostResolver::CreateStandaloneNetworkBoundResolver(
    NetLog* net_log,
    handles::NetworkHandle target_network,
    std::optional<ManagerOptions> options,
    std::string_view host_mapping_rules,
    bool enable_caching) {
#if BUILDFLAG(IS_ANDROID)
  // Note that the logic below uses Android APIs that don't work on a sandboxed
  // process: This is not problematic because this function is used only by
  // Cronet which doesn't enable sandboxing.

  auto resolve_context = std::make_unique<ResolveContext>(
      nullptr /*url_request_context */, enable_caching);
  auto manager_options = std::move(options).value_or(ManagerOptions());
  // Support the use of the built-in resolver when possible.
  bool is_builtin_resolver_supported =
      manager_options.insecure_dns_client_enabled &&
      base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_P;
  if (is_builtin_resolver_supported) {
    // Pre-existing DnsConfigOverrides is currently ignored, consider extending
    // if a use case arises.
    DCHECK(manager_options.dns_config_overrides == DnsConfigOverrides());

    std::vector<IPEndPoint> dns_servers;
    bool dns_over_tls_active;
    std::string dns_over_tls_hostname;
    std::vector<std::string> search_suffixes;
    if (android::GetDnsServersForNetwork(&dns_servers, &dns_over_tls_active,
                                         &dns_over_tls_hostname,
                                         &search_suffixes, target_network)) {
      DnsConfigOverrides dns_config_overrides =
          DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
      dns_config_overrides.nameservers = dns_servers;
      // Android APIs don't specify whether to use DoT or DoH. So, leave the
      // decision to `DnsConfig::allow_dns_over_https_upgrade` default value.
      dns_config_overrides.dns_over_tls_active = dns_over_tls_active;
      dns_config_overrides.dns_over_tls_hostname = dns_over_tls_hostname;
      dns_config_overrides.search = search_suffixes;

      manager_options.dns_config_overrides = dns_config_overrides;
      // Regardless of DoH vs DoT, the important contract to respect is not to
      // perform insecure DNS lookups if `dns_over_tls_active` == true.
      manager_options.additional_types_via_insecure_dns_enabled =
          !dns_over_tls_active;
    } else {
      // Disable when android::GetDnsServersForNetwork fails.
      is_builtin_resolver_supported = false;
    }
  }

  manager_options.insecure_dns_client_enabled = is_builtin_resolver_supported;
  return std::make_unique<ContextHostResolver>(
      HostResolverManager::CreateNetworkBoundHostResolverManager(
          manager_options, target_network, net_log),
      std::move(resolve_context));
#else   // !BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
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
  // TODO(crbug.com/40668952): Consider squashing ERR_INTERNET_DISCONNECTED.
  if (error == OK || error == ERR_IO_PENDING ||
      error == ERR_INTERNET_DISCONNECTED || error == ERR_NAME_NOT_RESOLVED ||
      error == ERR_DNS_NAME_HTTPS_ONLY) {
    return error;
  } else {
    return ERR_NAME_NOT_RESOLVED;
  }
}

// static
AddressList HostResolver::EndpointResultToAddressList(
    base::span<const HostResolverEndpointResult> endpoints,
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

// static
bool HostResolver::MayUseNAT64ForIPv4Literal(HostResolverFlags flags,
                                             HostResolverSource source,
                                             const IPAddress& ip_address) {
  return !(flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6) &&
         ip_address.IsValid() && ip_address.IsIPv4() &&
         (source != HostResolverSource::LOCAL_ONLY);
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

// static
std::unique_ptr<HostResolver::ServiceEndpointRequest>
HostResolver::CreateFailingServiceEndpointRequest(int error) {
  return std::make_unique<FailingServiceEndpointRequestImpl>(error);
}

}  // namespace net
