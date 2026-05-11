// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FIFO_BUFFER_H_
#define REMOTING_BASE_FIFO_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <optional>

#include "base/containers/span.h"

namespace remoting {

// A generic interface for writing to a Single-Producer Single-Consumer (SPSC)
// FIFO buffer of raw bytes.
class FifoBufferWriter {
 public:
  enum class Result {
    kSuccess,  // The entire span was successfully written.
    kFull,     // Insufficient space; nothing was written (alignment preserved).
    kFailed,   // The buffer is closed/fatal; nothing was written.
  };

  virtual ~FifoBufferWriter() = default;

  // Appends data to the buffer.
  //
  // Enforces All-or-None semantics on overflow: if the buffer does not have
  // sufficient space for the entire `data` span, it must write 0 bytes and
  // return Result::kFull (preserving frame alignment for subsequent
  // writes). Partial writes are not allowed.
  virtual Result Write(base::span<const uint8_t> data) = 0;
};

// A generic interface for reading from a Single-Producer Single-Consumer (SPSC)
// FIFO buffer of raw bytes.
class FifoBufferReader {
 public:
  virtual ~FifoBufferReader() = default;

  // Reads data from the buffer into `destination`. Returns the number of bytes
  // read, or std::nullopt if the buffer is closed.
  virtual std::optional<size_t> Read(base::span<uint8_t> destination) = 0;

  // Discards up to `bytes` from the buffer. Returns the number of bytes
  // actually discarded, or std::nullopt if the buffer is closed.
  virtual std::optional<size_t> Skip(size_t bytes) = 0;

  // Resets the buffer to its initial state.
  virtual void Clear() = 0;

  // Returns the number of bytes currently buffered, or std::nullopt if the
  // buffer is closed.
  virtual std::optional<size_t> GetBufferedBytes() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_FIFO_BUFFER_H_
