// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_COMMON_OFFSET_BYTE_QUEUE_H_
#define MEDIA_FORMATS_COMMON_OFFSET_BYTE_QUEUE_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"

namespace media {

// A wrapper around a ByteQueue which maintains a notion of a
// monotonically-increasing offset. All buffer access is done by passing these
// offsets into this class, going some way towards preventing the proliferation
// of many different meanings of "offset", "head", etc.
class MEDIA_EXPORT OffsetByteQueue {
 public:
  OffsetByteQueue();

  OffsetByteQueue(const OffsetByteQueue&) = delete;
  OffsetByteQueue& operator=(const OffsetByteQueue&) = delete;

  ~OffsetByteQueue();

  // These work like their underlying ByteQueue counterparts.
  void Reset();
  [[nodiscard]] bool Push(base::span<const uint8_t> buf);
  void Peek(const uint8_t** buf, int* size);
  void Pop(int count);

  // Sets |buf| to point at the first buffered byte corresponding to |offset|,
  // and |size| to the number of bytes available starting from that offset.
  //
  // It is an error if the offset is before the current head. It's not an error
  // if the current offset is beyond tail(), but you will of course get back
  // a null |buf| and a |size| of zero.
  void PeekAt(int64_t offset, const uint8_t** buf, int* size);

  // Marks the bytes up to (but not including) |max_offset| as ready for
  // deletion. This is relatively inexpensive, but will not necessarily reduce
  // the resident buffer size right away (or ever).
  //
  // Returns true if the full range of bytes were successfully trimmed,
  // including the case where |max_offset| is less than the current head.
  // Returns false if |max_offset| > tail() (although all bytes currently
  // buffered are still cleared).
  bool Trim(int64_t max_offset);

  // The head and tail positions, in terms of the file's absolute offsets.
  // tail() is an exclusive bound.
  int64_t head() { return head_; }
  int64_t tail() { return head_ + size_; }

 private:
  // Synchronize |buf_| and |size_| with |queue_|.
  void Sync();

  ByteQueue queue_;
  const uint8_t* buf_;
  int size_;
  int64_t head_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_OFFSET_BYTE_QUEUE_H_
