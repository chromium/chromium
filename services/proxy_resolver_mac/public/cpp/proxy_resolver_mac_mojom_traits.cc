// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/public/cpp/proxy_resolver_mac_mojom_traits.h"

namespace mojo {

// static
proxy_resolver::mojom::MacProxyStatus
EnumTraits<proxy_resolver::mojom::MacProxyStatus,
           net::MacProxyResolutionStatus>::ToMojom(net::MacProxyResolutionStatus
                                                       input) {
  switch (input) {
    case net::MacProxyResolutionStatus::kOk:
      return proxy_resolver::mojom::MacProxyStatus::kOk;
    case net::MacProxyResolutionStatus::kSystemConfigurationError:
      return proxy_resolver::mojom::MacProxyStatus::kSystemConfigurationError;
    case net::MacProxyResolutionStatus::kCFNetworkExecutePacScriptFailed:
      return proxy_resolver::mojom::MacProxyStatus::
          kCFNetworkExecutePacScriptFailed;
    case net::MacProxyResolutionStatus::kPacScriptFetchFailed:
      return proxy_resolver::mojom::MacProxyStatus::kPacScriptFetchFailed;
    case net::MacProxyResolutionStatus::kPacScriptExecutionFailed:
      return proxy_resolver::mojom::MacProxyStatus::kPacScriptExecutionFailed;
    case net::MacProxyResolutionStatus::kCFNetworkResolutionError:
      return proxy_resolver::mojom::MacProxyStatus::kCFNetworkResolutionError;
    case net::MacProxyResolutionStatus::kEmptyProxyList:
      return proxy_resolver::mojom::MacProxyStatus::kEmptyProxyList;
  }

  return proxy_resolver::mojom::MacProxyStatus::kOk;
}

// static
bool EnumTraits<proxy_resolver::mojom::MacProxyStatus,
                net::MacProxyResolutionStatus>::
    FromMojom(proxy_resolver::mojom::MacProxyStatus input,
              net::MacProxyResolutionStatus* output) {
  switch (input) {
    case proxy_resolver::mojom::MacProxyStatus::kOk:
      *output = net::MacProxyResolutionStatus::kOk;
      return true;
    case proxy_resolver::mojom::MacProxyStatus::kSystemConfigurationError:
      *output = net::MacProxyResolutionStatus::kSystemConfigurationError;
      return true;
    case proxy_resolver::mojom::MacProxyStatus::
        kCFNetworkExecutePacScriptFailed:
      *output = net::MacProxyResolutionStatus::kCFNetworkExecutePacScriptFailed;
      return true;
    case proxy_resolver::mojom::MacProxyStatus::kPacScriptFetchFailed:
      *output = net::MacProxyResolutionStatus::kPacScriptFetchFailed;
      return true;
    case proxy_resolver::mojom::MacProxyStatus::kPacScriptExecutionFailed:
      *output = net::MacProxyResolutionStatus::kPacScriptExecutionFailed;
      return true;
    case proxy_resolver::mojom::MacProxyStatus::kCFNetworkResolutionError:
      *output = net::MacProxyResolutionStatus::kCFNetworkResolutionError;
      return true;
    case proxy_resolver::mojom::MacProxyStatus::kEmptyProxyList:
      *output = net::MacProxyResolutionStatus::kEmptyProxyList;
      return true;
  }

  return false;
}

}  // namespace mojo
