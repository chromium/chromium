// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/chunk_stream.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace {
using TestChunkStream = ChunkStream<10>;

std::unique_ptr<TestChunkStream::ChunkData> CreateChunkData() {
  return std::make_unique<TestChunkStream::ChunkData>();
}

// Creates a chunk and populates it with incrementing values starting from
// `start_value`.
std::unique_ptr<TestChunkStream::ChunkData> CreateAndFillChunkData(
    uint8_t start_value) {
  std::unique_ptr<TestChunkStream::ChunkData> chunk = CreateChunkData();
  for (uint8_t& byte : *chunk) {
    byte = start_value++;
  }
  return chunk;
}
}  // namespace

TEST(ChunkStreamTest, InRow) {
  TestChunkStream stream;
  EXPECT_FALSE(stream.IsComplete());
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(0, 10)));
  stream.SetChunkData(0, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 10)));
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(0, 20)));
  stream.SetChunkData(1, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 20)));
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(0, 30)));
  stream.SetChunkData(2, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 30)));
  stream.set_eof_pos(25);
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(0, 30)));
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 25)));
  EXPECT_TRUE(stream.IsComplete());
}

TEST(ChunkStreamTest, InBackRow) {
  TestChunkStream stream;
  stream.set_eof_pos(25);
  EXPECT_FALSE(stream.IsComplete());
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(20, 25)));
  stream.SetChunkData(2, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(20, 25)));
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(10, 20)));
  stream.SetChunkData(1, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(10, 20)));
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(0, 10)));
  stream.SetChunkData(0, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 10)));
  EXPECT_TRUE(stream.IsComplete());
}

TEST(ChunkStreamTest, FillGap) {
  TestChunkStream stream;
  stream.set_eof_pos(25);
  EXPECT_FALSE(stream.IsComplete());
  stream.SetChunkData(0, CreateChunkData());
  stream.SetChunkData(2, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 10)));
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(20, 25)));
  EXPECT_FALSE(stream.IsRangeAvailable(gfx::Range(0, 25)));
  stream.SetChunkData(1, CreateChunkData());
  EXPECT_TRUE(stream.IsRangeAvailable(gfx::Range(0, 25)));
  EXPECT_TRUE(stream.IsComplete());
}

TEST(ChunkStreamTest, ReadWithInsufficientBuffer) {
  TestChunkStream stream;
  stream.set_eof_pos(25);
  stream.SetChunkData(0, CreateChunkData());

  // Buffer is smaller than the requested range.
  std::array<uint8_t, 5> small_buffer;
  std::ranges::fill(small_buffer, 0xFF);
  EXPECT_FALSE(stream.ReadData(gfx::Range(0, 10), small_buffer));
  // Verify buffer wasn't modified.
  EXPECT_THAT(small_buffer, testing::Each(testing::Eq(0xFF)));
}

TEST(ChunkStreamTest, ReadEmptyRange) {
  TestChunkStream stream;
  std::array<uint8_t, 10> buffer;
  std::ranges::fill(buffer, 0xFF);
  // Empty range should succeed without requiring data.
  EXPECT_TRUE(stream.ReadData(gfx::Range(0, 0), buffer));
  // Verify buffer wasn't modified.
  EXPECT_THAT(buffer, testing::Each(testing::Eq(0xFF)));
}

TEST(ChunkStreamTest, Read) {
  TestChunkStream stream;
  stream.set_eof_pos(25);
  constexpr uint8_t kStartValue = 34;
  stream.SetChunkData(0, CreateAndFillChunkData(kStartValue));
  stream.SetChunkData(
      2, CreateAndFillChunkData(kStartValue + 2 * TestChunkStream::kChunkSize));
  stream.SetChunkData(
      1, CreateAndFillChunkData(kStartValue + TestChunkStream::kChunkSize));

  std::array<uint8_t, 25> result_data;
  EXPECT_TRUE(stream.ReadData(gfx::Range(0, 25), result_data));

  uint8_t value = kStartValue;
  for (uint8_t byte : result_data) {
    EXPECT_EQ(value++, byte);
  }
}
}  // namespace chrome_pdf
