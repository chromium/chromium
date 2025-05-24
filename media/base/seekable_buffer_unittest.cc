// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/seekable_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>

#include "base/time/time.h"
#include "media/base/data_buffer.h"
#include "media/base/timestamp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class SeekableBufferTest : public testing::Test {
 public:
  SeekableBufferTest() : buffer_(kBufferSize, kBufferSize) {
  }

 protected:
  static constexpr size_t kDataSize = 409600;
  static constexpr size_t kBufferSize = 4096;
  static constexpr size_t kWriteSize = 512;

  void SetUp() override {
    // Note: We use srand() and rand() rather than base::RandXXX() to improve
    // unit test performance.  We don't need good random numbers, just
    // something that generates "mixed data."
    constexpr unsigned int kKnownSeed = 0x98765432;
    srand(kKnownSeed);

    // Create random test data samples.
    for (size_t i = 0; i < kDataSize; i++) {
      data_[i] = static_cast<char>(rand());
    }
  }

  size_t GetRandomNumber(size_t maximum) {
    return static_cast<size_t>(rand() % (maximum + 1));
  }

  SeekableBuffer buffer_;
  std::array<uint8_t, kDataSize> data_;
  std::array<uint8_t, kDataSize> write_buffer_;
};

TEST_F(SeekableBufferTest, RandomReadWrite) {
  size_t write_position = 0;
  size_t read_position = 0;
  while (read_position < kDataSize) {
    // Write a random amount of data.
    const size_t write_size =
        std::min(GetRandomNumber(kBufferSize), kDataSize - write_position);
    const bool should_append =
        buffer_.Append(base::span(data_).subspan(write_position, write_size));
    write_position += write_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
    EXPECT_EQ(should_append, buffer_.forward_bytes() < kBufferSize)
        << "Incorrect buffer full reported";

    // Peek a random amount of data.
    const size_t peek_size = GetRandomNumber(kBufferSize);
    auto peek_buffer = base::span(write_buffer_).first(peek_size);
    const size_t bytes_copied = buffer_.Peek(peek_buffer);
    EXPECT_GE(peek_size, bytes_copied);
    EXPECT_EQ(peek_buffer.first(bytes_copied),
              base::span(data_).subspan(read_position, bytes_copied));

    // Read a random amount of data.
    const size_t read_size = GetRandomNumber(kBufferSize);
    auto read_buffer = base::span(write_buffer_).first(read_size);
    const size_t bytes_read = buffer_.Read(read_buffer);
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(read_buffer.first(bytes_read),
              base::span(data_).subspan(read_position, bytes_read));

    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
  }
}

TEST_F(SeekableBufferTest, ReadWriteSeek) {
  const size_t kReadSize = kWriteSize / 4;

  for (int i = 0; i < 10; ++i) {
    // Write until buffer is full.
    for (size_t j = 0; j < kBufferSize; j += kWriteSize) {
      const bool should_append =
          buffer_.Append(base::span(data_).subspan(j, kWriteSize));
      EXPECT_EQ(j < kBufferSize - kWriteSize, should_append)
          << "Incorrect buffer full reported";
      EXPECT_EQ(j + kWriteSize, buffer_.forward_bytes());
    }

    // Simulate a read and seek pattern. Each loop reads 4 times, each time
    // reading a quarter of |kWriteSize|.
    size_t read_position = 0;
    size_t forward_bytes = kBufferSize;
    auto write_buffer = base::span(write_buffer_).first(kReadSize);
    for (size_t j = 0; j < kBufferSize; j += kWriteSize) {
      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_.Seek(2 * kReadSize));
      forward_bytes -= 2 * kReadSize;
      read_position += 2 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));
      read_position += kReadSize;

      // Seek backward.
      EXPECT_TRUE(buffer_.Seek(-3 * static_cast<int32_t>(kReadSize)));
      forward_bytes += 3 * kReadSize;
      read_position -= 3 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));
      read_position += kReadSize;

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(write_buffer,
                base::span(data_).subspan(read_position, kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_.Seek(kReadSize));
      forward_bytes -= kReadSize;
      read_position += kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
    }
  }
}

