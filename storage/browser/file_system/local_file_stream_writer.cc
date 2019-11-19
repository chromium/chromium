// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/local_file_stream_writer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace storage {

namespace {

const int kOpenFlagsForWrite =
    base::File::FLAG_OPEN | base::File::FLAG_WRITE | base::File::FLAG_ASYNC;
const int kCreateFlagsForWrite =
    base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_ASYNC;

}  // namespace

std::unique_ptr<FileStreamWriter> FileStreamWriter::CreateForLocalFile(
    base::TaskRunner* task_runner,
    const base::FilePath& file_path,
    int64_t initial_offset,
    OpenOrCreate open_or_create) {
  return base::WrapUnique(new LocalFileStreamWriter(
      task_runner, file_path, initial_offset, open_or_create));
}

LocalFileStreamWriter::~LocalFileStreamWriter() {
  // Invalidate weak pointers so that we won't receive any callbacks from
  // in-flight stream operations, which might be triggered during the file close
  // in the FileStream destructor.
  weak_factory_.InvalidateWeakPtrs();

  // FileStream's destructor closes the file safely, since we opened the file
  // by its Open() method.
}

int LocalFileStreamWriter::Write(net::IOBuffer* buf,
                                 int buf_len,
                                 net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_operation_);
  DCHECK(write_callback_.is_null());
  DCHECK(cancel_callback_.is_null());

  has_pending_operation_ = true;
  write_callback_ = std::move(callback);
  if (stream_impl_) {
    int result = InitiateWrite(buf, buf_len);
    if (result != net::ERR_IO_PENDING)
      has_pending_operation_ = false;
    return result;
  }
  return InitiateOpen(base::BindOnce(&LocalFileStreamWriter::ReadyToWrite,
                                     weak_factory_.GetWeakPtr(),
                                     base::RetainedRef(buf), buf_len));
}

int LocalFileStreamWriter::Cancel(net::CompletionOnceCallback callback) {
  if (!has_pending_operation_)
    return net::ERR_UNEXPECTED;

  DCHECK(!callback.is_null());
  cancel_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

int LocalFileStreamWriter::Flush(net::CompletionOnceCallback callback) {
  DCHECK(!has_pending_operation_);
  DCHECK(cancel_callback_.is_null());

  // Write() is not called yet, so there's nothing to flush.
  if (!stream_impl_)
    return net::OK;

  has_pending_operation_ = true;
  int result = InitiateFlush(std::move(callback));
  if (result != net::ERR_IO_PENDING)
    has_pending_operation_ = false;
  return result;
}

LocalFileStreamWriter::LocalFileStreamWriter(base::TaskRunner* task_runner,
                                             const base::FilePath& file_path,
                                             int64_t initial_offset,
                                             OpenOrCreate open_or_create)
    : file_path_(file_path),
      open_or_create_(open_or_create),
      initial_offset_(initial_offset),
      task_runner_(task_runner),
      has_pending_operation_(false) {}

int LocalFileStreamWriter::InitiateOpen(base::OnceClosure main_operation) {
  DCHECK(has_pending_operation_);
  DCHECK(!stream_impl_.get());

  stream_impl_.reset(new net::FileStream(task_runner_));

  int open_flags = 0;
  switch (open_or_create_) {
    case OPEN_EXISTING_FILE:
      open_flags = kOpenFlagsForWrite;
      break;
    case CREATE_NEW_FILE:
      open_flags = kCreateFlagsForWrite;
      break;
  }

  return stream_impl_->Open(
      file_path_, open_flags,
      base::BindOnce(&LocalFileStreamWriter::DidOpen,
                     weak_factory_.GetWeakPtr(), std::move(main_operation)));
}

void LocalFileStreamWriter::DidOpen(base::OnceClosure main_operation,
                                    int result) {
  DCHECK(has_pending_operation_);
  DCHECK(stream_impl_.get());

  if (CancelIfRequested())
    return;

  if (result != net::OK) {
    has_pending_operation_ = false;
    stream_impl_.reset(nullptr);
    std::move(write_callback_).Run(result);
    return;
  }

  InitiateSeek(std::move(main_operation));
}

void LocalFileStreamWriter::InitiateSeek(base::OnceClosure main_operation) {
  DCHECK(has_pending_operation_);
  DCHECK(stream_impl_.get());

  if (initial_offset_ == 0) {
    // No need to seek.
    std::move(main_operation).Run();
    return;
  }

  int result = stream_impl_->Seek(
      initial_offset_,
      base::BindOnce(&LocalFileStreamWriter::DidSeek,
                     weak_factory_.GetWeakPtr(), std::move(main_operation)));
  if (result != net::ERR_IO_PENDING) {
    has_pending_operation_ = false;
    std::move(write_callback_).Run(result);
  }
}

void LocalFileStreamWriter::DidSeek(base::OnceClosure main_operation,
                                    int64_t result) {
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;

  if (result != initial_offset_) {
    // TODO(kinaba) add a more specific error code.
    result = net::ERR_FAILED;
  }

  if (result < 0) {
    has_pending_operation_ = false;
    std::move(write_callback_).Run(static_cast<int>(result));
    return;
  }

  std::move(main_operation).Run();
}

void LocalFileStreamWriter::ReadyToWrite(net::IOBuffer* buf, int buf_len) {
  DCHECK(has_pending_operation_);

  int result = InitiateWrite(buf, buf_len);
  if (result != net::ERR_IO_PENDING) {
    has_pending_operation_ = false;
    std::move(write_callback_).Run(result);
  }
}

int LocalFileStreamWriter::InitiateWrite(net::IOBuffer* buf, int buf_len) {
  DCHECK(has_pending_operation_);
  DCHECK(stream_impl_.get());

  return stream_impl_->Write(buf, buf_len,
                             base::BindOnce(&LocalFileStreamWriter::DidWrite,
                                            weak_factory_.GetWeakPtr()));
}

void LocalFileStreamWriter::DidWrite(int result) {
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;
  has_pending_operation_ = false;
  std::move(write_callback_).Run(result);
}

int LocalFileStreamWriter::InitiateFlush(net::CompletionOnceCallback callback) {
  DCHECK(has_pending_operation_);
  DCHECK(stream_impl_.get());

  return stream_impl_->Flush(base::BindOnce(&LocalFileStreamWriter::DidFlush,
                                            weak_factory_.GetWeakPtr(),
                                            std::move(callback)));
}

void LocalFileStreamWriter::DidFlush(net::CompletionOnceCallback callback,
                                     int result) {
  DCHECK(has_pending_operation_);

  if (CancelIfRequested())
    return;
  has_pending_operation_ = false;
  std::move(callback).Run(result);
}

bool LocalFileStreamWriter::CancelIfRequested() {
  DCHECK(has_pending_operation_);

  if (cancel_callback_.is_null())
    return false;

  has_pending_operation_ = false;
  std::move(cancel_callback_).Run(net::OK);
  return true;
}

}  // namespace storage
