// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_win/public/cpp/proxy_resolver_win_mojom_traits.h"

namespace mojo {

// static
proxy_resolver_win::mojom::WinHttpStatus
EnumTraits<proxy_resolver_win::mojom::WinHttpStatus,
           net::WinHttpStatus>::ToMojom(net::WinHttpStatus input) {
  switch (input) {
    case net::WinHttpStatus::kOk:
      return proxy_resolver_win::mojom::WinHttpStatus::kOk;
    case net::WinHttpStatus::kAborted:
      return proxy_resolver_win::mojom::WinHttpStatus::kAborted;
    case net::WinHttpStatus::kWinHttpOpenFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::kWinHttpOpenFailed;
    case net::WinHttpStatus::kWinHttpSetTimeoutsFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::
          kWinHttpSetTimeoutsFailed;
    case net::WinHttpStatus::kWinHttpSetStatusCallbackFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::
          kWinHttpSetStatusCallbackFailed;
    case net::WinHttpStatus::kWinHttpGetIEProxyConfigForCurrentUserFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::
          kWinHttpGetIEProxyConfigForCurrentUserFailed;
    case net::WinHttpStatus::kWinHttpCreateProxyResolverFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::
          kWinHttpCreateProxyResolverFailed;
    case net::WinHttpStatus::kWinHttpGetProxyForURLExFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::
          kWinHttpGetProxyForURLExFailed;
    case net::WinHttpStatus::kStatusCallbackFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::kStatusCallbackFailed;
    case net::WinHttpStatus::kWinHttpGetProxyResultFailed:
      return proxy_resolver_win::mojom::WinHttpStatus::
          kWinHttpGetProxyResultFailed;
    case net::WinHttpStatus::kEmptyProxyList:
      return proxy_resolver_win::mojom::WinHttpStatus::kEmptyProxyList;
  }

  return proxy_resolver_win::mojom::WinHttpStatus::kOk;
}

// static
bool EnumTraits<proxy_resolver_win::mojom::WinHttpStatus, net::WinHttpStatus>::
    FromMojom(proxy_resolver_win::mojom::WinHttpStatus input,
              net::WinHttpStatus* output) {
  switch (input) {
    case proxy_resolver_win::mojom::WinHttpStatus::kOk:
      *output = net::WinHttpStatus::kOk;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::kAborted:
      *output = net::WinHttpStatus::kAborted;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::kWinHttpOpenFailed:
      *output = net::WinHttpStatus::kWinHttpOpenFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::kWinHttpSetTimeoutsFailed:
      *output = net::WinHttpStatus::kWinHttpSetTimeoutsFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::
        kWinHttpSetStatusCallbackFailed:
      *output = net::WinHttpStatus::kWinHttpSetStatusCallbackFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::
        kWinHttpGetIEProxyConfigForCurrentUserFailed:
      *output =
          net::WinHttpStatus::kWinHttpGetIEProxyConfigForCurrentUserFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::
        kWinHttpCreateProxyResolverFailed:
      *output = net::WinHttpStatus::kWinHttpCreateProxyResolverFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::
        kWinHttpGetProxyForURLExFailed:
      *output = net::WinHttpStatus::kWinHttpGetProxyForURLExFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::kStatusCallbackFailed:
      *output = net::WinHttpStatus::kStatusCallbackFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::kWinHttpGetProxyResultFailed:
      *output = net::WinHttpStatus::kWinHttpGetProxyResultFailed;
      return true;
    case proxy_resolver_win::mojom::WinHttpStatus::kEmptyProxyList:
      *output = net::WinHttpStatus::kEmptyProxyList;
      return true;
  }

  return false;
}

}  // namespace mojo
