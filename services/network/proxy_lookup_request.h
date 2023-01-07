// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_LOOKUP_REQUEST_H_
#define SERVICES_NETWORK_PROXY_LOOKUP_REQUEST_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "url/gurl.h"

namespace net {

class ProxyResolutionRequest;

}  // namespace net

namespace network {

class NetworkContext;

// Single-use object to manage a proxy lookup.
class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyLookupRequest {
 public:
  ProxyLookupRequest(
      mojo::PendingRemote<mojom::ProxyLookupClient> proxy_lookup_client,
      NetworkContext* network_context,
      const net::NetworkAnonymizationKey& network_anonymization_key);

  ProxyLookupRequest(const ProxyLookupRequest&) = delete;
  ProxyLookupRequest& operator=(const ProxyLookupRequest&) = delete;

  ~ProxyLookupRequest();

  // Starts looking up what proxy to use for |url|. On completion, will inform
  // both the ProxyLookupClient and the NetworkContext that it has completed.
  // On synchronous completion, will inform the NetworkContext of completion
  // re-entrantly. If destroyed before the lookup completes, informs the client
  // that the lookup was aborted.
  void Start(const GURL& url);

 private:
  void OnResolveComplete(int result);

  // Cancels |request_| and tells |network_context_| to delete |this|.
  void DestroySelf();

  const raw_ptr<NetworkContext> network_context_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
  mojo::Remote<mojom::ProxyLookupClient> proxy_lookup_client_;

  net::ProxyInfo proxy_info_;
  std::unique_ptr<net::ProxyResolutionRequest> request_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_LOOKUP_REQUEST_H_
