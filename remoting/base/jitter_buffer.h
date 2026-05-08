// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_JITTER_BUFFER_H_
#define REMOTING_BASE_JITTER_BUFFER_H_

#include <atomic>
#include <memory>

#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/base/fifo_buffer.h"

namespace remoting {

// A decorator for FifoBufferReader that adds jitter control (buffering
// thresholds, latency recovery, and starvation tracking) for raw audio bytes.
//
// Designed for Single-Producer Single-Consumer (SPSC) usage.
// All SPSC methods must be called from a single thread (lazily bound on the
// first call), but the instance can be safely constructed and destructed on a
// different sequence (such as the owner main thread) as long as there is no
// concurrent access.
class JitterBuffer : public FifoBufferReader {
 public:
  struct Config {
    // The size of a single frame (e.g., channels * bytes per sample).
    // All operations will be aligned to this size.
    size_t frame_size;

    // Max starvation to tolerate in the Playing state before switching back to
    // the Buffering state.
    size_t max_starvation_bytes;

    // Max buffer level to tolerate before skipping ahead to the threshold.
    size_t max_latency_bytes;

    // The minimum number of bytes that must be buffered before Read()
    // starts returning data.
    size_t minimum_threshold;
  };

  JitterBuffer(const Config& config,
               std::unique_ptr<FifoBufferReader> fifo_buffer_reader);

  JitterBuffer(const JitterBuffer&) = delete;
  JitterBuffer& operator=(const JitterBuffer&) = delete;

  ~JitterBuffer() override;

  // FifoBufferReader implementation.
  std::optional<size_t> Read(base::span<uint8_t> destination) override;
  std::optional<size_t> Skip(size_t bytes) override;
  void Clear() override;
  std::optional<size_t> GetBufferedBytes() const override;

 private:
  enum class State {
    // Buffer is accumulating data and will not return any from Read().
    kBuffering,
    // Buffer has reached the threshold and is outputting data.
    kPlaying,
  };

  const Config config_;
  std::unique_ptr<FifoBufferReader> fifo_buffer_reader_;

  State state_ GUARDED_BY_CONTEXT(consumer_sequence_checker_) =
      State::kBuffering;

  // Track starvation (underruns) while in the Playing state.
  size_t starvation_bytes_ GUARDED_BY_CONTEXT(consumer_sequence_checker_) = 0;

  SEQUENCE_CHECKER(consumer_sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_BASE_JITTER_BUFFER_H_
