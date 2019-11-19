// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_builder_from_stream.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"

namespace storage {

namespace {

// Size of individual type kBytes items the blob will be build from. The real
// limit is the min of this and limits_.max_bytes_data_item_size.
constexpr size_t kMaxMemoryChunkSize = 512 * 1024;

// Helper for RunCallbackWhenDataPipeReady, called when the watcher signals us.
void OnPipeReady(
    mojo::ScopedDataPipeConsumerHandle pipe,
    base::OnceCallback<void(mojo::ScopedDataPipeConsumerHandle)> callback,
    std::unique_ptr<mojo::SimpleWatcher> watcher,
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  // If no more data can be read we must be done, so invalidate the pipe.
  if (!state.readable())
    pipe.reset();
  std::move(callback).Run(std::move(pipe));
}

// Method that calls a callback when the provided data pipe becomes readable, or
// is closed.
void RunCallbackWhenDataPipeReady(
    mojo::ScopedDataPipeConsumerHandle pipe,
    base::OnceCallback<void(mojo::ScopedDataPipeConsumerHandle)> callback) {
  auto watcher = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
      base::SequencedTaskRunnerHandle::Get());
  auto* watcher_ptr = watcher.get();
  auto raw_pipe = pipe.get();
  watcher_ptr->Watch(
      raw_pipe, MOJO_HANDLE_SIGNAL_READABLE, MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&OnPipeReady, base::Passed(std::move(pipe)),
                          base::Passed(std::move(callback)),
                          base::Passed(std::move(watcher))));
}

// Helper base-class that reads upto a certain number of bytes from a data pipe.
// Deletes itself when done.
class DataPipeConsumerHelper {
 protected:
  DataPipeConsumerHelper(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      uint64_t max_bytes_to_read)
      : pipe_(std::move(pipe)),
        progress_client_(std::move(progress_client)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                 base::SequencedTaskRunnerHandle::Get()),
        max_bytes_to_read_(max_bytes_to_read) {
    watcher_.Watch(pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                   MOJO_WATCH_CONDITION_SATISFIED,
                   base::BindRepeating(&DataPipeConsumerHelper::DataPipeReady,
                                       base::Unretained(this)));
    watcher_.ArmOrNotify();
  }
  virtual ~DataPipeConsumerHelper() = default;

  // Return false if population fails.
  virtual bool Populate(base::span<const char> data,
                        uint64_t bytes_previously_written) = 0;
  virtual void InvokeDone(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      bool success,
      uint64_t bytes_written) = 0;

 private:
  void DataPipeReady(MojoResult result, const mojo::HandleSignalsState& state) {
    while (current_offset_ < max_bytes_to_read_) {
      const void* data;
      uint32_t size;
      MojoResult result =
          pipe_->BeginReadData(&data, &size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        watcher_.ArmOrNotify();
        return;
      }

      if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        // Pipe has closed, so we must be done.
        pipe_.reset();
        break;
      }
      DCHECK_EQ(MOJO_RESULT_OK, result);
      size = std::min<uint64_t>(size, max_bytes_to_read_ - current_offset_);
      if (!Populate(base::make_span(static_cast<const char*>(data), size),
                    current_offset_)) {
        InvokeDone(mojo::ScopedDataPipeConsumerHandle(), PassProgressClient(),
                   false, current_offset_);
        delete this;
        return;
      }
      if (progress_client_)
        progress_client_->OnProgress(size);
      current_offset_ += size;
      result = pipe_->EndReadData(size);
      DCHECK_EQ(MOJO_RESULT_OK, result);
    }

    // Either the pipe closed, or we filled the entire item.
    InvokeDone(std::move(pipe_), PassProgressClient(), true, current_offset_);
    delete this;
  }

  mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
  PassProgressClient() {
    if (!progress_client_)
      return mojo::NullAssociatedRemote();
    return progress_client_.Unbind();
  }

  mojo::ScopedDataPipeConsumerHandle pipe_;
  mojo::AssociatedRemote<blink::mojom::ProgressClient> progress_client_;
  mojo::SimpleWatcher watcher_;
  const uint64_t max_bytes_to_read_;
  uint64_t current_offset_ = 0;
};

}  // namespace

