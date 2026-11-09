// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace content {

namespace {

constexpr size_t kScratchBufferSize = 64 * 1024;

crypto::hash::HashKind HashKindForAlgorithm(
    blink::mojom::CrossOriginStorageAlgorithm algorithm) {
  switch (algorithm) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      return crypto::hash::kSha256;
  }
}

size_t DigestSizeForAlgorithm(
    blink::mojom::CrossOriginStorageAlgorithm algorithm) {
  switch (algorithm) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      return crypto::hash::kSha256Size;
  }
}

}  // namespace

CrossOriginStorageStagingWriter::Result::Result() = default;

CrossOriginStorageStagingWriter::Result::Result(Result&& other)
    : status(other.status),
      size(other.size),
      path(other.ReleasePath()),
      digest(std::move(other.digest)) {}

CrossOriginStorageStagingWriter::Result&
CrossOriginStorageStagingWriter::Result::operator=(Result&& other) {
  if (this != &other) {
    DeletePathIfOwned();
    status = other.status;
    size = other.size;
    path = other.ReleasePath();
    digest = std::move(other.digest);
  }
  return *this;
}

CrossOriginStorageStagingWriter::Result::~Result() {
  DeletePathIfOwned();
}

base::FilePath CrossOriginStorageStagingWriter::Result::ReleasePath() {
  return std::exchange(path, base::FilePath());
}

void CrossOriginStorageStagingWriter::Result::DeletePathIfOwned() {
  base::FilePath to_delete = ReleasePath();
  if (to_delete.empty()) {
    return;
  }
  if (base::ThreadPoolInstance::Get()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(std::move(to_delete)));
  } else {
    base::DeleteFile(to_delete);
  }
}

CrossOriginStorageStagingWriter::CrossOriginStorageStagingWriter(
    blink::mojom::CrossOriginStorageAlgorithm algorithm,
    base::FilePath path,
    base::File file,
    int64_t max_size_bytes)
    : algorithm_(algorithm),
      path_(std::move(path)),
      file_(std::move(file)),
      hasher_(HashKindForAlgorithm(algorithm)),
      max_size_bytes_(max_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_.IsValid()) {
    DLOG(ERROR) << "Invalid COS staging file: " << path_.value();
    status_ = CrossOriginStorageStatus::kOperationFailed;
    path_.clear();
  }
}

CrossOriginStorageStagingWriter::~CrossOriginStorageStagingWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the job was torn down without an explicit Finish()/Discard() (for
  // example because the renderer disconnected mid-upload), make sure the
  // partial file does not linger in the staging directory.
  if (!finished_) {
    Discard();
  }
}

CrossOriginStorageStatus CrossOriginStorageStagingWriter::Append(
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return WriteAtOffset(static_cast<uint64_t>(size_), data);
}

CrossOriginStorageStatus CrossOriginStorageStagingWriter::WriteAtOffset(
    uint64_t offset,
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    return status_;
  }
  if (finished_) {
    return CrossOriginStorageStatus::kOperationFailed;
  }

  int64_t end_offset = 0;
  if (!base::CheckAdd(offset, data.size()).AssignIfValid(&end_offset) ||
      end_offset < 0 || end_offset > max_size_bytes_) {
    DLOG(ERROR) << "COS staging write exceeds maximum resource size: "
                << path_.value();
    status_ = CrossOriginStorageStatus::kQuotaExceeded;
    DiscardStagingFile();
    return status_;
  }

  if (data.empty()) {
    return CrossOriginStorageStatus::kSuccess;
  }

  if (offset != static_cast<uint64_t>(size_)) {
    needs_rehash_ = true;
  }

  // Copy each chunk once into browser-owned memory (`scratch_buffer_`) before
  // writing to disk and updating `hasher_`. `data` may alias a Mojo data pipe's
  // `UnsafeSharedMemoryRegion` ring buffer that the sending process still maps
  // writable; operating on `owned_chunk` ensures `file_` and `hasher_` observe
  // identical bytes.
  const size_t required_scratch_size =
      std::min(data.size(), kScratchBufferSize);
  if (scratch_buffer_.size() < required_scratch_size) {
    scratch_buffer_.resize(required_scratch_size);
  }

  base::span<const uint8_t> remaining(data);
  int64_t write_offset = static_cast<int64_t>(offset);
  while (!remaining.empty()) {
    const size_t chunk_len = std::min(remaining.size(), kScratchBufferSize);
    base::span<uint8_t> owned_chunk =
        base::span(scratch_buffer_).first(chunk_len);
    owned_chunk.copy_from(remaining.first(chunk_len));
    if (!file_.WriteAndCheck(write_offset, owned_chunk)) {
      DLOG(ERROR) << "Failed to write to COS staging file: " << path_.value();
      status_ = CrossOriginStorageStatus::kOperationFailed;
      DiscardStagingFile();
      return status_;
    }
    if (!needs_rehash_) {
      hasher_.Update(owned_chunk);
    }
    write_offset += static_cast<int64_t>(chunk_len);
    remaining = remaining.subspan(chunk_len);
  }

  size_ = std::max(size_, end_offset);
  return CrossOriginStorageStatus::kSuccess;
}

