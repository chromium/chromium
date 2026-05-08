// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/jitter_buffer.h"

#include <memory>
#include <optional>
#include <vector>

#include "remoting/base/fifo_buffer_test_base.h"
#include "remoting/base/in_memory_fifo_buffer.h"

namespace remoting {

namespace {
constexpr size_t kCapacity = 1024;
constexpr size_t kFrameSize = 4;
}  // namespace

class JitterBufferTestDelegate {
 public:
  JitterBufferTestDelegate() {
    std::unique_ptr<InMemoryFifoBufferReader> reader;
    CHECK(CreateInMemoryFifoBuffer(kCapacity, writer_, reader));
    jitter_buffer_ = std::make_unique<JitterBuffer>(
        JitterBuffer::Config{.frame_size = kFrameSize,
                             .max_starvation_bytes = 0,
                             .max_latency_bytes = 0,
                             .minimum_threshold = 0},
        std::move(reader));
  }

  FifoBufferWriter& GetWriter() { return *writer_; }
  FifoBufferReader& GetReader() { return *jitter_buffer_; }

 private:
  std::unique_ptr<InMemoryFifoBufferWriter> writer_;
  std::unique_ptr<JitterBuffer> jitter_buffer_;
};

using JitterBufferTestTypes = testing::Types<JitterBufferTestDelegate>;
INSTANTIATE_TYPED_TEST_SUITE_P(Jitter, FifoBufferTest, JitterBufferTestTypes);

class JitterBufferTest : public testing::Test {
 protected:
  JitterBufferTestDelegate delegate_;
};

TEST_F(JitterBufferTest, Thresholding) {
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(kCapacity, writer, reader));
  JitterBuffer buffer({.frame_size = kFrameSize,
                       .max_starvation_bytes = 0,
                       .max_latency_bytes = 0,
                       .minimum_threshold = 12},
                      std::move(reader));

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(writer->Write(data), WriteResult::kSuccess);

  std::vector<uint8_t> read_data(8);
  // Below threshold.
  EXPECT_EQ(buffer.Read(read_data), 0u);

  EXPECT_EQ(writer->Write(data), WriteResult::kSuccess);
  // Now 16 bytes, above threshold. Reads full 8 bytes.
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);

  // Now 8 bytes left. Still in Playing state.
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);

  // Empty, should stay in Playing state for a bit (lazy re-buffering).
  EXPECT_EQ(buffer.Read(read_data), 0u);

  EXPECT_EQ(writer->Write(data), WriteResult::kSuccess);
  // Still in Playing state. Should read immediately even though it's below
  // the threshold (12).
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);
}

TEST_F(JitterBufferTest, PartialReadInPlayingState) {
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(kCapacity, writer, reader));
  JitterBuffer buffer({.frame_size = kFrameSize,
                       .max_starvation_bytes = 0,
                       .max_latency_bytes = 0,
                       .minimum_threshold = 8},
                      std::move(reader));
  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(writer->Write(data), WriteResult::kSuccess);

  std::vector<uint8_t> read_data(12);
  // Reached threshold. Reads 8 bytes.
  EXPECT_EQ(buffer.Read(read_data), 8u);
  EXPECT_EQ(std::vector<uint8_t>(read_data.begin(), read_data.begin() + 8),
            data);

  // Buffer is empty, but we stay in Playing state for a while (lazy
  // re-buffering).
  EXPECT_EQ(writer->Write({9, 10, 11, 12}), WriteResult::kSuccess);
  // Should read immediately even though it's below threshold (8).
  EXPECT_EQ(buffer.Read(read_data), 4u);
  EXPECT_EQ(std::vector<uint8_t>(read_data.begin(), read_data.begin() + 4),
            (std::vector<uint8_t>{9, 10, 11, 12}));
}

TEST_F(JitterBufferTest, LatencyRecovery) {
  constexpr size_t kLargeCapacity = 64 * 1024;
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(kLargeCapacity, writer, reader));
  JitterBuffer large_buffer({.frame_size = 4,
                             .max_starvation_bytes = 0,
                             .max_latency_bytes = 28800,
                             .minimum_threshold = 4},
                            std::move(reader));

  // Write a lot of data to trigger recovery (> 28800 bytes).
  std::vector<uint8_t> large_data(30000, 0xFF);
  EXPECT_EQ(writer->Write(large_data), WriteResult::kSuccess);

  std::vector<uint8_t> read_data(4);
  // Read should trigger recovery and skip ahead to threshold (4).
  EXPECT_EQ(large_buffer.Read(read_data), 4u);
  EXPECT_EQ(large_buffer.GetBufferedBytes(), 0u);  // read 4, so 0 left.
}

TEST_F(JitterBufferTest, ClearAdvancesReadIndex) {
  std::vector<uint8_t> data1 = {1, 2, 3, 4};
  EXPECT_EQ(delegate_.GetWriter().Write(data1), WriteResult::kSuccess);

  // Clear the buffer immediately.
  delegate_.GetReader().Clear();
  EXPECT_EQ(delegate_.GetReader().GetBufferedBytes(), 0u);

  // Next write should be fine.
  std::vector<uint8_t> data2 = {5, 6, 7, 8};
  EXPECT_EQ(delegate_.GetWriter().Write(data2), WriteResult::kSuccess);
  EXPECT_EQ(delegate_.GetReader().GetBufferedBytes(), 4u);

  std::vector<uint8_t> read_data(4);
  EXPECT_EQ(delegate_.GetReader().Read(read_data), 4u);
  EXPECT_EQ(read_data, data2);
}

TEST_F(JitterBufferTest, LazyRebuffering) {
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(kCapacity, writer, reader));
  JitterBuffer buffer({.frame_size = 4,
                       .max_starvation_bytes = 100,
                       .max_latency_bytes = 0,
                       .minimum_threshold = 100},
                      std::move(reader));
  std::vector<uint8_t> data(100, 0xAA);
  EXPECT_EQ(writer->Write(data), WriteResult::kSuccess);

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
  EXPECT_EQ(writer->Write(small_data), WriteResult::kSuccess);
  std::vector<uint8_t> small_read(4);
  EXPECT_EQ(buffer.Read(small_read), 4u);
  EXPECT_EQ(small_read, small_data);

  // Now starve it for real (> 100 bytes).
  EXPECT_EQ(buffer.Read(silence_req), 0u);
  EXPECT_EQ(buffer.Read(silence_req), 0u);
  EXPECT_EQ(buffer.Read(silence_req), 0u);  // Total 120 bytes silence.

  // Should now be in Buffering state.
  EXPECT_EQ(writer->Write(small_data), WriteResult::kSuccess);
  EXPECT_EQ(buffer.Read(small_read), 0u);  // Waiting for threshold (100)
}

}  // namespace remoting
