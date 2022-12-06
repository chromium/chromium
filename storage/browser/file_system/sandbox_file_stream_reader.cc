// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_stream_reader.h"

#include <stdint.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/memory_file_stream_reader.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"

// TODO(kinuko): Remove this temporary namespace hack after we move both
// blob and fileapi into content namespace.
namespace storage {

SandboxFileStreamReader::SandboxFileStreamReader(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    int64_t initial_offset,
    const base::Time& expected_modification_time)
    : file_system_context_(file_system_context),
      url_(url),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      has_pending_create_snapshot_(false) {}

SandboxFileStreamReader::~SandboxFileStreamReader() = default;

int SandboxFileStreamReader::Read(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  DCHECK(buf);
  if (file_reader_)
    return file_reader_->Read(buf, buf_len, std::move(callback));

  return CreateSnapshot(
      base::BindOnce(&SandboxFileStreamReader::DidCreateSnapshotForRead,
                     weak_factory_.GetWeakPtr(), base::Unretained(buf), buf_len,
                     std::move(callback)));
}

int64_t SandboxFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  if (file_reader_)
    return file_reader_->GetLength(std::move(callback));

  return CreateSnapshot(
      base::BindOnce(&SandboxFileStreamReader::DidCreateSnapshotForGetLength,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

int SandboxFileStreamReader::CreateSnapshot(SnapshotCallback callback) {
  DCHECK(!has_pending_create_snapshot_);
  has_pending_create_snapshot_ = true;
  file_system_context_->operation_runner()->CreateSnapshotFile(
      url_, std::move(callback));
  return net::ERR_IO_PENDING;
}

void SandboxFileStreamReader::DidCreateSnapshotForRead(
    net::IOBuffer* read_buf,
    int read_len,
    net::CompletionOnceCallback callback,
    base::File::Error file_error,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<ShareableFileReference> file_ref) {
  DCHECK(has_pending_create_snapshot_);
  DCHECK(!file_reader_.get());
  has_pending_create_snapshot_ = false;

  if (file_error != base::File::FILE_OK) {
    std::move(callback).Run(net::FileErrorToNetError(file_error));
    return;
  }

  // Keep the reference (if it's non-null) so that the file won't go away.
  snapshot_ref_ = std::move(file_ref);

  CreateFileReader(platform_path);

  auto callback_pair = base::SplitOnceCallback(std::move(callback));

  DCHECK(read_buf);
  int rv = Read(read_buf, read_len,
                base::BindOnce(&SandboxFileStreamReader::OnRead,
                               weak_factory_.GetWeakPtr(),
                               std::move(callback_pair.first)));
  if (rv != net::ERR_IO_PENDING) {
    std::move(callback_pair.second).Run(rv);
  }
}

void SandboxFileStreamReader::DidCreateSnapshotForGetLength(
    net::Int64CompletionOnceCallback callback,
    base::File::Error file_error,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<ShareableFileReference> file_ref) {
  DCHECK(has_pending_create_snapshot_);
  DCHECK(!file_reader_.get());
  has_pending_create_snapshot_ = false;

  if (file_error != base::File::FILE_OK) {
    std::move(callback).Run(net::FileErrorToNetError(file_error));
    return;
  }

  // Keep the reference (if it's non-null) so that the file won't go away.
  snapshot_ref_ = std::move(file_ref);

  CreateFileReader(platform_path);

  auto callback_pair = base::SplitOnceCallback(std::move(callback));

  int64_t rv = file_reader_->GetLength(base::BindOnce(
      &SandboxFileStreamReader::OnGetLength, weak_factory_.GetWeakPtr(),
      std::move(callback_pair.first)));
  if (rv != net::ERR_IO_PENDING)
    std::move(callback_pair.second).Run(rv);
}

void SandboxFileStreamReader::CreateFileReader(
    const base::FilePath& platform_path) {
  if (file_system_context_->is_incognito()) {
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_delegate =
        file_system_context_->sandbox_delegate()->memory_file_util_delegate();
    file_reader_ = std::make_unique<MemoryFileStreamReader>(
        file_system_context_->default_file_task_runner(),
        memory_file_util_delegate, platform_path, initial_offset_,
        expected_modification_time_);
  } else {
    file_reader_ = FileStreamReader::CreateForLocalFile(
        file_system_context_->default_file_task_runner(), platform_path,
        initial_offset_, expected_modification_time_);
  }
}

void SandboxFileStreamReader::OnRead(net::CompletionOnceCallback callback,
                                     int rv) {
  std::move(callback).Run(rv);
}

void SandboxFileStreamReader::OnGetLength(
    net::Int64CompletionOnceCallback callback,
    int64_t rv) {
  std::move(callback).Run(rv);
}

}  // namespace storage
