// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/data_buffer.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DataBufferTest, Constructor_ZeroSize) {
  // Zero-sized buffers are valid. In practice they aren't used very much but it
  // eliminates clients from worrying about null data pointers.
  scoped_refptr<DataBuffer> buffer = new DataBuffer(0);
  EXPECT_FALSE(buffer->data());
  EXPECT_FALSE(buffer->writable_data());
  EXPECT_EQ(0, buffer->data_size());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DataBufferTest, Constructor_NonZeroSize) {
  // Buffer size should be set.
  scoped_refptr<DataBuffer> buffer = new DataBuffer(10);
  EXPECT_TRUE(buffer->data());
  EXPECT_TRUE(buffer->writable_data());
  EXPECT_EQ(10, buffer->data_size());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DataBufferTest, Constructor_ScopedArray) {
  // Data should be passed and both data and buffer size should be set.
  const int kSize = 8;
  auto data = base::HeapArray<uint8_t>::Uninit(kSize);
  const uint8_t* kData = data.data();

  scoped_refptr<DataBuffer> buffer = new DataBuffer(std::move(data));
  EXPECT_TRUE(buffer->data());
  EXPECT_TRUE(buffer->writable_data());
  EXPECT_EQ(kData, buffer->data());
  EXPECT_EQ(kSize, buffer->data_size());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DataBufferTest, CopyFrom) {
  const uint8_t kTestData[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
  const int kTestDataSize = std::size(kTestData);

  scoped_refptr<DataBuffer> buffer = DataBuffer::CopyFrom(
      base::make_span(kTestData, static_cast<size_t>(kTestDataSize)));
  EXPECT_EQ(kTestDataSize, buffer->data_size());
  EXPECT_FALSE(buffer->end_of_stream());

  // Ensure we are copying the data, not just pointing to the original data.
  EXPECT_EQ(0, memcmp(buffer->data(), kTestData, kTestDataSize));
  buffer->writable_data()[0] = 0xFF;
  EXPECT_NE(0, memcmp(buffer->data(), kTestData, kTestDataSize));
}

TEST(DataBufferTest, CreateEOSBuffer) {
  scoped_refptr<DataBuffer> buffer = DataBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST(DataBufferTest, Timestamp) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimestampA = base::Microseconds(1337);
  const base::TimeDelta kTimestampB = base::Microseconds(1234);

  scoped_refptr<DataBuffer> buffer = new DataBuffer(0);
  EXPECT_TRUE(buffer->timestamp() == kZero);

  buffer->set_timestamp(kTimestampA);
  EXPECT_TRUE(buffer->timestamp() == kTimestampA);

  buffer->set_timestamp(kTimestampB);
  EXPECT_TRUE(buffer->timestamp() == kTimestampB);
}

TEST(DataBufferTest, Duration) {
  const base::TimeDelta kZero;
  const base::TimeDelta kDurationA = base::Microseconds(1337);
  const base::TimeDelta kDurationB = base::Microseconds(1234);

  scoped_refptr<DataBuffer> buffer = new DataBuffer(0);
  EXPECT_TRUE(buffer->duration() == kZero);

  buffer->set_duration(kDurationA);
  EXPECT_TRUE(buffer->duration() == kDurationA);

  buffer->set_duration(kDurationB);
  EXPECT_TRUE(buffer->duration() == kDurationB);
}

TEST(DataBufferTest, ReadingWriting) {
  const char kData[] = "hello";
  const int kDataSize = std::size(kData);
  const char kNewData[] = "chromium";
  const int kNewDataSize = std::size(kNewData);

  // Create a DataBuffer.
  scoped_refptr<DataBuffer> buffer(new DataBuffer(kDataSize));
  ASSERT_TRUE(buffer.get());

  uint8_t* data = buffer->writable_data();
  ASSERT_TRUE(data);
  memcpy(data, kData, kDataSize);
  buffer->set_data_size(kDataSize);
  const uint8_t* read_only_data = buffer->data();
  ASSERT_EQ(data, read_only_data);
  ASSERT_EQ(0, memcmp(read_only_data, kData, kDataSize));
  EXPECT_FALSE(buffer->end_of_stream());

  scoped_refptr<DataBuffer> buffer2(new DataBuffer(kNewDataSize + 10));
  data = buffer2->writable_data();
  ASSERT_TRUE(data);
  memcpy(data, kNewData, kNewDataSize);
  buffer2->set_data_size(kNewDataSize);
  read_only_data = buffer2->data();
  EXPECT_EQ(kNewDataSize, buffer2->data_size());
  ASSERT_EQ(data, read_only_data);
  EXPECT_EQ(0, memcmp(read_only_data, kNewData, kNewDataSize));
}

}  // namespace media
