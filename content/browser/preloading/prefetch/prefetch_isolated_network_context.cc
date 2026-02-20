// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_isolated_network_context.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace content {

PrefetchIsolatedNetworkContext::PrefetchIsolatedNetworkContext(
    mojo::Remote<network::mojom::NetworkContext> isolated_network_context,
    const PrefetchRequest& prefetch_request)
    : isolated_network_context_(std::move(isolated_network_context)),
      url_loader_factory_(
          CreatePrefetchURLLoaderFactory(isolated_network_context_.get(),
                                         prefetch_request)) {
  CHECK(url_loader_factory_);
}

PrefetchIsolatedNetworkContext::~PrefetchIsolatedNetworkContext() = default;

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchIsolatedNetworkContext::GetURLLoaderFactory() {
  return url_loader_factory_;
}

network::mojom::CookieManager*
PrefetchIsolatedNetworkContext::GetCookieManager() {
  if (!cookie_manager_) {
    isolated_network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());
  }

  return cookie_manager_.get();
}

void PrefetchIsolatedNetworkContext::CloseIdleConnections() {
  if (isolated_network_context_) {
    isolated_network_context_->CloseIdleConnections(base::DoNothing());
  }
}

}  // namespace content
