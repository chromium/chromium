// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_file_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace net {

namespace testing {

MockFileStream::MockFileStream(
    const scoped_refptr<base::TaskRunner>& task_runner)
    : FileStream(task_runner),
      forced_error_(OK),
      async_error_(false),
      throttled_(false) {}

MockFileStream::MockFileStream(
    base::File file,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : FileStream(std::move(file), task_runner),
      forced_error_(OK),
      async_error_(false),
      throttled_(false) {}

MockFileStream::~MockFileStream() = default;

int MockFileStream::Seek(int64_t offset, Int64CompletionOnceCallback callback) {
  Int64CompletionOnceCallback wrapped_callback =
      base::BindOnce(&MockFileStream::DoCallback64, weak_factory_.GetWeakPtr(),
                     std::move(callback));
  if (forced_error_ == OK)
    return FileStream::Seek(offset, std::move(wrapped_callback));
  return ErrorCallback64(std::move(wrapped_callback));
}

int MockFileStream::Read(IOBuffer* buf,
                         int buf_len,
                         CompletionOnceCallback callback) {
  CompletionOnceCallback wrapped_callback =
      base::BindOnce(&MockFileStream::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback));
  if (forced_error_ == OK)
    return FileStream::Read(buf, buf_len, std::move(wrapped_callback));
  return ErrorCallback(std::move(wrapped_callback));
}

int MockFileStream::Write(IOBuffer* buf,
                          int buf_len,
                          CompletionOnceCallback callback) {
  CompletionOnceCallback wrapped_callback =
      base::BindOnce(&MockFileStream::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback));
  if (forced_error_ == OK)
    return FileStream::Write(buf, buf_len, std::move(wrapped_callback));
  return ErrorCallback(std::move(wrapped_callback));
}

int MockFileStream::Flush(CompletionOnceCallback callback) {
  CompletionOnceCallback wrapped_callback =
      base::BindOnce(&MockFileStream::DoCallback, weak_factory_.GetWeakPtr(),
                     std::move(callback));
  if (forced_error_ == OK)
    return FileStream::Flush(std::move(wrapped_callback));
  return ErrorCallback(std::move(wrapped_callback));
}

void MockFileStream::ThrottleCallbacks() {
  CHECK(!throttled_);
  throttled_ = true;
}

void MockFileStream::ReleaseCallbacks() {
  CHECK(throttled_);
  throttled_ = false;

  if (!throttled_task_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(throttled_task_));
  }
}

void MockFileStream::DoCallback(CompletionOnceCallback callback, int result) {
  if (!throttled_) {
    std::move(callback).Run(result);
    return;
  }
  CHECK(throttled_task_.is_null());
  throttled_task_ = base::BindOnce(std::move(callback), result);
}

void MockFileStream::DoCallback64(Int64CompletionOnceCallback callback,
                                  int64_t result) {
  if (!throttled_) {
    std::move(callback).Run(result);
    return;
  }
  CHECK(throttled_task_.is_null());
  throttled_task_ = base::BindOnce(std::move(callback), result);
}

int MockFileStream::ErrorCallback(CompletionOnceCallback callback) {
  CHECK_NE(OK, forced_error_);
  if (async_error_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), forced_error_));
    clear_forced_error();
    return ERR_IO_PENDING;
  }
  int ret = forced_error_;
  clear_forced_error();
  return ret;
}

int64_t MockFileStream::ErrorCallback64(Int64CompletionOnceCallback callback) {
  CHECK_NE(OK, forced_error_);
  if (async_error_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), forced_error_));
    clear_forced_error();
    return ERR_IO_PENDING;
  }
  int64_t ret = forced_error_;
  clear_forced_error();
  return ret;
}

}  // namespace testing

}  // namespace net
