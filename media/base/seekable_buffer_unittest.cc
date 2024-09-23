// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
  static const int kDataSize = 409600;
  static const int kBufferSize = 4096;
  static const int kWriteSize = 512;

  void SetUp() override {
    // Note: We use srand() and rand() rather than base::RandXXX() to improve
    // unit test performance.  We don't need good random numbers, just
    // something that generates "mixed data."
    const unsigned int kKnownSeed = 0x98765432;
    srand(kKnownSeed);

    // Create random test data samples.
    for (int i = 0; i < kDataSize; i++) {
      data_[i] = static_cast<char>(rand());
    }
  }

  int GetRandomInt(int maximum) {
    return rand() % (maximum + 1);
  }

  SeekableBuffer buffer_;
  uint8_t data_[kDataSize];
  uint8_t write_buffer_[kDataSize];
};

TEST_F(SeekableBufferTest, RandomReadWrite) {
  int write_position = 0;
  int read_position = 0;
  while (read_position < kDataSize) {
    // Write a random amount of data.
    int write_size = GetRandomInt(kBufferSize);
    write_size = std::min(write_size, kDataSize - write_position);
    bool should_append = buffer_.Append(data_ + write_position, write_size);
    write_position += write_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
    EXPECT_EQ(should_append, buffer_.forward_bytes() < kBufferSize)
        << "Incorrect buffer full reported";

    // Peek a random amount of data.
    int copy_size = GetRandomInt(kBufferSize);
    int bytes_copied = buffer_.Peek(write_buffer_, copy_size);
    EXPECT_GE(copy_size, bytes_copied);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_copied));

    // Read a random amount of data.
    int read_size = GetRandomInt(kBufferSize);
    int bytes_read = buffer_.Read(write_buffer_, read_size);
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_read));
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
  }
}

TEST_F(SeekableBufferTest, ReadWriteSeek) {
  const int kReadSize = kWriteSize / 4;

  for (int i = 0; i < 10; ++i) {
    // Write until buffer is full.
    for (int j = 0; j < kBufferSize; j += kWriteSize) {
      bool should_append = buffer_.Append(data_ + j, kWriteSize);
      EXPECT_EQ(j < kBufferSize - kWriteSize, should_append)
          << "Incorrect buffer full reported";
      EXPECT_EQ(j + kWriteSize, buffer_.forward_bytes());
    }

    // Simulate a read and seek pattern. Each loop reads 4 times, each time
    // reading a quarter of |kWriteSize|.
    int read_position = 0;
    int forward_bytes = kBufferSize;
    for (int j = 0; j < kBufferSize; j += kWriteSize) {
      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_.Seek(2 * kReadSize));
      forward_bytes -= 2 * kReadSize;
      read_position += 2 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer_, kReadSize));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Seek backward.
      EXPECT_TRUE(buffer_.Seek(-3 * static_cast<int32_t>(kReadSize)));
      forward_bytes += 3 * kReadSize;
      read_position -= 3 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer_, kReadSize));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
      read_position += kReadSize;

      // Copy.
      EXPECT_EQ(kReadSize, buffer_.Peek(write_buffer_, kReadSize));
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));

      // Read.
      EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_.forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, kReadSize));
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
  const int kMaxWriteSize = 2 * kBufferSize;

  // Write and expect the buffer to be not full.
  for (int i = 0; i < kBufferSize - kWriteSize; i += kWriteSize) {
    EXPECT_TRUE(buffer_.Append(data_ + i, kWriteSize));
    EXPECT_EQ(i + kWriteSize, buffer_.forward_bytes());
  }

  // Write until we have kMaxWriteSize bytes in the buffer. Buffer is full in
  // these writes.
  for (int i = buffer_.forward_bytes(); i < kMaxWriteSize; i += kWriteSize) {
    EXPECT_FALSE(buffer_.Append(data_ + i, kWriteSize));
    EXPECT_EQ(i + kWriteSize, buffer_.forward_bytes());
  }

  // Read until the buffer is empty.
  int read_position = 0;
  while (buffer_.forward_bytes()) {
    // Read a random amount of data.
    int read_size = GetRandomInt(kBufferSize);
    int forward_bytes = buffer_.forward_bytes();
    int bytes_read = buffer_.Read(write_buffer_, read_size);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_read));
    if (read_size > forward_bytes)
      EXPECT_EQ(forward_bytes, bytes_read);
    else
      EXPECT_EQ(read_size, bytes_read);
    read_position += bytes_read;
    EXPECT_GE(kMaxWriteSize, read_position);
    EXPECT_EQ(kMaxWriteSize - read_position, buffer_.forward_bytes());
  }

  // Expects we have no bytes left.
  EXPECT_EQ(0, buffer_.forward_bytes());
  EXPECT_EQ(0, buffer_.Read(write_buffer_, 1));
}

TEST_F(SeekableBufferTest, SeekBackward) {
  EXPECT_EQ(0, buffer_.forward_bytes());
  EXPECT_EQ(0, buffer_.backward_bytes());
  EXPECT_FALSE(buffer_.Seek(1));
  EXPECT_FALSE(buffer_.Seek(-1));

  const int kReadSize = 256;

  // Write into buffer until it's full.
  for (int i = 0; i < kBufferSize; i += kWriteSize) {
    // Write a random amount of data.
    buffer_.Append(data_ + i, kWriteSize);
  }

  // Read until buffer is empty.
  for (int i = 0; i < kBufferSize; i += kReadSize) {
    EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + i, kReadSize));
  }

  // Seek backward.
  EXPECT_TRUE(buffer_.Seek(-static_cast<int32_t>(kBufferSize)));
  EXPECT_FALSE(buffer_.Seek(-1));

  // Read again.
  for (int i = 0; i < kBufferSize; i += kReadSize) {
    EXPECT_EQ(kReadSize, buffer_.Read(write_buffer_, kReadSize));
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + i, kReadSize));
  }
}

