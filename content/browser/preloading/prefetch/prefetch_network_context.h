// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class BrowserContext;
class PrefetchService;

// An isolated network context used for prefetches. The purpose of using a
// separate network context is to set a custom proxy configuration, and separate
// any cookies.
class CONTENT_EXPORT PrefetchNetworkContext {
 public:
  PrefetchNetworkContext(
      bool use_isolated_network_context,
      const PrefetchType& prefetch_type,
      const GlobalRenderFrameHostId& referring_render_frame_host_id,
      const url::Origin& referring_origin);
  ~PrefetchNetworkContext();

  PrefetchNetworkContext(const PrefetchNetworkContext&) = delete;
  const PrefetchNetworkContext operator=(const PrefetchNetworkContext&) =
      delete;

  // Get a reference to |url_loader_factory_|. If it is null, then
  // |network_context_| is bound and configured, and a new
  // |SharedURLLoaderFactory| is created.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory(
      PrefetchService* service);

  // Get a reference to |cookie_manager_|. If it is null, then it is bound to
  // the cookie manager of |network_context_|.
  network::mojom::CookieManager* GetCookieManager();

  // Close any idle connections with |network_context_|.
  void CloseIdleConnections();

 private:
  // Returns a URLLoaderFactory associated with the given |network_context|.
  scoped_refptr<network::SharedURLLoaderFactory> CreateNewURLLoaderFactory(
      BrowserContext* browser_context,
      network::mojom::NetworkContext* network_context);

  // Bind |network_context_| to a new network context and configure it to use
  // the prefetch proxy. Also set up |url_loader_factory_| as a new URL loader
  // factory for |network_context_|.
  void CreateIsolatedURLLoaderFactory(PrefetchService* service);

  // Whether an isolated network context or the default network context should
  // be used.
  const bool use_isolated_network_context_;

  // Used to determine if the prefetch proxy should be used.
  const PrefetchType prefetch_type_;

  // The referring RenderFrameHost is used when considering to proxy
  // |url_loader_factory_| by calling WillCreateURLLoaderFactory.
  // This should be empty when the trigger is browser-initiated.
  const GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The origin that initiates the prefetch request, used when considering to
  // proxy |url_loader_factory_| by calling WillCreateURLLoaderFactory.
  // For renderer-initiated prefetch, this is calculated by referring
  // RenderFrameHost's LastCommittedOrigin.
  const url::Origin referring_origin_;

  // The network context and URL loader factory to use when making prefetches.
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The cookie manager for the isolated |network_context_|. This is used when
  // copying cookies from the isolated prefetch network context to the default
  // network context after a prefetch is committed.
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_H_
