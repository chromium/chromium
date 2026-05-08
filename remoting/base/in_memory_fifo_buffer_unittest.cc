// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/in_memory_fifo_buffer.h"

#include <memory>
#include <vector>

#include "remoting/base/fifo_buffer_test_base.h"

namespace remoting {

namespace {
constexpr size_t kCapacity = 1024;
}  // namespace

class InMemoryFifoBufferTestDelegate {
 public:
  InMemoryFifoBufferTestDelegate() {
    CHECK(CreateInMemoryFifoBuffer(kCapacity, writer_, reader_));
  }

  FifoBufferWriter& GetWriter() { return *writer_; }
  FifoBufferReader& GetReader() { return *reader_; }

 private:
  std::unique_ptr<InMemoryFifoBufferWriter> writer_;
  std::unique_ptr<InMemoryFifoBufferReader> reader_;
};

using InMemoryFifoBufferTestTypes =
    testing::Types<InMemoryFifoBufferTestDelegate>;
INSTANTIATE_TYPED_TEST_SUITE_P(InMemory,
                               FifoBufferTest,
                               InMemoryFifoBufferTestTypes);

TEST(InMemoryFifoBufferTest, Overflow) {
  // This test is specific to the overflow behavior of InMemoryFifoBuffer,
  // which strictly drops extra bytes at capacity limit.
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(kCapacity, writer, reader));

  std::vector<uint8_t> data(kCapacity, 0xAA);
  EXPECT_EQ(writer->Write(data), FifoBufferWriter::Result::kSuccess);
  EXPECT_EQ(reader->GetBufferedBytes(), kCapacity);

  std::vector<uint8_t> extra_data = {1, 2, 3, 4};
  EXPECT_EQ(writer->Write(extra_data), FifoBufferWriter::Result::kFull);

  std::vector<uint8_t> read_data(kCapacity);
  EXPECT_EQ(reader->Read(read_data), kCapacity);
  EXPECT_EQ(read_data, data);
}

TEST(InMemoryFifoBufferTest, WrapAround) {
  // This test is specific to the circular buffer implementation details of
  // InMemoryFifoBuffer, so we keep it here as a standalone test.
  std::unique_ptr<InMemoryFifoBufferWriter> writer;
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  ASSERT_TRUE(CreateInMemoryFifoBuffer(kCapacity, writer, reader));

  std::vector<uint8_t> data(kCapacity - 4, 0xAA);
  EXPECT_EQ(writer->Write(data), FifoBufferWriter::Result::kSuccess);

  std::vector<uint8_t> read_data(kCapacity - 4);
  EXPECT_EQ(reader->Read(read_data), kCapacity - 4);

  // Write again, should wrap.
  std::vector<uint8_t> wrap_data = {1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(writer->Write(wrap_data), FifoBufferWriter::Result::kSuccess);

  std::vector<uint8_t> wrap_read(8);
  EXPECT_EQ(reader->Read(wrap_read), 8u);
  EXPECT_EQ(wrap_read, wrap_data);
}

}  // namespace remoting
