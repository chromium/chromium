// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_BACKGROUND_FETCH_CACHE_ENTRY_HANDLER_IMPL_H_
#define CONTENT_BROWSER_CACHE_STORAGE_BACKGROUND_FETCH_CACHE_ENTRY_HANDLER_IMPL_H_

#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"

namespace content {

class BackgroundFetchCacheEntryHandlerImpl
    : public CacheStorageCacheEntryHandler {
 public:
  explicit BackgroundFetchCacheEntryHandlerImpl(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);

  BackgroundFetchCacheEntryHandlerImpl(
      const BackgroundFetchCacheEntryHandlerImpl&) = delete;
  BackgroundFetchCacheEntryHandlerImpl& operator=(
      const BackgroundFetchCacheEntryHandlerImpl&) = delete;

  ~BackgroundFetchCacheEntryHandlerImpl() override;

  // CacheStorageCacheEntryHandler implementation:
  std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) override;
  void PopulateResponseBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                            blink::mojom::FetchAPIResponse* response) override;
  void PopulateRequestBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                           blink::mojom::FetchAPIRequest* request) override;

 private:
  base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() override;

  base::WeakPtrFactory<BackgroundFetchCacheEntryHandlerImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_BACKGROUND_FETCH_CACHE_ENTRY_HANDLER_IMPL_H_
