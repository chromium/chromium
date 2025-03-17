// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
@class FaviconAttributes;

// A class that manages asynchronously loading favicons.
class FaviconLoader : public KeyedService {
 public:
  // Type for completion block for FaviconForURL().
  using FaviconAttributesCompletionBlock = void (^)(FaviconAttributes*);

  FaviconLoader() = default;

  FaviconLoader(const FaviconLoader&) = delete;
  FaviconLoader& operator=(const FaviconLoader&) = delete;

  ~FaviconLoader() override = default;

  // Tries to find a FaviconAttributes in `favicon_cache_` with `page_url`:
  // If found, invokes `favicon_block_handler` and exits.
  // If not found, invokes `favicon_block_handler` with a default placeholder
  // then invokes it again asynchronously with the favicon fetched by trying
  // following methods:
  //   1. Use `large_icon_service_` to fetch from local DB managed by
  //      HistoryService;
  //   2. Use `large_icon_service_` to fetch from Google Favicon server if
  //      `fallback_to_google_server=true` (`size_in_points` is ignored when
  //      fetching from the Google server);
  //      ======================================================================
  //      IMPORTANT NOTE: You must only set `fallback_to_google_server` if it's
  //      acceptable to send history data to Google, per `CanSendHistoryData()`
  //      or equivalent checks.
  //      ======================================================================
  //   3. Create a favicon base on the fallback style from `large_icon_service`.
  // TODO(crbug.com/40266381): Remove the `fallback_to_google_server` param, and
  // instead have FaviconLoader determine this internally, based on
  // `CanSendHistoryData()`.
  virtual void FaviconForPageUrl(
      const GURL& page_url,
      float size_in_points,
      float min_size_in_points,
      bool fallback_to_google_server,
      FaviconAttributesCompletionBlock favicon_block_handler) = 0;

  // Tries to find a FaviconAttributes in `favicon_cache_` with `page_url`:
  // If found, invokes `favicon_block_handler` and exits.
  // If not found, invokes `favicon_block_handler` with a default placeholder
  // then invokes it again asynchronously with the favicon fetched by trying
  // following methods:
  //   1. Use `large_icon_service_` to fetch from local DB managed by
  //      HistoryService;
  //   2. Create a favicon base on the fallback style from `large_icon_service`.
  virtual void FaviconForPageUrlOrHost(
      const GURL& page_url,
      float size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler) = 0;

  // Tries to find a FaviconAttributes in `favicon_cache_` with `icon_url`:
  // If found, invokes `favicon_block_handler` and exits.
  // If not found, invokes `favicon_block_handler` with a default placeholder
  // then invokes it again asynchronously with the favicon fetched by trying
  // following methods:
  //   1. Use `large_icon_service_` to fetch from local DB managed by
  //      HistoryService;
  //   2. Create a favicon base on the fallback style from `large_icon_service`.
  virtual void FaviconForIconUrl(
      const GURL& icon_url,
      float size_in_points,
      float min_size_in_points,
      FaviconAttributesCompletionBlock favicon_block_handler) = 0;

  // Cancel all incomplete requests.
  virtual void CancellAllRequests() = 0;

  // Return a weak pointer to the current object.
  virtual base::WeakPtr<FaviconLoader> AsWeakPtr() = 0;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_LOADER_H_
