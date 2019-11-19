// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_overrides.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace mojo {

// This is made visible for use by network::HostResolver. Not intended to be
// used elsewhere.
base::Optional<net::DnsConfig::SecureDnsMode> FromOptionalSecureDnsMode(
    network::mojom::OptionalSecureDnsMode mode);

template <>
struct StructTraits<network::mojom::DnsConfigOverridesDataView,
                    net::DnsConfigOverrides> {
  static const base::Optional<std::vector<net::IPEndPoint>>& nameservers(
      const net::DnsConfigOverrides& overrides) {
    return overrides.nameservers;
  }

  static const base::Optional<std::vector<std::string>>& search(
      const net::DnsConfigOverrides& overrides) {
    return overrides.search;
  }

  static base::Optional<std::vector<network::mojom::DnsHostPtr>> hosts(
      const net::DnsConfigOverrides& overrides);

  static network::mojom::DnsConfigOverrides::Tristate
  append_to_multi_label_name(const net::DnsConfigOverrides& overrides);
  static network::mojom::DnsConfigOverrides::Tristate randomize_ports(
      const net::DnsConfigOverrides& overrides);

  static int ndots(const net::DnsConfigOverrides& overrides) {
    return overrides.ndots.value_or(-1);
  }

  static const base::Optional<base::TimeDelta>& timeout(
      const net::DnsConfigOverrides& overrides) {
    return overrides.timeout;
  }

  static int attempts(const net::DnsConfigOverrides& overrides) {
    return overrides.attempts.value_or(-1);
  }

  static network::mojom::DnsConfigOverrides::Tristate rotate(
      const net::DnsConfigOverrides& overrides);
  static network::mojom::DnsConfigOverrides::Tristate use_local_ipv6(
      const net::DnsConfigOverrides& overrides);

  static base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
  dns_over_https_servers(const net::DnsConfigOverrides& overrides);

  static network::mojom::OptionalSecureDnsMode secure_dns_mode(
      const net::DnsConfigOverrides& overrides);

  static network::mojom::DnsConfigOverrides::Tristate
  allow_dns_over_https_upgrade(const net::DnsConfigOverrides& overrides);

  static const base::Optional<std::vector<std::string>>&
  disabled_upgrade_providers(const net::DnsConfigOverrides& overrides) {
    return overrides.disabled_upgrade_providers;
  }

  static bool Read(network::mojom::DnsConfigOverridesDataView data,
                   net::DnsConfigOverrides* out);
};

template <>
struct EnumTraits<network::mojom::DnsQueryType, net::DnsQueryType> {
  static network::mojom::DnsQueryType ToMojom(net::DnsQueryType input);
  static bool FromMojom(network::mojom::DnsQueryType input,
                        net::DnsQueryType* output);
};

template <>
struct EnumTraits<network::mojom::ResolveHostParameters::Source,
                  net::HostResolverSource> {
  static network::mojom::ResolveHostParameters::Source ToMojom(
      net::HostResolverSource input);
  static bool FromMojom(network::mojom::ResolveHostParameters::Source input,
                        net::HostResolverSource* output);
};

template <>
struct EnumTraits<network::mojom::MdnsListenClient::UpdateType,
                  net::HostResolver::MdnsListener::Delegate::UpdateType> {
  static network::mojom::MdnsListenClient::UpdateType ToMojom(
      net::HostResolver::MdnsListener::Delegate::UpdateType input);
  static bool FromMojom(
      network::mojom::MdnsListenClient::UpdateType input,
      net::HostResolver::MdnsListener::Delegate::UpdateType* output);
};

template <>
struct EnumTraits<network::mojom::SecureDnsMode,
                  net::DnsConfig::SecureDnsMode> {
  static network::mojom::SecureDnsMode ToMojom(
      net::DnsConfig::SecureDnsMode secure_dns_mode);
  static bool FromMojom(network::mojom::SecureDnsMode in,
                        net::DnsConfig::SecureDnsMode* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
