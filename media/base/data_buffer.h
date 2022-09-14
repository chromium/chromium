// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include <stdint.h>

#include <memory>

#include "base/check_op.h"
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

  // Assumes valid data of size |buffer_size|.
  DataBuffer(std::unique_ptr<uint8_t[]> buffer, int buffer_size);

  DataBuffer(const DataBuffer&) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;

  // Create a DataBuffer whose |data_| is copied from |data|.
  //
  // |data| must not be null and |size| must be >= 0.
  static scoped_refptr<DataBuffer> CopyFrom(const uint8_t* data, int size);

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

  const uint8_t* data() const {
    DCHECK(!end_of_stream());
    return data_.get();
  }

  uint8_t* writable_data() {
    DCHECK(!end_of_stream());
    return data_.get();
  }

  // The size of valid data in bytes.
  //
  // Setting this value beyond the buffer size is disallowed.
  int data_size() const {
    DCHECK(!end_of_stream());
    return data_size_;
  }

  void set_data_size(int data_size) {
    DCHECK(!end_of_stream());
    CHECK_LE(data_size, buffer_size_);
    data_size_ = data_size;
  }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return data_ == NULL; }

 protected:
  friend class base::RefCountedThreadSafe<DataBuffer>;

  // Allocates buffer of size |data_size|, copies [data,data+data_size) to
  // the allocated buffer and sets data size to |data_size|.
  //
  // If |data| is null an end of stream buffer is created.
  DataBuffer(const uint8_t* data, int data_size);

  virtual ~DataBuffer();

 private:
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  std::unique_ptr<uint8_t[]> data_;
  int buffer_size_;
  int data_size_;
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