TEST_F(SeekableBufferTest, GetCurrentChunk) {
  const int kSeekSize = kWriteSize / 3;

  scoped_refptr<DataBuffer> buffer = DataBuffer::CopyFrom(
      base::make_span(data_, static_cast<size_t>(kWriteSize)));

  const uint8_t* data;
  int size;
  EXPECT_FALSE(buffer_.GetCurrentChunk(&data, &size));

  buffer_.Append(buffer.get());
  EXPECT_TRUE(buffer_.GetCurrentChunk(&data, &size));
  EXPECT_EQ(data, buffer->data());
  EXPECT_EQ(size, buffer->data_size());

  buffer_.Seek(kSeekSize);
  EXPECT_TRUE(buffer_.GetCurrentChunk(&data, &size));
  EXPECT_EQ(data, buffer->data() + kSeekSize);
  EXPECT_EQ(size, buffer->data_size() - kSeekSize);
}

TEST_F(SeekableBufferTest, SeekForward) {
  int write_position = 0;
  int read_position = 0;
  while (read_position < kDataSize) {
    for (int i = 0; i < 10 && write_position < kDataSize; ++i) {
      // Write a random amount of data.
      int write_size = GetRandomInt(kBufferSize);
      write_size = std::min(write_size, kDataSize - write_position);

      bool should_append = buffer_.Append(data_ + write_position, write_size);
      write_position += write_size;
      EXPECT_GE(write_position, read_position);
      EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
      EXPECT_EQ(should_append, buffer_.forward_bytes() < kBufferSize)
          << "Incorrect buffer full status reported";
    }

    // Read a random amount of data.
    int seek_size = GetRandomInt(kBufferSize);
    if (buffer_.Seek(seek_size))
      read_position += seek_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());

    // Read a random amount of data.
    int read_size = GetRandomInt(kBufferSize);
    int bytes_read = buffer_.Read(write_buffer_, read_size);
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(0, memcmp(write_buffer_, data_ + read_position, bytes_read));
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_.forward_bytes());
  }
}

TEST_F(SeekableBufferTest, AllMethods) {
  EXPECT_EQ(0, buffer_.Read(write_buffer_, 0));
  EXPECT_EQ(0, buffer_.Read(write_buffer_, 1));
  EXPECT_TRUE(buffer_.Seek(0));
  EXPECT_FALSE(buffer_.Seek(-1));
  EXPECT_FALSE(buffer_.Seek(1));
  EXPECT_EQ(0, buffer_.forward_bytes());
  EXPECT_EQ(0, buffer_.backward_bytes());
}

TEST_F(SeekableBufferTest, GetTime) {
  const int64_t kNoTS = kNoTimestamp.ToInternalValue();
  const struct {
    int64_t first_time_useconds;
    int64_t duration_useconds;
    int consume_bytes;
    int64_t expected_time;
  } tests[] = {
    { kNoTS, 1000000, 0, kNoTS },
    { kNoTS, 4000000, 0, kNoTS },
    { kNoTS, 8000000, 0, kNoTS },
    { kNoTS, 1000000, kWriteSize / 2, kNoTS },
    { kNoTS, 4000000, kWriteSize / 2, kNoTS },
    { kNoTS, 8000000, kWriteSize / 2, kNoTS },
    { kNoTS, 1000000, kWriteSize, kNoTS },
    { kNoTS, 4000000, kWriteSize, kNoTS },
    { kNoTS, 8000000, kWriteSize, kNoTS },
    { 0, 1000000, 0, 0 },
    { 0, 4000000, 0, 0 },
    { 0, 8000000, 0, 0 },
    { 0, 1000000, kWriteSize / 2, 500000 },
    { 0, 4000000, kWriteSize / 2, 2000000 },
    { 0, 8000000, kWriteSize / 2, 4000000 },
    { 0, 1000000, kWriteSize, 1000000 },
    { 0, 4000000, kWriteSize, 4000000 },
    { 0, 8000000, kWriteSize, 8000000 },
    { 5, 1000000, 0, 5 },
    { 5, 4000000, 0, 5 },
    { 5, 8000000, 0, 5 },
    { 5, 1000000, kWriteSize / 2, 500005 },
    { 5, 4000000, kWriteSize / 2, 2000005 },
    { 5, 8000000, kWriteSize / 2, 4000005 },
    { 5, 1000000, kWriteSize, 1000005 },
    { 5, 4000000, kWriteSize, 4000005 },
    { 5, 8000000, kWriteSize, 8000005 },
  };

  // current_time() must initially return kNoTimestamp.
  EXPECT_EQ(kNoTimestamp.ToInternalValue(),
            buffer_.current_time().ToInternalValue());

  scoped_refptr<DataBuffer> buffer = DataBuffer::CopyFrom(
      base::make_span(data_, static_cast<size_t>(kWriteSize)));

  for (size_t i = 0; i < std::size(tests); ++i) {
    buffer->set_timestamp(base::Microseconds(tests[i].first_time_useconds));
    buffer->set_duration(base::Microseconds(tests[i].duration_useconds));
    buffer_.Append(buffer.get());
    EXPECT_TRUE(buffer_.Seek(tests[i].consume_bytes));

    int64_t actual = buffer_.current_time().ToInternalValue();

    EXPECT_EQ(tests[i].expected_time, actual) << "With test = { start:"
        << tests[i].first_time_useconds << ", duration:"
        << tests[i].duration_useconds << ", consumed:"
        << tests[i].consume_bytes << " }\n";

    buffer_.Clear();
  }
}

}  // namespace media