// Helper class that reads upto a certain number of bytes from a datapipe and
// writes those bytes to a file. When done, or if the pipe is closed, calls its
// callback.
class BlobBuilderFromStream::WritePipeToFileHelper
    : public DataPipeConsumerHelper {
 public:
  using DoneCallback =
      base::OnceCallback<void(bool success,
                              uint64_t bytes_written,
                              mojo::ScopedDataPipeConsumerHandle pipe,
                              mojo::PendingAssociatedRemote<
                                  blink::mojom::ProgressClient> progress_client,
                              const base::Time& modification_time)>;

  static void CreateAndAppend(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      base::FilePath file_path,
      uint64_t max_file_size,
      DoneCallback callback) {
    base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &WritePipeToFileHelper::CreateAndAppendOnFileSequence,
                std::move(pipe), std::move(progress_client),
                std::move(file_path), max_file_size,
                base::SequencedTaskRunnerHandle::Get(), std::move(callback)));
  }

  static void CreateAndStart(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      base::File file,
      uint64_t max_file_size,
      DoneCallback callback) {
    base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&WritePipeToFileHelper::CreateAndStartOnFileSequence,
                           std::move(pipe), std::move(progress_client),
                           std::move(file), max_file_size,
                           base::SequencedTaskRunnerHandle::Get(),
                           std::move(callback)));
  }

 private:
  static void CreateAndAppendOnFileSequence(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      base::FilePath file_path,
      uint64_t max_file_size,
      scoped_refptr<base::TaskRunner> reply_runner,
      DoneCallback callback) {
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_APPEND);
    new WritePipeToFileHelper(std::move(pipe), std::move(progress_client),
                              std::move(file), max_file_size,
                              std::move(reply_runner), std::move(callback));
  }

  static void CreateAndStartOnFileSequence(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      base::File file,
      uint64_t max_file_size,
      scoped_refptr<base::TaskRunner> reply_runner,
      DoneCallback callback) {
    new WritePipeToFileHelper(std::move(pipe), std::move(progress_client),
                              std::move(file), max_file_size,
                              std::move(reply_runner), std::move(callback));
  }

  WritePipeToFileHelper(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      base::File file,
      uint64_t max_file_size,
      scoped_refptr<base::TaskRunner> reply_runner,
      DoneCallback callback)
      : DataPipeConsumerHelper(std::move(pipe),
                               std::move(progress_client),
                               max_file_size),
        file_(std::move(file)),
        reply_runner_(std::move(reply_runner)),
        callback_(std::move(callback)) {}

  bool Populate(base::span<const char> data,
                uint64_t bytes_previously_written) override {
    return file_.WriteAtCurrentPos(data.data(), data.size()) >= 0;
  }

  void InvokeDone(mojo::ScopedDataPipeConsumerHandle pipe,
                  mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
                      progress_client,
                  bool success,
                  uint64_t bytes_written) override {
    base::Time last_modified;
    if (success) {
      base::File::Info info;
      if (file_.Flush() && file_.GetInfo(&info))
        last_modified = info.last_modified;
    }
    reply_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), success, bytes_written,
                                  std::move(pipe), std::move(progress_client),
                                  last_modified));
  }

  base::File file_;
  scoped_refptr<base::TaskRunner> reply_runner_;
  DoneCallback callback_;
};

