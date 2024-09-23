// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/cache_storage/background_fetch_cache_entry_handler_impl.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/filter/source_stream.h"
#include "services/network/public/cpp/source_stream_to_data_pipe.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {

namespace {

// Adapter for a DiskCacheBlobEntry to be read as a net::SourceStream.
class DiskCacheStream : public net::SourceStream {
 public:
  DiskCacheStream(
      scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
          blob_entry,
      CacheStorageCache::EntryIndex cache_index,
      uint64_t offset,
      uint64_t length)
      : SourceStream(net::SourceStream::SourceType::TYPE_NONE),
        blob_entry_(blob_entry),
        cache_index_(cache_index),
        orig_offset_(offset),
        orig_length_(length) {}

  int Read(net::IOBuffer* dst_buffer,
           int buffer_size,
           net::CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    uint64_t offset = orig_offset_ + bytes_read_;

    // Finished reading.
    if (!MayHaveMoreBytes())
      return 0;

    if (buffer_size < 0)
      return net::ERR_INVALID_ARGUMENT;

    uint64_t length = std::min(static_cast<uint64_t>(buffer_size),
                               orig_length_ - bytes_read_);
    int result = blob_entry_->Read(
        std::move(dst_buffer), cache_index_, offset, length,
        base::BindOnce(
            [](DiskCacheStream* stream, net::CompletionOnceCallback callback,
               int result) {
              // |blob_entry_| is strongly owned by |stream| so this can be
              // safely Unretained.
              if (result > 0)
                stream->bytes_read_ += result;
              std::move(callback).Run(result);
            },
            base::Unretained(this), std::move(callback)));

    if (result > 0)
      bytes_read_ += result;
    return result;
  }

  std::string Description() const override { return "DiskCacheStream"; }

  bool MayHaveMoreBytes() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return bytes_read_ < orig_length_;
  }

 private:
  const scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
      blob_entry_;
  const CacheStorageCache::EntryIndex cache_index_;
  const uint64_t orig_offset_;
  const uint64_t orig_length_;
  uint64_t bytes_read_ = 0;
  SEQUENCE_CHECKER(sequence_checker_);
};

// A |storage::mojom::BlobDataItemReader| implementation that
// wraps a |DiskCacheBlobEntry|.  In addition, each |EntryReaderImpl| maps the
// main and side data to particular disk_cache indices.
class EntryReaderImpl : public storage::mojom::BlobDataItemReader {
 public:
  EntryReaderImpl(
      scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
          blob_entry,
      CacheStorageCache::EntryIndex disk_cache_index,
      CacheStorageCache::EntryIndex side_data_disk_cache_index)
      : blob_entry_(std::move(blob_entry)),
        disk_cache_index_(disk_cache_index),
        side_data_disk_cache_index_(side_data_disk_cache_index) {}

  EntryReaderImpl(const EntryReaderImpl&) = delete;
  EntryReaderImpl& operator=(const EntryReaderImpl&) = delete;

  void Read(uint64_t offset,
            uint64_t length,
            mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    uint64_t size = blob_entry_->GetSize(disk_cache_index_);
    if (offset > size) {
      std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
      return;
    }
    if (length > size - offset) {
      std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
      return;
    }

    auto stream = std::make_unique<DiskCacheStream>(
        blob_entry_, disk_cache_index_, offset, length);
    auto adapter = std::make_unique<network::SourceStreamToDataPipe>(
        std::move(stream), std::move(pipe));
    auto* adapter_raw = adapter.get();
    adapter_raw->Start(base::BindOnce(
        [](ReadCallback callback,
           std::unique_ptr<network::SourceStreamToDataPipe> adapter,
           int result) { std::move(callback).Run(result); },
        std::move(callback), std::move(adapter)));
  }

  void ReadSideData(ReadSideDataCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Use a WrappedIOBuffer so that the DiskCacheBlobEntry writes directly
    // to the BigBuffer without a copy.
    int length = blob_entry_->GetSize(side_data_disk_cache_index_);
    mojo_base::BigBuffer output_buf(static_cast<size_t>(length));
    auto wrapped_buf = base::MakeRefCounted<net::WrappedIOBuffer>(output_buf);

    auto split_callback = base::SplitOnceCallback(base::BindOnce(
        [](mojo_base::BigBuffer output_buf, ReadSideDataCallback callback,
           int result) {
          std::move(callback).Run(result, std::move(output_buf));
        },
        std::move(output_buf), std::move(callback)));

    uint64_t offset = 0;
    int result =
        blob_entry_->Read(std::move(wrapped_buf), side_data_disk_cache_index_,
                          offset, length, std::move(split_callback.first));

    if (result == net::ERR_IO_PENDING)
      return;
    std::move(split_callback.second).Run(result);
  }

