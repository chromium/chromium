// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_lookup_request.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "url/gurl.h"

namespace network {

ProxyLookupRequest::ProxyLookupRequest(
    mojo::PendingRemote<mojom::ProxyLookupClient> proxy_lookup_client,
    NetworkContext* network_context,
    const net::NetworkAnonymizationKey& network_anonymization_key)
    : network_context_(network_context),
      network_anonymization_key_(network_anonymization_key),
      proxy_lookup_client_(std::move(proxy_lookup_client)) {
  DCHECK(proxy_lookup_client_);
}

ProxyLookupRequest::~ProxyLookupRequest() {
  // |request_| should be non-null only when the network service is being torn
  // down.
  if (request_)
    proxy_lookup_client_->OnProxyLookupComplete(net::ERR_ABORTED, std::nullopt);
}

void ProxyLookupRequest::Start(const GURL& url) {
  proxy_lookup_client_.set_disconnect_handler(
      base::BindOnce(&ProxyLookupRequest::DestroySelf, base::Unretained(this)));
  // TODO(mmenke): The NetLogWithSource() means nothing is logged. Fix that.
  //
  // TODO(crbug.com/40107017): Pass along a NetworkAnonymizationKey.
  int result =
      network_context_->url_request_context()
          ->proxy_resolution_service()
          ->ResolveProxy(url, std::string(), network_anonymization_key_,
                         &proxy_info_,
                         base::BindOnce(&ProxyLookupRequest::OnResolveComplete,
                                        base::Unretained(this)),
                         &request_, net::NetLogWithSource());
  if (result != net::ERR_IO_PENDING)
    OnResolveComplete(result);
}

void ProxyLookupRequest::OnResolveComplete(int result) {
  if (result == net::OK) {
    proxy_lookup_client_->OnProxyLookupComplete(
        net::OK, std::optional<net::ProxyInfo>(std::move(proxy_info_)));
  } else {
    proxy_lookup_client_->OnProxyLookupComplete(result, std::nullopt);
  }
  DestroySelf();
}

void ProxyLookupRequest::DestroySelf() {
  request_.reset();
  network_context_->OnProxyLookupComplete(this);
}

}  // namespace network
