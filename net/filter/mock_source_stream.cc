// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/filter/mock_source_stream.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MockSourceStream::MockSourceStream() : SourceStream(SourceStream::TYPE_NONE) {}

MockSourceStream::~MockSourceStream() {
  DCHECK(!awaiting_completion_);
  if (expect_all_input_consumed_) {
    // All data should have been consumed.
    EXPECT_TRUE(results_.empty());
  }
}

int MockSourceStream::Read(IOBuffer* dest_buffer,
                           int buffer_size,
                           CompletionOnceCallback callback) {
  DCHECK(!awaiting_completion_);
  DCHECK(!results_.empty());

  if (results_.empty())
    return ERR_UNEXPECTED;

  QueuedResult r = results_.front();
  DCHECK_GE(buffer_size, r.len);
  if (r.mode == ASYNC) {
    awaiting_completion_ = true;
    dest_buffer_ = dest_buffer;
    dest_buffer_size_ = buffer_size;
    callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  results_.pop();
  std::copy(r.data, r.data + r.len, dest_buffer->data());
  return r.error == OK ? r.len : r.error;
}

std::string MockSourceStream::Description() const {
  return "";
}

bool MockSourceStream::MayHaveMoreBytes() const {
  if (always_report_has_more_bytes_)
    return true;
  return !results_.empty();
}

MockSourceStream::QueuedResult::QueuedResult(const char* data,
                                             int len,
                                             Error error,
                                             Mode mode)
    : data(data), len(len), error(error), mode(mode) {}

void MockSourceStream::AddReadResult(const char* data,
                                     int len,
                                     Error error,
                                     Mode mode) {
  if (error != OK) {
    // Doesn't make any sense to have both an error and data.
    DCHECK_EQ(len, 0);
  } else {
    // The read result must be between 0 and 32k (inclusive) because the read
    // buffer used in FilterSourceStream is 32k.
    DCHECK_GE(32 * 1024, len);
    DCHECK_LE(0, len);
  }

  if (len > 0 && read_one_byte_at_a_time_) {
    for (int i = 0; i < len; ++i) {
      QueuedResult result(data + i, 1, OK, mode);
      results_.push(result);
    }
    return;
  }

  QueuedResult result(data, len, error, mode);
  results_.push(result);
}

void MockSourceStream::CompleteNextRead() {
  DCHECK(awaiting_completion_);

  awaiting_completion_ = false;
  QueuedResult r = results_.front();
  DCHECK_EQ(ASYNC, r.mode);
  results_.pop();
  DCHECK_GE(dest_buffer_size_, r.len);
  std::copy(r.data, r.data + r.len, dest_buffer_->data());
  dest_buffer_ = nullptr;
  std::move(callback_).Run(r.error == OK ? r.len : r.error);
}

}  // namespace net