 private:
  const scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
      blob_entry_;
  const CacheStorageCache::EntryIndex disk_cache_index_;
  const CacheStorageCache::EntryIndex side_data_disk_cache_index_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

CacheStorageCacheEntryHandler::DiskCacheBlobEntry::DiskCacheBlobEntry(
    base::PassKey<CacheStorageCacheEntryHandler> key,
    base::WeakPtr<CacheStorageCacheEntryHandler> entry_handler,
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr disk_cache_entry)
    : entry_handler_(std::move(entry_handler)),
      cache_handle_(std::move(cache_handle)),
      disk_cache_entry_(std::move(disk_cache_entry)) {}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::Read(
    scoped_refptr<net::IOBuffer> dst_buffer,
    CacheStorageCache::EntryIndex disk_cache_index,
    uint64_t offset,
    int bytes_to_read,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!disk_cache_entry_)
    return net::ERR_CACHE_READ_FAILURE;

  return disk_cache_entry_->ReadData(disk_cache_index, offset, dst_buffer.get(),
                                     bytes_to_read, std::move(callback));
}

int CacheStorageCacheEntryHandler::DiskCacheBlobEntry::GetSize(
    CacheStorageCache::EntryIndex disk_cache_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!disk_cache_entry_)
    return 0;
  switch (disk_cache_index) {
    case CacheStorageCache::INDEX_INVALID:
      return 0;
    case CacheStorageCache::INDEX_HEADERS:
      return disk_cache_entry_->GetDataSize(CacheStorageCache::INDEX_HEADERS);
    case CacheStorageCache::INDEX_RESPONSE_BODY:
      return disk_cache_entry_->GetDataSize(
          CacheStorageCache::INDEX_RESPONSE_BODY);
    case CacheStorageCache::INDEX_SIDE_DATA:
      return disk_cache_entry_->GetDataSize(CacheStorageCache::INDEX_SIDE_DATA);
  }
  NOTREACHED_IN_MIGRATION();
}

void CacheStorageCacheEntryHandler::DiskCacheBlobEntry::Invalidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_handle_ = std::nullopt;
  entry_handler_ = nullptr;
  disk_cache_entry_ = nullptr;
}

disk_cache::ScopedEntryPtr&
CacheStorageCacheEntryHandler::DiskCacheBlobEntry::disk_cache_entry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return disk_cache_entry_;
}

CacheStorageCacheEntryHandler::DiskCacheBlobEntry::~DiskCacheBlobEntry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (entry_handler_)
    entry_handler_->EraseDiskCacheBlobEntry(this);
}

PutContext::PutContext(blink::mojom::FetchAPIRequestPtr request,
                       blink::mojom::FetchAPIResponsePtr response,
                       mojo::PendingRemote<blink::mojom::Blob> blob,
                       uint64_t blob_size,
                       mojo::PendingRemote<blink::mojom::Blob> side_data_blob,
                       uint64_t side_data_blob_size,
                       int64_t trace_id)
    : request(std::move(request)),
      response(std::move(response)),
      blob(std::move(blob)),
      blob_size(blob_size),
      side_data_blob(std::move(side_data_blob)),
      side_data_blob_size(side_data_blob_size),
      trace_id(trace_id) {}

PutContext::~PutContext() = default;

// Default implementation of CacheStorageCacheEntryHandler.
class CacheStorageCacheEntryHandlerImpl : public CacheStorageCacheEntryHandler {
 public:
  CacheStorageCacheEntryHandlerImpl(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context)
      : CacheStorageCacheEntryHandler(std::move(blob_storage_context)) {}
  ~CacheStorageCacheEntryHandlerImpl() override = default;

  std::unique_ptr<PutContext> CreatePutContext(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::FetchAPIResponsePtr response,
      int64_t trace_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::PendingRemote<blink::mojom::Blob> blob;
    uint64_t blob_size = blink::BlobUtils::kUnknownSize;
    mojo::PendingRemote<blink::mojom::Blob> side_data_blob;
    uint64_t side_data_blob_size = blink::BlobUtils::kUnknownSize;

    if (response->blob) {
      blob = std::move(response->blob->blob);
      blob_size = response->blob->size;
    }
    if (response->side_data_blob_for_cache_put) {
      side_data_blob = std::move(response->side_data_blob_for_cache_put->blob);
      side_data_blob_size = response->side_data_blob_for_cache_put->size;
    }

    return std::make_unique<PutContext>(
        std::move(request), std::move(response), std::move(blob), blob_size,
        std::move(side_data_blob), side_data_blob_size, trace_id);
  }

