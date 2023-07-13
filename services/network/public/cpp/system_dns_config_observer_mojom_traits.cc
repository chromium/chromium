// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/system_dns_config_observer_mojom_traits.h"

namespace mojo {

using network::mojom::DnsConfigDataView;

// static
bool StructTraits<DnsConfigDataView, net::DnsConfig>::Read(
    DnsConfigDataView data,
    net::DnsConfig* out) {
  if (!data.ReadNameservers(&out->nameservers)) {
    return false;
  }

  out->dns_over_tls_active = data.dns_over_tls_active();

  if (!data.ReadDnsOverTlsHostname(&out->dns_over_tls_hostname)) {
    return false;
  }

  if (!data.ReadSearch(&out->search)) {
    return false;
  }

  out->unhandled_options = data.unhandled_options();

  return true;
}

}  // namespace mojo
