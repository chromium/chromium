// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_MOJOM_TRAITS_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(PROXY_RESOLVER_CPP)
    StructTraits<proxy_resolver::mojom::ProxyInfoDataView, net::ProxyInfo> {
  static const std::vector<net::ProxyChain>& proxy_chains(
      const net::ProxyInfo& info) {
    return info.proxy_list().AllChains();
  }

  static bool Read(proxy_resolver::mojom::ProxyInfoDataView data,
                   net::ProxyInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_PROXY_RESOLVER_MOJOM_TRAITS_H_
