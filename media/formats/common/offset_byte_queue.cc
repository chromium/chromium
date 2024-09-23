// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/common/offset_byte_queue.h"

#include "base/check.h"
#include "base/logging.h"

namespace media {

OffsetByteQueue::OffsetByteQueue() : buf_(nullptr), size_(0), head_(0) {}
OffsetByteQueue::~OffsetByteQueue() = default;

void OffsetByteQueue::Reset() {
  queue_.Reset();
  buf_ = nullptr;
  size_ = 0;
  head_ = 0;
}

bool OffsetByteQueue::Push(base::span<const uint8_t> buf) {
  if (!queue_.Push(buf)) {
    DVLOG(4) << "Failed to push buf of size " << buf.size();
    Sync();
    return false;
  }
  Sync();
  DVLOG(4) << "Buffer pushed. head=" << head() << " tail=" << tail();
  return true;
}

void OffsetByteQueue::Peek(const uint8_t** buf, int* size) {
  *buf = size_ > 0 ? buf_ : nullptr;
  *size = size_;
}

void OffsetByteQueue::Pop(int count) {
  queue_.Pop(count);
  head_ += count;
  Sync();
}

void OffsetByteQueue::PeekAt(int64_t offset, const uint8_t** buf, int* size) {
  DCHECK(offset >= head());
  if (offset < head() || offset >= tail()) {
    *buf = nullptr;
    *size = 0;
    return;
  }
  *buf = &buf_[offset - head()];
  *size = tail() - offset;
}

bool OffsetByteQueue::Trim(int64_t max_offset) {
  if (max_offset < head_) return true;
  if (max_offset > tail()) {
    Pop(size_);
    return false;
  }
  Pop(max_offset - head_);
  return true;
}

void OffsetByteQueue::Sync() {
  queue_.Peek(&buf_, &size_);
}

}  // namespace media
