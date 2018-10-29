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
#include "net/dns/dns_config_overrides.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/host_resolver.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace mojo {

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

  static bool Read(network::mojom::DnsConfigOverridesDataView data,
                   net::DnsConfigOverrides* out);
};

template <>
struct EnumTraits<network::mojom::ResolveHostParameters::DnsQueryType,
                  net::HostResolver::DnsQueryType> {
  static network::mojom::ResolveHostParameters::DnsQueryType ToMojom(
      net::HostResolver::DnsQueryType input);
  static bool FromMojom(
      network::mojom::ResolveHostParameters::DnsQueryType input,
      net::HostResolver::DnsQueryType* output);
};

template <>
struct EnumTraits<network::mojom::ResolveHostParameters::Source,
                  net::HostResolverSource> {
  static network::mojom::ResolveHostParameters::Source ToMojom(
      net::HostResolverSource input);
  static bool FromMojom(network::mojom::ResolveHostParameters::Source input,
                        net::HostResolverSource* output);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HOST_RESOLVER_MOJOM_TRAITS_H_
