// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_reader.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace storage {

namespace {

const int kOpenFlagsForRead =
    base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;

struct GetFileInfoResults {
  base::File::Error error;
  base::File::Info info;
};

using GetFileInfoCallback =
    base::OnceCallback<void(base::File::Error, const base::File::Info&)>;

GetFileInfoResults DoGetFileInfo(const base::FilePath& path) {
  GetFileInfoResults results;
  if (!base::PathExists(path)) {
    results.error = base::File::FILE_ERROR_NOT_FOUND;
    return results;
  }
  results.error = base::GetFileInfo(path, &results.info)
                      ? base::File::FILE_OK
                      : base::File::FILE_ERROR_FAILED;
  return results;
}

void SendGetFileInfoResults(GetFileInfoCallback callback,
                            const GetFileInfoResults& results) {
  std::move(callback).Run(results.error, results.info);
}

}  // namespace

std::unique_ptr<FileStreamReader> FileStreamReader::CreateForLocalFile(
    base::TaskRunner* task_runner,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time) {
  return base::WrapUnique(new LocalFileStreamReader(
      task_runner, file_path, initial_offset, expected_modification_time));
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
  bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE, base::BindOnce(&DoGetFileInfo, file_path_),
      base::BindOnce(
          &SendGetFileInfoResults,
          base::BindOnce(&LocalFileStreamReader::DidGetFileInfoForGetLength,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
  DCHECK(posted);
  return net::ERR_IO_PENDING;
}

LocalFileStreamReader::LocalFileStreamReader(
    base::TaskRunner* task_runner,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time)
    : task_runner_(task_runner),
      file_path_(file_path),
      initial_offset_(initial_offset),
      expected_modification_time_(expected_modification_time),
      has_pending_open_(false) {}

void LocalFileStreamReader::Open(net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_open_);
  DCHECK(!stream_impl_.get());
  has_pending_open_ = true;

  // Call GetLength first to make it perform last-modified-time verification,
  // and then call DidVerifyForOpen to do the rest.
  int64_t verify_result = GetLength(
      base::BindOnce(&LocalFileStreamReader::DidVerifyForOpen,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  DCHECK_EQ(verify_result, net::ERR_IO_PENDING);
}

void LocalFileStreamReader::DidVerifyForOpen(
    net::CompletionOnceCallback callback,
    int64_t get_length_result) {
  if (get_length_result < 0) {
    std::move(callback).Run(static_cast<int>(get_length_result));
    return;
  }

  stream_impl_.reset(new net::FileStream(task_runner_));
  callback_ = std::move(callback);
  const int result = stream_impl_->Open(
      file_path_, kOpenFlagsForRead,
      base::BindOnce(&LocalFileStreamReader::DidOpenFileStream,
                     weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    std::move(callback_).Run(result);
}

void LocalFileStreamReader::DidOpenFileStream(int result) {
  if (result != net::OK) {
    std::move(callback_).Run(result);
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
    base::File::Error error,
    const base::File::Info& file_info) {
  if (file_info.is_directory) {
    std::move(callback).Run(net::ERR_FILE_NOT_FOUND);
    return;
  }
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(net::FileErrorToNetError(error));
    return;
  }
  if (!VerifySnapshotTime(expected_modification_time_, file_info)) {
    std::move(callback).Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }
  std::move(callback).Run(file_info.size);
}

void LocalFileStreamReader::OnRead(int read_result) {
  std::move(callback_).Run(read_result);
}

}  // namespace storage
