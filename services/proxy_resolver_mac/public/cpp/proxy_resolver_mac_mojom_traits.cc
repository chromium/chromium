// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/public/cpp/proxy_resolver_mac_mojom_traits.h"

#include "base/notreached.h"

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
net::MacProxyResolutionStatus EnumTraits<proxy_resolver::mojom::MacProxyStatus,
                                         net::MacProxyResolutionStatus>::
    FromMojom(proxy_resolver::mojom::MacProxyStatus input) {
  switch (input) {
    case proxy_resolver::mojom::MacProxyStatus::kOk:
      return net::MacProxyResolutionStatus::kOk;
    case proxy_resolver::mojom::MacProxyStatus::kSystemConfigurationError:
      return net::MacProxyResolutionStatus::kSystemConfigurationError;
    case proxy_resolver::mojom::MacProxyStatus::
        kCFNetworkExecutePacScriptFailed:
      return net::MacProxyResolutionStatus::kCFNetworkExecutePacScriptFailed;
    case proxy_resolver::mojom::MacProxyStatus::kPacScriptFetchFailed:
      return net::MacProxyResolutionStatus::kPacScriptFetchFailed;
    case proxy_resolver::mojom::MacProxyStatus::kPacScriptExecutionFailed:
      return net::MacProxyResolutionStatus::kPacScriptExecutionFailed;
    case proxy_resolver::mojom::MacProxyStatus::kCFNetworkResolutionError:
      return net::MacProxyResolutionStatus::kCFNetworkResolutionError;
    case proxy_resolver::mojom::MacProxyStatus::kEmptyProxyList:
      return net::MacProxyResolutionStatus::kEmptyProxyList;
  }

  NOTREACHED();
}

}  // namespace mojo
