// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_win/public/cpp/proxy_resolver_win_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
proxy_resolver::mojom::WinHttpStatus
EnumTraits<proxy_resolver::mojom::WinHttpStatus, net::WinHttpStatus>::ToMojom(
    net::WinHttpStatus input) {
  switch (input) {
    case net::WinHttpStatus::kOk:
      return proxy_resolver::mojom::WinHttpStatus::kOk;
    case net::WinHttpStatus::kAborted:
      return proxy_resolver::mojom::WinHttpStatus::kAborted;
    case net::WinHttpStatus::kWinHttpOpenFailed:
      return proxy_resolver::mojom::WinHttpStatus::kWinHttpOpenFailed;
    case net::WinHttpStatus::kWinHttpSetTimeoutsFailed:
      return proxy_resolver::mojom::WinHttpStatus::kWinHttpSetTimeoutsFailed;
    case net::WinHttpStatus::kWinHttpSetStatusCallbackFailed:
      return proxy_resolver::mojom::WinHttpStatus::
          kWinHttpSetStatusCallbackFailed;
    case net::WinHttpStatus::kWinHttpGetIEProxyConfigForCurrentUserFailed:
      return proxy_resolver::mojom::WinHttpStatus::
          kWinHttpGetIEProxyConfigForCurrentUserFailed;
    case net::WinHttpStatus::kWinHttpCreateProxyResolverFailed:
      return proxy_resolver::mojom::WinHttpStatus::
          kWinHttpCreateProxyResolverFailed;
    case net::WinHttpStatus::kWinHttpGetProxyForURLExFailed:
      return proxy_resolver::mojom::WinHttpStatus::
          kWinHttpGetProxyForURLExFailed;
    case net::WinHttpStatus::kStatusCallbackFailed:
      return proxy_resolver::mojom::WinHttpStatus::kStatusCallbackFailed;
    case net::WinHttpStatus::kWinHttpGetProxyResultFailed:
      return proxy_resolver::mojom::WinHttpStatus::kWinHttpGetProxyResultFailed;
    case net::WinHttpStatus::kEmptyProxyList:
      return proxy_resolver::mojom::WinHttpStatus::kEmptyProxyList;
  }

  return proxy_resolver::mojom::WinHttpStatus::kOk;
}

// static
net::WinHttpStatus
EnumTraits<proxy_resolver::mojom::WinHttpStatus, net::WinHttpStatus>::FromMojom(
    proxy_resolver::mojom::WinHttpStatus input) {
  switch (input) {
    case proxy_resolver::mojom::WinHttpStatus::kOk:
      return net::WinHttpStatus::kOk;
    case proxy_resolver::mojom::WinHttpStatus::kAborted:
      return net::WinHttpStatus::kAborted;
    case proxy_resolver::mojom::WinHttpStatus::kWinHttpOpenFailed:
      return net::WinHttpStatus::kWinHttpOpenFailed;
    case proxy_resolver::mojom::WinHttpStatus::kWinHttpSetTimeoutsFailed:
      return net::WinHttpStatus::kWinHttpSetTimeoutsFailed;
    case proxy_resolver::mojom::WinHttpStatus::kWinHttpSetStatusCallbackFailed:
      return net::WinHttpStatus::kWinHttpSetStatusCallbackFailed;
    case proxy_resolver::mojom::WinHttpStatus::
        kWinHttpGetIEProxyConfigForCurrentUserFailed:
      return net::WinHttpStatus::kWinHttpGetIEProxyConfigForCurrentUserFailed;
    case proxy_resolver::mojom::WinHttpStatus::
        kWinHttpCreateProxyResolverFailed:
      return net::WinHttpStatus::kWinHttpCreateProxyResolverFailed;
    case proxy_resolver::mojom::WinHttpStatus::kWinHttpGetProxyForURLExFailed:
      return net::WinHttpStatus::kWinHttpGetProxyForURLExFailed;
    case proxy_resolver::mojom::WinHttpStatus::kStatusCallbackFailed:
      return net::WinHttpStatus::kStatusCallbackFailed;
    case proxy_resolver::mojom::WinHttpStatus::kWinHttpGetProxyResultFailed:
      return net::WinHttpStatus::kWinHttpGetProxyResultFailed;
    case proxy_resolver::mojom::WinHttpStatus::kEmptyProxyList:
      return net::WinHttpStatus::kEmptyProxyList;
  }

  NOTREACHED();
}

}  // namespace mojo
