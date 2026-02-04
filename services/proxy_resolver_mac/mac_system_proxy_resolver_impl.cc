// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/mac_system_proxy_resolver_impl.h"

#include <utility>

#include "base/notimplemented.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/proxy_list.h"

namespace proxy_resolver_mac {

MacSystemProxyResolverImpl::MacSystemProxyResolverImpl(
    mojo::PendingReceiver<proxy_resolver::mojom::SystemProxyResolver> receiver)
    : MacSystemProxyResolverImpl(std::move(receiver), MacAPIWrapper::Create()) {
}

MacSystemProxyResolverImpl::MacSystemProxyResolverImpl(
    mojo::PendingReceiver<proxy_resolver::mojom::SystemProxyResolver> receiver,
    std::unique_ptr<MacAPIWrapper> mac_api_wrapper)
    : receiver_(this, std::move(receiver)),
      mac_api_wrapper_(std::move(mac_api_wrapper)) {}

MacSystemProxyResolverImpl::~MacSystemProxyResolverImpl() = default;

void MacSystemProxyResolverImpl::GetProxyForUrl(
    const GURL& url,
    GetProxyForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED_LOG_ONCE();

  // Return an error status for now.
  auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
  status->is_success = false;
  status->os_error = 0;
  status->mac_proxy_status =
      net::MacProxyResolutionStatus::kSystemConfigurationError;

  std::move(callback).Run(net::ProxyList(), std::move(status));
}

}  // namespace proxy_resolver_mac
