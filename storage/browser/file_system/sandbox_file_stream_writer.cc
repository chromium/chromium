// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_stream_writer.h"

#include <stdint.h>

#include <limits>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/memory_file_stream_writer.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {

// Adjust the |quota| value to make the remaining quota calculation easier. This
// allows us to simply compare written bytes against the adjusted quota.
int64_t AdjustQuotaForOverlap(int64_t quota,
                              int64_t file_offset,
                              int64_t file_size) {
  if (quota < 0)
    quota = 0;
  // |overlap| can be negative if |file_offset| is past the end of the file.
  // Negative |overlap| ensures null bytes between the end of the file and the
  // |file_offset| are counted towards the file's quota.
  int64_t overlap = file_size - file_offset;
  if (overlap < 0 || std::numeric_limits<int64_t>::max() - overlap > quota)
    quota += overlap;
  return quota;
}

}  // namespace

SandboxFileStreamWriter::SandboxFileStreamWriter(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    int64_t initial_offset,
    const UpdateObserverList& observers)
    : file_system_context_(file_system_context),
      url_(url),
      initial_offset_(initial_offset),
      observers_(observers),
      file_size_(0),
      total_bytes_written_(0),
      allowed_bytes_to_write_(0),
      has_pending_operation_(false),
      default_quota_(std::numeric_limits<int64_t>::max()) {
  DCHECK(url_.is_valid());
}

SandboxFileStreamWriter::~SandboxFileStreamWriter() = default;

int SandboxFileStreamWriter::Write(net::IOBuffer* buf,
                                   int buf_len,
                                   net::CompletionOnceCallback callback) {
  DCHECK(!write_callback_);
  has_pending_operation_ = true;
  if (file_writer_) {
    const int result = WriteInternal(buf, buf_len);
    if (result == net::ERR_IO_PENDING)
      write_callback_ = std::move(callback);
    return result;
  }

  write_callback_ = std::move(callback);
  net::CompletionOnceCallback write_task = base::BindOnce(
      &SandboxFileStreamWriter::DidInitializeForWrite,
      weak_factory_.GetWeakPtr(), base::RetainedRef(buf), buf_len);
  file_system_context_->operation_runner()->CreateSnapshotFile(
      url_, base::BindOnce(&SandboxFileStreamWriter::DidCreateSnapshotFile,
                           weak_factory_.GetWeakPtr(), std::move(write_task)));
  return net::ERR_IO_PENDING;
}

