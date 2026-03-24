// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_mojom_traits.h"

#include "base/values.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "services/network/public/cpp/ip_address_mojom_traits.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "services/network/public/mojom/host_resolver.mojom-shared.h"

namespace mojo {

using Tristate = network::mojom::DnsConfigOverrides_Tristate;
using HostResolverSource = network::mojom::ResolveHostParameters_Source;
using MdnsListenerUpdateType = network::mojom::MdnsListenClient_UpdateType;

using network::mojom::DnsConfigOverridesDataView;
using network::mojom::DnsQueryType;
using network::mojom::OptionalSecureDnsMode;

namespace {

Tristate ToTristate(std::optional<bool> optional) {
  if (!optional) {
    return Tristate::NO_OVERRIDE;
  }
  if (optional.value()) {
    return Tristate::TRISTATE_TRUE;
  }
  return Tristate::TRISTATE_FALSE;
}

std::optional<bool> FromTristate(Tristate tristate) {
  switch (tristate) {
    case Tristate::NO_OVERRIDE:
      return std::nullopt;
    case Tristate::TRISTATE_TRUE:
      return true;
    case Tristate::TRISTATE_FALSE:
      return false;
  }
}

OptionalSecureDnsMode ToOptionalSecureDnsMode(
    std::optional<net::SecureDnsMode> optional) {
  if (!optional) {
    return OptionalSecureDnsMode::NO_OVERRIDE;
  }
  switch (optional.value()) {
    case net::SecureDnsMode::kOff:
      return OptionalSecureDnsMode::OFF;
    case net::SecureDnsMode::kAutomatic:
      return OptionalSecureDnsMode::AUTOMATIC;
    case net::SecureDnsMode::kSecure:
      return OptionalSecureDnsMode::SECURE;
  }
}

std::optional<net::SecureDnsMode> FromOptionalSecureDnsMode(
    OptionalSecureDnsMode mode) {
  switch (mode) {
    case OptionalSecureDnsMode::NO_OVERRIDE:
      return std::nullopt;
    case OptionalSecureDnsMode::OFF:
      return net::SecureDnsMode::kOff;
    case OptionalSecureDnsMode::AUTOMATIC:
      return net::SecureDnsMode::kAutomatic;
    case OptionalSecureDnsMode::SECURE:
      return net::SecureDnsMode::kSecure;
  }
}

}  // namespace

// static
bool StructTraits<network::mojom::DnsOverHttpsServerConfigDataView,
                  net::DnsOverHttpsServerConfig>::
    Read(network::mojom::DnsOverHttpsServerConfigDataView data,
         net::DnsOverHttpsServerConfig* out_server) {
  std::string server_template;
  if (!data.ReadServerTemplate(&server_template)) {
    return false;
  }
  net::DnsOverHttpsServerConfig::Endpoints endpoints;
  if (!data.ReadEndpoints(&endpoints)) {
    return false;
  }
  auto server = net::DnsOverHttpsServerConfig::FromString(
      std::move(server_template), std::move(endpoints));
  if (!server.has_value()) {
    return false;
  }
  *out_server = std::move(server.value());
  return true;
}

// static
bool StructTraits<network::mojom::DnsOverHttpsConfigDataView,
                  net::DnsOverHttpsConfig>::
    Read(network::mojom::DnsOverHttpsConfigDataView data,
         net::DnsOverHttpsConfig* out_config) {
  std::vector<net::DnsOverHttpsServerConfig> servers;
  if (!data.ReadServers(&servers)) {
    return false;
  }
  *out_config = net::DnsOverHttpsConfig(std::move(servers));
  return true;
}

// static
Tristate StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    append_to_multi_label_name(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.append_to_multi_label_name);
}

// static
Tristate
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::rotate(
    const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.rotate);
}

// static
Tristate StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    use_local_ipv6(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.use_local_ipv6);
}

// static
OptionalSecureDnsMode
StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    secure_dns_mode(const net::DnsConfigOverrides& overrides) {
  return ToOptionalSecureDnsMode(overrides.secure_dns_mode);
}

