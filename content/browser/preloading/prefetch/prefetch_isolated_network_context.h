// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_ISOLATED_NETWORK_CONTEXT_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_ISOLATED_NETWORK_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

class PrefetchRequest;

// An isolated network context used for prefetches. The purpose of using a
// separate network context is to set a custom proxy configuration, and separate
// any cookies.
class CONTENT_EXPORT PrefetchIsolatedNetworkContext {
 public:
  // Creates the `URLLoaderFactory` inside ctor.
  // A valid `isolated_network_context` should be passed.
  PrefetchIsolatedNetworkContext(
      mojo::Remote<network::mojom::NetworkContext> isolated_network_context,
      const PrefetchRequest& prefetch_request);
  ~PrefetchIsolatedNetworkContext();

  PrefetchIsolatedNetworkContext(const PrefetchIsolatedNetworkContext&) =
      delete;
  const PrefetchIsolatedNetworkContext operator=(
      const PrefetchIsolatedNetworkContext&) = delete;

  // Get a reference to |url_loader_factory_|.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Get a reference to |cookie_manager_|. If it is null, then it is bound to
  // the cookie manager of |isolated_network_context_|.
  network::mojom::CookieManager* GetCookieManager();

  // Close any idle connections with |isolated_network_context_|.
  void CloseIdleConnections();

 private:
  // The network context and URL loader factory to use when making prefetches.
  mojo::Remote<network::mojom::NetworkContext> isolated_network_context_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The cookie manager for the isolated |isolated_network_context_|. This is
  // used when copying cookies from the isolated prefetch network context to the
  // default network context after a prefetch is committed.
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_ISOLATED_NETWORK_CONTEXT_H_
