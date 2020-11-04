// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/chrome_appcache_service.h"

#include <utility>

#include "base/files/file_path.h"
#include "content/browser/appcache/appcache_storage_impl.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/net_errors.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

ChromeAppCacheService::ChromeAppCacheService(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<StoragePartitionImpl> partition)
    : AppCacheServiceImpl(std::move(quota_manager_proxy),
                          std::move(partition)) {}

void ChromeAppCacheService::Initialize(
    const base::FilePath& cache_path,
    BrowserContext* browser_context,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  cache_path_ = cache_path;
  DCHECK(browser_context);
  browser_context_ = browser_context;

  // Init our base class.
  set_appcache_policy(this);
  AppCacheServiceImpl::Initialize(cache_path_);
  set_special_storage_policy(special_storage_policy.get());
}

void ChromeAppCacheService::CreateBackend(
    int process_id,
    int routing_id,
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  receivers_.Add(
      std::make_unique<AppCacheBackendImpl>(this, process_id, routing_id),
      std::move(receiver));
}

void ChromeAppCacheService::Shutdown() {
  receivers_.Clear();
  partition_ = nullptr;
}

bool ChromeAppCacheService::CanLoadAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetContentClient()->browser()->AllowAppCache(
      manifest_url, site_for_cookies, top_frame_origin, browser_context_);
}

bool ChromeAppCacheService::CanCreateAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetContentClient()->browser()->AllowAppCache(
      manifest_url, site_for_cookies, top_frame_origin, browser_context_);
}

bool ChromeAppCacheService::IsOriginTrialRequiredForAppCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetContentClient()->browser()->IsOriginTrialRequiredForAppCache(
      browser_context_);
}

ChromeAppCacheService::~ChromeAppCacheService() = default;

void ChromeAppCacheService::DeleteOnCorrectThread() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
    return;
  }
  if (BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
    return;
  }
  // Better to leak than crash on shutdown.
}

}  // namespace content
