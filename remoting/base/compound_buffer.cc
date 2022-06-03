// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/compound_buffer.h"

#include <algorithm>
#include <functional>

#include "base/check_op.h"
#include "net/base/io_buffer.h"

namespace remoting {

CompoundBuffer::DataChunk::DataChunk(scoped_refptr<net::IOBuffer> buffer,
                                     const char* start,
                                     int size)
    : buffer(std::move(buffer)), start(start), size(size) {}

CompoundBuffer::DataChunk::DataChunk(const DataChunk& other) = default;

CompoundBuffer::DataChunk::~DataChunk() = default;

CompoundBuffer::CompoundBuffer()
    : total_bytes_(0),
      locked_(false) {
}

CompoundBuffer::~CompoundBuffer() = default;

void CompoundBuffer::Clear() {
  CHECK(!locked_);
  chunks_.clear();
  total_bytes_ = 0;
}

void CompoundBuffer::Append(scoped_refptr<net::IOBuffer> buffer,
                            const char* start,
                            int size) {
  // A weak check that the |start| is within |buffer|.
  DCHECK_GE(start, buffer->data());
  DCHECK_GT(size, 0);

  CHECK(!locked_);

  chunks_.emplace_back(std::move(buffer), start, size);
  total_bytes_ += size;
}

void CompoundBuffer::Append(scoped_refptr<net::IOBuffer> buffer, int size) {
  const char* start = buffer->data();
  Append(std::move(buffer), start, size);
}

void CompoundBuffer::Append(const CompoundBuffer& buffer) {
  for (DataChunkList::const_iterator it = buffer.chunks_.begin();
       it != buffer.chunks_.end(); ++it) {
    Append(it->buffer, it->start, it->size);
  }
}

void CompoundBuffer::Prepend(scoped_refptr<net::IOBuffer> buffer,
                             const char* start,
                             int size) {
  // A weak check that the |start| is within |buffer|.
  DCHECK_GE(start, buffer->data());
  DCHECK_GT(size, 0);

  CHECK(!locked_);

  chunks_.emplace_front(std::move(buffer), start, size);
  total_bytes_ += size;
}

void CompoundBuffer::Prepend(scoped_refptr<net::IOBuffer> buffer, int size) {
  const char* start = buffer->data();
  Prepend(std::move(buffer), start, size);
}

void CompoundBuffer::Prepend(const CompoundBuffer& buffer) {
  for (DataChunkList::const_iterator it = buffer.chunks_.begin();
       it != buffer.chunks_.end(); ++it) {
    Prepend(it->buffer, it->start, it->size);
  }
}
void CompoundBuffer::AppendCopyOf(const char* data, int size) {
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(size);
  memcpy(buffer->data(), data, size);
  Append(std::move(buffer), size);
}

void CompoundBuffer::PrependCopyOf(const char* data, int size) {
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(size);
  memcpy(buffer->data(), data, size);
  Prepend(std::move(buffer), size);
}

void CompoundBuffer::CropFront(int bytes) {
  CHECK(!locked_);

  if (total_bytes_ <= bytes) {
    Clear();
    return;
  }

  total_bytes_ -= bytes;
  while (!chunks_.empty() && chunks_.front().size <= bytes) {
    bytes -= chunks_.front().size;
    chunks_.pop_front();
  }
  if (!chunks_.empty() && bytes > 0) {
    chunks_.front().start += bytes;
    chunks_.front().size -= bytes;
    DCHECK_GT(chunks_.front().size, 0);
    bytes = 0;
  }
  DCHECK_EQ(bytes, 0);
}

void CompoundBuffer::CropBack(int bytes) {
  CHECK(!locked_);

  if (total_bytes_ <= bytes) {
    Clear();
    return;
  }

  total_bytes_ -= bytes;
  while (!chunks_.empty() && chunks_.back().size <= bytes) {
    bytes -= chunks_.back().size;
    chunks_.pop_back();
  }
  if (!chunks_.empty() && bytes > 0) {
    chunks_.back().size -= bytes;
    DCHECK_GT(chunks_.back().size, 0);
    bytes = 0;
  }
  DCHECK_EQ(bytes, 0);
}

void CompoundBuffer::Lock() {
  locked_ = true;
}

scoped_refptr<net::IOBufferWithSize> CompoundBuffer::ToIOBufferWithSize()
    const {
  scoped_refptr<net::IOBufferWithSize> result =
      base::MakeRefCounted<net::IOBufferWithSize>(total_bytes_);
  CopyTo(result->data(), total_bytes_);
  return result;
}

void CompoundBuffer::CopyTo(char* data, int size) const {
  int pos = 0;
  for (DataChunkList::const_iterator it = chunks_.begin();
       it != chunks_.end() && pos < size; ++it) {
    int bytes_to_copy = std::min(size - pos, it->size);
    memcpy(data + pos, it->start, bytes_to_copy);
    pos += bytes_to_copy;
  }
}

void CompoundBuffer::CopyFrom(const CompoundBuffer& source,
                              int start, int end) {
  // Check that 0 <= |start| <= |end| <= |total_bytes_|.
  DCHECK_LE(0, start);
  DCHECK_LE(start, end);
  DCHECK_LE(end, source.total_bytes());

  Clear();

  if (end == start) {
    return;
  }

  // Iterate over chunks in the |source| and add those that we need.
  int pos = 0;
  for (DataChunkList::const_iterator it = source.chunks_.begin();
       it != source.chunks_.end(); ++it) {

    // Add data from the current chunk only if it is in the specified interval.
    if (pos + it->size > start && pos < end) {
      int relative_start = std::max(0, start - pos);
      int relative_end = std::min(it->size, end - pos);
      DCHECK_LE(0, relative_start);
      DCHECK_LT(relative_start, relative_end);
      DCHECK_LE(relative_end, it->size);
      Append(it->buffer.get(), it->start + relative_start,
             relative_end - relative_start);
    }

    pos += it->size;
    if (pos >= end) {
      // We've got all the data we need.
      break;
    }
  }

  DCHECK_EQ(total_bytes_, end - start);
}

CompoundBufferInputStream::CompoundBufferInputStream(
    const CompoundBuffer* buffer)
    : buffer_(buffer),
      current_chunk_(0),
      current_chunk_position_(0),
      position_(0),
      last_returned_size_(0) {
  DCHECK(buffer_->locked());
}

CompoundBufferInputStream::~CompoundBufferInputStream() = default;

bool CompoundBufferInputStream::Next(const void** data, int* size) {
  if (current_chunk_ < buffer_->chunks_.size()) {
    // Reply with the number of bytes remaining in the current buffer.
    const CompoundBuffer::DataChunk& chunk = buffer_->chunks_[current_chunk_];
    int read_size = chunk.size - current_chunk_position_;
    *data = chunk.start + current_chunk_position_;
    *size = read_size;

    // Adjust position.
    ++current_chunk_;
    current_chunk_position_ = 0;
    position_ += read_size;

    last_returned_size_ = read_size;
    return true;
  }

  DCHECK_EQ(position_, buffer_->total_bytes());

  // We've reached the end of the stream. So reset |last_returned_size_|
  // to zero to prevent any backup request.
  // This is the same as in ArrayInputStream.
  // See google/protobuf/io/zero_copy_stream_impl_lite.cc.
  last_returned_size_ = 0;
  return false;
}

void CompoundBufferInputStream::BackUp(int count) {
  DCHECK_LE(count, last_returned_size_);
  DCHECK_GT(current_chunk_, 0u);

  // Rewind one buffer and rewind data offset by |count| bytes.
  --current_chunk_;
  const CompoundBuffer::DataChunk& chunk = buffer_->chunks_[current_chunk_];
  current_chunk_position_ = chunk.size - count;
  position_ -= count;
  DCHECK_GE(position_, 0);

  // Prevent additional backups.
  last_returned_size_ = 0;
}

bool CompoundBufferInputStream::Skip(int count) {
  DCHECK_GE(count, 0);
  last_returned_size_ = 0;

  while (count > 0 && current_chunk_ < buffer_->chunks_.size()) {
    const CompoundBuffer::DataChunk& chunk = buffer_->chunks_[current_chunk_];
    int read = std::min(count, chunk.size - current_chunk_position_);

    // Advance the current buffer offset and position.
    current_chunk_position_ += read;
    position_ += read;
    count -= read;

    // If the current buffer is fully read, then advance to the next buffer.
    if (current_chunk_position_ == chunk.size) {
      ++current_chunk_;
      current_chunk_position_ = 0;
    }
  }

  return count == 0;
}

int64_t CompoundBufferInputStream::ByteCount() const {
  return position_;
}

}  // namespace remoting
