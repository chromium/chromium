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
#include "media/base/media_export.h"

namespace media {

// A simple buffer that takes ownership of the given data pointer or allocates
// as necessary.
//
// Unlike DecoderBuffer, allocations are assumed to be allocated with the
// default memory allocator (i.e., new uint8_t[]).
//
// NOTE: It is illegal to call any method when end_of_stream() is true.
class MEDIA_EXPORT DataBuffer : public base::RefCountedThreadSafe<DataBuffer> {
 public:
  // Allocates buffer of size |buffer_size| >= 0.
  explicit DataBuffer(int buffer_size);

  explicit DataBuffer(base::HeapArray<uint8_t> buffer);

  DataBuffer(const DataBuffer&) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;

  // Create a DataBuffer whose |data_| is copied from |data|.
  static scoped_refptr<DataBuffer> CopyFrom(base::span<const uint8_t> data);

  // Create a DataBuffer indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  static scoped_refptr<DataBuffer> CreateEOSBuffer();

  base::TimeDelta timestamp() const {
    DCHECK(!end_of_stream());
    return timestamp_;
  }

  void set_timestamp(const base::TimeDelta& timestamp) {
    DCHECK(!end_of_stream());
    timestamp_ = timestamp;
  }

  base::TimeDelta duration() const {
    DCHECK(!end_of_stream());
    return duration_;
  }

  void set_duration(const base::TimeDelta& duration) {
    DCHECK(!end_of_stream());
    duration_ = duration;
  }

  // TODO(b/335662415): Change data(), writable_data() so they always return a
  // span to remove no data_size() member.
  const uint8_t* data() const {
    DCHECK(!end_of_stream());
    return data_.data();
  }

  uint8_t* writable_data() {
    DCHECK(!end_of_stream());
    return data_.data();
  }

  // The size of valid data in bytes.
  //
  // Setting this value beyond the buffer size is disallowed.
  int data_size() const {
    DCHECK(!end_of_stream());
    return data_size_;
  }

  void set_data_size(size_t data_size) {
    DCHECK(!end_of_stream());
    CHECK_LE(data_size, data_.size());
    data_size_ = data_size;
  }

  bool end_of_stream() const { return is_end_of_stream_; }

 protected:
  friend class base::RefCountedThreadSafe<DataBuffer>;
  enum class DataBufferType { kNormal, kEndOfStream };

  // Allocates a buffer with a copy of |data| in it
  explicit DataBuffer(base::span<const uint8_t> data);

  explicit DataBuffer(DataBufferType data_buffer_type);

  virtual ~DataBuffer();

 private:
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  base::HeapArray<uint8_t> data_;
  int data_size_;
  const bool is_end_of_stream_ = false;
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
