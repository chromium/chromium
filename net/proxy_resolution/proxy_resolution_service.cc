// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolution_service.h"

#include <utility>

#include "base/logging.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"

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

  for (const auto& [proxy_chain, retry_info] : new_retry_info) {
    auto [it, inserted] = proxy_retry_info.try_emplace(proxy_chain, retry_info);

    if (inserted) {
      if (proxy_delegate) {
        DCHECK(!proxy_chain.is_direct());
        proxy_delegate->OnFallback(proxy_chain, retry_info.net_error);
      }
    } else if (it->second.bad_until < retry_info.bad_until) {
      it->second.bad_until = retry_info.bad_until;
    }
  }
}

// static
base::Value::List ProxyResolutionService::BuildBadProxiesList(
    const ProxyRetryInfoMap& proxy_retry_info) {
  base::Value::List list;
  list.reserve(proxy_retry_info.size());

  for (const auto& [proxy_chain, retry_info] : proxy_retry_info) {
    base::Value::Dict dict;
    dict.Set("proxy_chain_uri", proxy_chain.ToDebugString());
    dict.Set("bad_until", NetLog::TickCountToString(retry_info.bad_until));

    list.Append(base::Value(std::move(dict)));
  }
  return list;
}

// static
void ProxyResolutionService::DeprioritizeBadProxyChains(
    const ProxyRetryInfoMap& proxy_retry_info,
    ProxyInfo* result,
    const NetLogWithSource& net_log) {
  // This check is done to only log the NetLog event when necessary, it's
  // not a performance optimization.
  if (!proxy_retry_info.empty()) {
    result->DeprioritizeBadProxyChains(proxy_retry_info);
    net_log.AddEvent(
        NetLogEventType::PROXY_RESOLUTION_SERVICE_DEPRIORITIZED_BAD_PROXIES);
  }
}

}  // namespace net
