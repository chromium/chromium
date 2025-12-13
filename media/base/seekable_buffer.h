// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SeekableBuffer to support backward and forward seeking in a buffer for
// reading a media data source.
//
// In order to support backward and forward seeking, this class buffers data in
// both backward and forward directions, the current read position can be reset
// to anywhere in the buffered data.
//
// The amount of data buffered is regulated by two variables at construction,
// `backward_capacity` and `forward_capacity`.
//
// In the case of reading and seeking forward, the current read position
// advances and there will be more data in the backward direction. If backward
// bytes exceeds `backward_capacity`, the exceeding bytes are evicted and thus
// backward_bytes() will always be less than or equal to `backward_capacity`.
// The eviction will be caused by Read() and Seek() in the forward direction and
// is done internally when the mentioned criteria is fulfilled.
//
// In the case of appending data to the buffer, there is an advisory limit of
// how many bytes can be kept in the forward direction, regulated by
// `forward_capacity`. The append operation (by calling Append()) that caused
// forward bytes to exceed `forward_capacity` will have a return value that
// advises a halt of append operation, further append operations are allowed but
// are not advised. Since this class is used as a backend buffer for caching
// media files downloaded from network we cannot afford losing data, we can
// only advise a halt of further writing to this buffer.
// This class is not inherently thread-safe. Concurrent access must be
// externally serialized.

#ifndef MEDIA_BASE_SEEKABLE_BUFFER_H_
#define MEDIA_BASE_SEEKABLE_BUFFER_H_

#include <stdint.h>

#include <list>
#include <optional>

#include "base/containers/span_or_size.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class DataBuffer;

class MEDIA_EXPORT SeekableBuffer {
 public:
  // Constructs an instance with `forward_capacity` and `backward_capacity`.
  // The buffer capacity is measured in bytes.
  SeekableBuffer(size_t backward_capacity, size_t forward_capacity);

  SeekableBuffer(const SeekableBuffer&) = delete;
  SeekableBuffer& operator=(const SeekableBuffer&) = delete;

  ~SeekableBuffer();

  // Clears the buffer queue.
  void Clear();

  // Reads from the current read position into `data`, up to the size of `data`.
  // Returns the number of bytes read. The current read position will advance by
  // the amount of bytes read. If reading caused backward_bytes() to exceed
  // backward_capacity(), an eviction of the backward buffer will be done
  // internally.
  size_t Read(base::span<uint8_t> data);

  // Copies bytes from current position to `data` up to the size of `data`.
  // Returns number of bytes copied. Doesn't advance current position.
  // Optionally starts at a `forward_offset` from current position.
  size_t Peek(base::span<uint8_t> data, size_t forward_offset = 0);

  // Returns a view to the current subsection of data that is ready for
  // consumption in the current data buffer, or an empty span if there is no
  // data left at all in the buffer queue. The returned `data` value becomes
  // invalid when Read(), Append() or Seek() are called.
  base::span<const uint8_t> GetCurrentChunk() const;

  // Appends `buffer_in` to this buffer up to the capacity of this buffer. A
  // false return value means the user should stop appending more data to this
  // buffer. The data is added to the buffer in any case.
  bool Append(const scoped_refptr<DataBuffer>& buffer_in);

  // Appends `data` to the buffer. Result is the same as for Append(const
  // scoped_refptr<DataBuffer>&).
  bool Append(base::span<const uint8_t> data);

  // Moves the read position by `offset` bytes. If `offset` is positive, the
  // current read position is moved forward. If negative, the current read
  // position is moved backward. A zero `offset` value will keep the current
  // read position stationary.
  //
  // If `offset` exceeds bytes buffered in either direction, reported by
  // forward_bytes() when seeking forward and backward_bytes() when seeking
  // backward, the seek operation will fail and return value will be false.
  // If the seek operation fails, the current read position will not be updated.
  // If a forward seeking caused backward_bytes() to exceed backward_capacity(),
  // this method call will cause an eviction of the backward buffer.
  bool Seek(ptrdiff_t offset);

  // Returns the number of bytes buffered beyond the current read position.
  size_t forward_bytes() const { return forward_bytes_.ValueOrDie(); }

  // Returns the number of bytes buffered that precedes the current read
  // position.
  size_t backward_bytes() const { return backward_bytes_.ValueOrDie(); }

  // Sets the forward_capacity to `new_forward_capacity` bytes.
  void set_forward_capacity(size_t new_forward_capacity) {
    forward_capacity_ = new_forward_capacity;
  }

  // Sets the backward_capacity to `new_backward_capacity` bytes.
  void set_backward_capacity(size_t new_backward_capacity) {
    backward_capacity_ = new_backward_capacity;
  }

  // Returns the maximum number of bytes that should be kept in the forward
  // direction.
  size_t forward_capacity() const { return forward_capacity_; }

  // Returns the maximum number of bytes that should be kept in the backward
  // direction.
  size_t backward_capacity() const { return backward_capacity_; }

  // Returns the timestamp associated with the current read position. The value
  // is defined as the timestamp of the current buffer plus the fraction of
  // the buffer's duration corresponding to the current buffer offset.
  //
  // If timestamp for the current buffer is set to 0 or the data was added with
  // a call to `Append()`, then returns value that corresponds to the
  // last position in a buffer that had timestamp set.
  // kNoTimestamp is returned if no buffers we read from had timestamp set.
  base::TimeDelta current_time() const { return current_time_; }

 private:
  using BufferQueue = std::list<scoped_refptr<DataBuffer>>;

  // A helper method to evict buffers in the backward direction until backward
  // bytes is within the backward capacity.
  void EvictBackwardBuffers();

  // An internal method shared by Read() and SeekForward() that actually does
  // reading. It reads a maximum of `num_bytes` bytes into `data`. Returns the
  // number of bytes read. The current read position will be moved forward by
  // the number of bytes read. If `data` is NULL, only the current read position
  // will advance but no data will be copied.
  size_t InternalRead(base::SpanOrSize<uint8_t> data,
                      bool advance_position,
                      size_t forward_offset);

  // A helper method that moves the current read position forward by `num_bytes`
  // bytes.
  // If the return value is true, the operation completed successfully.
  // If the return value is false, `num_bytes` is greater than forward_bytes()
  // and the seek operation failed. The current read position is not updated.
  bool SeekForward(size_t num_bytes);

  // A helper method that moves the current read position backward by
  // `num_bytes` bytes. If the return value is true, the operation completed
  // successfully. If the return value is false, `num_bytes` is greater than
  // backward_bytes() and the seek operation failed. The current read position
  // is not updated.
  bool SeekBackward(size_t num_bytes);

  // Updates `current_time_` with the time that corresponds to the
  // specified position in the buffer.
  void UpdateCurrentTime(BufferQueue::iterator buffer, size_t offset);

  BufferQueue buffers_;
  BufferQueue::iterator current_buffer_;
  size_t current_buffer_offset_ = 0;

  size_t backward_capacity_;
  base::CheckedNumeric<size_t> backward_bytes_ = 0;

  size_t forward_capacity_;
  base::CheckedNumeric<size_t> forward_bytes_ = 0;

  // Keeps track of the most recent time we've seen in case the `buffers_` is
  // empty when our owner asks what time it is.
  base::TimeDelta current_time_;
};

}  // namespace media

#endif  // MEDIA_BASE_SEEKABLE_BUFFER_H_
