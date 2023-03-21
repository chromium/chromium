// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include <utility>

#include "base/command_line.h"
#include "content/browser/loader/cached_navigation_url_loader.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/navigation_ui_data.h"
#include "services/network/public/cpp/features.h"

namespace content {

static NavigationURLLoaderFactory* g_loader_factory = nullptr;

// static
std::unique_ptr<NavigationURLLoader> NavigationURLLoader::Create(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerMainResourceHandle* service_worker_handle,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    NavigationURLLoaderDelegate* delegate,
    LoaderType loader_type,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
        trust_token_observer,
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
    network::mojom::URLResponseHeadPtr cached_response_head,
    std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
        initial_interceptors) {
  // Prioritize CachedNavigationURLLoader over `g_loader_factory` even for tests
  // as prerendered page activation needs to run synchronously and
  // CachedNavigationURLLoader serves a fake response synchronously.
  if (loader_type == LoaderType::kNoopForPrerender) {
    DCHECK(cached_response_head);
    return CachedNavigationURLLoader::Create(loader_type,
                                             std::move(request_info), delegate,
                                             std::move(cached_response_head));
  }

  if (g_loader_factory) {
    return g_loader_factory->CreateLoader(
        storage_partition, std::move(request_info),
        std::move(navigation_ui_data), service_worker_handle, delegate,
        loader_type);
  }

  // TODO(https://crbug.com/1226442): Merge this into the kNoopForPrerender path
  // above.
  if (loader_type == LoaderType::kNoopForBackForwardCache) {
    DCHECK(cached_response_head);
    return CachedNavigationURLLoader::Create(loader_type,
                                             std::move(request_info), delegate,
                                             std::move(cached_response_head));
  }

  return std::make_unique<NavigationURLLoaderImpl>(
      browser_context, storage_partition, std::move(request_info),
      std::move(navigation_ui_data), service_worker_handle,
      std::move(prefetched_signed_exchange_cache), delegate,
      std::move(cookie_observer), std::move(trust_token_observer),
      std::move(url_loader_network_observer), std::move(devtools_observer),
      std::move(initial_interceptors));
}

// static
void NavigationURLLoader::SetFactoryForTesting(
    NavigationURLLoaderFactory* factory) {
  DCHECK(g_loader_factory == nullptr || factory == nullptr);
  g_loader_factory = factory;
}

}  // namespace content
