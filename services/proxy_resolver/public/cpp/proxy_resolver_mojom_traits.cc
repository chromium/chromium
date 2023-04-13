// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/public/cpp/proxy_resolver_mojom_traits.h"

#include "net/base/proxy_server.h"
#include "services/network/public/cpp/network_param_mojom_traits.h"

namespace mojo {

bool StructTraits<proxy_resolver::mojom::ProxyInfoDataView, net::ProxyInfo>::
    Read(proxy_resolver::mojom::ProxyInfoDataView data, net::ProxyInfo* out) {
  std::vector<net::ProxyServer> proxy_servers;
  if (!data.ReadProxyServers(&proxy_servers)) {
    return false;
  }

  net::ProxyList proxy_list;
  for (const auto& server : proxy_servers) {
    proxy_list.AddProxyServer(server);
  }

  out->UseProxyList(proxy_list);
  return true;
}

}  // namespace mojo
