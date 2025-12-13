// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service.h"

#import "base/feature_list.h"
#import "base/files/file_util.h"
#import "base/task/thread_pool.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/page_content_annotations/core/page_content_cache.h"
#import "ios/chrome/browser/intelligence/features/features.h"

PageContentCacheBridgeService::PageContentCacheBridgeService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& storage_path,
    base::TimeDelta max_context_age) {
  if (!base::FeatureList::IsEnabled(
          page_content_annotations::features::kPageContentCache) ||
      !IsPersistTabContextEnabled() ||
      GetPersistTabContextStorageType() != PersistTabStorageType::kSQLite) {
    return;
  }

  // Post a task to create the directory on a background thread.
  // When done, initialize the cache on the UI thread.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          ^(base::FilePath path) {
            if (!base::DirectoryExists(path)) {
              base::CreateDirectory(path);
            }
          },
          storage_path),
      base::BindOnce(&PageContentCacheBridgeService::InitializePageContentCache,
                     weak_ptr_factory_.GetWeakPtr(), os_crypt_async,
                     storage_path, max_context_age));
}

PageContentCacheBridgeService::~PageContentCacheBridgeService() = default;

void PageContentCacheBridgeService::InitializePageContentCache(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& storage_path,
    base::TimeDelta max_context_age) {
  // Now that the directory exists, we can safely create the cache.
  page_content_cache_ =
      std::make_unique<page_content_annotations::PageContentCache>(
          os_crypt_async, storage_path, max_context_age);
}

void PageContentCacheBridgeService::GetPageContentForTab(
    int64_t tab_id,
    GetPageContentCallback callback) {
  if (IsCacheInitialized()) {
    page_content_cache_->GetPageContentForTab(tab_id, std::move(callback));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PageContentCacheBridgeService::CachePageContent(
    int64_t tab_id,
    const GURL& url,
    const base::Time& visit_timestamp,
    const base::Time& extraction_timestamp,
    const optimization_guide::proto::PageContext& page_context) {
  if (IsCacheInitialized()) {
    page_content_cache_->CachePageContent(tab_id, url, visit_timestamp,
                                          extraction_timestamp, page_context);
  }
}

void PageContentCacheBridgeService::RemovePageContentForTab(int64_t tab_id) {
  if (IsCacheInitialized()) {
    page_content_cache_->RemovePageContentForTab(tab_id);
  }
}

void PageContentCacheBridgeService::GetAllTabIds(
    base::OnceCallback<void(std::vector<int64_t>)> callback) {
  if (IsCacheInitialized()) {
    page_content_cache_->GetAllTabIds(std::move(callback));
  } else {
    std::move(callback).Run({});
  }
}

bool PageContentCacheBridgeService::IsCacheInitialized() const {
  return page_content_cache_ != nullptr;
}
