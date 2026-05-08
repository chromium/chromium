// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/jitter_buffer.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

namespace remoting {

JitterBuffer::JitterBuffer(const Config& config,
                           std::unique_ptr<FifoBufferReader> fifo_buffer_reader)
    : config_(config), fifo_buffer_reader_(std::move(fifo_buffer_reader)) {
  CHECK(fifo_buffer_reader_);
  CHECK_GT(config.frame_size, 0u);
  CHECK_EQ(config.minimum_threshold % config.frame_size, 0u)
      << "Minimum threshold must be aligned to frame_size.";
  if (config.max_latency_bytes > 0) {
    CHECK_GT(config.max_latency_bytes, config.minimum_threshold)
        << "Max latency must be greater than minimum threshold.";
  }

  DETACH_FROM_SEQUENCE(consumer_sequence_checker_);
}

JitterBuffer::~JitterBuffer() = default;

std::optional<size_t> JitterBuffer::Read(base::span<uint8_t> destination) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  DCHECK_EQ(destination.size() % config_.frame_size, 0u);

  // Never use LOG in this method, since this is called on a real-time thread
  // and logging acquires locks.

  std::optional<size_t> buffered_opt = fifo_buffer_reader_->GetBufferedBytes();
  if (!buffered_opt.has_value()) {
    return std::nullopt;
  }
  size_t buffered = *buffered_opt;

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
    skip_bytes -= skip_bytes % config_.frame_size;  // Align to frame size
    fifo_buffer_reader_->Skip(skip_bytes);
    buffered -= skip_bytes;
  }

  size_t to_read = std::min(destination.size(), buffered);
  to_read -= to_read % config_.frame_size;  // Align to frame size

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
    return 0;
  }

  return fifo_buffer_reader_->Read(destination.first(to_read));
}

std::optional<size_t> JitterBuffer::Skip(size_t bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  DCHECK_EQ(bytes % config_.frame_size, 0u);
  return fifo_buffer_reader_->Skip(bytes);
}

void JitterBuffer::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(consumer_sequence_checker_);
  state_ = State::kBuffering;
  starvation_bytes_ = 0;
  fifo_buffer_reader_->Clear();
}

std::optional<size_t> JitterBuffer::GetBufferedBytes() const {
  return fifo_buffer_reader_->GetBufferedBytes();
}

}  // namespace remoting
