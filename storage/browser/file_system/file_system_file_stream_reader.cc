// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_file_stream_reader.h"

#include <stdint.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/plugin_private_file_system_backend.h"

using storage::FileStreamReader;

// TODO(kinuko): Remove this temporary namespace hack after we move both
// blob and fileapi into content namespace.
namespace storage {

std::unique_ptr<FileStreamReader> FileStreamReader::CreateForFileSystemFile(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    int64_t initial_offset,
    const base::Time& expected_modification_time) {
  return base::WrapUnique(new FileSystemFileStreamReader(
      file_system_context, url, initial_offset, expected_modification_time));
}

FileSystemFileStreamReader::FileSystemFileStreamReader(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    int64_t initial_offset,
    const base::Time& expected_modification_time)
    : read_buf_(nullptr),
      read_buf_len_(0),
      file_system_context_(file_system_context),
      url_(url),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      has_pending_create_snapshot_(false) {}

FileSystemFileStreamReader::~FileSystemFileStreamReader() = default;

int FileSystemFileStreamReader::Read(net::IOBuffer* buf,
                                     int buf_len,
                                     net::CompletionOnceCallback callback) {
  if (file_reader_)
    return file_reader_->Read(buf, buf_len, std::move(callback));

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = std::move(callback);
  return CreateSnapshot();
}

int64_t FileSystemFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  if (file_reader_)
    return file_reader_->GetLength(std::move(callback));

  get_length_callback_ = std::move(callback);
  return CreateSnapshot();
}

int FileSystemFileStreamReader::CreateSnapshot() {
  DCHECK(!has_pending_create_snapshot_);
  has_pending_create_snapshot_ = true;
  file_system_context_->operation_runner()->CreateSnapshotFile(
      url_, base::BindOnce(&FileSystemFileStreamReader::DidCreateSnapshot,
                           weak_factory_.GetWeakPtr()));
  return net::ERR_IO_PENDING;
}

void FileSystemFileStreamReader::DidCreateSnapshot(
    base::File::Error file_error,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
  DCHECK(has_pending_create_snapshot_);
  DCHECK(!file_reader_.get());
  has_pending_create_snapshot_ = false;

  if (file_error != base::File::FILE_OK) {
    if (read_callback_) {
      DCHECK(!get_length_callback_);
      std::move(read_callback_).Run(net::FileErrorToNetError(file_error));
      return;
    }
    std::move(get_length_callback_).Run(net::FileErrorToNetError(file_error));
    return;
  }

  // Keep the reference (if it's non-null) so that the file won't go away.
  snapshot_ref_ = std::move(file_ref);

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
    file_reader_ = FileStreamReader::CreateForMemoryFile(
        memory_file_util_delegate, platform_path, initial_offset_,
        expected_modification_time_);
  } else {
    file_reader_ = FileStreamReader::CreateForLocalFile(
        file_system_context_->default_file_task_runner(), platform_path,
        initial_offset_, expected_modification_time_);
  }

  if (read_callback_) {
    DCHECK(!get_length_callback_);
    int rv = Read(read_buf_, read_buf_len_,
                  base::BindOnce(&FileSystemFileStreamReader::OnRead,
                                 weak_factory_.GetWeakPtr()));
    if (rv != net::ERR_IO_PENDING)
      std::move(read_callback_).Run(rv);
    return;
  }

  int64_t rv = file_reader_->GetLength(base::BindOnce(
      &FileSystemFileStreamReader::OnGetLength, weak_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    std::move(get_length_callback_).Run(rv);
}

void FileSystemFileStreamReader::OnRead(int rv) {
  std::move(read_callback_).Run(rv);
}

void FileSystemFileStreamReader::OnGetLength(int64_t rv) {
  std::move(get_length_callback_).Run(rv);
}

}  // namespace storage
