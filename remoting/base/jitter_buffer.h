// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_JITTER_BUFFER_H_
#define REMOTING_BASE_JITTER_BUFFER_H_

#include <atomic>
#include <vector>

#include "base/containers/span.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

namespace remoting {

// A Single-Producer Single-Consumer (SPSC) lock-free jitter buffer for raw
// bytes. This buffer is designed to be used by a non-time-sensitive producer
// thread and a real-time consumer thread.
//
// All operations are thread-safe as long as there is only one producer and one
// consumer.
class JitterBuffer {
 public:
  struct Config {
    // Total buffer size in bytes. Must be a power of two.
    size_t capacity;

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

  explicit JitterBuffer(const Config& config);

  JitterBuffer(const JitterBuffer&) = delete;
  JitterBuffer& operator=(const JitterBuffer&) = delete;

  ~JitterBuffer();

  // Appends data to the buffer. Returns the number of bytes written.
  // If the buffer is full, it will write as much as possible and return.
  // This is called from the producer thread.
  size_t Write(base::span<const uint8_t> data);

  // Reads data from the buffer into `destination`. Returns the number of bytes
  // read. If the buffer has not reached the minimum threshold since the last
  // underrun, it returns 0.
  // This is called from the consumer thread.
  size_t Read(base::span<uint8_t> destination);

  // Resets the buffer to its initial state.
  // Safe to call from any thread.
  void Clear();

  // Returns the number of bytes currently buffered.
  size_t GetBufferedBytes() const;

 private:
  enum class State {
    // Buffer is accumulating data and will not return any from Read().
    kBuffering,
    // Buffer has reached the threshold and is outputting data.
    kPlaying,
  };

  const Config config_;

  // A bitmask used to wrap indices into `buffer_`. Equal to `config_.capacity -
  // 1`.
  const size_t mask_;

  std::vector<uint8_t> buffer_;

  std::atomic<size_t> read_index_{0};
  std::atomic<size_t> write_index_{0};

  std::atomic<bool> pending_clear_{false};

  State state_ GUARDED_BY_CONTEXT(consumer_sequence_checker_) =
      State::kBuffering;

  // Track starvation (underruns) while in the Playing state.
  size_t starvation_bytes_ GUARDED_BY_CONTEXT(consumer_sequence_checker_) = 0;

  SEQUENCE_CHECKER(producer_sequence_checker_);
  SEQUENCE_CHECKER(consumer_sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_BASE_JITTER_BUFFER_H_