void CrossOriginStorageStagingWriter::WriteStream(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    scoped_refptr<CancellationFlag> cancelled,
    WriteStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    std::move(callback).Run(status_, 0);
    return;
  }
  if (finished_ || active_write_callback_) {
    std::move(callback).Run(CrossOriginStorageStatus::kOperationFailed, 0);
    return;
  }

  if (WriteAtOffset(offset, {}) != CrossOriginStorageStatus::kSuccess) {
    std::move(callback).Run(status_, 0);
    return;
  }

  current_offset_ = offset;
  active_stream_ = std::move(stream);
  active_write_callback_ = std::move(callback);
  active_write_cancelled_ = std::move(cancelled);
  bytes_written_in_call_ = 0;

  DrainDataPipe();
}

void CrossOriginStorageStagingWriter::OnDataPipeReadable(
    MojoResult /*result*/,
    const mojo::HandleSignalsState& /*state*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DrainDataPipe();
}

void CrossOriginStorageStagingWriter::DrainDataPipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (true) {
    if (active_write_cancelled_ &&
        active_write_cancelled_->data.load(std::memory_order_relaxed)) {
      data_pipe_watcher_.reset();
      active_stream_.reset();
      active_write_cancelled_.reset();
      status_ = CrossOriginStorageStatus::kOperationAborted;
      Discard();
      if (active_write_callback_) {
        std::move(active_write_callback_).Run(status_, 0);
      }
      return;
    }

    if (status_ != CrossOriginStorageStatus::kSuccess || finished_) {
      data_pipe_watcher_.reset();
      active_stream_.reset();
      active_write_cancelled_.reset();
      if (active_write_callback_) {
        std::move(active_write_callback_)
            .Run(status_ != CrossOriginStorageStatus::kSuccess
                     ? status_
                     : CrossOriginStorageStatus::kOperationFailed,
                 0);
      }
      return;
    }

    base::span<const uint8_t> buffer;
    MojoResult rv =
        active_stream_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      if (!data_pipe_watcher_) {
        data_pipe_watcher_ = std::make_unique<mojo::SimpleWatcher>(
            FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
            base::SequencedTaskRunner::GetCurrentDefault());
        data_pipe_watcher_->Watch(
            active_stream_.get(),
            MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
            MOJO_WATCH_CONDITION_SATISFIED,
            base::BindRepeating(
                &CrossOriginStorageStagingWriter::OnDataPipeReadable,
                base::Unretained(this)));
      }
      data_pipe_watcher_->ArmOrNotify();
      return;
    }
    if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
      data_pipe_watcher_.reset();
      active_stream_.reset();
      active_write_cancelled_.reset();
      uint64_t bytes_written = bytes_written_in_call_;
      bytes_written_in_call_ = 0;
      std::move(active_write_callback_)
          .Run(CrossOriginStorageStatus::kSuccess, bytes_written);
      return;
    }
    if (rv != MOJO_RESULT_OK) {
      data_pipe_watcher_.reset();
      active_stream_.reset();
      active_write_cancelled_.reset();
      status_ = CrossOriginStorageStatus::kOperationFailed;
      DiscardStagingFile();
      if (active_write_callback_) {
        std::move(active_write_callback_).Run(status_, 0);
      }
      return;
    }

    size_t chunk_size = buffer.size();
    CrossOriginStorageStatus write_status =
        WriteAtOffset(current_offset_, buffer);
    active_stream_->EndReadData(chunk_size);
    if (write_status != CrossOriginStorageStatus::kSuccess) {
      data_pipe_watcher_.reset();
      active_stream_.reset();
      active_write_cancelled_.reset();
      if (active_write_callback_) {
        std::move(active_write_callback_).Run(write_status, 0);
      }
      return;
    }

    bytes_written_in_call_ += chunk_size;
    current_offset_ += chunk_size;
  }
}

CrossOriginStorageStatus CrossOriginStorageStagingWriter::Truncate(
    uint64_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    return status_;
  }
  if (finished_) {
    return CrossOriginStorageStatus::kOperationFailed;
  }
  int64_t new_size = 0;
  if (!base::CheckedNumeric<uint64_t>(length).AssignIfValid(&new_size) ||
      new_size < 0 || new_size > max_size_bytes_) {
    status_ = CrossOriginStorageStatus::kQuotaExceeded;
    DiscardStagingFile();
    return status_;
  }
  if (new_size == size_) {
    return CrossOriginStorageStatus::kSuccess;
  }
  if (!file_.SetLength(new_size)) {
    DLOG(ERROR) << "Failed to truncate COS staging file: " << path_.value();
    status_ = CrossOriginStorageStatus::kOperationFailed;
    DiscardStagingFile();
    return status_;
  }
  if (new_size == 0) {
    hasher_ = crypto::hash::Hasher(HashKindForAlgorithm(algorithm_));
    needs_rehash_ = false;
  } else {
    needs_rehash_ = true;
  }
  size_ = new_size;
  return CrossOriginStorageStatus::kSuccess;
}

CrossOriginStorageStagingWriter::Result CrossOriginStorageStagingWriter::Finish(
    scoped_refptr<CancellationFlag> cancelled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Result result;
  if (cancelled && cancelled->data.load(std::memory_order_relaxed)) {
    status_ = CrossOriginStorageStatus::kOperationFailed;
    DiscardStagingFile();
    result.status = status_;
    return result;
  }
  if (status_ != CrossOriginStorageStatus::kSuccess) {
    // Do not leave a partially written file behind.
    DiscardStagingFile();
    result.status = status_;
    return result;
  }
  if (finished_) {
    result.status = CrossOriginStorageStatus::kOperationFailed;
    return result;
  }

  result.digest.resize(DigestSizeForAlgorithm(algorithm_));
  if (needs_rehash_) {
    if (!RehashFromDisk(result.digest, cancelled.get())) {
      result.digest.clear();
      status_ = CrossOriginStorageStatus::kOperationFailed;
      DiscardStagingFile();
      result.status = status_;
      return result;
    }
  } else {
    hasher_.Finish(result.digest);
  }

  if (!file_.Flush()) {
    DLOG(ERROR) << "Failed to flush COS staging file: " << path_.value();
    result.digest.clear();
    status_ = CrossOriginStorageStatus::kOperationFailed;
    DiscardStagingFile();
    result.status = status_;
    return result;
  }

  finished_ = true;
  scratch_buffer_ = {};
  file_.Close();
  // `finished_ = true` prevents the destructor from deleting the staging file
  // now that ownership transfers to `CrossOriginStorageRegistry::Commit()`.
  result.path = std::move(path_);
  path_.clear();
  result.size = size_;
  result.status = CrossOriginStorageStatus::kSuccess;
  return result;
}

bool CrossOriginStorageStagingWriter::RehashFromDisk(
    base::span<uint8_t> out,
    const CancellationFlag* cancelled) {
  crypto::hash::Hasher disk_hasher(HashKindForAlgorithm(algorithm_));
  const size_t required_scratch_size =
      static_cast<size_t>(std::min<int64_t>(size_, kScratchBufferSize));
  if (scratch_buffer_.size() < required_scratch_size) {
    scratch_buffer_.resize(required_scratch_size);
  }
  int64_t offset = 0;
  while (offset < size_) {
    if (cancelled && cancelled->data.load(std::memory_order_relaxed)) {
      return false;
    }
    const size_t to_read = static_cast<size_t>(
        std::min<int64_t>(size_ - offset, kScratchBufferSize));
    std::optional<size_t> bytes_read =
        file_.Read(offset, base::span(scratch_buffer_).first(to_read));
    if (!bytes_read.has_value() || *bytes_read == 0) {
      DLOG(ERROR) << "Failed to re-read COS staging file for hashing: "
                  << path_.value();
      return false;
    }
    disk_hasher.Update(base::span(scratch_buffer_).first(*bytes_read));
    offset += static_cast<int64_t>(*bytes_read);
  }
  disk_hasher.Finish(out);
  return true;
}

void CrossOriginStorageStagingWriter::Discard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ == CrossOriginStorageStatus::kSuccess) {
    status_ = CrossOriginStorageStatus::kOperationFailed;
  }
  data_pipe_watcher_.reset();
  active_stream_.reset();
  active_write_cancelled_.reset();
  active_write_callback_.Reset();
  DiscardStagingFile();
}

void CrossOriginStorageStagingWriter::DiscardStagingFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  finished_ = true;
  scratch_buffer_ = {};
  if (file_.IsValid()) {
    file_.Close();
  }
  if (!path_.empty()) {
    if (!base::DeleteFile(path_)) {
      DLOG(WARNING) << "Failed to delete COS staging file: " << path_.value();
    }
    path_.clear();
  }
}

}  // namespace content
