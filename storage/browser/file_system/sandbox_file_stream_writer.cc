// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_stream_writer.h"

#include <stdint.h>

#include <limits>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/plugin_private_file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

namespace {

// Adjust the |quota| value in overwriting case (i.e. |file_size| > 0 and
// |file_offset| < |file_size|) to make the remaining quota calculation easier.
// Specifically this widens the quota for overlapping range (so that we can
// simply compare written bytes against the adjusted quota).
int64_t AdjustQuotaForOverlap(int64_t quota,
                              int64_t file_offset,
                              int64_t file_size) {
  DCHECK_LE(file_offset, file_size);
  if (quota < 0)
    quota = 0;
  int64_t overlap = file_size - file_offset;
  if (std::numeric_limits<int64_t>::max() - overlap > quota)
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
  write_callback_ = std::move(callback);
  if (file_writer_)
    return WriteInternal(buf, buf_len);

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
    has_pending_operation_ = false;
    return net::ERR_FILE_NO_SPACE;
  }

  if (buf_len > allowed_bytes_to_write_ - total_bytes_written_)
    buf_len = allowed_bytes_to_write_ - total_bytes_written_;

  DCHECK(file_writer_.get());
  const int result =
      file_writer_->Write(buf, buf_len,
                          base::BindOnce(&SandboxFileStreamWriter::DidWrite,
                                         weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    has_pending_operation_ = false;
  return result;
}

void SandboxFileStreamWriter::DidCreateSnapshotFile(
    net::CompletionOnceCallback callback,
    base::File::Error file_error,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
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
  if (initial_offset_ > file_size_) {
    // We should not be writing pass the end of the file.
    std::move(callback).Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }
  DCHECK(!file_writer_.get());

  if (file_system_context_->is_incognito()) {
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_delegate;
    if (url_.type() == kFileSystemTypePluginPrivate) {
      auto* backend = static_cast<PluginPrivateFileSystemBackend*>(
          file_system_context_->GetFileSystemBackend(
              kFileSystemTypePluginPrivate));
      memory_file_util_delegate =
          backend->obfuscated_file_util_memory_delegate()->GetWeakPtr();
    } else {
      memory_file_util_delegate =
          file_system_context_->sandbox_delegate()->memory_file_util_delegate();
    }
    file_writer_ = FileStreamWriter::CreateForMemoryFile(
        memory_file_util_delegate, platform_path, initial_offset_,
        FileStreamWriter::OPEN_EXISTING_FILE);

  } else {
    file_writer_ = FileStreamWriter::CreateForLocalFile(
        file_system_context_->default_file_task_runner(), platform_path,
        initial_offset_, FileStreamWriter::OPEN_EXISTING_FILE);
  }
  storage::QuotaManagerProxy* quota_manager_proxy =
      file_system_context_->quota_manager_proxy();
  if (!quota_manager_proxy) {
    // If we don't have the quota manager or the requested filesystem type
    // does not support quota, we should be able to let it go.
    allowed_bytes_to_write_ = default_quota_;
    std::move(callback).Run(net::OK);
    return;
  }

  DCHECK(quota_manager_proxy->quota_manager());
  quota_manager_proxy->quota_manager()->GetUsageAndQuota(
      url_.origin(), FileSystemTypeToQuotaStorageType(url_.type()),
      base::BindOnce(&SandboxFileStreamWriter::DidGetUsageAndQuota,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SandboxFileStreamWriter::DidGetUsageAndQuota(
    net::CompletionOnceCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  if (CancelIfRequested())
    return;
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    LOG(WARNING) << "Got unexpected quota error : " << static_cast<int>(status);

    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  allowed_bytes_to_write_ = quota - usage;
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
  if (result != net::ERR_IO_PENDING)
    std::move(write_callback_).Run(result);
}

void SandboxFileStreamWriter::DidWrite(int write_response) {
  DCHECK(has_pending_operation_);
  has_pending_operation_ = false;

  if (write_response <= 0) {
    if (CancelIfRequested())
      return;
    std::move(write_callback_).Run(write_response);
    return;
  }

  if (total_bytes_written_ + write_response + initial_offset_ > file_size_) {
    int overlapped = file_size_ - total_bytes_written_ - initial_offset_;
    if (overlapped < 0)
      overlapped = 0;
    observers_.Notify(&FileUpdateObserver::OnUpdate, url_,
                      write_response - overlapped);
  }
  total_bytes_written_ += write_response;

  if (CancelIfRequested())
    return;
  std::move(write_callback_).Run(write_response);
}

bool SandboxFileStreamWriter::CancelIfRequested() {
  if (cancel_callback_.is_null())
    return false;

  has_pending_operation_ = false;
  std::move(cancel_callback_).Run(net::OK);
  return true;
}

int SandboxFileStreamWriter::Flush(net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_operation_);
  DCHECK(cancel_callback_.is_null());

  // Write() is not called yet, so there's nothing to flush.
  if (!file_writer_)
    return net::OK;

  return file_writer_->Flush(std::move(callback));
}

}  // namespace storage
