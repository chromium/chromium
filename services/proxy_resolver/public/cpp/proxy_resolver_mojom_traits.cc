// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/public/cpp/proxy_resolver_mojom_traits.h"

#include "net/base/proxy_server.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"

namespace mojo {

bool StructTraits<proxy_resolver::mojom::ProxyInfoDataView, net::ProxyInfo>::
    Read(proxy_resolver::mojom::ProxyInfoDataView data, net::ProxyInfo* out) {
  std::vector<net::ProxyChain> proxy_chains;
  if (!data.ReadProxyChains(&proxy_chains)) {
    return false;
  }

  net::ProxyList proxy_list;
  for (const auto& chain : proxy_chains) {
    proxy_list.AddProxyChain(chain);
  }

  out->UseProxyList(proxy_list);
  return true;
}

}  // namespace mojo
