// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/proxy_lookup_client_impl.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace content {

ProxyLookupClientImpl::ProxyLookupClientImpl(ProxyLookupCallback callback)
    : callback_(std::move(callback)) {
  CHECK(callback_);
}

void ProxyLookupClientImpl::CreateAndStart(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    ProxyLookupCallback callback,
    network::mojom::NetworkContext* network_context) {
  CHECK(network_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto client = std::make_unique<ProxyLookupClientImpl>(std::move(callback));

  // `client` is alive as long as the `mojo::SelfOwnedReceiver` is alive.
  auto disconnect_handler = base::BindOnce(&ProxyLookupClientImpl::OnDisconnect,
                                           base::Unretained(client.get()));

  mojo::PendingRemote<network::mojom::ProxyLookupClient> pending_remote;
  mojo::MakeSelfOwnedReceiver(std::move(client),
                              pending_remote.InitWithNewPipeAndPassReceiver())
      ->set_connection_error_handler(std::move(disconnect_handler));

  network_context->LookUpProxyForURL(url, network_anonymization_key,
                                     std::move(pending_remote));
}

ProxyLookupClientImpl::~ProxyLookupClientImpl() = default;

void ProxyLookupClientImpl::OnProxyLookupComplete(
    int32_t net_error,
    const std::optional<net::ProxyInfo>& proxy_info) {
  bool has_proxy = proxy_info.has_value() && !proxy_info->is_direct();
  std::move(callback_).Run(has_proxy);
}

void ProxyLookupClientImpl::OnDisconnect() {
  if (!callback_) {
    // Do nothing if disconnected after `OnProxyLookupComplete()` is called.
    return;
  }

  std::move(callback_).Run(/*has_proxy=*/false);
}

}  // namespace content