int SandboxFileStreamWriter::Cancel(net::CompletionOnceCallback callback) {
  if (!has_pending_operation_)
    return net::ERR_UNEXPECTED;

  DCHECK(!callback.is_null());
  cancel_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

int SandboxFileStreamWriter::WriteInternal(net::IOBuffer* buf, int buf_len) {
  // allowed_bytes_to_write could be negative if the file size is
  // greater than the current (possibly new) quota.
  DCHECK(total_bytes_written_ <= allowed_bytes_to_write_ ||
         allowed_bytes_to_write_ < 0);
  if (total_bytes_written_ >= allowed_bytes_to_write_) {
    const int out_of_quota = net::ERR_FILE_NO_SPACE;
    DidWrite(out_of_quota);
    return out_of_quota;
  }

  if (buf_len > allowed_bytes_to_write_ - total_bytes_written_)
    buf_len = allowed_bytes_to_write_ - total_bytes_written_;

  DCHECK(file_writer_.get());
  const int result =
      file_writer_->Write(buf, buf_len,
                          base::BindOnce(&SandboxFileStreamWriter::DidWrite,
                                         weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    DidWrite(result);
  return result;
}

void SandboxFileStreamWriter::DidCreateSnapshotFile(
    net::CompletionOnceCallback callback,
    base::File::Error file_error,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<ShareableFileReference> file_ref) {
  DCHECK(!file_ref.get());

  if (CancelIfRequested())
    return;
  if (file_error != base::File::FILE_OK) {
    std::move(callback).Run(net::FileErrorToNetError(file_error));
    return;
  }
  if (file_info.is_directory) {
    // We should not be writing to a directory.
    std::move(callback).Run(net::ERR_ACCESS_DENIED);
    return;
  }
  file_size_ = file_info.size;

  DCHECK(!file_writer_.get());

  if (file_system_context_->is_incognito()) {
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_delegate =
        file_system_context_->sandbox_delegate()->memory_file_util_delegate();
    file_writer_ = std::make_unique<MemoryFileStreamWriter>(
        file_system_context_->default_file_task_runner(),
        memory_file_util_delegate, platform_path, initial_offset_);

  } else {
    file_writer_ = FileStreamWriter::CreateForLocalFile(
        file_system_context_->default_file_task_runner(), platform_path,
        initial_offset_, FileStreamWriter::OPEN_EXISTING_FILE);
  }
  const scoped_refptr<QuotaManagerProxy>& quota_manager_proxy =
      file_system_context_->quota_manager_proxy();
  if (!quota_manager_proxy) {
    // If we don't have the quota manager or the requested filesystem type
    // does not support quota, we should be able to let it go.
    allowed_bytes_to_write_ = default_quota_;
    std::move(callback).Run(net::OK);
    return;
  }

  BucketLocator bucket = url_.bucket().value_or(
      BucketLocator::ForDefaultBucket(url_.storage_key()));
  bucket.type = FileSystemTypeToQuotaStorageType(url_.type());

  DCHECK(quota_manager_proxy);
  quota_manager_proxy->GetBucketSpaceRemaining(
      bucket, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&SandboxFileStreamWriter::DidGetBucketSpaceRemaining,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SandboxFileStreamWriter::DidGetBucketSpaceRemaining(
    net::CompletionOnceCallback callback,
    storage::QuotaErrorOr<int64_t> space_remaining) {
  if (CancelIfRequested())
    return;
  if (!space_remaining.has_value()) {
    LOG(WARNING) << "Got unexpected quota error";
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  allowed_bytes_to_write_ = space_remaining.value();
  std::move(callback).Run(net::OK);
}

void SandboxFileStreamWriter::DidInitializeForWrite(net::IOBuffer* buf,
                                                    int buf_len,
                                                    int init_status) {
  if (CancelIfRequested())
    return;
  if (init_status != net::OK) {
    has_pending_operation_ = false;
    std::move(write_callback_).Run(init_status);
    return;
  }
  allowed_bytes_to_write_ = AdjustQuotaForOverlap(allowed_bytes_to_write_,
                                                  initial_offset_, file_size_);
  const int result = WriteInternal(buf, buf_len);
  if (result == net::ERR_IO_PENDING)
    DCHECK(!write_callback_.is_null());
}

void SandboxFileStreamWriter::DidWrite(int write_response) {
  DCHECK(has_pending_operation_);
  has_pending_operation_ = false;

  if (write_response <= 0) {
    // TODO(crbug.com/40134326): Consider listening explicitly for out
    // of space errors instead of surfacing all write errors to quota.
    const scoped_refptr<QuotaManagerProxy>& quota_manager_proxy =
        file_system_context_->quota_manager_proxy();
    if (quota_manager_proxy) {
      quota_manager_proxy->OnClientWriteFailed(url_.storage_key());
    }
    if (CancelIfRequested())
      return;
    if (write_callback_)
      std::move(write_callback_).Run(write_response);
    return;
  }

  if (total_bytes_written_ + write_response + initial_offset_ > file_size_) {
    int overlapped = file_size_ - total_bytes_written_ - initial_offset_;
    // If writing past the end of a file, the distance seeked past the file
    // needs to be accounted for. This adjustment should only be made for the
    // first such write (when |total_bytes_written_| is 0).
    if (overlapped < 0 && total_bytes_written_ != 0)
      overlapped = 0;
    observers_.Notify(&FileUpdateObserver::OnUpdate, url_,
                      write_response - overlapped);
  }
  total_bytes_written_ += write_response;

  if (CancelIfRequested())
    return;
  if (write_callback_)
    std::move(write_callback_).Run(write_response);
}

bool SandboxFileStreamWriter::CancelIfRequested() {
  if (cancel_callback_.is_null())
    return false;

  has_pending_operation_ = false;
  std::move(cancel_callback_).Run(net::OK);
  return true;
}

int SandboxFileStreamWriter::Flush(FlushMode flush_mode,
                                   net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_operation_);
  DCHECK(cancel_callback_.is_null());

  // Write() is not called yet, so there's nothing to flush.
  if (!file_writer_)
    return net::OK;

  has_pending_operation_ = true;
  int result = file_writer_->Flush(
      flush_mode,
      base::BindOnce(&SandboxFileStreamWriter::DidFlush,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  if (result != net::ERR_IO_PENDING)
    has_pending_operation_ = false;
  return result;
}

void SandboxFileStreamWriter::DidFlush(net::CompletionOnceCallback callback,
                                       int result) {
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;
  has_pending_operation_ = false;
  std::move(callback).Run(result);
}

}  // namespace storage
