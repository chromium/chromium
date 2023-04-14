// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_MOJOM_TRAITS_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_MOJOM_TRAITS_H_

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom-shared.h"

namespace net {
class ProxyInfo;
class ProxyServer;
}  // namespace net

namespace mojo {

template <>
struct EnumTraits<proxy_resolver::mojom::ProxyScheme,
                  net::ProxyServer::Scheme> {
  static proxy_resolver::mojom::ProxyScheme ToMojom(
      net::ProxyServer::Scheme scheme);
  static bool FromMojom(proxy_resolver::mojom::ProxyScheme scheme,
                        net::ProxyServer::Scheme* out);
};

template <>
struct StructTraits<proxy_resolver::mojom::ProxyServerDataView,
                    net::ProxyServer> {
  static net::ProxyServer::Scheme scheme(const net::ProxyServer& s) {
    return s.scheme();
  }

  static base::StringPiece host(const net::ProxyServer& s);
  static uint16_t port(const net::ProxyServer& s);

  static bool Read(proxy_resolver::mojom::ProxyServerDataView data,
                   net::ProxyServer* out);
};

template <>
struct StructTraits<proxy_resolver::mojom::ProxyInfoDataView, net::ProxyInfo> {
  static const std::vector<net::ProxyServer>& proxy_servers(
      const net::ProxyInfo& info) {
    return info.proxy_list().GetAll();
  }

  static bool Read(proxy_resolver::mojom::ProxyInfoDataView data,
                   net::ProxyInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_MOJOM_TRAITS_H_
