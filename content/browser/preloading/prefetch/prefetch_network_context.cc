// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context.h"

#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace content {

PrefetchNetworkContext::PrefetchNetworkContext(
    bool use_isolated_network_context,
    mojo::Remote<network::mojom::NetworkContext> isolated_network_context,
    const PrefetchRequest& prefetch_request)
    : use_isolated_network_context_(use_isolated_network_context),
      network_context_(std::move(isolated_network_context)) {
  network::mojom::NetworkContext* network_context;
  if (use_isolated_network_context_) {
    network_context = network_context_.get();
  } else {
    CHECK(!network_context_);
    network_context = prefetch_request.browser_context()
                          ->GetDefaultStoragePartition()
                          ->GetNetworkContext();
  }
  CHECK(network_context);
  url_loader_factory_ =
      CreatePrefetchURLLoaderFactory(network_context, prefetch_request);
  CHECK(url_loader_factory_);
}

PrefetchNetworkContext::~PrefetchNetworkContext() = default;

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchNetworkContext::GetURLLoaderFactory() {
  return url_loader_factory_;
}

network::mojom::CookieManager* PrefetchNetworkContext::GetCookieManager() {
  CHECK(use_isolated_network_context_);
  CHECK(network_context_);
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
