// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

const int CacheStorageBlobToDiskCache::kBufferSize = 1024 * 512;

CacheStorageBlobToDiskCache::CacheStorageBlobToDiskCache(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    const blink::StorageKey& storage_key)
    : handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunner::GetCurrentDefault()),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      storage_key_(storage_key) {}

CacheStorageBlobToDiskCache::~CacheStorageBlobToDiskCache() = default;

void CacheStorageBlobToDiskCache::StreamBlobToCache(
    ScopedWritableEntry entry,
    int disk_cache_body_index,
    mojo::PendingRemote<blink::mojom::Blob> blob_remote,
    uint64_t blob_size,
    EntryAndBoolCallback callback) {
  DCHECK(entry);
  DCHECK_LE(0, disk_cache_body_index);
  DCHECK(blob_remote);
  DCHECK(!consumer_handle_.is_valid());
  DCHECK(!pending_read_);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = blink::BlobUtils::GetDataPipeCapacity(blob_size);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(&options, producer_handle, consumer_handle_);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback).Run(std::move(entry), false /* success */);
    return;
  }

  disk_cache_body_index_ = disk_cache_body_index;
  entry_ = std::move(entry);
  callback_ = std::move(callback);

  mojo::Remote<blink::mojom::Blob> blob(std::move(blob_remote));
  blob->ReadAll(std::move(producer_handle),
                client_receiver_.BindNewPipeAndPassRemote());

  handle_watcher_.Watch(
      consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(&CacheStorageBlobToDiskCache::OnDataPipeReadable,
                          base::Unretained(this)));
  ReadFromBlob();
}

void CacheStorageBlobToDiskCache::OnComplete(int32_t status,
                                             uint64_t data_length) {
  if (status != net::OK) {
    RunCallback(false /* success */);
    return;
  }

  // OnComplete might get called before the last data is read from the data
  // pipe, so make sure to not call the callback until all data is read.
  received_on_complete_ = true;
  expected_total_size_ = data_length;
  if (data_pipe_closed_) {
    RunCallback(static_cast<uint64_t>(cache_entry_offset_) ==
                expected_total_size_);
  }
}

void CacheStorageBlobToDiskCache::ReadFromBlob() {
  handle_watcher_.ArmOrNotify();
}

void CacheStorageBlobToDiskCache::DidWriteDataToEntry(int expected_bytes,
                                                      int rv) {
  if (rv != expected_bytes) {
    quota_manager_proxy_->OnClientWriteFailed(storage_key_);
    RunCallback(false /* success */);
    return;
  }
  cache_entry_offset_ += rv;

  ReadFromBlob();
}

void CacheStorageBlobToDiskCache::RunCallback(bool success) {
  if (callback_)
    std::move(callback_).Run(std::move(entry_), success);
}

void CacheStorageBlobToDiskCache::OnDataPipeReadable(MojoResult unused) {
  // Get the handle_ from a previous read operation if we have one.
  if (pending_read_) {
    DCHECK(pending_read_->IsComplete());
    consumer_handle_ = pending_read_->ReleaseHandle();
    pending_read_ = nullptr;
  }

  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &consumer_handle_, &pending_read_);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    handle_watcher_.ArmOrNotify();
    return;
  }

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Done reading, but only signal success if OnComplete has also been called.
    data_pipe_closed_ = true;
    if (received_on_complete_) {
      RunCallback(static_cast<uint64_t>(cache_entry_offset_) ==
                  expected_total_size_);
    }
    return;
  }

  if (result != MOJO_RESULT_OK) {
    RunCallback(false /* success */);
    return;
  }

  const int bytes_to_read = std::min<int>(kBufferSize, pending_read_->size());
  auto buffer = base::MakeRefCounted<network::MojoToNetIOBuffer>(pending_read_,
                                                                 bytes_to_read);

  net::CompletionOnceCallback cache_write_callback =
      base::BindOnce(&CacheStorageBlobToDiskCache::DidWriteDataToEntry,
                     weak_ptr_factory_.GetWeakPtr(), bytes_to_read);

  int rv = entry_->WriteData(
      disk_cache_body_index_, cache_entry_offset_, buffer.get(), bytes_to_read,
      std::move(cache_write_callback), true /* truncate */);

  if (rv != net::ERR_IO_PENDING) {
    CacheStorageBlobToDiskCache::DidWriteDataToEntry(bytes_to_read, rv);
  }
}

}  // namespace content
