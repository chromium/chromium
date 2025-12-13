// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/offset_byte_queue.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace media {

OffsetByteQueue::OffsetByteQueue() = default;
OffsetByteQueue::~OffsetByteQueue() = default;

void OffsetByteQueue::Reset() {
  queue_.Reset();
  head_ = 0;
}

bool OffsetByteQueue::Push(base::span<const uint8_t> buf) {
  if (!queue_.Push(buf)) {
    DVLOG(4) << "Failed to push buf of size " << buf.size();
    return false;
  }
  DVLOG(4) << "Buffer pushed. head=" << head() << " tail=" << tail();
  return true;
}

base::span<const uint8_t> OffsetByteQueue::Data() const {
  return queue_.Data().empty() ? base::span<const uint8_t>() : queue_.Data();
}

void OffsetByteQueue::Pop(int count) {
  queue_.Pop(count);
  head_ += count;
}

base::span<const uint8_t> OffsetByteQueue::DataAt(int64_t offset) {
  if (offset < head() || offset >= tail()) {
    return {};
  }
  auto offset_span =
      queue_.Data().subspan(base::checked_cast<size_t>(offset - head()));
  return offset_span.empty() ? base::span<const uint8_t>() : offset_span;
}

bool OffsetByteQueue::Trim(int64_t max_offset) {
  if (max_offset < head_) return true;
  if (max_offset > tail()) {
    Pop(queue_.Data().size());
    return false;
  }
  Pop(max_offset - head_);
  return true;
}

}  // namespace media
