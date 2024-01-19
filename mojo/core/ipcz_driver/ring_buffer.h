// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_RING_BUFFER_H_
#define MOJO_CORE_IPCZ_DRIVER_RING_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/shared_buffer_mapping.h"
#include "mojo/core/system_impl_export.h"

namespace mojo::core::ipcz_driver {

// RingBuffer implements a simple circular data buffer over a shared memory
// mapping. This class is not thread-safe and it makes no effort to synchronize
// access to the underlying buffer.
class MOJO_SYSTEM_IMPL_EXPORT RingBuffer {
 public:
  // A DirectWriter exposes the first contiguous span of available capacity
  // within a RingBuffer for direct writing.
  class DirectWriter {
   public:
    using Bytes = base::span<uint8_t>;

    // Constructs a new DirectWriter to write into `buffer`.
    explicit DirectWriter(RingBuffer& buffer)
        : buffer_(&buffer), bytes_(buffer_->GetAvailableCapacityView()) {}
    DirectWriter(const DirectWriter&) = delete;
    DirectWriter& operator=(const DirectWriter&) = delete;
    DirectWriter(DirectWriter&& other)
        : buffer_(std::exchange(other.buffer_, nullptr)),
          bytes_(other.bytes_) {}

    // The span of bytes available for writing. Any writes here are incomplete
    // until Commit() is called.
    Bytes bytes() const { return bytes_; }

    // Commits the first `n` bytes of `bytes()` into the buffer and invalidates
    // the DirectWriter. Returns true on success or false if `n` exceeds the
    // size of `bytes()`.
    bool Commit(size_t n) && {
      return n <= bytes_.size() && buffer_->ExtendDataRange(n);
    }

   private:
    raw_ptr<RingBuffer> buffer_;
    const Bytes bytes_;
  };

  // A DirectReader exposes the first contiguous span of data within a
  // RingBuffer for direct reading.
  class DirectReader {
   public:
    using Bytes = base::span<const uint8_t>;

    explicit DirectReader(RingBuffer& buffer)
        : buffer_(&buffer), bytes_(buffer_->GetReadableDataView()) {}
    DirectReader(const DirectReader&) = delete;
    DirectReader& operator=(const DirectReader&) = delete;
    DirectReader(DirectReader&& other)
        : buffer_(std::exchange(other.buffer_, nullptr)),
          bytes_(other.bytes_) {}

    // The span of bytes available for reading.
    Bytes bytes() const { return bytes_; }

    // Consumes `n` bytes from the front of `bytes()`, making that space
    // available for subsequent writes and invalidating the DirectReader.
    // Returns true on success or false if `n` exceeds the size of `bytes()`.
    bool Consume(size_t n) && {
      DCHECK(buffer_);
      return n <= bytes_.size() && buffer_->DiscardAll(n);
    }

   private:
    raw_ptr<RingBuffer> buffer_;
    const Bytes bytes_;
  };

  // Constructs a new empty RingBuffer backed by the entire region of `mapping`.
  explicit RingBuffer(scoped_refptr<SharedBufferMapping> mapping);
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
  ~RingBuffer();

  SharedBufferMapping& mapping() const { return *mapping_; }

  // The total capacity of this RingBuffer in bytes.
  size_t capacity() const { return mapping_->size(); }

  // The number of bytes of data currently in the RingBuffer.
  size_t data_size() const { return data_range_.size; }

  // The number of bytes of available capacity in the RingBuffer.
  size_t available_capacity() const { return capacity() - data_size(); }

  // Attempts to append `source` to the buffer. Returns the number of bytes
  // written, which may be less than the length of `source` if there was
  // insufficient capacity available.
  size_t Write(base::span<const uint8_t> source);

  // Like Write() but this only writes data if there's enough room for all of
  // `source`. Returns true if the write happened and false otherwise.
  bool WriteAll(base::span<const uint8_t> source);

  // Attempts to copy bytes from the front of the buffer and into `target`,
  // discarding them from the buffer. May consume less than the length of
  // `target` if the buffer doesn't have enough data to read. Returns the number
  // of bytes consumed.
  size_t Read(base::span<uint8_t> target);

  // Like Read() but this only reads data if there's enough to fill `target`.
  // Returns true if the read happened and false otherwise.
  bool ReadAll(base::span<uint8_t> target);

  // Same semantics as Read() but no data is discarded from the buffer.
  size_t Peek(base::span<uint8_t> target);

  // Like Peek() but this only reads data if there's enough to fill `target`.
  // Returns true if the read happened and false otherwise.
  bool PeekAll(base::span<uint8_t> target);

  // Attempts to discard `n` bytes from the front of the buffer. Returns the
  // number of bytes discarded, which may be smaller than `n` if there weren't
  // enough bytes in the buffer.
  size_t Discard(size_t n);

  // Like Discard() but this only discards data if there are `n` bytes of data
  // to discard. Returns true on success or false if there wasn't enough data.
  bool DiscardAll(size_t n);

  // Attempts to extend the range of readable data in this RingBuffer by `n`
  // bytes, implying that the data has already been populated within the buffer
  // immediately following any currently readable data. Returns true if
  // successful or false if `n` exceeds the available capacity of the buffer.
  bool ExtendDataRange(size_t n);

  // Serialize and deserialize the state of this RingBuffer.
  struct SerializedState {
    uint32_t offset = 0;
    uint32_t size = 0;
  };
  static_assert(sizeof(SerializedState) == 8, "Invalid SerializedState size");
  void Serialize(SerializedState& state);
  bool Deserialize(const SerializedState& state);

 private:
  // Range describes a range of bytes within the underlying physical buffer.
  // Ranges are circular. For a 16-byte buffer, a Range of 8 bytes at offset 13
  // refers to bytes 13-15 followed immediately by bytes 0-4:
  //
  //                           Range(13, 8)
  //                       end        start
  //                         v        v
  //  Buffer (16 bytes) [xxxxx........xxx]
  //                     ^^^^^        ^^^
  //                 bytes 0-4        bytes 13-15
  struct Range {
    // The buffer offset of the first byte in the range.
    size_t offset = 0;

    // The size of the range in bytes. Must be no larger than the buffer size.
    size_t size = 0;
  };

  // Returns one or two non-empty spans of data which correspond precisely to
  // the range of bytes within this buffer described by `range`.
  using SplitBytes = std::pair<base::span<uint8_t>, base::span<uint8_t>>;
  SplitBytes MapRange(const Range& range) const;

  // Returns the complement of `range` within the underlying buffer: that is
  // the range which includes only all bytes NOT in `range`.
  Range ComplementRange(const Range& range) const;

  // Returns the longest contiguous span of available capacity within the
  // buffer, starting from the first byte of available capacity.
  base::span<uint8_t> GetAvailableCapacityView() const;

  // Returns the longest contiguous span of readable data within the buffer,
  // starting from the first byte of readable data.
  base::span<const uint8_t> GetReadableDataView() const;

  // The memory mapping which backs this RingBuffer.
  const scoped_refptr<SharedBufferMapping> mapping_;

  // Tracks the range of bytes currently occupied by readable data.
  Range data_range_{.offset = 0, .size = 0};
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_RING_BUFFER_H_