TEST_F(SeekableBufferTest, BufferFull) {
  const size_t kMaxWriteSize = 2 * kBufferSize;

  // Write and expect the buffer to be not full.
  for (size_t i = 0; i < kBufferSize - kWriteSize; i += kWriteSize) {
    EXPECT_TRUE(buffer_.Append(base::span(data_).subspan(i, kWriteSize)));
    EXPECT_EQ(i + kWriteSize, buffer_.forward_bytes());
  }

  // Write until we have kMaxWriteSize bytes in the buffer. Buffer is full in
  // these writes.
  for (size_t i = buffer_.forward_bytes(); i < kMaxWriteSize; i += kWriteSize) {
    EXPECT_FALSE(buffer_.Append(base::span(data_).subspan(i, kWriteSize)));
    EXPECT_EQ(i + kWriteSize, buffer_.forward_bytes());
  }

  // Read until the buffer is empty.
  size_t read_position = 0;
  while (buffer_.forward_bytes()) {
    // Read a random amount of data.
    const size_t read_size = GetRandomNumber(kBufferSize);
    const size_t forward_bytes = buffer_.forward_bytes();

    auto write_buffer = base::span(write_buffer_).first(read_size);
    const size_t bytes_read = buffer_.Read(write_buffer);
    EXPECT_EQ(bytes_read, std::min(read_size, forward_bytes));
    EXPECT_EQ(write_buffer.first(bytes_read),
              base::span(data_).subspan(read_position, bytes_read));

    read_position += bytes_read;
    EXPECT_GE(kMaxWriteSize, read_position);
    EXPECT_EQ(kMaxWriteSize - read_position, buffer_.forward_bytes());
  }

  // Expects we have no bytes left.
  EXPECT_EQ(0u, buffer_.forward_bytes());
  EXPECT_EQ(0u, buffer_.Read(base::span(write_buffer_).first(1u)));
}

TEST_F(SeekableBufferTest, SeekBackward) {
  EXPECT_EQ(0u, buffer_.forward_bytes());
  EXPECT_EQ(0u, buffer_.backward_bytes());
  EXPECT_FALSE(buffer_.Seek(1));
  EXPECT_FALSE(buffer_.Seek(-1));

  const size_t kReadSize = 256;

  // Write into buffer until it's full.
  for (size_t i = 0; i < kBufferSize; i += kWriteSize) {
    // Write a random amount of data.
    buffer_.Append(base::span(data_).subspan(i, kWriteSize));
  }

  // Read until buffer is empty.
  for (size_t i = 0; i < kBufferSize; i += kReadSize) {
    auto write_buffer = base::span(write_buffer_).first(kReadSize);

    EXPECT_EQ(kReadSize, buffer_.Read(write_buffer));
    EXPECT_EQ(write_buffer, base::span(data_).subspan(i, kReadSize));
  }

  // Seek backward.
  EXPECT_TRUE(buffer_.Seek(-static_cast<int32_t>(kBufferSize)));
  EXPECT_FALSE(buffer_.Seek(-1));

  // Read again.
  for (size_t i = 0; i < kBufferSize; i += kReadSize) {
    auto write_buffer = base::span(write_buffer_).first(kReadSize);
    EXPECT_EQ(kReadSize, buffer_.Read(write_buffer));
    EXPECT_EQ(write_buffer, base::span(data_).subspan(i, kReadSize));
  }
}

TEST_F(SeekableBufferTest, GetCurrentChunk) {
  const size_t kSeekSize = kWriteSize / 3;

  scoped_refptr<DataBuffer> buffer =
      DataBuffer::CopyFrom(base::span(data_).first(kWriteSize));

  EXPECT_TRUE(buffer_.GetCurrentChunk().empty());

  buffer_.Append(buffer.get());
  const base::span<const uint8_t> data = buffer_.GetCurrentChunk();
  EXPECT_EQ(data, buffer->data());
  EXPECT_EQ(data.size(), buffer->size());

  buffer_.Seek(kSeekSize);
  const base::span<const uint8_t> new_data = buffer_.GetCurrentChunk();
  EXPECT_FALSE(new_data.empty());
  EXPECT_EQ(new_data, buffer->data().subspan(kSeekSize));
  EXPECT_EQ(new_data.size(), buffer->size() - kSeekSize);
}

TEST_F(SeekableBufferTest, SeekForward) {
  size_t write_position = 0;
  size_t read_position = 0;
  while (read_position < kDataSize) {
    for (int i = 0; i < 10 && write_position < kDataSize; ++i) {
      // Write a random amount of data.
      const size_t write_size =
          std::min(GetRandomNumber(kBufferSize), kDataSize - write_position);

      const bool should_append =
          buffer_.Append(base::span(data_).subspan(write_position, write_size));
      write_position += write_size;
      EXPECT_GE(write_position, read_position);
      EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
      EXPECT_EQ(should_append, buffer_.forward_bytes() < kBufferSize)
          << "Incorrect buffer full status reported";
    }

    // Read a random amount of data.
    const size_t seek_size = GetRandomNumber(kBufferSize);
    if (buffer_.Seek(seek_size)) {
      read_position += seek_size;
    }
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());

    // Read a random amount of data.
    const size_t read_size = GetRandomNumber(kBufferSize);
    auto write_buffer = base::span(write_buffer_).first(read_size);
    const size_t bytes_read = buffer_.Read(write_buffer);
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(write_buffer.first(bytes_read),
              base::span(data_).subspan(read_position, bytes_read));

    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
  }
}