// Similar helper class that writes upto a certain number of bytes from a data
// pipe into a FutureData element.
class BlobBuilderFromStream::WritePipeToFutureDataHelper
    : public DataPipeConsumerHelper {
 public:
  using DoneCallback = base::OnceCallback<void(
      uint64_t bytes_written,
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client)>;

  static void CreateAndStart(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      scoped_refptr<BlobDataItem> item,
      DoneCallback callback) {
    new WritePipeToFutureDataHelper(std::move(pipe), std::move(progress_client),
                                    std::move(item), std::move(callback));
  }

 private:
  WritePipeToFutureDataHelper(
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
          progress_client,
      scoped_refptr<BlobDataItem> item,
      DoneCallback callback)
      : DataPipeConsumerHelper(std::move(pipe),
                               std::move(progress_client),
                               item->length()),
        item_(std::move(item)),
        callback_(std::move(callback)) {}

  bool Populate(base::span<const char> data,
                uint64_t bytes_previously_written) override {
    if (item_->type() == BlobDataItem::Type::kBytesDescription)
      item_->AllocateBytes();
    std::memcpy(item_->mutable_bytes()
                    .subspan(bytes_previously_written, data.size())
                    .data(),
                data.data(), data.size());
    return true;
  }

  void InvokeDone(mojo::ScopedDataPipeConsumerHandle pipe,
                  mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
                      progress_client,
                  bool success,
                  uint64_t bytes_written) override {
    DCHECK(success);
    std::move(callback_).Run(bytes_written, std::move(pipe),
                             std::move(progress_client));
  }

  scoped_refptr<BlobDataItem> item_;
  DoneCallback callback_;
};

BlobBuilderFromStream::BlobBuilderFromStream(
    base::WeakPtr<BlobStorageContext> context,
    std::string content_type,
    std::string content_disposition,
    ResultCallback callback)
    : kMemoryBlockSize(std::min(
          kMaxMemoryChunkSize,
          context->memory_controller().limits().max_bytes_data_item_size)),
      kMaxBytesInMemory(
          context->memory_controller().limits().min_page_file_size),
      kFileBlockSize(context->memory_controller().limits().min_page_file_size),
      kMaxFileSize(context->memory_controller().limits().max_file_size),
      context_(std::move(context)),
      callback_(std::move(callback)),
      content_type_(std::move(content_type)),
      content_disposition_(std::move(content_disposition)) {
  DCHECK(context_);
}

BlobBuilderFromStream::~BlobBuilderFromStream() {
  DCHECK(!callback_) << "BlobBuilderFromStream was destroyed before finishing";
}

void BlobBuilderFromStream::Start(
    uint64_t length_hint,
    mojo::ScopedDataPipeConsumerHandle data,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
        progress_client) {
  context_->mutable_memory_controller()->CallWhenStorageLimitsAreKnown(
      base::BindOnce(&BlobBuilderFromStream::AllocateMoreMemorySpace,
                     weak_factory_.GetWeakPtr(), length_hint,
                     std::move(progress_client), std::move(data)));
}

void BlobBuilderFromStream::Abort() {
  OnError(Result::kAborted);
}

void BlobBuilderFromStream::AllocateMoreMemorySpace(
    uint64_t length_hint,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    mojo::ScopedDataPipeConsumerHandle pipe) {
  if (!context_ || !callback_) {
    OnError(Result::kAborted);
    return;
  }
  if (!pipe.is_valid()) {
    OnSuccess();
    return;
  }

  // If too much data has already been saved in memory, switch to using disk
  // backed data.
  if (ShouldStoreNextBlockOnDisk(length_hint)) {
    AllocateMoreFileSpace(length_hint, std::move(progress_client),
                          std::move(pipe));
    return;
  }

  if (!length_hint)
    length_hint = kMemoryBlockSize;

  if (context_->memory_controller().GetAvailableMemoryForBlobs() <
      length_hint) {
    OnError(Result::kMemoryAllocationFailed);
    return;
  }

  std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items;
  while (length_hint > 0) {
    const auto block_size = std::min<uint64_t>(kMemoryBlockSize, length_hint);
    chunk_items.push_back(base::MakeRefCounted<ShareableBlobDataItem>(
        BlobDataItem::CreateBytesDescription(block_size),
        ShareableBlobDataItem::QUOTA_NEEDED));
    length_hint -= block_size;
  }
  auto items_copy = chunk_items;
  pending_quota_task_ =
      context_->mutable_memory_controller()->ReserveMemoryQuota(
          std::move(chunk_items),
          base::BindOnce(&BlobBuilderFromStream::MemoryQuotaAllocated,
                         base::Unretained(this), std::move(pipe),
                         std::move(progress_client), std::move(items_copy), 0));
}

