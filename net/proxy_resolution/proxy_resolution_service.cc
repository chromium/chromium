// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolution_service.h"

#include "base/logging.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"

namespace net {

// static
void ProxyResolutionService::ProcessProxyRetryInfo(
    const ProxyRetryInfoMap& new_retry_info,
    ProxyRetryInfoMap& proxy_retry_info,
    ProxyDelegate* proxy_delegate) {
  if (new_retry_info.empty()) {
    return;
  }

  if (proxy_delegate) {
    proxy_delegate->OnSuccessfulRequestAfterFailures(new_retry_info);
  }

  for (const auto& iter : new_retry_info) {
    auto existing = proxy_retry_info.find(iter.first);
    if (existing == proxy_retry_info.end()) {
      proxy_retry_info[iter.first] = iter.second;
      if (proxy_delegate) {
        const ProxyChain& bad_proxy = iter.first;
        DCHECK(!bad_proxy.is_direct());
        const ProxyRetryInfo& proxy_retry_info_item = iter.second;
        proxy_delegate->OnFallback(bad_proxy, proxy_retry_info_item.net_error);
      }
    } else if (existing->second.bad_until < iter.second.bad_until) {
      existing->second.bad_until = iter.second.bad_until;
    }
  }
}

}  // namespace net