// static
Tristate StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::
    allow_dns_over_https_upgrade(const net::DnsConfigOverrides& overrides) {
  return ToTristate(overrides.allow_dns_over_https_upgrade);
}

// static
bool StructTraits<DnsConfigOverridesDataView, net::DnsConfigOverrides>::Read(
    DnsConfigOverridesDataView data,
    net::DnsConfigOverrides* out) {
  if (!data.ReadNameservers(&out->nameservers)) {
    return false;
  }
  if (!data.ReadSearch(&out->search)) {
    return false;
  }

  out->append_to_multi_label_name =
      FromTristate(data.append_to_multi_label_name());

  if (data.ndots() < -1) {
    return false;
  }
  if (data.ndots() >= 0) {
    out->ndots = data.ndots();
  }
  // if == -1, leave nullopt.

  if (!data.ReadFallbackPeriod(&out->fallback_period)) {
    return false;
  }

  if (data.attempts() < -1) {
    return false;
  }
  if (data.attempts() >= 0) {
    out->attempts = data.attempts();
  }
  // if == -1, leave nullopt.

  out->rotate = FromTristate(data.rotate());
  out->use_local_ipv6 = FromTristate(data.use_local_ipv6());

  if (!data.ReadDnsOverHttpsConfig(&out->dns_over_https_config)) {
    return false;
  }

  out->secure_dns_mode = FromOptionalSecureDnsMode(data.secure_dns_mode());

  out->allow_dns_over_https_upgrade =
      FromTristate(data.allow_dns_over_https_upgrade());

  if (!data.ReadFallbackDohNameservers(&out->fallback_doh_nameservers)) {
    return false;
  }

  out->clear_hosts = data.clear_hosts();

  return true;
}

// static
DnsQueryType EnumTraits<DnsQueryType, net::DnsQueryType>::ToMojom(
    net::DnsQueryType input) {
  switch (input) {
    case net::DnsQueryType::UNSPECIFIED:
      return DnsQueryType::UNSPECIFIED;
    case net::DnsQueryType::A:
      return DnsQueryType::A;
    case net::DnsQueryType::AAAA:
      return DnsQueryType::AAAA;
    case net::DnsQueryType::TXT:
      return DnsQueryType::TXT;
    case net::DnsQueryType::PTR:
      return DnsQueryType::PTR;
    case net::DnsQueryType::SRV:
      return DnsQueryType::SRV;
    case net::DnsQueryType::HTTPS:
      return DnsQueryType::HTTPS;
  }
}

// static
net::DnsQueryType EnumTraits<DnsQueryType, net::DnsQueryType>::FromMojom(
    DnsQueryType input) {
  switch (input) {
    case DnsQueryType::UNSPECIFIED:
      return net::DnsQueryType::UNSPECIFIED;
    case DnsQueryType::A:
      return net::DnsQueryType::A;
    case DnsQueryType::AAAA:
      return net::DnsQueryType::AAAA;
    case DnsQueryType::TXT:
      return net::DnsQueryType::TXT;
    case DnsQueryType::PTR:
      return net::DnsQueryType::PTR;
    case DnsQueryType::SRV:
      return net::DnsQueryType::SRV;
    case DnsQueryType::HTTPS:
      return net::DnsQueryType::HTTPS;
  }
  NOTREACHED();
}

// static
HostResolverSource
EnumTraits<HostResolverSource, net::HostResolverSource>::ToMojom(
    net::HostResolverSource input) {
  switch (input) {
    case net::HostResolverSource::ANY:
      return HostResolverSource::ANY;
    case net::HostResolverSource::SYSTEM:
      return HostResolverSource::SYSTEM;
    case net::HostResolverSource::DNS:
      return HostResolverSource::DNS;
    case net::HostResolverSource::MULTICAST_DNS:
      return HostResolverSource::MULTICAST_DNS;
    case net::HostResolverSource::LOCAL_ONLY:
      return HostResolverSource::LOCAL_ONLY;
  }
}

