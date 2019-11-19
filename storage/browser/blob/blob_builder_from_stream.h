// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_BUILDER_FROM_STREAM_H
#define STORAGE_BROWSER_BLOB_BLOB_BUILDER_FROM_STREAM_H

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/shareable_blob_data_item.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"

namespace storage {

// This class can be used to create a blob from a stream source, when it is not
// known in advance how large the blob is going to be. The actual blob won't be
// created until all the data is received. This class deals with memory
// management and quota allocation for the new blob, and makes sure we have
// enough available blob quota for the blob being constructed.
//
// This class will allocate space from the BlobMemoryController as data is
// received. If more data is received than we can allocate space for we abort
// the creation of the blob, free up all space already allocated and return an
// error.
//
// The high-level strategy for how this class allocated space is that it stores
// the first several KB of a blob in memory, and switches to files on disk if
// the blob grows bigger than a certain size (assuming paging to disk is
// enabled, otherwise the entire blob of course has to be stored in memory).
// Space is allocated in chunks, before data is received for that chunk. If the
// end of the stream is reached before a chunk is filled up, the chunk (and its
// space allocation) is shrunk to match the actual size of the data in it.
//
// Generally this class tries avoid evicting other blobs that are already in
// memory to disk. So if there is no more room in-memory so store the blob being
// built we might switch to disk before the normal trigger size is reached.
//
// Finally you can pass an |length_hint| to the constructor. If this is done,
// the size is used for an initial space allocation, and if the size is too
// large to fit in memory anyway, the entire blob will be stored on disk.
//
// If this needs to be destroyed before building has finished, you should make
// sure to call Abort() before destroying the instance. No blob will be created
// in that case.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobBuilderFromStream {
 public:
  using ResultCallback =
      base::OnceCallback<void(BlobBuilderFromStream*,
                              std::unique_ptr<BlobDataHandle>)>;

  BlobBuilderFromStream(
      base::WeakPtr<BlobStorageContext> context,
      std::string content_type,
      std::string content_disposition,
      ResultCallback callback);
  ~BlobBuilderFromStream();

  // This may call |callback| synchronously when |length_hint| is larger than
  // the disk space.
  void Start(uint64_t length_hint,
             mojo::ScopedDataPipeConsumerHandle data,
             mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
                 progress_client);

  void Abort();

 private:
  class WritePipeToFileHelper;
  class WritePipeToFutureDataHelper;

  void AllocateMoreMemorySpace(
      uint64_t length_hint,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      mojo::ScopedDataPipeConsumerHandle pipe);
  void MemoryQuotaAllocated(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
      size_t item_to_populate,
      bool success);
  void DidWriteToMemory(
      std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
      size_t populated_item_index,
      uint64_t bytes_written,
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client);

  void AllocateMoreFileSpace(
      uint64_t length_hint,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      mojo::ScopedDataPipeConsumerHandle pipe);
  void FileQuotaAllocated(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
      size_t item_to_populate,
      std::vector<BlobMemoryController::FileCreationInfo> info,
      bool success);
  void DidWriteToFile(
      std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
      std::vector<BlobMemoryController::FileCreationInfo> info,
      size_t populated_item_index,
      bool success,
      uint64_t bytes_written,
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      const base::Time& modification_time);
  void DidWriteToExtendedFile(
      scoped_refptr<ShareableFileReference> file_reference,
      uint64_t old_file_size,
      bool success,
      uint64_t bytes_written,
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      const base::Time& modification_time);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    kSuccess = 0,
    kAborted = 1,
    kMemoryAllocationFailed = 2,
    kFileAllocationFailed = 3,
    kFileWriteFailed = 4,
    kMaxValue = kFileWriteFailed
  };

  void OnError(Result result);
  void OnSuccess();
  void RecordResult(Result result);

  bool ShouldStoreNextBlockOnDisk(uint64_t length_hint);

  // Amount of memory space we allocate at a time.
  const size_t kMemoryBlockSize;

  // Maximum total amount of space this blob will take up in memory. If the
  // blob becomes bigger than this we switch to files.
  const size_t kMaxBytesInMemory;

  // Amount of file space we allocate at a time.
  const uint64_t kFileBlockSize;

  // Maximum size of individual files.
  const uint64_t kMaxFileSize;

  base::WeakPtr<BlobStorageContext> context_;
  ResultCallback callback_;

  std::string content_type_;
  std::string content_disposition_;

  std::vector<scoped_refptr<ShareableBlobDataItem>> items_;
  uint64_t current_total_size_ = 0;
  base::WeakPtr<BlobMemoryController::QuotaAllocationTask> pending_quota_task_;
  base::WeakPtrFactory<BlobBuilderFromStream> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_BUILDER_FROM_STREAM_H