TEST_F(SeekableBufferTest, AllMethods) {
  EXPECT_EQ(0u, buffer_.Read(base::span(write_buffer_).first(0u)));
  EXPECT_EQ(0u, buffer_.Read(base::span(write_buffer_).first(1u)));
  EXPECT_TRUE(buffer_.Seek(0));
  EXPECT_FALSE(buffer_.Seek(-1));
  EXPECT_FALSE(buffer_.Seek(1));
  EXPECT_EQ(0u, buffer_.forward_bytes());
  EXPECT_EQ(0u, buffer_.backward_bytes());
}

TEST_F(SeekableBufferTest, GetTime) {
  struct TestConfiguration {
    base::TimeDelta first_time;
    base::TimeDelta duration;
    size_t consume_bytes;
    base::TimeDelta expected_time;
  };

  constexpr std::array<TestConfiguration, 27> kTests{
      {{kNoTimestamp, base::Seconds(1), 0, kNoTimestamp},
       {kNoTimestamp, base::Seconds(4), 0, kNoTimestamp},
       {kNoTimestamp, base::Seconds(8), 0, kNoTimestamp},
       {kNoTimestamp, base::Seconds(1), kWriteSize / 2, kNoTimestamp},
       {kNoTimestamp, base::Seconds(4), kWriteSize / 2, kNoTimestamp},
       {kNoTimestamp, base::Seconds(8), kWriteSize / 2, kNoTimestamp},
       {kNoTimestamp, base::Seconds(1), kWriteSize, kNoTimestamp},
       {kNoTimestamp, base::Seconds(4), kWriteSize, kNoTimestamp},
       {kNoTimestamp, base::Seconds(8), kWriteSize, kNoTimestamp},
       {base::TimeDelta{}, base::Seconds(1), 0, base::TimeDelta{}},
       {base::TimeDelta{}, base::Seconds(4), 0, base::TimeDelta{}},
       {base::TimeDelta{}, base::Seconds(8), 0, base::TimeDelta{}},
       {base::TimeDelta{}, base::Seconds(1), kWriteSize / 2,
        base::Microseconds(500000)},
       {base::TimeDelta{}, base::Seconds(4), kWriteSize / 2, base::Seconds(2)},
       {base::TimeDelta{}, base::Seconds(8), kWriteSize / 2, base::Seconds(4)},
       {base::TimeDelta{}, base::Seconds(1), kWriteSize, base::Seconds(1)},
       {base::TimeDelta{}, base::Seconds(4), kWriteSize, base::Seconds(4)},
       {base::TimeDelta{}, base::Seconds(8), kWriteSize, base::Seconds(8)},
       {base::Microseconds(5), base::Seconds(1), 0, base::Microseconds(5)},
       {base::Microseconds(5), base::Seconds(4), 0, base::Microseconds(5)},
       {base::Microseconds(5), base::Seconds(8), 0, base::Microseconds(5)},
       {base::Microseconds(5), base::Seconds(1), kWriteSize / 2,
        base::Microseconds(500005)},
       {base::Microseconds(5), base::Seconds(4), kWriteSize / 2,
        base::Microseconds(2000005)},
       {base::Microseconds(5), base::Seconds(8), kWriteSize / 2,
        base::Microseconds(4000005)},
       {base::Microseconds(5), base::Seconds(1), kWriteSize,
        base::Microseconds(1000005)},
       {base::Microseconds(5), base::Seconds(4), kWriteSize,
        base::Microseconds(4000005)},
       {base::Microseconds(5), base::Seconds(8), kWriteSize,
        base::Microseconds(8000005)}}};

  // current_time() must initially return kNoTimestamp.
  EXPECT_EQ(kNoTimestamp, buffer_.current_time());

  scoped_refptr<DataBuffer> buffer =
      DataBuffer::CopyFrom(base::span(data_).first(kWriteSize));

  for (const TestConfiguration& test : kTests) {
    buffer->set_timestamp(test.first_time);
    buffer->set_duration(test.duration);
    buffer_.Append(buffer.get());
    EXPECT_TRUE(buffer_.Seek(test.consume_bytes));

    const base::TimeDelta actual = buffer_.current_time();
    EXPECT_EQ(test.expected_time, actual)
        << "With test = { start:" << test.first_time
        << ", duration:" << test.duration << ", consumed:" << test.consume_bytes
        << " }\n";

    buffer_.Clear();
  }
}

}  // namespace media
