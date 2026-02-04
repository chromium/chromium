// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_MAC_PUBLIC_CPP_PROXY_RESOLVER_MAC_MOJOM_TRAITS_H_
#define SERVICES_PROXY_RESOLVER_MAC_PUBLIC_CPP_PROXY_RESOLVER_MAC_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<proxy_resolver::mojom::MacProxyStatus,
                  net::MacProxyResolutionStatus> {
  static proxy_resolver::mojom::MacProxyStatus ToMojom(
      net::MacProxyResolutionStatus input);

  static bool FromMojom(proxy_resolver::mojom::MacProxyStatus input,
                        net::MacProxyResolutionStatus* output);
};

}  // namespace mojo

#endif  // SERVICES_PROXY_RESOLVER_MAC_PUBLIC_CPP_PROXY_RESOLVER_MAC_MOJOM_TRAITS_H_
