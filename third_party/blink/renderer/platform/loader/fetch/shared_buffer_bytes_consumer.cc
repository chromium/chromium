// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/shared_buffer_bytes_consumer.h"

#include <utility>

#include "base/not_fatal_until.h"

namespace blink {

SharedBufferBytesConsumer::SharedBufferBytesConsumer(
    scoped_refptr<const SharedBuffer> data)
    : data_(std::move(data)), iterator_(data_->begin()) {}

BytesConsumer::Result SharedBufferBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  buffer = {};
  if (iterator_ == data_->end())
    return Result::kDone;
  buffer = iterator_->subspan(bytes_read_in_chunk_);
  return Result::kOk;
}

BytesConsumer::Result SharedBufferBytesConsumer::EndRead(size_t read_size) {
  CHECK(iterator_ != data_->end(), base::NotFatalUntil::M130);
  DCHECK_LE(read_size + bytes_read_in_chunk_, iterator_->size());
  bytes_read_in_chunk_ += read_size;
  if (bytes_read_in_chunk_ == iterator_->size()) {
    bytes_read_in_chunk_ = 0;
    ++iterator_;
  }
  if (iterator_ == data_->end())
    return Result::kDone;
  return Result::kOk;
}

void SharedBufferBytesConsumer::Cancel() {
  iterator_ = data_->end();
  bytes_read_in_chunk_ = 0;
}

BytesConsumer::PublicState SharedBufferBytesConsumer::GetPublicState() const {
  if (iterator_ == data_->end())
    return PublicState::kClosed;
  return PublicState::kReadableOrWaiting;
}

String SharedBufferBytesConsumer::DebugName() const {
  return "SharedBufferBytesConsumer";
}

}  // namespace blink
