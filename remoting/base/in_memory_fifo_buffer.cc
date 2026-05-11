// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/in_memory_fifo_buffer.h"

#include <algorithm>
#include <bit>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

namespace remoting {

// Shared SPSC lock-free in-memory FIFO buffer storage.
// Ref-counted to ensure memory safety between producer and consumer threads.
class InMemoryFifoBuffer
    : public base::RefCountedThreadSafe<InMemoryFifoBuffer> {
 public:
  // `capacity` must be a power of two.
  explicit InMemoryFifoBuffer(size_t capacity);

  InMemoryFifoBuffer(const InMemoryFifoBuffer&) = delete;
  InMemoryFifoBuffer& operator=(const InMemoryFifoBuffer&) = delete;

  FifoBufferWriter::Result Write(base::span<const uint8_t> data);
  std::optional<size_t> Read(base::span<uint8_t> destination);
  std::optional<size_t> Skip(size_t bytes);
  void Clear();
  std::optional<size_t> GetBufferedBytes() const;

 private:
  friend class base::RefCountedThreadSafe<InMemoryFifoBuffer>;
  ~InMemoryFifoBuffer();

  const size_t capacity_;
  // Bitmask used for fast modulo operations on circular indices (`capacity_ -
  // 1`).
  const size_t mask_;

  // Circular memory buffer storage. Size is `capacity_`.
  std::vector<uint8_t> buffer_;

  // Atomically shared indices.
  std::atomic<size_t> read_index_{0};
  std::atomic<size_t> write_index_{0};

  SEQUENCE_CHECKER(producer_sequence_checker_);
  SEQUENCE_CHECKER(consumer_sequence_checker_);
};

// =============================================================================
// InMemoryFifoBuffer (Shared Storage Implementation)
// =============================================================================

InMemoryFifoBuffer::InMemoryFifoBuffer(size_t capacity)
    : capacity_(capacity), mask_(capacity - 1), buffer_(capacity) {
  CHECK_GT(capacity, 0u);
  CHECK(std::has_single_bit(capacity)) << "Capacity must be a power of two.";

  DETACH_FROM_SEQUENCE(producer_sequence_checker_);
  DETACH_FROM_SEQUENCE(consumer_sequence_checker_);
}

InMemoryFifoBuffer::~InMemoryFifoBuffer() = default;

FifoBufferWriter::Result InMemoryFifoBuffer::Write(
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(producer_sequence_checker_);

  // Producer thread: load `read_index_` with acquire to see the latest consumer
  // state.
  size_t read_idx = read_index_.load(std::memory_order_acquire);
  size_t write_idx = write_index_.load(std::memory_order_relaxed);

  size_t buffered = write_idx - read_idx;
  size_t space = capacity_ - buffered;

  if (space < data.size()) {
    return FifoBufferWriter::Result::kFull;
  }

  size_t first_part = std::min(data.size(), capacity_ - (write_idx & mask_));
  std::copy(data.begin(), data.begin() + first_part,
            buffer_.begin() + (write_idx & mask_));

  if (first_part < data.size()) {
    std::copy(data.begin() + first_part, data.begin() + data.size(),
              buffer_.begin());
  }

  // Producer thread: store `write_index_` with release to make data visible to
  // consumer.
  write_index_.store(write_idx + data.size(), std::memory_order_release);
  return FifoBufferWriter::Result::kSuccess;
}

std::optional<size_t> InMemoryFifoBuffer::Read(
    base::span<uint8_t> destination) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);

  // Consumer thread: load `write_index_` with acquire to see the latest
  // producer state.
  size_t write_idx = write_index_.load(std::memory_order_acquire);
  size_t read_idx = read_index_.load(std::memory_order_relaxed);

  size_t buffered = write_idx - read_idx;
  size_t to_read = std::min(destination.size(), buffered);

  if (to_read == 0) {
    return 0;
  }

  size_t first_part = std::min(to_read, capacity_ - (read_idx & mask_));
  std::copy(buffer_.begin() + (read_idx & mask_),
            buffer_.begin() + (read_idx & mask_) + first_part,
            destination.begin());

  if (first_part < to_read) {
    std::copy(buffer_.begin(), buffer_.begin() + to_read - first_part,
              destination.begin() + first_part);
  }

  // Consumer thread: store `read_index_` with release to signal to producer.
  read_index_.store(read_idx + to_read, std::memory_order_release);
  return to_read;
}

std::optional<size_t> InMemoryFifoBuffer::Skip(size_t bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);

  size_t write_idx = write_index_.load(std::memory_order_acquire);
  size_t read_idx = read_index_.load(std::memory_order_relaxed);

  size_t buffered = write_idx - read_idx;
  size_t to_skip = std::min(bytes, buffered);

  if (to_skip == 0) {
    return 0;
  }

  read_index_.store(read_idx + to_skip, std::memory_order_release);
  return to_skip;
}

void InMemoryFifoBuffer::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  read_index_.store(write_index_.load(std::memory_order_acquire),
                    std::memory_order_release);
}

std::optional<size_t> InMemoryFifoBuffer::GetBufferedBytes() const {
  size_t read_idx = read_index_.load(std::memory_order_acquire);
  size_t write_idx = write_index_.load(std::memory_order_acquire);
  return std::min(write_idx - read_idx, capacity_);
}

// =============================================================================
// InMemoryFifoBufferWriter
// =============================================================================

InMemoryFifoBufferWriter::InMemoryFifoBufferWriter(
    scoped_refptr<InMemoryFifoBuffer> buffer)
    : buffer_(std::move(buffer)) {
  CHECK(buffer_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InMemoryFifoBufferWriter::~InMemoryFifoBufferWriter() = default;

FifoBufferWriter::Result InMemoryFifoBufferWriter::Write(
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_->Write(data);
}

// =============================================================================
// InMemoryFifoBufferReader
// =============================================================================

InMemoryFifoBufferReader::InMemoryFifoBufferReader(
    scoped_refptr<InMemoryFifoBuffer> buffer)
    : buffer_(std::move(buffer)) {
  CHECK(buffer_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InMemoryFifoBufferReader::~InMemoryFifoBufferReader() = default;

std::optional<size_t> InMemoryFifoBufferReader::Read(
    base::span<uint8_t> destination) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_->Read(destination);
}

std::optional<size_t> InMemoryFifoBufferReader::Skip(size_t bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_->Skip(bytes);
}

void InMemoryFifoBufferReader::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  buffer_->Clear();
}

std::optional<size_t> InMemoryFifoBufferReader::GetBufferedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_->GetBufferedBytes();
}

bool CreateInMemoryFifoBuffer(
    size_t capacity,
    std::unique_ptr<InMemoryFifoBufferWriter>& writer,
    std::unique_ptr<InMemoryFifoBufferReader>& reader) {
  auto buffer = base::MakeRefCounted<InMemoryFifoBuffer>(capacity);
  writer = std::make_unique<InMemoryFifoBufferWriter>(buffer);
  reader = std::make_unique<InMemoryFifoBufferReader>(buffer);
  return true;
}

}  // namespace remoting
