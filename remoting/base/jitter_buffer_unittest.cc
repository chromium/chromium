// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/jitter_buffer.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
constexpr size_t kCapacity = 1024;
constexpr size_t kFrameSize = 4;
}  // namespace

class JitterBufferTest : public testing::Test {
 public:
  JitterBufferTest()
      : buffer_({.capacity = kCapacity,
                 .frame_size = kFrameSize,
                 .max_starvation_bytes = 0,
                 .max_latency_bytes = 0,
                 .minimum_threshold = 0}) {}

 protected:
  JitterBuffer buffer_;
};

TEST_F(JitterBufferTest, BasicWriteRead) {
  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(buffer_.Write(data), 8u);
  EXPECT_EQ(buffer_.GetBufferedBytes(), 8u);

  std::vector<uint8_t> read_data(8);
  // Default threshold is 0, so it should play immediately.
  EXPECT_EQ(buffer_.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);
  EXPECT_EQ(buffer_.GetBufferedBytes(), 0u);
}

TEST_F(JitterBufferTest, Thresholding) {
  JitterBuffer buffer({.capacity = kCapacity,
                       .frame_size = kFrameSize,
                       .max_starvation_bytes = 0,
                       .max_latency_bytes = 0,
                       .minimum_threshold = 12});

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(buffer.Write(data), 8u);

  std::vector<uint8_t> read_data(8);
  // Below threshold.
  EXPECT_EQ(buffer.Read(read_data), 0u);

  EXPECT_EQ(buffer.Write(data), 8u);
  // Now 16 bytes, above threshold. Reads full 8 bytes.
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);

  // Now 8 bytes left. Still in Playing state.
  // Original test expected 0 because it was below threshold, but now we allow
  // partial reads or remaining data in Playing state.
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);

  // Empty, should stay in Playing state for a bit (lazy re-buffering).
  EXPECT_EQ(buffer.Read(read_data), 0u);

  EXPECT_EQ(buffer.Write(data), 8u);
  // Still in Playing state. Should read immediately even though it's below
  // the threshold (12).
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);
}

TEST_F(JitterBufferTest, PartialReadInPlayingState) {
  JitterBuffer buffer({.capacity = kCapacity,
                       .frame_size = kFrameSize,
                       .max_starvation_bytes = 0,
                       .max_latency_bytes = 0,
                       .minimum_threshold = 8});
  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  buffer.Write(data);

  std::vector<uint8_t> read_data(12);
  // Reached threshold. Reads 8 bytes.
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(std::vector<uint8_t>(read_data.begin(), read_data.begin() + 8),
            data);

  // Buffer is empty, but we stay in Playing state for a while (lazy
  // re-buffering).
  buffer.Write({9, 10, 11, 12});
  // Should read immediately even though it's below threshold (8).
  EXPECT_EQ(buffer.Read(read_data), 4u);
  EXPECT_EQ(std::vector<uint8_t>(read_data.begin(), read_data.begin() + 4),
            (std::vector<uint8_t>{9, 10, 11, 12}));
}

TEST_F(JitterBufferTest, LatencyRecovery) {
  constexpr size_t kLargeCapacity = 64 * 1024;
  JitterBuffer large_buffer({.capacity = kLargeCapacity,
                             .frame_size = 4,
                             .max_starvation_bytes = 0,
                             .max_latency_bytes = 28800,
                             .minimum_threshold = 4});

  // Write a lot of data to trigger recovery (> 28800 bytes).
  std::vector<uint8_t> large_data(30000, 0xFF);
  large_buffer.Write(large_data);

  std::vector<uint8_t> read_data(4);
  // Read should trigger recovery and skip ahead to threshold (4).
  EXPECT_EQ(large_buffer.Read(read_data), 4u);
  EXPECT_EQ(large_buffer.GetBufferedBytes(),
            0u);  // read 4, so 0 left (it skipped to threshold).
}

