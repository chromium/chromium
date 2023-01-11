// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_BLOB_TO_DISK_CACHE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_BLOB_TO_DISK_CACHE_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/scoped_writable_entry.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/net_adapters.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {
class QuotaManagerProxy;
}

namespace content {

// Streams data from a blob and writes it to a given disk_cache::Entry.
class CONTENT_EXPORT CacheStorageBlobToDiskCache
    : public blink::mojom::BlobReaderClient {
 public:
  using EntryAndBoolCallback =
      base::OnceCallback<void(ScopedWritableEntry, bool)>;

  // The buffer size used for reading from blobs and writing to disk cache.
  static const int kBufferSize;

  CacheStorageBlobToDiskCache(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      const blink::StorageKey& storage_key);

  CacheStorageBlobToDiskCache(const CacheStorageBlobToDiskCache&) = delete;
  CacheStorageBlobToDiskCache& operator=(const CacheStorageBlobToDiskCache&) =
      delete;

  ~CacheStorageBlobToDiskCache() override;

  // Writes the body of |blob_remote| to |entry| with index
  // |disk_cache_body_index|. |entry| is passed to the callback once complete.
  // Only call this once per instantiation of CacheStorageBlobToDiskCache.
  void StreamBlobToCache(ScopedWritableEntry entry,
                         int disk_cache_body_index,
                         mojo::PendingRemote<blink::mojom::Blob> blob_remote,
                         uint64_t blob_size,
                         EntryAndBoolCallback callback);

  // BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override;

 protected:
  // Virtual for testing.
  virtual void ReadFromBlob();
  void DidWriteDataToEntry(int expected_bytes, int rv);
  const blink::StorageKey& storage_key() const { return storage_key_; }

 private:
  void RunCallback(bool success);

  void OnDataPipeReadable(MojoResult result);

  int cache_entry_offset_ = 0;
  ScopedWritableEntry entry_;

  int disk_cache_body_index_ = 0;
  EntryAndBoolCallback callback_;

  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  scoped_refptr<network::MojoToNetPendingBuffer> pending_read_;
  mojo::SimpleWatcher handle_watcher_;
  mojo::Receiver<BlobReaderClient> client_receiver_{this};

  bool received_on_complete_ = false;
  uint64_t expected_total_size_ = 0;
  bool data_pipe_closed_ = false;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  const blink::StorageKey storage_key_;

  base::WeakPtrFactory<CacheStorageBlobToDiskCache> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_BLOB_TO_DISK_CACHE_H_
