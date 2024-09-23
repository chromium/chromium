// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_HOST_MOJOM_TRAITS_H_
#define SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_HOST_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(PROXY_RESOLVER_CPP)
    EnumTraits<proxy_resolver::mojom::HostResolveOperation,
               net::ProxyResolveDnsOperation> {
  static proxy_resolver::mojom::HostResolveOperation ToMojom(
      net::ProxyResolveDnsOperation input);

  static bool FromMojom(proxy_resolver::mojom::HostResolveOperation input,
                        net::ProxyResolveDnsOperation* output);
};

}  // namespace mojo

#endif  // SERVICES_PROXY_RESOLVER_PUBLIC_CPP_MOJO_HOST_MOJOM_TRAITS_H_