void BlobBuilderFromStream::MemoryQuotaAllocated(
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
    size_t item_to_populate,
    bool success) {
  if (!success || !context_ || !callback_) {
    OnError(success ? Result::kAborted : Result::kMemoryAllocationFailed);
    return;
  }
  DCHECK_LT(item_to_populate, chunk_items.size());
  auto item = chunk_items[item_to_populate];
  WritePipeToFutureDataHelper::CreateAndStart(
      std::move(pipe), std::move(progress_client), item->item(),
      base::BindOnce(&BlobBuilderFromStream::DidWriteToMemory,
                     weak_factory_.GetWeakPtr(), std::move(chunk_items),
                     item_to_populate));
}

void BlobBuilderFromStream::DidWriteToMemory(
    std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
    size_t populated_item_index,
    uint64_t bytes_written,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient>
        progress_client) {
  if (!context_ || !callback_) {
    OnError(Result::kAborted);
    return;
  }
  DCHECK_LE(populated_item_index, chunk_items.size());
  auto item = chunk_items[populated_item_index];
  item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
  current_total_size_ += bytes_written;
  if (pipe.is_valid()) {
    DCHECK_EQ(item->item()->length(), bytes_written);
    items_.push_back(std::move(item));
    // If we still have allocated items for this chunk, just keep going with
    // those items.
    if (populated_item_index + 1 < chunk_items.size()) {
      MemoryQuotaAllocated(std::move(pipe), std::move(progress_client),
                           std::move(chunk_items), populated_item_index + 1,
                           true);
    } else {
      RunCallbackWhenDataPipeReady(
          std::move(pipe),
          base::BindOnce(&BlobBuilderFromStream::AllocateMoreMemorySpace,
                         weak_factory_.GetWeakPtr(), 0,
                         std::move(progress_client)));
    }
  } else {
    // Pipe has closed, so we must be done. If we allocated more items than we
    // ended up filling, those remaining items in |chunk_items| will just go out
    // of scope, resulting in them being destroyed and their allocations to be
    // freed.
    DCHECK_LE(bytes_written, item->item()->length());
    if (bytes_written > 0) {
      item->item()->ShrinkBytes(bytes_written);
      context_->mutable_memory_controller()->ShrinkMemoryAllocation(item.get());
      items_.push_back(std::move(item));
    }
    OnSuccess();
  }
}

