// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace content {

PrefetchNetworkContext::PrefetchNetworkContext(
    mojo::Remote<network::mojom::NetworkContext> isolated_network_context,
    const PrefetchRequest& prefetch_request)
    : network_context_(std::move(isolated_network_context)),
      url_loader_factory_(CreatePrefetchURLLoaderFactory(network_context_.get(),
                                                         prefetch_request)) {
  CHECK(url_loader_factory_);
}

PrefetchNetworkContext::~PrefetchNetworkContext() = default;

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchNetworkContext::GetURLLoaderFactory() {
  return url_loader_factory_;
}

network::mojom::CookieManager* PrefetchNetworkContext::GetCookieManager() {
  if (!cookie_manager_) {
    network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());
  }

  return cookie_manager_.get();
}

void PrefetchNetworkContext::CloseIdleConnections() {
  if (network_context_) {
    network_context_->CloseIdleConnections(base::DoNothing());
  }
}

}  // namespace content
