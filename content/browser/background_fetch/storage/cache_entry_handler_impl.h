// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CACHE_ENTRY_HANDLER_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CACHE_ENTRY_HANDLER_IMPL_H_

#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"

#include "content/browser/cache_storage/cache_storage_cache.h"

namespace content {
namespace background_fetch {

class CacheEntryHandlerImpl : public CacheStorageCacheEntryHandler {
 public:
  explicit CacheEntryHandlerImpl(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);
  ~CacheEntryHandlerImpl() override;

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

  base::WeakPtrFactory<CacheEntryHandlerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CacheEntryHandlerImpl);
};

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_CACHE_ENTRY_HANDLER_IMPL_H_
