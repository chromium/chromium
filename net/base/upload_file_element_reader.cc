// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_file_element_reader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// In tests, this value is used to override the return value of
// UploadFileElementReader::GetContentLength() when set to non-zero.
uint64_t overriding_content_length = 0;

}  // namespace

UploadFileElementReader::UploadFileElementReader(
    base::TaskRunner* task_runner,
    base::File file,
    const base::FilePath& path,
    uint64_t range_offset,
    uint64_t range_length,
    const base::Time& expected_modification_time)
    : task_runner_(task_runner),
      path_(path),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      content_length_(0),
      bytes_remaining_(0),
      next_state_(State::IDLE),
      init_called_while_operation_pending_(false) {
  DCHECK(file.IsValid());
  DCHECK(task_runner_.get());
  file_stream_ = std::make_unique<FileStream>(std::move(file), task_runner);
}

UploadFileElementReader::UploadFileElementReader(
    base::TaskRunner* task_runner,
    const base::FilePath& path,
    uint64_t range_offset,
    uint64_t range_length,
    const base::Time& expected_modification_time)
    : task_runner_(task_runner),
      path_(path),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      content_length_(0),
      bytes_remaining_(0),
      next_state_(State::IDLE),
      init_called_while_operation_pending_(false) {
  DCHECK(task_runner_.get());
}

UploadFileElementReader::~UploadFileElementReader() = default;

const UploadFileElementReader* UploadFileElementReader::AsFileReader() const {
  return this;
}

