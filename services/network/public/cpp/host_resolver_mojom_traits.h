// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "services/network/public/mojom/host_resolver.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    StructTraits<network::mojom::DnsOverHttpsServerConfigDataView,
                 net::DnsOverHttpsServerConfig> {
 public:
  static std::string_view server_template(
      const net::DnsOverHttpsServerConfig& server) {
    return server.server_template();
  }
  static const net::DnsOverHttpsServerConfig::Endpoints& endpoints(
      const net::DnsOverHttpsServerConfig& server) {
    return server.endpoints();
  }
  static bool Read(network::mojom::DnsOverHttpsServerConfigDataView data,
                   net::DnsOverHttpsServerConfig* out_config);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    StructTraits<network::mojom::DnsOverHttpsConfigDataView,
                 net::DnsOverHttpsConfig> {
 public:
  static const std::vector<net::DnsOverHttpsServerConfig>& servers(
      const net::DnsOverHttpsConfig& doh_config) {
    return doh_config.servers();
  }
  static bool Read(network::mojom::DnsOverHttpsConfigDataView data,
                   net::DnsOverHttpsConfig* out_config);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    StructTraits<network::mojom::DnsConfigOverridesDataView,
                 net::DnsConfigOverrides> {
  static const std::optional<std::vector<net::IPEndPoint>>& nameservers(
      const net::DnsConfigOverrides& overrides) {
    return overrides.nameservers;
  }

  static const std::optional<std::vector<std::string>>& search(
      const net::DnsConfigOverrides& overrides) {
    return overrides.search;
  }

  static network::mojom::DnsConfigOverrides_Tristate append_to_multi_label_name(
      const net::DnsConfigOverrides& overrides);

  static int ndots(const net::DnsConfigOverrides& overrides) {
    return overrides.ndots.value_or(-1);
  }

  static const std::optional<base::TimeDelta>& fallback_period(
      const net::DnsConfigOverrides& overrides) {
    return overrides.fallback_period;
  }

  static int attempts(const net::DnsConfigOverrides& overrides) {
    return overrides.attempts.value_or(-1);
  }

  static network::mojom::DnsConfigOverrides_Tristate rotate(
      const net::DnsConfigOverrides& overrides);
  static network::mojom::DnsConfigOverrides_Tristate use_local_ipv6(
      const net::DnsConfigOverrides& overrides);

  static const std::optional<net::DnsOverHttpsConfig>& dns_over_https_config(
      const net::DnsConfigOverrides& overrides) {
    return overrides.dns_over_https_config;
  }

  static network::mojom::OptionalSecureDnsMode secure_dns_mode(
      const net::DnsConfigOverrides& overrides);

  static network::mojom::DnsConfigOverrides_Tristate
  allow_dns_over_https_upgrade(const net::DnsConfigOverrides& overrides);

  static bool clear_hosts(const net::DnsConfigOverrides& overrides) {
    return overrides.clear_hosts;
  }

  static bool Read(network::mojom::DnsConfigOverridesDataView data,
                   net::DnsConfigOverrides* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    EnumTraits<network::mojom::DnsQueryType, net::DnsQueryType> {
  static network::mojom::DnsQueryType ToMojom(net::DnsQueryType input);
  static bool FromMojom(network::mojom::DnsQueryType input,
                        net::DnsQueryType* output);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    EnumTraits<network::mojom::ResolveHostParameters_Source,
               net::HostResolverSource> {
  static network::mojom::ResolveHostParameters_Source ToMojom(
      net::HostResolverSource input);
  static bool FromMojom(network::mojom::ResolveHostParameters_Source input,
                        net::HostResolverSource* output);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    EnumTraits<network::mojom::MdnsListenClient_UpdateType,
               net::MdnsListenerUpdateType> {
  static network::mojom::MdnsListenClient_UpdateType ToMojom(
      net::MdnsListenerUpdateType input);
  static bool FromMojom(network::mojom::MdnsListenClient_UpdateType input,
                        net::MdnsListenerUpdateType* output);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    EnumTraits<network::mojom::SecureDnsMode, net::SecureDnsMode> {
  static network::mojom::SecureDnsMode ToMojom(
      net::SecureDnsMode secure_dns_mode);
  static bool FromMojom(network::mojom::SecureDnsMode in,
                        net::SecureDnsMode* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_HOST_RESOLVER)
    EnumTraits<network::mojom::SecureDnsPolicy, net::SecureDnsPolicy> {
  static network::mojom::SecureDnsPolicy ToMojom(
      net::SecureDnsPolicy secure_dns_mode);
  static bool FromMojom(network::mojom::SecureDnsPolicy in,
                        net::SecureDnsPolicy* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
