// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_

#include <memory>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/scoped_writable_entry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace content {

// The state needed to pass when writing to a cache.
struct PutContext {
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;

  PutContext(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::FetchAPIResponsePtr response,
             mojo::PendingRemote<blink::mojom::Blob> blob,
             uint64_t blob_size,
             mojo::PendingRemote<blink::mojom::Blob> side_data_blob,
             uint64_t side_data_blob_size,
             int64_t trace_id);

  PutContext(const PutContext&) = delete;
  PutContext& operator=(const PutContext&) = delete;

  ~PutContext();

  // Provided by the constructor.
  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::FetchAPIResponsePtr response;
  mojo::PendingRemote<blink::mojom::Blob> blob;
  uint64_t blob_size;
  mojo::PendingRemote<blink::mojom::Blob> side_data_blob;
  uint64_t side_data_blob_size;
  int64_t trace_id;

  // Provided while writing to the cache.
  ErrorCallback callback;
  ScopedWritableEntry cache_entry;
};

class CacheStorageCacheEntryHandler {
 public:
  // The DiskCacheBlobEntry is a ref-counted object containing both
  // a disk_cache Entry and a Handle to the cache in which it lives.  This
  // blob entry can then be used to create an |EntryReaderImpl|.
  class DiskCacheBlobEntry : public base::RefCounted<DiskCacheBlobEntry> {
   public:
    // Use |CacheStorageCacheEntryHandler::CreateDiskCacheBlobEntry|.
    DiskCacheBlobEntry(
        base::PassKey<CacheStorageCacheEntryHandler> key,
        base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
        CacheStorageCacheHandle cache_handle,
        disk_cache::ScopedEntryPtr disk_cache_entry);

    DiskCacheBlobEntry(const DiskCacheBlobEntry&) = delete;
    DiskCacheBlobEntry& operator=(const DiskCacheBlobEntry&) = delete;

    int Read(scoped_refptr<net::IOBuffer> dst_buffer,
             CacheStorageCache::EntryIndex disk_cache_index,
             uint64_t offset,
             int bytes_to_read,
             base::OnceCallback<void(int)> callback);

    int GetSize(CacheStorageCache::EntryIndex disk_cache_index) const;

    void Invalidate();

    disk_cache::ScopedEntryPtr& disk_cache_entry();

   private:
    friend class base::RefCounted<DiskCacheBlobEntry>;
    ~DiskCacheBlobEntry();

    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler_;
    std::optional<CacheStorageCacheHandle> cache_handle_;
    disk_cache::ScopedEntryPtr disk_cache_entry_;

    SEQUENCE_CHECKER(sequence_checker_);
  };

  scoped_refptr<DiskCacheBlobEntry> CreateDiskCacheBlobEntry(
      CacheStorageCacheHandle cache_handle,
      disk_cache::ScopedEntryPtr disk_cache_entry);

  CacheStorageCacheEntryHandler(const CacheStorageCacheEntryHandler&) = delete;
  CacheStorageCacheEntryHandler& operator=(
      const CacheStorageCacheEntryHandler&) = delete;

  virtual ~CacheStorageCacheEntryHandler();

  virtual std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) = 0;
  virtual void PopulateResponseBody(
      scoped_refptr<DiskCacheBlobEntry> blob_entry,
      blink::mojom::FetchAPIResponse* response) = 0;
  virtual void PopulateRequestBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                                   blink::mojom::FetchAPIRequest* request) = 0;

  static std::unique_ptr<CacheStorageCacheEntryHandler> CreateCacheEntryHandler(
      storage::mojom::CacheStorageOwner owner,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);

  void InvalidateDiskCacheBlobEntrys();
  void EraseDiskCacheBlobEntry(DiskCacheBlobEntry* blob_entry);

 protected:
  CacheStorageCacheEntryHandler(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);

  // Create a serialized blob from the given entry and disk_cache index.  This
  // blob will not have any side data.
  blink::mojom::SerializedBlobPtr CreateBlob(
      scoped_refptr<DiskCacheBlobEntry> blob_entry,
      CacheStorageCache::EntryIndex disk_cache_index);

  // Create a serialized blob from the given entry and disk_cache indices.
  blink::mojom::SerializedBlobPtr CreateBlobWithSideData(
      scoped_refptr<DiskCacheBlobEntry> blob_entry,
      CacheStorageCache::EntryIndex disk_cache_index,
      CacheStorageCache::EntryIndex side_data_disk_cache_index);

  // Wrapper for storage::mojom::BlobStorageContext bound to this sequence.
  scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  // Every subclass should provide its own implementation to avoid partial
  // destruction.
  virtual base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() = 0;

  // We keep track of the DiskCacheBlobEntry instances to allow us to invalidate
  // them if the cache has to be deleted while there are still references to
  // data in it.  DiskCacheBlobEntries are owned by EntryReaderImpl, which
  // are owned by their mojo remote (which indirectly is is owned by some blob).
  std::set<raw_ptr<DiskCacheBlobEntry, SetExperimental>> blob_entries_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_ENTRY_HANDLER_H_
