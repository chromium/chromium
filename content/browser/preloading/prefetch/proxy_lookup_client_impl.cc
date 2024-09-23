// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/proxy_lookup_client_impl.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace content {

ProxyLookupClientImpl::ProxyLookupClientImpl(
    const GURL& url,
    ProxyLookupCallback callback,
    network::mojom::NetworkContext* network_context)
    : callback_(std::move(callback)) {
  DCHECK(network_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(url));

  network_context->LookUpProxyForURL(url, network_anonymization_key,
                                     receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&ProxyLookupClientImpl::OnProxyLookupComplete,
                     base::Unretained(this), net::ERR_ABORTED, std::nullopt));
}

ProxyLookupClientImpl::~ProxyLookupClientImpl() = default;

void ProxyLookupClientImpl::OnProxyLookupComplete(
    int32_t net_error,
    const std::optional<net::ProxyInfo>& proxy_info) {
  bool has_proxy = proxy_info.has_value() && !proxy_info->is_direct();
  std::move(callback_).Run(has_proxy);
}

}  // namespace content
