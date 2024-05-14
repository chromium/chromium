// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/public/cpp/mojo_host_mojom_traits.h"

#include <utility>

#include "net/base/address_list.h"

namespace mojo {

// static
proxy_resolver::mojom::HostResolveOperation
EnumTraits<proxy_resolver::mojom::HostResolveOperation,
           net::ProxyResolveDnsOperation>::ToMojom(net::ProxyResolveDnsOperation
                                                       input) {
  switch (input) {
    case net::ProxyResolveDnsOperation::DNS_RESOLVE:
      return proxy_resolver::mojom::HostResolveOperation::DNS_RESOLVE;
    case net::ProxyResolveDnsOperation::DNS_RESOLVE_EX:
      return proxy_resolver::mojom::HostResolveOperation::DNS_RESOLVE_EX;
    case net::ProxyResolveDnsOperation::MY_IP_ADDRESS:
      return proxy_resolver::mojom::HostResolveOperation::MY_IP_ADDRESS;
    case net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX:
      return proxy_resolver::mojom::HostResolveOperation::MY_IP_ADDRESS_EX;
  }

  NOTREACHED_IN_MIGRATION();
  return proxy_resolver::mojom::HostResolveOperation::kMinValue;
}

// static
bool EnumTraits<proxy_resolver::mojom::HostResolveOperation,
                net::ProxyResolveDnsOperation>::
    FromMojom(proxy_resolver::mojom::HostResolveOperation input,
              net::ProxyResolveDnsOperation* output) {
  switch (input) {
    case proxy_resolver::mojom::HostResolveOperation::DNS_RESOLVE:
      *output = net::ProxyResolveDnsOperation::DNS_RESOLVE;
      return true;
    case proxy_resolver::mojom::HostResolveOperation::DNS_RESOLVE_EX:
      *output = net::ProxyResolveDnsOperation::DNS_RESOLVE_EX;
      return true;
    case proxy_resolver::mojom::HostResolveOperation::MY_IP_ADDRESS:
      *output = net::ProxyResolveDnsOperation::MY_IP_ADDRESS;
      return true;
    case proxy_resolver::mojom::HostResolveOperation::MY_IP_ADDRESS_EX:
      *output = net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
