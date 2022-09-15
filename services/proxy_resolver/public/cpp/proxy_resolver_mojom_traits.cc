// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/public/cpp/proxy_resolver_mojom_traits.h"

#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_info.h"

namespace mojo {

proxy_resolver::mojom::ProxyScheme
EnumTraits<proxy_resolver::mojom::ProxyScheme,
           net::ProxyServer::Scheme>::ToMojom(net::ProxyServer::Scheme scheme) {
  using net::ProxyServer;
  switch (scheme) {
    case ProxyServer::SCHEME_INVALID:
      return proxy_resolver::mojom::ProxyScheme::INVALID;
    case ProxyServer::SCHEME_DIRECT:
      return proxy_resolver::mojom::ProxyScheme::DIRECT;
    case ProxyServer::SCHEME_HTTP:
      return proxy_resolver::mojom::ProxyScheme::HTTP;
    case ProxyServer::SCHEME_SOCKS4:
      return proxy_resolver::mojom::ProxyScheme::SOCKS4;
    case ProxyServer::SCHEME_SOCKS5:
      return proxy_resolver::mojom::ProxyScheme::SOCKS5;
    case ProxyServer::SCHEME_HTTPS:
      return proxy_resolver::mojom::ProxyScheme::HTTPS;
    case ProxyServer::SCHEME_QUIC:
      return proxy_resolver::mojom::ProxyScheme::QUIC;
  }
  NOTREACHED();
  return proxy_resolver::mojom::ProxyScheme::INVALID;
}

bool EnumTraits<proxy_resolver::mojom::ProxyScheme, net::ProxyServer::Scheme>::
    FromMojom(proxy_resolver::mojom::ProxyScheme scheme,
              net::ProxyServer::Scheme* out) {
  using net::ProxyServer;
  switch (scheme) {
    case proxy_resolver::mojom::ProxyScheme::INVALID:
      *out = ProxyServer::SCHEME_INVALID;
      return true;
    case proxy_resolver::mojom::ProxyScheme::DIRECT:
      *out = ProxyServer::SCHEME_DIRECT;
      return true;
    case proxy_resolver::mojom::ProxyScheme::HTTP:
      *out = ProxyServer::SCHEME_HTTP;
      return true;
    case proxy_resolver::mojom::ProxyScheme::SOCKS4:
      *out = ProxyServer::SCHEME_SOCKS4;
      return true;
    case proxy_resolver::mojom::ProxyScheme::SOCKS5:
      *out = ProxyServer::SCHEME_SOCKS5;
      return true;
    case proxy_resolver::mojom::ProxyScheme::HTTPS:
      *out = ProxyServer::SCHEME_HTTPS;
      return true;
    case proxy_resolver::mojom::ProxyScheme::QUIC:
      *out = ProxyServer::SCHEME_QUIC;
      return true;
  }
  return false;
}

base::StringPiece
StructTraits<proxy_resolver::mojom::ProxyServerDataView,
             net::ProxyServer>::host(const net::ProxyServer& s) {
  if (s.scheme() == net::ProxyServer::SCHEME_DIRECT ||
      s.scheme() == net::ProxyServer::SCHEME_INVALID) {
    return base::StringPiece();
  }
  return s.host_port_pair().host();
}

uint16_t StructTraits<proxy_resolver::mojom::ProxyServerDataView,
                      net::ProxyServer>::port(const net::ProxyServer& s) {
  if (s.scheme() == net::ProxyServer::SCHEME_DIRECT ||
      s.scheme() == net::ProxyServer::SCHEME_INVALID) {
    return 0;
  }
  return s.host_port_pair().port();
}

bool StructTraits<
    proxy_resolver::mojom::ProxyServerDataView,
    net::ProxyServer>::Read(proxy_resolver::mojom::ProxyServerDataView data,
                            net::ProxyServer* out) {
  net::ProxyServer::Scheme scheme;
  if (!data.ReadScheme(&scheme))
    return false;

  base::StringPiece host;
  if (!data.ReadHost(&host))
    return false;

  if ((scheme == net::ProxyServer::SCHEME_DIRECT ||
       scheme == net::ProxyServer::SCHEME_INVALID) &&
      (!host.empty() || data.port())) {
    return false;
  }

  *out = net::ProxyServer(scheme,
                          net::HostPortPair(std::string(host), data.port()));
  return true;
}

bool StructTraits<proxy_resolver::mojom::ProxyInfoDataView, net::ProxyInfo>::
    Read(proxy_resolver::mojom::ProxyInfoDataView data, net::ProxyInfo* out) {
  std::vector<net::ProxyServer> proxy_servers;
  if (!data.ReadProxyServers(&proxy_servers))
    return false;

  net::ProxyList proxy_list;
  for (const auto& server : proxy_servers)
    proxy_list.AddProxyServer(server);

  out->UseProxyList(proxy_list);
  return true;
}

}  // namespace mojo