void BlobBuilderFromStream::AllocateMoreFileSpace(
    uint64_t length_hint,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    mojo::ScopedDataPipeConsumerHandle pipe) {
  if (!context_ || !callback_) {
    OnError(Result::kAborted);
    return;
  }
  if (!pipe.is_valid()) {
    OnSuccess();
    return;
  }

  if (!length_hint)
    length_hint = kFileBlockSize;

  if (context_->memory_controller().GetAvailableFileSpaceForBlobs() <
      length_hint) {
    OnError(Result::kFileAllocationFailed);
    return;
  }

  // If the previous item was also a file, and the file isn't at its maximum
  // size yet, extend the previous file rather than creating a new one.
  if (!items_.empty() &&
      items_.back()->item()->type() == BlobDataItem::Type::kFile &&
      items_.back()->item()->length() < kMaxFileSize) {
    auto item = items_.back()->item();
    uint64_t old_file_size = item->length();
    scoped_refptr<ShareableFileReference> file_reference = item->file_ref_;
    DCHECK(file_reference);
    auto file_size_delta = std::min(kMaxFileSize - old_file_size, length_hint);
    context_->mutable_memory_controller()->GrowFileAllocation(
        file_reference.get(), file_size_delta);
    item->GrowFile(old_file_size + file_size_delta);
    base::FilePath path = file_reference->path();
    WritePipeToFileHelper::CreateAndAppend(
        std::move(pipe), std::move(progress_client), path, file_size_delta,
        base::BindOnce(&BlobBuilderFromStream::DidWriteToExtendedFile,
                       weak_factory_.GetWeakPtr(), std::move(file_reference),
                       old_file_size));
    return;
  }

  std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items;
  while (length_hint > 0) {
    const auto file_size = std::min(kMaxFileSize, length_hint);
    chunk_items.push_back(base::MakeRefCounted<ShareableBlobDataItem>(
        BlobDataItem::CreateFutureFile(0, file_size, chunk_items.size()),
        ShareableBlobDataItem::QUOTA_NEEDED));
    length_hint -= file_size;
  }
  auto items_copy = chunk_items;
  pending_quota_task_ = context_->mutable_memory_controller()->ReserveFileQuota(
      std::move(chunk_items),
      base::BindOnce(&BlobBuilderFromStream::FileQuotaAllocated,
                     base::Unretained(this), std::move(pipe),
                     std::move(progress_client), std::move(items_copy), 0));
}

void BlobBuilderFromStream::FileQuotaAllocated(
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
    size_t item_to_populate,
    std::vector<BlobMemoryController::FileCreationInfo> info,
    bool success) {
  if (!success || !context_ || !callback_) {
    OnError(success ? Result::kAborted : Result::kFileAllocationFailed);
    return;
  }
  DCHECK_EQ(chunk_items.size(), info.size());
  DCHECK_LT(item_to_populate, chunk_items.size());
  auto item = chunk_items[item_to_populate];
  base::File file = std::move(info[item_to_populate].file);
  WritePipeToFileHelper::CreateAndStart(
      std::move(pipe), std::move(progress_client), std::move(file),
      item->item()->length(),
      base::BindOnce(&BlobBuilderFromStream::DidWriteToFile,
                     weak_factory_.GetWeakPtr(), std::move(chunk_items),
                     std::move(info), item_to_populate));
}

void BlobBuilderFromStream::DidWriteToFile(
    std::vector<scoped_refptr<ShareableBlobDataItem>> chunk_items,
    std::vector<BlobMemoryController::FileCreationInfo> info,
    size_t populated_item_index,
    bool success,
    uint64_t bytes_written,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    const base::Time& modification_time) {
  if (!success || !context_ || !callback_) {
    OnError(success ? Result::kAborted : Result::kFileWriteFailed);
    return;
  }
  DCHECK_EQ(chunk_items.size(), info.size());
  DCHECK_LE(populated_item_index, chunk_items.size());
  auto item = chunk_items[populated_item_index];
  auto file = info[populated_item_index].file_reference;
  item->item()->PopulateFile(file->path(), modification_time, file);
  item->set_state(ShareableBlobDataItem::POPULATED_WITH_QUOTA);
  current_total_size_ += bytes_written;
  if (pipe.is_valid()) {
    DCHECK_EQ(item->item()->length(), bytes_written);
    items_.push_back(std::move(item));
    // If we still have allocated items for this chunk, just keep going with
    // those items.
    if (populated_item_index + 1 < chunk_items.size()) {
      FileQuotaAllocated(std::move(pipe), std::move(progress_client),
                         std::move(chunk_items), populated_item_index + 1,
                         std::move(info), true);
    } else {
      // Once we start writing to file, we keep writing to file.
      RunCallbackWhenDataPipeReady(
          std::move(pipe),
          base::BindOnce(&BlobBuilderFromStream::AllocateMoreFileSpace,
                         weak_factory_.GetWeakPtr(), 0,
                         std::move(progress_client)));
    }
  } else {
    // Pipe has closed, so we must be done. If we allocated more items than we
    // ended up filling, those remaining items in |chunk_items| will just go out
    // of scope, resulting in them being destroyed and their allocations to be
    // freed.
    DCHECK_LE(bytes_written, item->item()->length());
    if (bytes_written > 0) {
      context_->mutable_memory_controller()->ShrinkFileAllocation(
          file.get(), item->item()->length(), bytes_written);
      item->item()->ShrinkFile(bytes_written);
      items_.push_back(std::move(item));
    }
    OnSuccess();
  }
}

