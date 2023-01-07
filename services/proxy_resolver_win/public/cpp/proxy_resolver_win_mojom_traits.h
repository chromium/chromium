// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_WIN_PUBLIC_CPP_PROXY_RESOLVER_WIN_MOJOM_TRAITS_H_
#define SERVICES_PROXY_RESOLVER_WIN_PUBLIC_CPP_PROXY_RESOLVER_WIN_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/proxy_resolution/win/winhttp_status.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<proxy_resolver_win::mojom::WinHttpStatus,
                  net::WinHttpStatus> {
  static proxy_resolver_win::mojom::WinHttpStatus ToMojom(
      net::WinHttpStatus input);

  static bool FromMojom(proxy_resolver_win::mojom::WinHttpStatus input,
                        net::WinHttpStatus* output);
};

}  // namespace mojo

#endif  // SERVICES_PROXY_RESOLVER_WIN_PUBLIC_CPP_PROXY_RESOLVER_WIN_MOJOM_TRAITS_H_