// static
net::HostResolverSource
EnumTraits<HostResolverSource, net::HostResolverSource>::FromMojom(
    HostResolverSource input) {
  switch (input) {
    case HostResolverSource::ANY:
      return net::HostResolverSource::ANY;
    case HostResolverSource::SYSTEM:
      return net::HostResolverSource::SYSTEM;
    case HostResolverSource::DNS:
      return net::HostResolverSource::DNS;
    case HostResolverSource::MULTICAST_DNS:
      return net::HostResolverSource::MULTICAST_DNS;
    case HostResolverSource::LOCAL_ONLY:
      return net::HostResolverSource::LOCAL_ONLY;
  }
  NOTREACHED();
}

// static
MdnsListenerUpdateType
EnumTraits<MdnsListenerUpdateType, net::MdnsListenerUpdateType>::ToMojom(
    net::MdnsListenerUpdateType input) {
  switch (input) {
    case net::MdnsListenerUpdateType::kAdded:
      return MdnsListenerUpdateType::ADDED;
    case net::MdnsListenerUpdateType::kChanged:
      return MdnsListenerUpdateType::CHANGED;
    case net::MdnsListenerUpdateType::kRemoved:
      return MdnsListenerUpdateType::REMOVED;
  }
}

// static
net::MdnsListenerUpdateType
EnumTraits<MdnsListenerUpdateType, net::MdnsListenerUpdateType>::FromMojom(
    MdnsListenerUpdateType input) {
  switch (input) {
    case MdnsListenerUpdateType::ADDED:
      return net::MdnsListenerUpdateType::kAdded;
    case MdnsListenerUpdateType::CHANGED:
      return net::MdnsListenerUpdateType::kChanged;
    case MdnsListenerUpdateType::REMOVED:
      return net::MdnsListenerUpdateType::kRemoved;
  }
  NOTREACHED();
}

// static
network::mojom::SecureDnsMode
EnumTraits<network::mojom::SecureDnsMode, net::SecureDnsMode>::ToMojom(
    net::SecureDnsMode secure_dns_mode) {
  switch (secure_dns_mode) {
    case net::SecureDnsMode::kOff:
      return network::mojom::SecureDnsMode::OFF;
    case net::SecureDnsMode::kAutomatic:
      return network::mojom::SecureDnsMode::AUTOMATIC;
    case net::SecureDnsMode::kSecure:
      return network::mojom::SecureDnsMode::SECURE;
  }
}

// static
net::SecureDnsMode
EnumTraits<network::mojom::SecureDnsMode, net::SecureDnsMode>::FromMojom(
    network::mojom::SecureDnsMode in) {
  switch (in) {
    case network::mojom::SecureDnsMode::OFF:
      return net::SecureDnsMode::kOff;
    case network::mojom::SecureDnsMode::AUTOMATIC:
      return net::SecureDnsMode::kAutomatic;
    case network::mojom::SecureDnsMode::SECURE:
      return net::SecureDnsMode::kSecure;
  }
  NOTREACHED();
}

// static
network::mojom::SecureDnsPolicy
EnumTraits<network::mojom::SecureDnsPolicy, net::SecureDnsPolicy>::ToMojom(
    net::SecureDnsPolicy secure_dns_policy) {
  switch (secure_dns_policy) {
    case net::SecureDnsPolicy::kAllow:
      return network::mojom::SecureDnsPolicy::ALLOW;
    case net::SecureDnsPolicy::kDisable:
      return network::mojom::SecureDnsPolicy::DISABLE;
    case net::SecureDnsPolicy::kBootstrap:
      NOTREACHED();  // The bootstrap policy is only for use within the net
                     // component.
  }
}

// static
net::SecureDnsPolicy
EnumTraits<network::mojom::SecureDnsPolicy, net::SecureDnsPolicy>::FromMojom(
    network::mojom::SecureDnsPolicy in) {
  switch (in) {
    case network::mojom::SecureDnsPolicy::ALLOW:
      return net::SecureDnsPolicy::kAllow;
    case network::mojom::SecureDnsPolicy::DISABLE:
      return net::SecureDnsPolicy::kDisable;
  }
  NOTREACHED();
}

}  // namespace mojo
