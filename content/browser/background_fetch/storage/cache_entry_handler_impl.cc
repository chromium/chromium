// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/cache_entry_handler_impl.h"

#include "base/guid.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {
namespace background_fetch {

CacheEntryHandlerImpl::CacheEntryHandlerImpl(
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context)
    : CacheStorageCacheEntryHandler(std::move(blob_storage_context)) {}

CacheEntryHandlerImpl::~CacheEntryHandlerImpl() = default;

std::unique_ptr<PutContext> CacheEntryHandlerImpl::CreatePutContext(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchAPIResponsePtr response,
    int64_t trace_id) {
  mojo::PendingRemote<blink::mojom::Blob> response_blob;
  uint64_t response_blob_size = blink::BlobUtils::kUnknownSize;
  mojo::PendingRemote<blink::mojom::Blob> request_blob;
  uint64_t request_blob_size = blink::BlobUtils::kUnknownSize;

  if (response->blob) {
    response_blob = std::move(response->blob->blob);
    response_blob_size = response->blob->size;
  }
  if (request->blob) {
    request_blob = std::move(request->blob->blob);
    request_blob_size = request->blob->size;
  }

  return std::make_unique<PutContext>(
      std::move(request), std::move(response), std::move(response_blob),
      response_blob_size, std::move(request_blob), request_blob_size, trace_id);
}

void CacheEntryHandlerImpl::PopulateResponseBody(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    blink::mojom::FetchAPIResponse* response) {
  response->blob =
      CreateBlob(std::move(blob_entry), CacheStorageCache::INDEX_RESPONSE_BODY);
}

void CacheEntryHandlerImpl::PopulateRequestBody(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    blink::mojom::FetchAPIRequest* request) {
  if (!blob_entry->disk_cache_entry() ||
      !blob_entry->disk_cache_entry()->GetDataSize(
          CacheStorageCache::INDEX_SIDE_DATA)) {
    return;
  }

  request->blob =
      CreateBlob(std::move(blob_entry), CacheStorageCache::INDEX_SIDE_DATA);
}

base::WeakPtr<CacheStorageCacheEntryHandler>
CacheEntryHandlerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace background_fetch
}  // namespace content
