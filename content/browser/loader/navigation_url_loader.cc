// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include <utility>

#include "base/command_line.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/cached_navigation_url_loader.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/navigation_ui_data.h"
#include "services/network/public/cpp/features.h"

namespace content {

static NavigationURLLoaderFactory* g_loader_factory = nullptr;

std::unique_ptr<NavigationURLLoader> NavigationURLLoader::Create(
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    NavigationURLLoaderDelegate* delegate,
    bool is_served_from_back_forward_cache,
    std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
        initial_interceptors) {
  if (g_loader_factory) {
    return g_loader_factory->CreateLoader(
        storage_partition, std::move(request_info),
        std::move(navigation_ui_data), service_worker_handle, delegate,
        is_served_from_back_forward_cache);
  }

  if (is_served_from_back_forward_cache)
    return CachedNavigationURLLoader::Create(std::move(request_info), delegate);

  return std::make_unique<NavigationURLLoaderImpl>(
      browser_context, storage_partition, std::move(request_info),
      std::move(navigation_ui_data), service_worker_handle, appcache_handle,
      std::move(prefetched_signed_exchange_cache), delegate,
      std::move(initial_interceptors));
}

void NavigationURLLoader::SetFactoryForTesting(
    NavigationURLLoaderFactory* factory) {
  DCHECK(g_loader_factory == nullptr || factory == nullptr);
  g_loader_factory = factory;
}

}  // namespace content