void BlobBuilderFromStream::DidWriteToExtendedFile(
    scoped_refptr<ShareableFileReference> file_reference,
    uint64_t old_file_size,
    bool success,
    uint64_t bytes_written,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingAssociatedRemote<blink::mojom::ProgressClient> progress_client,
    const base::Time& modification_time) {
  if (!success || !context_ || !callback_) {
    OnError(success ? Result::kAborted : Result::kFileWriteFailed);
    return;
  }
  DCHECK(!items_.empty());
  auto item = items_.back()->item();
  DCHECK_EQ(item->type(), BlobDataItem::Type::kFile);
  DCHECK_EQ(item->file_ref_, file_reference.get());

  item->SetFileModificationTime(modification_time);
  current_total_size_ += bytes_written;

  if (pipe.is_valid()) {
    DCHECK_EQ(item->length(), old_file_size + bytes_written);
    // Once we start writing to file, we keep writing to file.
    RunCallbackWhenDataPipeReady(
        std::move(pipe),
        base::BindOnce(&BlobBuilderFromStream::AllocateMoreFileSpace,
                       weak_factory_.GetWeakPtr(), 0,
                       std::move(progress_client)));
  } else {
    // Pipe has closed, so we must be done.
    DCHECK_LE(old_file_size + bytes_written, item->length());
    context_->mutable_memory_controller()->ShrinkFileAllocation(
        file_reference.get(), item->length(), old_file_size + bytes_written);
    item->ShrinkFile(old_file_size + bytes_written);
    OnSuccess();
  }
}

void BlobBuilderFromStream::OnError(Result result) {
  if (pending_quota_task_)
    pending_quota_task_->Cancel();

  // Clear |items_| to avoid holding on to ShareableDataItems.
  items_.clear();

  if (!callback_)
    return;
  RecordResult(result);
  std::move(callback_).Run(this, nullptr);
}

void BlobBuilderFromStream::OnSuccess() {
  DCHECK(context_);
  DCHECK(callback_);
  RecordResult(Result::kSuccess);
  std::move(callback_).Run(
      this, context_->AddFinishedBlob(base::GenerateGUID(), content_type_,
                                      content_disposition_, std::move(items_)));
}

void BlobBuilderFromStream::RecordResult(Result result) {
  UMA_HISTOGRAM_ENUMERATION("Storage.Blob.BuildFromStreamResult", result);
}

bool BlobBuilderFromStream::ShouldStoreNextBlockOnDisk(uint64_t length_hint) {
  DCHECK(context_);
  const BlobMemoryController& controller = context_->memory_controller();

  // Can't write to disk if paging isn't enabled.
  if (!controller.file_paging_enabled())
    return false;

  // If we need more space than we want to fit in memory, immediately
  // start writing to disk.
  if (length_hint > kMaxBytesInMemory)
    return true;

  // If the next memory block would cause us to use more memory than we'd like,
  // switch to disk.
  if (current_total_size_ + kMemoryBlockSize > kMaxBytesInMemory &&
      controller.GetAvailableFileSpaceForBlobs() >= kFileBlockSize) {
    return true;
  }

  // Switch to disk if otherwise we'd need to page out some other blob.
  return controller.GetAvailableMemoryForBlobs() < kMemoryBlockSize;
}

}  // namespace storage