TEST_F(JitterBufferTest, WrapAround) {
  std::vector<uint8_t> data(kCapacity - 4, 0xAA);
  EXPECT_EQ(buffer_.Write(data), kCapacity - 4);

  std::vector<uint8_t> read_data(kCapacity - 4);
  EXPECT_EQ(buffer_.Read(read_data), kCapacity - 4);

  // Write again, should wrap.
  std::vector<uint8_t> wrap_data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(buffer_.Write(wrap_data), 8u);

  std::vector<uint8_t> wrap_read(8);
  EXPECT_EQ(buffer_.Read(wrap_read), 8u);
  EXPECT_EQ(wrap_read, wrap_data);
}

TEST_F(JitterBufferTest, FullBuffer) {
  std::vector<uint8_t> data(kCapacity, 0xBB);
  EXPECT_EQ(buffer_.Write(data), kCapacity);
  EXPECT_EQ(buffer_.GetBufferedBytes(), kCapacity);

  // Try to write more.
  std::vector<uint8_t> extra_data = {1, 2, 3, 4};
  EXPECT_EQ(buffer_.Write(extra_data), 0u);

  std::vector<uint8_t> read_data(kCapacity);
  EXPECT_EQ(buffer_.Read(read_data), kCapacity);
  EXPECT_EQ(read_data, data);
}

TEST_F(JitterBufferTest, Clear) {
  std::vector<uint8_t> data = {1, 2, 3, 4};
  buffer_.Write(data);
  EXPECT_EQ(buffer_.GetBufferedBytes(), 4u);

  buffer_.Clear();
  EXPECT_EQ(buffer_.GetBufferedBytes(), 0u);

  // Call Read() with an empty span to process the clear.
  buffer_.Read({});

  std::vector<uint8_t> read_data(4);
  EXPECT_EQ(buffer_.Read(read_data), 0u);
}

TEST_F(JitterBufferTest, ClearAdvancesReadIndex) {
  std::vector<uint8_t> data1 = {1, 2, 3, 4};
  buffer_.Write(data1);

  // Clear should set the pending flag.
  buffer_.Clear();
  EXPECT_EQ(buffer_.GetBufferedBytes(), 0u);

  // Call Read() with an empty span to process the clear.
  buffer_.Read({});

  // Next write should be fine.
  std::vector<uint8_t> data2 = {5, 6, 7, 8};
  buffer_.Write(data2);
  EXPECT_EQ(buffer_.GetBufferedBytes(), 4u);

  std::vector<uint8_t> read_data(4);
  EXPECT_EQ(buffer_.Read(read_data), 4u);
  EXPECT_EQ(read_data, data2);
}

TEST_F(JitterBufferTest, LazyRebuffering) {
  JitterBuffer buffer({.capacity = kCapacity,
                       .frame_size = 4,
                       .max_starvation_bytes = 100,
                       .max_latency_bytes = 0,
                       .minimum_threshold = 100});
  std::vector<uint8_t> data(100, 0xAA);
  buffer.Write(data);

  std::vector<uint8_t> read_data(100);
  // Reached threshold. Reads full 100 bytes.
  EXPECT_EQ(buffer.Read(read_data), 100u);

  // Buffer is empty. Should stay in Playing state for 100 bytes of silence.
  std::vector<uint8_t> silence_req(40);
  EXPECT_EQ(buffer.Read(silence_req), 0u);
  EXPECT_EQ(buffer.Read(silence_req), 0u);

  // Still in Playing state because we've only "read" 80 bytes of silence.
  // If we write a small amount of data now, it should be readable immediately
  // even though it's below the threshold (100).
  std::vector<uint8_t> small_data = {1, 2, 3, 4};
  buffer.Write(small_data);
  std::vector<uint8_t> small_read(4);
  EXPECT_EQ(buffer.Read(small_read), 4u);
  EXPECT_EQ(small_read, small_data);

  // Now starve it for real (> 100 bytes).
  EXPECT_EQ(buffer.Read(silence_req), 0u);
  EXPECT_EQ(buffer.Read(silence_req), 0u);
  EXPECT_EQ(buffer.Read(silence_req), 0u);  // Total 120 bytes silence.

  // Should now be in Buffering state.
  buffer.Write(small_data);
  EXPECT_EQ(buffer.Read(small_read), 0u);  // Waiting for threshold (100)
}

}  // namespace remoting