  void PopulateResponseBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                            blink::mojom::FetchAPIResponse* response) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // First create the blob and store it in the field for the main body
    // loading.
    response->blob = CreateBlobWithSideData(
        std::move(blob_entry), CacheStorageCache::INDEX_RESPONSE_BODY,
        CacheStorageCache::INDEX_SIDE_DATA);

    // Then clone the blob to the |side_data_blob| field for loading code_cache.
    mojo::Remote<blink::mojom::Blob> blob_remote(
        std::move(response->blob->blob));
    blob_remote->Clone(response->blob->blob.InitWithNewPipeAndPassReceiver());
    response->side_data_blob = blink::mojom::SerializedBlob::New(
        response->blob->uuid, response->blob->content_type,
        response->blob->size, blob_remote.Unbind());
  }

  void PopulateRequestBody(scoped_refptr<DiskCacheBlobEntry> blob_entry,
                           blink::mojom::FetchAPIRequest* request) override {}

 private:
  base::WeakPtr<CacheStorageCacheEntryHandler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<CacheStorageCacheEntryHandlerImpl> weak_ptr_factory_{
      this};
};

CacheStorageCacheEntryHandler::CacheStorageCacheEntryHandler(
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context)
    : blob_storage_context_(std::move(blob_storage_context)) {}

scoped_refptr<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>
CacheStorageCacheEntryHandler::CreateDiskCacheBlobEntry(
    CacheStorageCacheHandle cache_handle,
    disk_cache::ScopedEntryPtr disk_cache_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto blob_entry =
      base::MakeRefCounted<CacheStorageCacheEntryHandler::DiskCacheBlobEntry>(
          base::PassKey<CacheStorageCacheEntryHandler>(), GetWeakPtr(),
          std::move(cache_handle), std::move(disk_cache_entry));
  DCHECK_EQ(blob_entries_.count(blob_entry.get()), 0u);
  blob_entries_.insert(blob_entry.get());
  return blob_entry;
}

CacheStorageCacheEntryHandler::~CacheStorageCacheEntryHandler() = default;

void CacheStorageCacheEntryHandler::InvalidateDiskCacheBlobEntrys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Calling Invalidate() can cause the CacheStorageCacheEntryHandler to be
  // destroyed. Be careful not to touch |this| after calling Invalidate().
  std::set<raw_ptr<DiskCacheBlobEntry, SetExperimental>> entries =
      std::move(blob_entries_);
  for (DiskCacheBlobEntry* entry : entries) {
    entry->Invalidate();
  }
}

void CacheStorageCacheEntryHandler::EraseDiskCacheBlobEntry(
    DiskCacheBlobEntry* blob_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(blob_entries_.count(blob_entry), 0u);
  blob_entries_.erase(blob_entry);
}

// static
std::unique_ptr<CacheStorageCacheEntryHandler>
CacheStorageCacheEntryHandler::CreateCacheEntryHandler(
    storage::mojom::CacheStorageOwner owner,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context) {
  switch (owner) {
    case storage::mojom::CacheStorageOwner::kCacheAPI:
      return std::make_unique<CacheStorageCacheEntryHandlerImpl>(
          std::move(blob_storage_context));
    case storage::mojom::CacheStorageOwner::kBackgroundFetch:
      return std::make_unique<BackgroundFetchCacheEntryHandlerImpl>(
          std::move(blob_storage_context));
  }
  NOTREACHED_IN_MIGRATION();
}

blink::mojom::SerializedBlobPtr CacheStorageCacheEntryHandler::CreateBlob(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    CacheStorageCache::EntryIndex disk_cache_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateBlobWithSideData(std::move(blob_entry), disk_cache_index,
                                CacheStorageCache::INDEX_INVALID);
}

blink::mojom::SerializedBlobPtr
CacheStorageCacheEntryHandler::CreateBlobWithSideData(
    scoped_refptr<DiskCacheBlobEntry> blob_entry,
    CacheStorageCache::EntryIndex disk_cache_index,
    CacheStorageCache::EntryIndex side_data_disk_cache_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto blob = blink::mojom::SerializedBlob::New();
  blob->size = blob_entry->GetSize(disk_cache_index);
  blob->uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  auto element = storage::mojom::BlobDataItem::New();
  element->size = blob_entry->GetSize(disk_cache_index);
  element->side_data_size =
      side_data_disk_cache_index == CacheStorageCache::INDEX_INVALID
          ? 0
          : blob_entry->GetSize(side_data_disk_cache_index);
  element->type = storage::mojom::BlobDataItemType::kCacheStorage;

  auto handle = std::make_unique<EntryReaderImpl>(
      std::move(blob_entry), disk_cache_index, side_data_disk_cache_index);
  mojo::MakeSelfOwnedReceiver(std::move(handle),
                              element->reader.InitWithNewPipeAndPassReceiver());

  blob_storage_context_->context()->RegisterFromDataItem(
      blob->blob.InitWithNewPipeAndPassReceiver(), blob->uuid,
      std::move(element));

  return blob;
}

}  // namespace content
