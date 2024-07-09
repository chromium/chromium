// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_reader.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace storage {

namespace {

const int kOpenFlagsForRead =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;

base::FileErrorOr<base::File::Info> DoGetFileInfo(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::unexpected(base::File::FILE_ERROR_NOT_FOUND);

  base::File::Info info;
  bool success = base::GetFileInfo(path, &info);
  if (!success)
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  return info;
}

}  // namespace

std::unique_ptr<FileStreamReader> FileStreamReader::CreateForLocalFile(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  return std::make_unique<LocalFileStreamReader>(
      std::move(task_runner), file_path, initial_offset,
      expected_modification_time, base::PassKey<FileStreamReader>(),
      std::move(file_access));
}

LocalFileStreamReader::~LocalFileStreamReader() = default;

int LocalFileStreamReader::Read(net::IOBuffer* buf,
                                int buf_len,
                                net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_open_);

  if (stream_impl_)
    return stream_impl_->Read(buf, buf_len, std::move(callback));

  Open(base::BindOnce(&LocalFileStreamReader::DidOpenForRead,
                      weak_factory_.GetWeakPtr(), base::RetainedRef(buf),
                      buf_len, std::move(callback)));

  return net::ERR_IO_PENDING;
}

int64_t LocalFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  bool posted = task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetFileInfo, file_path_),
      base::BindOnce(&LocalFileStreamReader::DidGetFileInfoForGetLength,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

LocalFileStreamReader::LocalFileStreamReader(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time,
    base::PassKey<FileStreamReader> /*pass_key*/,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access)
    : task_runner_(std::move(task_runner)),
      file_path_(file_path),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      file_access_(std::move(file_access)) {}

void LocalFileStreamReader::Open(net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = true;

  base::OnceCallback<void(file_access::ScopedFileAccess)> open_cb =
      base::BindOnce(&LocalFileStreamReader::OnScopedFileAccessRequested,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  if (file_access_) {
    file_access_.Run({file_path_}, std::move(open_cb));
    return;
  }

  file_access::ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
      {file_path_}, std::move(open_cb));
}

void LocalFileStreamReader::OnScopedFileAccessRequested(
    net::CompletionOnceCallback callback,
    file_access::ScopedFileAccess scoped_file_access) {
  if (!scoped_file_access.is_allowed()) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED);
    return;
  }

  int64_t verify_result = GetLength(base::BindOnce(
      &LocalFileStreamReader::DidVerifyForOpen, weak_factory_.GetWeakPtr(),
      std::move(callback), std::move(scoped_file_access)));
  DCHECK_EQ(verify_result, net::ERR_IO_PENDING);
}

void LocalFileStreamReader::DidVerifyForOpen(
    net::CompletionOnceCallback callback,
    file_access::ScopedFileAccess scoped_file_access,
    int64_t get_length_result) {
  if (get_length_result < 0) {
    std::move(callback).Run(static_cast<int>(get_length_result));
    return;
  }

  stream_impl_ = std::make_unique<net::FileStream>(task_runner_);
  callback_ = std::move(callback);
  const int result = stream_impl_->Open(
      file_path_, kOpenFlagsForRead,
      base::BindOnce(&LocalFileStreamReader::DidOpenFileStream,
                     weak_factory_.GetWeakPtr(),
                     std::move(scoped_file_access)));
  if (result != net::ERR_IO_PENDING)
    std::move(callback_).Run(result);
}

void LocalFileStreamReader::DidOpenFileStream(
    file_access::ScopedFileAccess /*scoped_file_access*/,
    int result) {
  if (result != net::OK) {
    std::move(callback_).Run(result);
    return;
  }
  // Avoid seek if possible since it fails on android for virtual content-uris.
  if (initial_offset_ == 0) {
    std::move(callback_).Run(net::OK);
    return;
  }
  result = stream_impl_->Seek(
      initial_offset_, base::BindOnce(&LocalFileStreamReader::DidSeekFileStream,
                                      weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING) {
    std::move(callback_).Run(result);
  }
}

void LocalFileStreamReader::DidSeekFileStream(int64_t seek_result) {
  if (seek_result < 0) {
    std::move(callback_).Run(static_cast<int>(seek_result));
    return;
  }
  if (seek_result != initial_offset_) {
    std::move(callback_).Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }
  std::move(callback_).Run(net::OK);
}

void LocalFileStreamReader::DidOpenForRead(net::IOBuffer* buf,
                                           int buf_len,
                                           net::CompletionOnceCallback callback,
                                           int open_result) {
  DCHECK(has_pending_open_);
  has_pending_open_ = false;
  if (open_result != net::OK) {
    stream_impl_.reset();
    std::move(callback).Run(open_result);
    return;
  }
  DCHECK(stream_impl_.get());

  callback_ = std::move(callback);
  const int read_result =
      stream_impl_->Read(buf, buf_len,
                         base::BindOnce(&LocalFileStreamReader::OnRead,
                                        weak_factory_.GetWeakPtr()));
  if (read_result != net::ERR_IO_PENDING)
    std::move(callback_).Run(read_result);
}

void LocalFileStreamReader::DidGetFileInfoForGetLength(
    net::Int64CompletionOnceCallback callback,
    base::FileErrorOr<base::File::Info> result) {
  std::move(callback).Run([&]() -> int64_t {
    ASSIGN_OR_RETURN(const auto& file_info, result, net::FileErrorToNetError);
    if (file_info.is_directory) {
      return net::ERR_FILE_NOT_FOUND;
    }
    if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
      return net::ERR_UPLOAD_FILE_CHANGED;
    }
    return file_info.size;
  }());
}

void LocalFileStreamReader::OnRead(int read_result) {
  std::move(callback_).Run(read_result);
}

}  // namespace storage
