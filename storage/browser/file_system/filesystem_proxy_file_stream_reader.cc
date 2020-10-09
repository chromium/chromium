// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/filesystem_proxy_file_stream_reader.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace storage {

namespace {

const int kOpenFlagsForRead =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;

using GetFileInfoCallback =
    base::OnceCallback<void(base::File::Error, const base::File::Info&)>;

FileErrorOr<base::File::Info> DoGetFileInfo(
    const base::FilePath& path,
    scoped_refptr<FilesystemProxyFileStreamReader::SharedFilesystemProxy>
        shared_filesystem_proxy) {
  if (!shared_filesystem_proxy->data->PathExists(path)) {
    return base::File::FILE_ERROR_NOT_FOUND;
  }

  base::Optional<base::File::Info> info =
      shared_filesystem_proxy->data->GetFileInfo(path);
  if (!info.has_value()) {
    return base::File::FILE_ERROR_FAILED;
  }

  return std::move(*info);
}

FileErrorOr<base::File> DoOpenFile(
    const base::FilePath& path,
    scoped_refptr<FilesystemProxyFileStreamReader::SharedFilesystemProxy>
        shared_filesystem_proxy) {
  return shared_filesystem_proxy->data->OpenFile(path, kOpenFlagsForRead);
}

}  // namespace

std::unique_ptr<FileStreamReader> FileStreamReader::CreateForFilesystemProxy(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::FilePath& file_path,
    std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
    int64_t initial_offset,
    const base::Time& expected_modification_time) {
  DCHECK(filesystem_proxy);
  constexpr bool emit_metrics = false;
  return base::WrapUnique(new FilesystemProxyFileStreamReader(
      std::move(task_runner), file_path, std::move(filesystem_proxy),
      initial_offset, expected_modification_time, emit_metrics));
}

std::unique_ptr<FileStreamReader>
FileStreamReader::CreateForIndexedDBDataItemReader(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::FilePath& file_path,
    std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
    int64_t initial_offset,
    const base::Time& expected_modification_time) {
  DCHECK(filesystem_proxy);
  constexpr bool emit_metrics = true;
  return base::WrapUnique(new FilesystemProxyFileStreamReader(
      std::move(task_runner), file_path, std::move(filesystem_proxy),
      initial_offset, expected_modification_time, emit_metrics));
}

FilesystemProxyFileStreamReader::~FilesystemProxyFileStreamReader() = default;

int FilesystemProxyFileStreamReader::Read(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_open_);

  if (stream_impl_)
    return stream_impl_->Read(buf, buf_len, std::move(callback));

  Open(base::BindOnce(&FilesystemProxyFileStreamReader::DidOpenForRead,
                      weak_factory_.GetWeakPtr(), base::RetainedRef(buf),
                      buf_len, std::move(callback)));

  return net::ERR_IO_PENDING;
}

int64_t FilesystemProxyFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&DoGetFileInfo, file_path_, shared_filesystem_proxy_),
      base::BindOnce(
          &FilesystemProxyFileStreamReader::DidGetFileInfoForGetLength,
          weak_factory_.GetWeakPtr(), std::move(callback)));
  return net::ERR_IO_PENDING;
}

FilesystemProxyFileStreamReader::FilesystemProxyFileStreamReader(
    scoped_refptr<base::TaskRunner> task_runner,
    const base::FilePath& file_path,
    std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
    int64_t initial_offset,
    const base::Time& expected_modification_time,
    bool emit_metrics)
    : task_runner_(std::move(task_runner)),
      shared_filesystem_proxy_(base::MakeRefCounted<SharedFilesystemProxy>(
          std::move(filesystem_proxy))),
      file_path_(file_path),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      emit_metrics_(emit_metrics) {}

void FilesystemProxyFileStreamReader::Open(
    net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = true;

  // Call GetLength first to make it perform last-modified-time verification,
  // and then call DidVerifyForOpen to do the rest.
  int64_t verify_result = GetLength(
      base::BindOnce(&FilesystemProxyFileStreamReader::DidVerifyForOpen,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  DCHECK_EQ(verify_result, net::ERR_IO_PENDING);
}

void FilesystemProxyFileStreamReader::DidVerifyForOpen(
    net::CompletionOnceCallback callback,
    int64_t get_length_result) {
  if (get_length_result < 0) {
    std::move(callback).Run(static_cast<int>(get_length_result));
    return;
  }

  callback_ = std::move(callback);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&DoOpenFile, file_path_, shared_filesystem_proxy_),
      base::BindOnce(&FilesystemProxyFileStreamReader::DidOpenFile,
                     weak_factory_.GetWeakPtr()));
}

void FilesystemProxyFileStreamReader::DidOpenFile(
    FileErrorOr<base::File> open_result) {
  if (open_result.is_error()) {
    std::move(callback_).Run(open_result.error());
    return;
  }

  stream_impl_ = std::make_unique<net::FileStream>(
      std::move(open_result.value()), task_runner_);
  int seek_result = stream_impl_->Seek(
      initial_offset_,
      base::BindOnce(&FilesystemProxyFileStreamReader::DidSeekFileStream,
                     weak_factory_.GetWeakPtr()));
  if (seek_result != net::ERR_IO_PENDING) {
    std::move(callback_).Run(seek_result);
  }
}

void FilesystemProxyFileStreamReader::DidSeekFileStream(int64_t seek_result) {
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

void FilesystemProxyFileStreamReader::DidOpenForRead(
    net::IOBuffer* buf,
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
  const int read_result = stream_impl_->Read(
      buf, buf_len,
      base::BindOnce(&FilesystemProxyFileStreamReader::OnRead,
                     weak_factory_.GetWeakPtr()));
  if (read_result != net::ERR_IO_PENDING)
    std::move(callback_).Run(read_result);
}

void FilesystemProxyFileStreamReader::DidGetFileInfoForGetLength(
    net::Int64CompletionOnceCallback callback,
    FileErrorOr<base::File::Info> result) {
  // TODO(enne): track rate of missing blobs for http://crbug.com/1131151
  if (emit_metrics_) {
    bool file_was_found = !result.is_error() ||
                          result.error() != base::File::FILE_ERROR_NOT_FOUND;
    UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.FoundBlobFileForValue",
                          file_was_found);
  }

  if (result.is_error()) {
    std::move(callback).Run(net::FileErrorToNetError(result.error()));
    return;
  }

  const auto& file_info = result.value();
  if (file_info.is_directory) {
    std::move(callback).Run(net::ERR_FILE_NOT_FOUND);
    return;
  }
  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    std::move(callback).Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }
  std::move(callback).Run(file_info.size);
}

void FilesystemProxyFileStreamReader::OnRead(int read_result) {
  std::move(callback_).Run(read_result);
}

}  // namespace storage
