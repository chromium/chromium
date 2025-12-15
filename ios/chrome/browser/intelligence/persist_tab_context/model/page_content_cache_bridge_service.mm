// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_bridge_service.h"

#import "base/feature_list.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/page_content_annotations/core/page_content_cache.h"
#import "ios/chrome/browser/intelligence/features/features.h"

PageContentCacheBridgeService::PageContentCacheBridgeService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path,
    base::TimeDelta max_context_age)
    : page_content_cache_(
          base::FeatureList::IsEnabled(
              page_content_annotations::features::kPageContentCache) &&
                  IsPersistTabContextEnabled() &&
                  GetPersistTabContextStorageType() ==
                      PersistTabStorageType::kSQLite
              ? std::make_unique<page_content_annotations::PageContentCache>(
                    os_crypt_async,
                    profile_path,
                    max_context_age)
              : nullptr) {}

PageContentCacheBridgeService::~PageContentCacheBridgeService() = default;

page_content_annotations::PageContentCache*
PageContentCacheBridgeService::GetPageContentCache() {
  return page_content_cache_.get();
}
