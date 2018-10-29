// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/chunk_stream.h"

#include <array>
#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace {
typedef ChunkStream<10> TestChunkStream;

std::unique_ptr<TestChunkStream::ChunkData> CreateChunkData() {
  return std::make_unique<TestChunkStream::ChunkData>();
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

TEST(ChunkStreamTest, Read) {
  TestChunkStream stream;
  stream.set_eof_pos(25);
  constexpr unsigned char start_value = 33;
  unsigned char value = start_value;
  auto chunk_0 = CreateChunkData();
  for (auto& it : *chunk_0) {
    it = ++value;
  }
  auto chunk_1 = CreateChunkData();
  for (auto& it : *chunk_1) {
    it = ++value;
  }
  auto chunk_2 = CreateChunkData();
  for (auto& it : *chunk_2) {
    it = ++value;
  }
  stream.SetChunkData(0, std::move(chunk_0));
  stream.SetChunkData(2, std::move(chunk_2));
  stream.SetChunkData(1, std::move(chunk_1));

  std::array<unsigned char, 25> result_data;
  EXPECT_TRUE(stream.ReadData(gfx::Range(0, 25), result_data.data()));

  value = start_value;
  for (const auto& it : result_data) {
    EXPECT_EQ(++value, it);
  }
}
}  // namespace chrome_pdf
