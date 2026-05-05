// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/jitter_buffer.h"

#include <algorithm>
#include <bit>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "remoting/base/logging.h"

namespace remoting {

JitterBuffer::JitterBuffer(const Config& config)
    : config_(config), mask_(config.capacity - 1), buffer_(config.capacity) {
  CHECK_GT(config.capacity, 0u);
  CHECK(std::has_single_bit(config.capacity))
      << "Capacity must be a power of two.";
  CHECK_GT(config.frame_size, 0u);
  CHECK_EQ(config.capacity % config.frame_size, 0u)
      << "Capacity must be a multiple of frame_size.";
  CHECK_EQ(config.minimum_threshold % config.frame_size, 0u)
      << "Minimum threshold must be aligned to frame_size.";
  if (config.max_latency_bytes > 0) {
    CHECK_GT(config.max_latency_bytes, config.minimum_threshold)
        << "Max latency must be greater than minimum threshold.";
  }

  DETACH_FROM_SEQUENCE(producer_sequence_checker_);
  DETACH_FROM_SEQUENCE(consumer_sequence_checker_);
}

JitterBuffer::~JitterBuffer() = default;

size_t JitterBuffer::Write(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(producer_sequence_checker_);
  DCHECK_EQ(data.size() % config_.frame_size, 0u);

  // Producer thread: load `read_index_` with acquire to see the latest consumer
  // state.
  size_t read_idx = read_index_.load(std::memory_order_acquire);
  size_t write_idx = write_index_.load(std::memory_order_relaxed);

  size_t buffered = write_idx - read_idx;
  size_t space = config_.capacity - buffered;

  size_t to_write = std::min(data.size(), space);

  if (to_write < data.size()) {
    LOG(WARNING) << "JitterBuffer overflow, dropping "
                 << (data.size() - to_write)
                 << " bytes. Buffered: " << buffered;
  }

  size_t first_part =
      std::min(to_write, config_.capacity - (write_idx & mask_));
  std::copy(data.begin(), data.begin() + first_part,
            buffer_.begin() + (write_idx & mask_));

  if (first_part < to_write) {
    std::copy(data.begin() + first_part, data.begin() + to_write,
              buffer_.begin());
  }

  // Producer thread: store `write_index_` with release to make data visible to
  // consumer.
  write_index_.store(write_idx + to_write, std::memory_order_release);

  return to_write;
}

size_t JitterBuffer::Read(base::span<uint8_t> destination) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  DCHECK_EQ(destination.size() % config_.frame_size, 0u);

  // Never use LOG in this method, since this is called on a real-time thread
  // and logging acquires locks.

  // Consumer thread: load `write_index_` with acquire to see the latest
  // producer state.
  size_t write_idx = write_index_.load(std::memory_order_acquire);
  size_t read_idx = read_index_.load(std::memory_order_relaxed);

  if (pending_clear_.load(std::memory_order_acquire)) {
    read_index_.store(write_idx, std::memory_order_release);
    state_ = State::kBuffering;
    starvation_bytes_ = 0;
    pending_clear_.store(false, std::memory_order_relaxed);
    return 0;
  }

  size_t buffered = write_idx - read_idx;

  if (state_ == State::kBuffering) {
    if (buffered >= config_.minimum_threshold) {
      state_ = State::kPlaying;
    } else {
      return 0;
    }
  }

  // Latency Recovery: If we've accumulated too much lag, skip ahead to the
  // threshold.
  if (config_.max_latency_bytes > 0 && buffered > config_.max_latency_bytes) {
    size_t skip_bytes = buffered - config_.minimum_threshold;
    read_idx += skip_bytes;
    buffered -= skip_bytes;
  }

  size_t to_read = std::min(destination.size(), buffered);

  if (to_read == destination.size()) {
    starvation_bytes_ = 0;
  } else {
    starvation_bytes_ += destination.size() - to_read;
    if (config_.max_starvation_bytes > 0 &&
        starvation_bytes_ >= config_.max_starvation_bytes) {
      state_ = State::kBuffering;
      starvation_bytes_ = 0;
    }
  }

  if (to_read == 0) {
    if (read_idx != read_index_.load(std::memory_order_relaxed)) {
      read_index_.store(read_idx, std::memory_order_release);
    }
    return 0;
  }

  size_t first_part = std::min(to_read, config_.capacity - (read_idx & mask_));
  std::copy(buffer_.begin() + (read_idx & mask_),
            buffer_.begin() + (read_idx & mask_) + first_part,
            destination.begin());

  if (first_part < to_read) {
    std::copy(buffer_.begin(), buffer_.begin() + to_read - first_part,
              destination.begin() + first_part);
  }

  // Consumer thread: store `read_index_` with release to signal to producer
  // that space is available.
  read_index_.store(read_idx + to_read, std::memory_order_release);

  return to_read;
}

void JitterBuffer::Clear() {
  pending_clear_.store(true, std::memory_order_release);
}

size_t JitterBuffer::GetBufferedBytes() const {
  if (pending_clear_.load(std::memory_order_relaxed)) {
    return 0;
  }
  // Use acquire to get a consistent snapshot. To avoid underflow, read_index_
  // should be loaded before write_index_.
  size_t read_idx = read_index_.load(std::memory_order_acquire);
  size_t write_idx = write_index_.load(std::memory_order_acquire);
  return std::min(write_idx - read_idx, config_.capacity);
}

}  // namespace remoting
