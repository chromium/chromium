// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

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
  PageContentCacheBridgeService(os_crypt_async::OSCryptAsync* os_crypt_async,
                                const base::FilePath& profile_path,
                                base::TimeDelta max_context_age);
  ~PageContentCacheBridgeService() override;

  PageContentCacheBridgeService(const PageContentCacheBridgeService&) = delete;
  PageContentCacheBridgeService& operator=(
      const PageContentCacheBridgeService&) = delete;

  page_content_annotations::PageContentCache* GetPageContentCache();

 private:
  // TODO: crbug.com/466397202 - Investigate if directly passing in a pointer to
  // the PageContentCache is the right approach.
  const std::unique_ptr<page_content_annotations::PageContentCache>
      page_content_cache_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_H_
