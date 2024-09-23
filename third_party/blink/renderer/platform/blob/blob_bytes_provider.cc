// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/blob_bytes_provider.h"

#include <utility>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

// Helper class that streams all the bytes from a vector of RawData RefPtrs to
// a mojo data pipe. Instances will delete themselves when all data has been
// written, or when the data pipe is disconnected.
class BlobBytesStreamer {
  USING_FAST_MALLOC(BlobBytesStreamer);

 public:
  BlobBytesStreamer(Vector<scoped_refptr<RawData>> data,
                    mojo::ScopedDataPipeProducerHandle pipe)
      : data_(std::move(data)),
        pipe_(std::move(pipe)),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {
    watcher_.Watch(pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   MOJO_WATCH_CONDITION_SATISFIED,
                   WTF::BindRepeating(&BlobBytesStreamer::OnWritable,
                                      WTF::Unretained(this)));
  }

  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (result == MOJO_RESULT_CANCELLED ||
        result == MOJO_RESULT_FAILED_PRECONDITION) {
      delete this;
      return;
    }
    DCHECK_EQ(result, MOJO_RESULT_OK);

    while (true) {
      base::span<const uint8_t> bytes =
          base::as_byte_span(*data_[current_item_])
              .subspan(current_item_offset_);
      size_t actually_written_bytes = 0;
      MojoResult write_result = pipe_->WriteData(
          bytes, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
      if (write_result == MOJO_RESULT_OK) {
        current_item_offset_ += actually_written_bytes;
        if (current_item_offset_ >= data_[current_item_]->size()) {
          data_[current_item_] = nullptr;
          current_item_++;
          current_item_offset_ = 0;
          if (current_item_ >= data_.size()) {
            // All items were sent completely.
            delete this;
            return;
          }
        }
      } else if (write_result == MOJO_RESULT_SHOULD_WAIT) {
        break;
      } else {
        // Writing failed. This isn't necessarily bad, as this could just mean
        // the browser no longer needs the data for this blob. So just delete
        // this as sending data is definitely finished.
        delete this;
        return;
      }
    }
  }

 private:
  // The index of the item currently being written.
  wtf_size_t current_item_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  // The offset into the current item of the first byte not yet written to the
  // data pipe.
  size_t current_item_offset_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  // The data being written.
  Vector<scoped_refptr<RawData>> data_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::ScopedDataPipeProducerHandle pipe_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::SimpleWatcher watcher_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

constexpr size_t BlobBytesProvider::kMaxConsolidatedItemSizeInBytes;

BlobBytesProvider::BlobBytesProvider() {
  IncreaseChildProcessRefCount();
}

BlobBytesProvider::~BlobBytesProvider() {
  DecreaseChildProcessRefCount();
}

void BlobBytesProvider::AppendData(scoped_refptr<RawData> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!data_.empty()) {
    uint64_t last_offset = offsets_.empty() ? 0 : offsets_.back();
    offsets_.push_back(last_offset + data_.back()->size());
  }
  data_.push_back(std::move(data));
}

void BlobBytesProvider::AppendData(base::span<const char> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_.empty() ||
      data_.back()->size() + data.size() > kMaxConsolidatedItemSizeInBytes) {
    AppendData(RawData::Create());
  }
  data_.back()->MutableData()->AppendSpan(data);
}

// static
void BlobBytesProvider::Bind(
    std::unique_ptr<BlobBytesProvider> provider,
    mojo::PendingReceiver<mojom::blink::BytesProvider> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(provider->sequence_checker_);
  DETACH_FROM_SEQUENCE(provider->sequence_checker_);

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  // TODO(mek): Consider binding BytesProvider on the IPC thread instead, only
  // using the MayBlock taskrunner for actual file operations.
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<BlobBytesProvider> provider,
             mojo::PendingReceiver<mojom::blink::BytesProvider> receiver) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(provider->sequence_checker_);
            mojo::MakeSelfOwnedReceiver(std::move(provider),
                                        std::move(receiver));
          },
          std::move(provider), std::move(receiver)));
}

void BlobBytesProvider::RequestAsReply(RequestAsReplyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(mek): Once better metrics are created we could experiment with ways
  // to reduce the number of copies of data that are made here.
  Vector<uint8_t> result;
  for (const auto& d : data_)
    result.AppendSpan(base::span(*d));
  std::move(callback).Run(result);
}

void BlobBytesProvider::RequestAsStream(
    mojo::ScopedDataPipeProducerHandle pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // BlobBytesStreamer will self delete when done.
  new BlobBytesStreamer(std::move(data_), std::move(pipe));
}

void BlobBytesProvider::RequestAsFile(uint64_t source_offset,
                                      uint64_t source_size,
                                      base::File file,
                                      uint64_t file_offset,
                                      RequestAsFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file.IsValid()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  int64_t seek_distance = file.Seek(base::File::FROM_BEGIN,
                                    base::checked_cast<int64_t>(file_offset));
  bool seek_failed = seek_distance < 0;
  if (seek_failed) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Find first data item that should be read from (by finding the first offset
  // that starts after the offset we want to start reading from).
  wtf_size_t data_index = static_cast<wtf_size_t>(
      std::upper_bound(offsets_.begin(), offsets_.end(), source_offset) -
      offsets_.begin());

  // Offset of the current data chunk in the overall stream provided by this
  // provider.
  uint64_t offset = data_index == 0 ? 0 : offsets_[data_index - 1];
  for (; data_index < data_.size(); ++data_index) {
    const auto& data = data_[data_index];

    // We're done if the beginning of the current chunk is past the end of the
    // data to write.
    if (offset >= source_offset + source_size)
      break;

    // Offset within this chunk where writing needs to start from.
    uint64_t data_offset = offset > source_offset ? 0 : source_offset - offset;
    uint64_t data_size =
        std::min(data->size() - data_offset,
                 source_offset + source_size - offset - data_offset);
    auto partial_data = base::as_byte_span(*data).subspan(
        base::checked_cast<size_t>(data_offset),
        base::checked_cast<size_t>(data_size));
    while (!partial_data.empty()) {
      std::optional<size_t> actual_written =
          file.WriteAtCurrentPos(partial_data);
      if (!actual_written.has_value()) {
        std::move(callback).Run(std::nullopt);
        return;
      }
      partial_data = partial_data.subspan(*actual_written);
    }

    offset += data->size();
  }

  if (!file.Flush()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  base::File::Info info;
  if (!file.GetInfo(&info)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(info.last_modified);
}

// This keeps the process alive while blobs are being transferred.
void BlobBytesProvider::IncreaseChildProcessRefCount() {
  if (!WTF::IsMainThread()) {
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted()),
        FROM_HERE,
        CrossThreadBindOnce(&BlobBytesProvider::IncreaseChildProcessRefCount));
    return;
  }
  Platform::Current()->SuddenTerminationChanged(false);
}

void BlobBytesProvider::DecreaseChildProcessRefCount() {
  if (!WTF::IsMainThread()) {
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted()),
        FROM_HERE,
        CrossThreadBindOnce(&BlobBytesProvider::DecreaseChildProcessRefCount));
    return;
  }
  Platform::Current()->SuddenTerminationChanged(true);
}

}  // namespace blink
