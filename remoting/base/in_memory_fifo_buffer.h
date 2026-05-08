// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_IN_MEMORY_FIFO_BUFFER_H_
#define REMOTING_BASE_IN_MEMORY_FIFO_BUFFER_H_

#include <atomic>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/base/fifo_buffer.h"

namespace remoting {

class InMemoryFifoBuffer;
class InMemoryFifoBufferWriter;
class InMemoryFifoBufferReader;

// Creates an in-memory SPSC lock-free FIFO buffer pair with the specified
// `capacity`. `capacity` must be a power of two. If successful, populates
// `writer` and `reader` and returns true.
bool CreateInMemoryFifoBuffer(
    size_t capacity,
    std::unique_ptr<InMemoryFifoBufferWriter>& writer,
    std::unique_ptr<InMemoryFifoBufferReader>& reader);

// Concrete implementation of FifoBufferWriter backed by InMemoryFifoBuffer.
// All SPSC methods must be called from a single thread (lazily bound on the
// first call), but the instance can be safely constructed and destructed on a
// different sequence (such as the owner main thread) as long as there is no
// concurrent access.
class InMemoryFifoBufferWriter : public FifoBufferWriter {
 public:
  explicit InMemoryFifoBufferWriter(scoped_refptr<InMemoryFifoBuffer> buffer);

  InMemoryFifoBufferWriter(const InMemoryFifoBufferWriter&) = delete;
  InMemoryFifoBufferWriter& operator=(const InMemoryFifoBufferWriter&) = delete;

  ~InMemoryFifoBufferWriter() override;

  // FifoBufferWriter implementation.
  Result Write(base::span<const uint8_t> data) override;

 private:
  const scoped_refptr<InMemoryFifoBuffer> buffer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// Concrete implementation of FifoBufferReader backed by InMemoryFifoBuffer.
// All SPSC methods must be called from a single thread (lazily bound on the
// first call), but the instance can be safely constructed and destructed on a
// different sequence (such as the owner main thread) as long as there is no
// concurrent access.
class InMemoryFifoBufferReader : public FifoBufferReader {
 public:
  explicit InMemoryFifoBufferReader(scoped_refptr<InMemoryFifoBuffer> buffer);

  InMemoryFifoBufferReader(const InMemoryFifoBufferReader&) = delete;
  InMemoryFifoBufferReader& operator=(const InMemoryFifoBufferReader&) = delete;

  ~InMemoryFifoBufferReader() override;

  // FifoBufferReader implementation.
  std::optional<size_t> Read(base::span<uint8_t> destination) override;
  std::optional<size_t> Skip(size_t bytes) override;
  void Clear() override;
  std::optional<size_t> GetBufferedBytes() const override;

 private:
  const scoped_refptr<InMemoryFifoBuffer> buffer_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_BASE_IN_MEMORY_FIFO_BUFFER_H_
