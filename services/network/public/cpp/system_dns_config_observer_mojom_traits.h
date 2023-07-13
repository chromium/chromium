// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SYSTEM_DNS_CONFIG_OBSERVER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SYSTEM_DNS_CONFIG_OBSERVER_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/dns/dns_config.h"
#include "services/network/public/mojom/system_dns_config_observer.mojom.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::DnsConfigDataView, net::DnsConfig> {
  static const std::vector<net::IPEndPoint>& nameservers(
      const net::DnsConfig& config) {
    return config.nameservers;
  }

  static bool dns_over_tls_active(const net::DnsConfig& config) {
    return config.dns_over_tls_active;
  }

  static const std::string& dns_over_tls_hostname(
      const net::DnsConfig& config) {
    return config.dns_over_tls_hostname;
  }
  static const std::vector<std::string>& search(const net::DnsConfig& config) {
    return config.search;
  }

  static bool unhandled_options(const net::DnsConfig& config) {
    return config.unhandled_options;
  }

  static bool Read(network::mojom::DnsConfigDataView data, net::DnsConfig* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SYSTEM_DNS_CONFIG_OBSERVER_MOJOM_TRAITS_H_