int UploadFileElementReader::Init(CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  bytes_remaining_ = 0;
  content_length_ = 0;
  pending_callback_.Reset();

  // If the file is being opened, just update the callback, and continue
  // waiting.
  if (next_state_ == State::OPEN_COMPLETE) {
    DCHECK(file_stream_);
    pending_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  // If there's already a pending operation, wait for it to complete before
  // restarting the request.
  if (next_state_ != State::IDLE) {
    init_called_while_operation_pending_ = true;
    pending_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  DCHECK(!init_called_while_operation_pending_);

  if (file_stream_) {
    // If the file is already open, just re-use it.
    // TODO(mmenke): Consider reusing file info, too.
    next_state_ = State::SEEK;
  } else {
    next_state_ = State::OPEN;
  }
  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING)
    pending_callback_ = std::move(callback);
  return result;
}

uint64_t UploadFileElementReader::GetContentLength() const {
  if (overriding_content_length)
    return overriding_content_length;
  return content_length_;
}

uint64_t UploadFileElementReader::BytesRemaining() const {
  return bytes_remaining_;
}

int UploadFileElementReader::Read(IOBuffer* buf,
                                  int buf_length,
                                  CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(next_state_, State::IDLE);
  DCHECK(file_stream_);

  int num_bytes_to_read = static_cast<int>(
      std::min(BytesRemaining(), static_cast<uint64_t>(buf_length)));
  if (num_bytes_to_read == 0)
    return 0;

  next_state_ = State::READ_COMPLETE;
  int result = file_stream_->Read(
      buf, num_bytes_to_read,
      base::BindOnce(base::IgnoreResult(&UploadFileElementReader::OnIOComplete),
                     weak_ptr_factory_.GetWeakPtr()));

  if (result != ERR_IO_PENDING)
    result = DoLoop(result);

  if (result == ERR_IO_PENDING)
    pending_callback_ = std::move(callback);

  return result;
}

int UploadFileElementReader::DoLoop(int result) {
  DCHECK_NE(result, ERR_IO_PENDING);

  if (init_called_while_operation_pending_) {
    // File should already have been opened successfully.
    DCHECK_NE(next_state_, State::OPEN_COMPLETE);

    next_state_ = State::SEEK;
    init_called_while_operation_pending_ = false;
    result = net::OK;
  }

  while (next_state_ != State::IDLE && result != ERR_IO_PENDING) {
    State state = next_state_;
    next_state_ = State::IDLE;
    switch (state) {
      case State::IDLE:
        NOTREACHED();
        break;
      case State::OPEN:
        // Ignore previous result here. It's typically OK, but if Init()
        // interrupted the previous operation, it may be an error.
        result = DoOpen();
        break;
      case State::OPEN_COMPLETE:
        result = DoOpenComplete(result);
        break;
      case State::SEEK:
        DCHECK_EQ(OK, result);
        result = DoSeek();
        break;
      case State::GET_FILE_INFO:
        result = DoGetFileInfo(result);
        break;
      case State::GET_FILE_INFO_COMPLETE:
        result = DoGetFileInfoComplete(result);
        break;

      case State::READ_COMPLETE:
        result = DoReadComplete(result);
        break;
    }
  }

  return result;
}

int UploadFileElementReader::DoOpen() {
  DCHECK(!file_stream_);

  next_state_ = State::OPEN_COMPLETE;

  file_stream_.reset(new FileStream(task_runner_.get()));
  int result = file_stream_->Open(
      path_,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC,
      base::BindOnce(&UploadFileElementReader::OnIOComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  DCHECK_GT(0, result);
  return result;
}

int UploadFileElementReader::DoOpenComplete(int result) {
  if (result < 0) {
    DLOG(WARNING) << "Failed to open \"" << path_.value()
                  << "\" for reading: " << result;
    file_stream_.reset();
    return result;
  }

  if (range_offset_) {
    next_state_ = State::SEEK;
  } else {
    next_state_ = State::GET_FILE_INFO;
  }
  return net::OK;
}

int UploadFileElementReader::DoSeek() {
  next_state_ = State::GET_FILE_INFO;
  return file_stream_->Seek(
      range_offset_,
      base::BindOnce(
          [](base::WeakPtr<UploadFileElementReader> weak_this, int64_t result) {
            if (!weak_this)
              return;
            weak_this->OnIOComplete(result >= 0 ? OK
                                                : static_cast<int>(result));
          },
          weak_ptr_factory_.GetWeakPtr()));
}

int UploadFileElementReader::DoGetFileInfo(int result) {
  if (result < 0) {
    DLOG(WARNING) << "Failed to seek \"" << path_.value()
                  << "\" to offset: " << range_offset_ << " (" << result << ")";
    return result;
  }

  next_state_ = State::GET_FILE_INFO_COMPLETE;

  base::File::Info* owned_file_info = new base::File::Info;
  result = file_stream_->GetFileInfo(
      owned_file_info,
      base::BindOnce(
          [](base::WeakPtr<UploadFileElementReader> weak_this,
             base::File::Info* file_info, int result) {
            if (!weak_this)
              return;
            weak_this->file_info_ = *file_info;
            weak_this->OnIOComplete(result);
          },
          weak_ptr_factory_.GetWeakPtr(), base::Owned(owned_file_info)));
  // GetFileInfo() can't succeed synchronously.
  DCHECK_NE(result, OK);
  return result;
}

int UploadFileElementReader::DoGetFileInfoComplete(int result) {
  if (result != OK) {
    DLOG(WARNING) << "Failed to get file info of \"" << path_.value() << "\"";
    return result;
  }

  int64_t length = file_info_.size;
  if (range_offset_ < static_cast<uint64_t>(length)) {
    // Compensate for the offset.
    length = std::min(length - range_offset_, range_length_);
  }

  // If the underlying file has been changed and the expected file modification
  // time is set, treat it as error. Note that |expected_modification_time_| may
  // have gone through multiple conversion steps involving loss of precision
  // (including conversion to time_t). Therefore the check below only verifies
  // that the timestamps are within one second of each other. This check is used
  // for sliced files.
  if (!expected_modification_time_.is_null() &&
      (expected_modification_time_ - file_info_.last_modified)
              .magnitude()
              .InSeconds() != 0) {
    return ERR_UPLOAD_FILE_CHANGED;
  }

  content_length_ = length;
  bytes_remaining_ = GetContentLength();
  return result;
}

int UploadFileElementReader::DoReadComplete(int result) {
  if (result == 0)  // Reached end-of-file earlier than expected.
    return ERR_UPLOAD_FILE_CHANGED;

  if (result > 0) {
    DCHECK_GE(bytes_remaining_, static_cast<uint64_t>(result));
    bytes_remaining_ -= result;
  }

  return result;
}

void UploadFileElementReader::OnIOComplete(int result) {
  DCHECK(pending_callback_);

  result = DoLoop(result);

  if (result != ERR_IO_PENDING)
    std::move(pending_callback_).Run(result);
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
    ScopedOverridingContentLengthForTests(uint64_t value) {
  overriding_content_length = value;
}

UploadFileElementReader::ScopedOverridingContentLengthForTests::
~ScopedOverridingContentLengthForTests() {
  overriding_content_length = 0;
}

}  // namespace net
