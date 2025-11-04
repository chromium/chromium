// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/compound_buffer.h"

#include <algorithm>
#include <functional>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/io_buffer.h"

namespace remoting {

CompoundBuffer::DataChunk::DataChunk(scoped_refptr<net::IOBuffer> buffer,
                                     base::span<const uint8_t> data)
    : buffer(std::move(buffer)), data(data) {}

CompoundBuffer::DataChunk::DataChunk(const DataChunk& other) = default;

CompoundBuffer::DataChunk::~DataChunk() = default;

CompoundBuffer::CompoundBuffer() = default;

CompoundBuffer::~CompoundBuffer() = default;

void CompoundBuffer::Clear() {
  CHECK(!locked_);
  chunks_.clear();
  total_bytes_ = 0;
}

void CompoundBuffer::Append(scoped_refptr<net::IOBuffer> buffer,
                            base::span<const uint8_t> data) {
  // A weak check that the `data` is within `buffer`.
  DCHECK_GE(data.data(), buffer->bytes());
  DCHECK(!data.empty());

  CHECK(!locked_);

  chunks_.emplace_back(std::move(buffer), data);
  total_bytes_ += data.size();
}

void CompoundBuffer::Append(scoped_refptr<net::IOBuffer> buffer, size_t size) {
  base::span<const uint8_t> data = buffer->span();
  Append(std::move(buffer), data.first(size));
}

void CompoundBuffer::Append(const CompoundBuffer& buffer) {
  for (DataChunkList::const_iterator it = buffer.chunks_.begin();
       it != buffer.chunks_.end(); ++it) {
    Append(it->buffer, it->data);
  }
}

void CompoundBuffer::Prepend(scoped_refptr<net::IOBuffer> buffer,
                             base::span<const uint8_t> data) {
  // A weak check that the |start| is within |buffer|.
  DCHECK_GE(data.data(), buffer->bytes());
  DCHECK(!data.empty());

  CHECK(!locked_);

  chunks_.emplace_front(std::move(buffer), data);
  total_bytes_ += data.size();
}

void CompoundBuffer::Prepend(scoped_refptr<net::IOBuffer> buffer, size_t size) {
  base::span<const uint8_t> data = buffer->span();
  Prepend(std::move(buffer), data.first(size));
}

void CompoundBuffer::Prepend(const CompoundBuffer& buffer) {
  for (DataChunkList::const_iterator it = buffer.chunks_.begin();
       it != buffer.chunks_.end(); ++it) {
    Prepend(it->buffer, it->data);
  }
}

void CompoundBuffer::AppendCopyOf(base::span<const uint8_t> data) {
  const size_t size = data.size();
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  buffer->span().copy_from(data);
  Append(std::move(buffer), size);
}

void CompoundBuffer::PrependCopyOf(base::span<const uint8_t> data) {
  const size_t size = data.size();
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  buffer->span().copy_from(data);
  Prepend(std::move(buffer), size);
}

void CompoundBuffer::CropFront(size_t bytes) {
  CHECK(!locked_);

  if (total_bytes_ <= bytes) {
    Clear();
    return;
  }

  total_bytes_ -= bytes;
  while (!chunks_.empty() && chunks_.front().data.size() <= bytes) {
    bytes -= chunks_.front().data.size();
    chunks_.pop_front();
  }
  if (!chunks_.empty() && bytes > 0) {
    chunks_.front().data = chunks_.front().data.subspan(bytes);
    DCHECK(!chunks_.front().data.empty());
    bytes = 0;
  }
  DCHECK_EQ(bytes, 0u);
}

void CompoundBuffer::CropBack(size_t bytes) {
  CHECK(!locked_);

  if (total_bytes_ <= bytes) {
    Clear();
    return;
  }

  total_bytes_ -= bytes;
  while (!chunks_.empty() && chunks_.back().data.size() <= bytes) {
    bytes -= chunks_.back().data.size();
    chunks_.pop_back();
  }
  if (!chunks_.empty() && bytes > 0) {
    chunks_.back().data =
        chunks_.back().data.first(chunks_.back().data.size() - bytes);
    DCHECK(!chunks_.back().data.empty());
    bytes = 0;
  }
  DCHECK_EQ(bytes, 0u);
}

void CompoundBuffer::Lock() {
  locked_ = true;
}

scoped_refptr<net::IOBufferWithSize> CompoundBuffer::ToIOBufferWithSize()
    const {
  scoped_refptr<net::IOBufferWithSize> result =
      base::MakeRefCounted<net::IOBufferWithSize>(total_bytes_);
  CopyTo(result->span());
  return result;
}

void CompoundBuffer::CopyTo(base::span<uint8_t> data) const {
  for (DataChunkList::const_iterator it = chunks_.begin();
       it != chunks_.end() && !data.empty(); ++it) {
    size_t bytes_to_copy = std::min(data.size(), it->data.size());
    data.take_first(bytes_to_copy).copy_from(it->data.first(bytes_to_copy));
  }
}

void CompoundBuffer::CopyFrom(const CompoundBuffer& source,
                              size_t start,
                              size_t end) {
  // Check that `start` <= `end` <= `total_bytes_`.
  DCHECK_LE(start, end);
  DCHECK_LE(end, source.total_bytes());

  Clear();

  if (end == start) {
    return;
  }

  // Iterate over chunks in the |source| and add those that we need.
  size_t pos = 0;
  for (DataChunkList::const_iterator it = source.chunks_.begin();
       it != source.chunks_.end(); ++it) {
    // Add data from the current chunk only if it is in the specified interval.
    if (pos + it->data.size() > start && pos < end) {
      size_t relative_start = start > pos ? (start - pos) : 0;
      size_t relative_end = std::min(it->data.size(), end - pos);
      DCHECK_LT(relative_start, relative_end);
      DCHECK_LE(relative_end, it->data.size());
      Append(it->buffer.get(),
             it->data.subspan(relative_start, relative_end - relative_start));
    }

    pos += it->data.size();
    if (pos >= end) {
      // We've got all the data we need.
      break;
    }
  }

  DCHECK_EQ(total_bytes_, end - start);
}

CompoundBufferInputStream::CompoundBufferInputStream(
    const CompoundBuffer* buffer)
    : buffer_(buffer) {
  DCHECK(buffer_->locked());
}

CompoundBufferInputStream::~CompoundBufferInputStream() = default;

bool CompoundBufferInputStream::Next(const void** data, int* size) {
  if (current_chunk_ < buffer_->chunks_.size()) {
    // Reply with the number of bytes remaining in the current buffer.
    const CompoundBuffer::DataChunk& chunk = buffer_->chunks_[current_chunk_];
    int read_size = chunk.data.size() - current_chunk_position_;
    *data = chunk.data.subspan(current_chunk_position_).data();
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
  DCHECK_LE(base::checked_cast<size_t>(count), last_returned_size_);
  DCHECK_GT(current_chunk_, 0u);

  // Rewind one buffer and rewind data offset by |count| bytes.
  --current_chunk_;
  const CompoundBuffer::DataChunk& chunk = buffer_->chunks_[current_chunk_];
  current_chunk_position_ = chunk.data.size() - count;
  position_ -= count;

  // Prevent additional backups.
  last_returned_size_ = 0;
}

bool CompoundBufferInputStream::Skip(int count) {
  DCHECK_GE(count, 0);
  last_returned_size_ = 0;

  while (count > 0 && current_chunk_ < buffer_->chunks_.size()) {
    const CompoundBuffer::DataChunk& chunk = buffer_->chunks_[current_chunk_];
    size_t read = std::min(base::checked_cast<size_t>(count),
                           chunk.data.size() - current_chunk_position_);

    // Advance the current buffer offset and position.
    current_chunk_position_ += read;
    position_ += read;
    count -= read;

    // If the current buffer is fully read, then advance to the next buffer.
    if (current_chunk_position_ == chunk.data.size()) {
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
