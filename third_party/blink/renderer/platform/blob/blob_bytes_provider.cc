// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/blob_bytes_provider.h"

#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Helper class that streams all the bytes from a vector of RawData RefPtrs to
// a mojo data pipe. Instances will delete themselves when all data has been
// written, or when the data pipe is disconnected.
class BlobBytesStreamer {
  USING_FAST_MALLOC(BlobBytesStreamer);

 public:
  BlobBytesStreamer(Vector<scoped_refptr<RawData>> data,
                    mojo::ScopedDataPipeProducerHandle pipe,
                    scoped_refptr<base::SequencedTaskRunner> task_runner)
      : data_(std::move(data)),
        pipe_(std::move(pipe)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                 std::move(task_runner)) {
    watcher_.Watch(pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   MOJO_WATCH_CONDITION_SATISFIED,
                   WTF::BindRepeating(&BlobBytesStreamer::OnWritable,
                                      WTF::Unretained(this)));
  }

  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state) {
    if (result == MOJO_RESULT_CANCELLED ||
        result == MOJO_RESULT_FAILED_PRECONDITION) {
      delete this;
      return;
    }
    DCHECK_EQ(result, MOJO_RESULT_OK);

    while (true) {
      uint32_t num_bytes = base::saturated_cast<uint32_t>(
          data_[current_item_]->length() - current_item_offset_);
      MojoResult write_result =
          pipe_->WriteData(data_[current_item_]->data() + current_item_offset_,
                           &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
      if (write_result == MOJO_RESULT_OK) {
        current_item_offset_ += num_bytes;
        if (current_item_offset_ >= data_[current_item_]->length()) {
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
  size_t current_item_ = 0;
  // The offset into the current item of the first byte not yet written to the
  // data pipe.
  size_t current_item_offset_ = 0;
  // The data being written.
  Vector<scoped_refptr<RawData>> data_;

  mojo::ScopedDataPipeProducerHandle pipe_;
  mojo::SimpleWatcher watcher_;
};

// This keeps the process alive while blobs are being transferred.
void IncreaseChildProcessRefCount() {
  if (!WTF::IsMainThread()) {
    PostCrossThreadTask(*Thread::MainThread()->GetTaskRunner(), FROM_HERE,
                        CrossThreadBindOnce(&IncreaseChildProcessRefCount));
    return;
  }
  Platform::Current()->SuddenTerminationChanged(false);
}

void DecreaseChildProcessRefCount() {
  if (!WTF::IsMainThread()) {
    PostCrossThreadTask(*Thread::MainThread()->GetTaskRunner(), FROM_HERE,
                        CrossThreadBindOnce(&DecreaseChildProcessRefCount));
    return;
  }
  Platform::Current()->SuddenTerminationChanged(true);
}

}  // namespace

constexpr size_t BlobBytesProvider::kMaxConsolidatedItemSizeInBytes;

// static
BlobBytesProvider* BlobBytesProvider::CreateAndBind(
    mojo::PendingReceiver<mojom::blink::BytesProvider> receiver) {
  auto task_runner = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  auto provider = base::WrapUnique(new BlobBytesProvider(task_runner));
  auto* result = provider.get();
  // TODO(mek): Consider binding BytesProvider on the IPC thread instead, only
  // using the MayBlock taskrunner for actual file operations.
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<BlobBytesProvider> provider,
             mojo::PendingReceiver<mojom::blink::BytesProvider> receiver) {
            mojo::MakeSelfOwnedReceiver(std::move(provider),
                                        std::move(receiver));
          },
          WTF::Passed(std::move(provider)), WTF::Passed(std::move(receiver))));
  return result;
}

// static
std::unique_ptr<BlobBytesProvider> BlobBytesProvider::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return base::WrapUnique(new BlobBytesProvider(std::move(task_runner)));
}

BlobBytesProvider::~BlobBytesProvider() {
  DecreaseChildProcessRefCount();
}

void BlobBytesProvider::AppendData(scoped_refptr<RawData> data) {
  if (!data_.IsEmpty()) {
    uint64_t last_offset = offsets_.IsEmpty() ? 0 : offsets_.back();
    offsets_.push_back(last_offset + data_.back()->length());
  }
  data_.push_back(std::move(data));
}

void BlobBytesProvider::AppendData(base::span<const char> data) {
  if (data_.IsEmpty() ||
      data_.back()->length() + data.size() > kMaxConsolidatedItemSizeInBytes) {
    AppendData(RawData::Create());
  }
  data_.back()->MutableData()->Append(data.data(), data.size());
}

void BlobBytesProvider::RequestAsReply(RequestAsReplyCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // TODO(mek): Once better metrics are created we could experiment with ways
  // to reduce the number of copies of data that are made here.
  Vector<uint8_t> result;
  for (const auto& d : data_)
    result.Append(d->data(), d->length());
  std::move(callback).Run(result);
}

void BlobBytesProvider::RequestAsStream(
    mojo::ScopedDataPipeProducerHandle pipe) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // BlobBytesStreamer will self delete when done.
  new BlobBytesStreamer(std::move(data_), std::move(pipe), task_runner_);
}

void BlobBytesProvider::RequestAsFile(uint64_t source_offset,
                                      uint64_t source_size,
                                      base::File file,
                                      uint64_t file_offset,
                                      RequestAsFileCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BooleanHistogram, seek_histogram,
                                  ("Storage.Blob.RendererFileSeekFailed"));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BooleanHistogram, write_histogram,
                                  ("Storage.Blob.RendererFileWriteFailed"));

  if (!file.IsValid()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  int64_t seek_distance =
      file.Seek(base::File::FROM_BEGIN, SafeCast<int64_t>(file_offset));
  bool seek_failed = seek_distance < 0;
  seek_histogram.Count(seek_failed);
  if (seek_failed) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Find first data item that should be read from (by finding the first offset
  // that starts after the offset we want to start reading from).
  size_t data_index =
      std::upper_bound(offsets_.begin(), offsets_.end(), source_offset) -
      offsets_.begin();

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
        std::min(data->length() - data_offset,
                 source_offset + source_size - offset - data_offset);
    size_t written = 0;
    while (written < data_size) {
      int writing_size = base::saturated_cast<int>(data_size - written);
      int actual_written = file.WriteAtCurrentPos(
          data->data() + data_offset + written, writing_size);
      bool write_failed = actual_written < 0;
      write_histogram.Count(write_failed);
      if (write_failed) {
        std::move(callback).Run(base::nullopt);
        return;
      }
      written += actual_written;
    }

    offset += data->length();
  }

  if (!file.Flush()) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  base::File::Info info;
  if (!file.GetInfo(&info)) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::move(callback).Run(info.last_modified);
}

BlobBytesProvider::BlobBytesProvider(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  IncreaseChildProcessRefCount();
}

}  // namespace blink
