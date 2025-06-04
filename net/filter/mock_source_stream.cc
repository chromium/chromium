// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/filter/mock_source_stream.h"

#include <stdint.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"
#include "net/filter/source_stream_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MockSourceStream::MockSourceStream() : SourceStream(SourceStreamType::kNone) {}

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
  CHECK_GE(buffer_size, base::checked_cast<int>(r.data.size()));
  if (r.mode == ASYNC) {
    awaiting_completion_ = true;
    dest_buffer_ = dest_buffer;
    dest_buffer_size_ = base::checked_cast<size_t>(buffer_size);
    callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  results_.pop();
  dest_buffer->span().copy_prefix_from(r.data);
  return r.error == OK ? base::checked_cast<int>(r.data.size()) : r.error;
}

std::string MockSourceStream::Description() const {
  return "";
}

bool MockSourceStream::MayHaveMoreBytes() const {
  if (always_report_has_more_bytes_)
    return true;
  return !results_.empty();
}

MockSourceStream::QueuedResult::QueuedResult(base::span<const uint8_t> data,
                                             Error error,
                                             Mode mode)
    : data(data), error(error), mode(mode) {}

void MockSourceStream::AddReadResult(base::span<const uint8_t> data,
                                     Error error,
                                     Mode mode) {
  if (error != OK) {
    // Doesn't make any sense to have both an error and data.
    DCHECK_EQ(data.size(), 0u);
  } else {
    // The read result must be between 0 and 32k (inclusive) because the read
    // buffer used in FilterSourceStream is 32k.
    DCHECK_GE(32 * 1024u, data.size());
  }

  if (data.size() > 0 && read_one_byte_at_a_time_) {
    for (const uint8_t& byte : data) {
      results_.emplace(base::span_from_ref(byte), OK, mode);
    }
    return;
  }

  QueuedResult result(data, error, mode);
  results_.push(result);
}

void MockSourceStream::AddReadResult(std::string_view data,
                                     Error error,
                                     Mode mode) {
  AddReadResult(base::as_byte_span(data), error, mode);
}

void MockSourceStream::CompleteNextRead() {
  DCHECK(awaiting_completion_);

  awaiting_completion_ = false;
  QueuedResult r = results_.front();
  DCHECK_EQ(ASYNC, r.mode);
  results_.pop();
  DCHECK_GE(dest_buffer_size_, r.data.size());
  dest_buffer_->span().copy_prefix_from(r.data);
  dest_buffer_ = nullptr;
  std::move(callback_).Run(
      r.error == OK ? base::checked_cast<int>(r.data.size()) : r.error);
}

}  // namespace net
