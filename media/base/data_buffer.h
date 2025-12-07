// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "media/base/media_export.h"

namespace media {

// A simple buffer that takes ownership of the given data pointer or allocates
// as necessary. The capacity of the buffer is constant and set at construction,
// and its size may fluctuate as data is written to the buffer.
//
// Unlike DecoderBuffer, allocations are assumed to be allocated with the
// default memory allocator (i.e., new uint8_t[]).
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DataBuffer : public base::RefCountedThreadSafe<DataBuffer> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // Constructs an empty buffer with max `capacity`, uninitialized `data_` and
  // a size of zero.
  explicit DataBuffer(size_t capacity);

  // Constructs a filled buffer by moving `buffer` into the `data_` property,
  // with a size of `buffer.size()`.
  explicit DataBuffer(base::HeapArray<uint8_t> buffer);

  // Allocates a buffer with a copy of `data` in it
  DataBuffer(base::PassKey<DataBuffer>, base::span<const uint8_t> data);

  enum class DataBufferType { kNormal, kEndOfStream };
  DataBuffer(base::PassKey<DataBuffer>, DataBufferType data_buffer_type);

  DataBuffer(DataBuffer&&) = delete;
  DataBuffer(const DataBuffer&) = delete;
  DataBuffer& operator=(DataBuffer&&) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;

  // Create a DataBuffer whose |data_| is copied from |data|.
  static scoped_refptr<DataBuffer> CopyFrom(base::span<const uint8_t> data);

  // Create a DataBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DataBuffer> CreateEOSBuffer();

  // Convenience method for initializing `data_` to zero. By default, data_
  // is constructed with its value uninitialized.
  void FillWithZeroes() {
    CHECK(!end_of_stream());
    std::fill(data_.begin(), data_.end(), 0);
  }

  base::TimeDelta timestamp() const {
    CHECK(!end_of_stream());
    return timestamp_;
  }

  void set_timestamp(const base::TimeDelta& timestamp) {
    CHECK(!end_of_stream());
    timestamp_ = timestamp;
  }

  base::TimeDelta duration() const {
    CHECK(!end_of_stream());
    return duration_;
  }

  void set_duration(const base::TimeDelta& duration) {
    CHECK(!end_of_stream());
    duration_ = duration;
  }

  // The capacity of the buffer, set at construction.
  size_t capacity() const {
    CHECK(!end_of_stream());
    return data_.size();
  }

  // Adds the elements from data to the beginning of the free space within the
  // buffer, and updates `size_`. Caller is responsible for ensuring there is
  // enough space for this method to succeed.
  void Append(base::span<const uint8_t> data);

  // Returns a span over `data_` that is truncated by the valid size().
  base::span<const uint8_t> data() const {
    CHECK(!end_of_stream());
    return data_.first(size_);
  }

  // Returns a span over `data_`, including any initialized portion. Care should
  // be taken that uninitialized memory is written to before being accessed.
  base::span<uint8_t> writable_data() {
    CHECK(!end_of_stream());
    return data_;
  }

  // The size of valid data in bytes.
  //
  // Setting this value beyond the buffer capacity is disallowed.
  size_t size() const {
    CHECK(!end_of_stream());
    return size_;
  }

  void set_size(size_t size) {
    CHECK(!end_of_stream());
    CHECK_LE(size, data_.size());
    size_ = size;
  }

  bool end_of_stream() const { return is_end_of_stream_; }

 private:
  friend class base::RefCountedThreadSafe<DataBuffer>;
  virtual ~DataBuffer();

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  base::HeapArray<uint8_t> data_;
  size_t size_ = 0;
  const bool is_end_of_stream_ = false;
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
