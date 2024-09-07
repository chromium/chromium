// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PROXY_LOOKUP_CLIENT_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PROXY_LOOKUP_CLIENT_IMPL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"

class GURL;

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace content {

// This class looks up if there is a proxy set up for the given URL in the given
// NetworkContext. Instances of this class must be deleted immediately after the
// callback is invoked.
class CONTENT_EXPORT ProxyLookupClientImpl
    : public network::mojom::ProxyLookupClient {
 public:
  using ProxyLookupCallback = base::OnceCallback<void(bool has_proxy)>;

  // Starts the proxy lookup for |url| in |network_context|. Once the lookup is
  // completed, |callback| will be invoked.
  ProxyLookupClientImpl(const GURL& url,
                        ProxyLookupCallback callback,
                        network::mojom::NetworkContext* network_context);
  ~ProxyLookupClientImpl() override;

  ProxyLookupClientImpl(const ProxyLookupClientImpl&) = delete;
  ProxyLookupClientImpl& operator=(const ProxyLookupClientImpl&) = delete;

  // network::mojom::ProxyLookupClient
  void OnProxyLookupComplete(
      int32_t net_error,
      const std::optional<net::ProxyInfo>& proxy_info) override;

 private:
  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};
  ProxyLookupCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PROXY_LOOKUP_CLIENT_IMPL_H_
