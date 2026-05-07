// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FIFO_BUFFER_TEST_BASE_H_
#define REMOTING_BASE_FIFO_BUFFER_TEST_BASE_H_

#include <optional>
#include <vector>

#include "remoting/base/fifo_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// A template test fixture for FifoBuffer implementations.
// TDelegate must provide:
//   - FifoBufferWriter& GetWriter()
//   - FifoBufferReader& GetReader()
template <typename TDelegate>
class FifoBufferTest : public testing::Test {
 protected:
  TDelegate delegate_;
};

TYPED_TEST_SUITE_P(FifoBufferTest);

TYPED_TEST_P(FifoBufferTest, BasicWriteRead) {
  FifoBufferWriter& writer = this->delegate_.GetWriter();
  FifoBufferReader& reader = this->delegate_.GetReader();

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(writer.Write(data), WriteResult::kSuccess);
  EXPECT_EQ(reader.GetBufferedBytes(), 8u);

  std::vector<uint8_t> read_data(8);
  EXPECT_EQ(reader.Read(read_data), 8u);
  EXPECT_EQ(read_data, data);
  EXPECT_EQ(reader.GetBufferedBytes(), 0u);
}

TYPED_TEST_P(FifoBufferTest, Clear) {
  FifoBufferWriter& writer = this->delegate_.GetWriter();
  FifoBufferReader& reader = this->delegate_.GetReader();

  std::vector<uint8_t> data = {1, 2, 3, 4};
  writer.Write(data);
  EXPECT_EQ(reader.GetBufferedBytes(), 4u);

  reader.Clear();
  EXPECT_EQ(reader.GetBufferedBytes(), 0u);

  std::vector<uint8_t> read_data(4);
  EXPECT_EQ(reader.Read(read_data), 0u);
}

TYPED_TEST_P(FifoBufferTest, Skip) {
  FifoBufferWriter& writer = this->delegate_.GetWriter();
  FifoBufferReader& reader = this->delegate_.GetReader();

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  writer.Write(data);
  EXPECT_EQ(reader.GetBufferedBytes(), 12u);

  EXPECT_EQ(reader.Skip(4), 4u);
  EXPECT_EQ(reader.GetBufferedBytes(), 8u);

  std::vector<uint8_t> read_data(8);
  EXPECT_EQ(reader.Read(read_data), 8u);
  std::vector<uint8_t> expected = {5, 6, 7, 8, 9, 10, 11, 12};
  EXPECT_EQ(read_data, expected);
}

TYPED_TEST_P(FifoBufferTest, SkipMoreThanBuffered) {
  FifoBufferWriter& writer = this->delegate_.GetWriter();
  FifoBufferReader& reader = this->delegate_.GetReader();

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
  writer.Write(data);

  EXPECT_EQ(reader.Skip(12), 8u);
  EXPECT_EQ(reader.GetBufferedBytes(), 0u);
}

TYPED_TEST_P(FifoBufferTest, ReadEmpty) {
  FifoBufferReader& reader = this->delegate_.GetReader();
  std::vector<uint8_t> read_data(8);
  EXPECT_EQ(reader.Read(read_data), 0u);
  EXPECT_EQ(reader.GetBufferedBytes(), 0u);
}

REGISTER_TYPED_TEST_SUITE_P(FifoBufferTest,
                            BasicWriteRead,
                            ReadEmpty,
                            Clear,
                            Skip,
                            SkipMoreThanBuffered);

}  // namespace remoting

#endif  // REMOTING_BASE_FIFO_BUFFER_TEST_BASE_H_
