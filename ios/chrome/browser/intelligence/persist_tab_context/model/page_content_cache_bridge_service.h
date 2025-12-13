// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

class GURL;

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace os_crypt_async {
class OSCryptAsync;
}

namespace page_content_annotations {
class PageContentCache;
}

// iOS service that provides access to the PageContentCache.
class PageContentCacheBridgeService : public KeyedService {
 public:
  using GetPageContentCallback = base::OnceCallback<void(
      std::optional<optimization_guide::proto::PageContext>)>;

  PageContentCacheBridgeService(os_crypt_async::OSCryptAsync* os_crypt_async,
                                const base::FilePath& storage_path,
                                base::TimeDelta max_context_age);
  ~PageContentCacheBridgeService() override;

  PageContentCacheBridgeService(const PageContentCacheBridgeService&) = delete;
  PageContentCacheBridgeService& operator=(
      const PageContentCacheBridgeService&) = delete;

  // Retrieves the page content for a given tab ID.
  void GetPageContentForTab(int64_t tab_id, GetPageContentCallback callback);

  // Caches page content for a specific tab.
  void CachePageContent(
      int64_t tab_id,
      const GURL& url,
      const base::Time& visit_timestamp,
      const base::Time& extraction_timestamp,
      const optimization_guide::proto::PageContext& page_context);

  // Removes content associated with the given tab.
  void RemovePageContentForTab(int64_t tab_id);

  // Retrieves all tab IDs for tabs that have page contents cached.
  void GetAllTabIds(base::OnceCallback<void(std::vector<int64_t>)> callback);

  // Returns true if the internal cache has been fully initialized.
  bool IsCacheInitialized() const;

 private:
  void InitializePageContentCache(os_crypt_async::OSCryptAsync* os_crypt_async,
                                  const base::FilePath& storage_path,
                                  base::TimeDelta max_context_age);

  std::unique_ptr<page_content_annotations::PageContentCache>
      page_content_cache_;

  base::WeakPtrFactory<PageContentCacheBridgeService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_H_
