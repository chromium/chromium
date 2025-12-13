// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_LOADER_IMPL_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_LOADER_IMPL_H_

#import <Foundation/Foundation.h>

#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "ios/chrome/browser/favicon/model/favicon_loader.h"

@class FaviconLoaderCacheKey;

namespace favicon {
class LargeIconService;
}  // namespace favicon

namespace favicon_base {
struct LargeIconResult;
enum class GoogleFaviconServerRequestStatus;
}  // namespace favicon_base

// A concrete implementation of the FaviconLoader service.
//
// It manages asynchronously loading favicons or fallback attributes
// from LargeIconService and caching them, given a URL.
class FaviconLoaderImpl final : public FaviconLoader {
 public:
  explicit FaviconLoaderImpl(favicon::LargeIconService* large_icon_service);

  ~FaviconLoaderImpl() final;

  // FaviconLoader implementation.
  void FaviconForPageUrl(
      const GURL& page_url,
      float size_in_points,
      float min_size_in_points,
      bool fallback_to_google_server,
      FaviconAttributesCompletionBlock favicon_block_handler) final;
  void FaviconForPageUrlOrHost(
      const GURL& page_url,
      float size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler) final;
  void FaviconForIconUrl(
      const GURL& icon_url,
      float size_in_points,
      float min_size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler) final;
  void CancellAllRequests() final;
  base::WeakPtr<FaviconLoader> AsWeakPtr() final;

 private:
  // Class representing the parameters for fetching a favicon.
  class Request;

  // Implements fetching the favicon according to `request`.
  void FetchFavicon(Request request);

  // Invoked when fetching the favicon from LargeIconService completes.
  void OnFaviconFetched(CGFloat scale,
                        Request request,
                        const favicon_base::LargeIconResult& result);

  // Invoked when callback to google servers completes.
  void OnGoogleServerFallbackCompleted(
      Request request,
      favicon_base::GoogleFaviconServerRequestStatus status);

  // Returns the cached attributes (if present) for `key`.
  FaviconAttributes* GetCachedAttributes(FaviconLoaderCacheKey* key) const;

  // Stores `attributes` into the cache with a key derived from `key`.
  void StoreAttributesInCache(FaviconAttributes* attributes,
                              FaviconLoaderCacheKey* key);

  SEQUENCE_CHECKER(sequence_checker_);

  // The LargeIconService used to retrieve favicon.
  raw_ptr<favicon::LargeIconService> large_icon_service_;

  // Tracks tasks sent to FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Holds cached favicons. This NSCache is populated as favicons or fallback
  // attributes are retrieved from `large_icon_service_`. Contents will be
  // removed during low-memory conditions based on its inherent LRU removal
  // algorithm. Keyed by NSString of URL (page URL or icon URL) spec.
  NSCache<FaviconLoaderCacheKey*, FaviconAttributes*>* favicon_cache_;

  base::WeakPtrFactory<FaviconLoaderImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_LOADER_IMPL_H_
