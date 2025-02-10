// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_buffer.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DataBufferTest, Constructor_ZeroCapacity) {
  // Zero-sized buffers are valid. In practice they aren't used very much but it
  // eliminates clients from worrying about null data pointers.
  scoped_refptr<DataBuffer> buffer = base::MakeRefCounted<DataBuffer>(0);
  EXPECT_TRUE(buffer->data().empty());
  EXPECT_TRUE(buffer->writable_data().empty());
  EXPECT_EQ(0u, buffer->size());
  EXPECT_EQ(0u, buffer->capacity());
  EXPECT_EQ(0u, buffer->data().size());
  EXPECT_EQ(0u, buffer->writable_data().size());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DataBufferTest, Constructor_NonZeroCapacity) {
  constexpr size_t kBufferSize = 10u;
  auto buffer = base::MakeRefCounted<DataBuffer>(kBufferSize);

  // Buffer should be properly initialized.
  EXPECT_EQ(0u, buffer->size());
  EXPECT_EQ(kBufferSize, buffer->capacity());
  EXPECT_FALSE(buffer->end_of_stream());

  // The immutable data() should only return the portion of the buffer that has
  // been written to.
  EXPECT_TRUE(buffer->data().empty());
  EXPECT_EQ(0u, buffer->data().size());

  // The mutable writable_data() should return the entire buffer.
  EXPECT_FALSE(buffer->writable_data().empty());
  EXPECT_EQ(kBufferSize, buffer->writable_data().size());
}

TEST(DataBufferTest, Constructor_ScopedArray) {
  constexpr std::array<const uint8_t, 8> kTestData = {0x00, 0x11, 0x22, 0x33,
                                                      0x44, 0x55, 0x66, 0x77};

  // Data should be passed and both data and buffer size should be set.
  auto data = base::HeapArray<uint8_t>::CopiedFrom(kTestData);
  auto buffer = base::MakeRefCounted<DataBuffer>(std::move(data));

  EXPECT_EQ(kTestData.size(), buffer->size());
  EXPECT_EQ(kTestData.size(), buffer->capacity());
  EXPECT_EQ(kTestData, buffer->data());
  EXPECT_EQ(buffer->writable_data(), buffer->data());
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DataBufferTest, CopyFrom) {
  constexpr const uint8_t kTestData[] = {0x00, 0x11, 0x22, 0x33,
                                         0x44, 0x55, 0x66, 0x77};
  const base::span<const uint8_t> kTestDataSpan(kTestData);

  scoped_refptr<DataBuffer> buffer = DataBuffer::CopyFrom(kTestDataSpan);
  EXPECT_EQ(kTestDataSpan.size(), buffer->size());
  EXPECT_FALSE(buffer->end_of_stream());

  // Ensure we are copying the data, not just pointing to the original data.
  EXPECT_EQ(buffer->data(), kTestDataSpan);
  buffer->writable_data()[0] = 0xFF;
  EXPECT_NE(buffer->data(), kTestDataSpan);
}

TEST(DataBufferTest, CreateEOSBuffer) {
  scoped_refptr<DataBuffer> buffer = DataBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST(DataBufferTest, Timestamp) {
  constexpr base::TimeDelta kTimestampA = base::Microseconds(1337);
  constexpr base::TimeDelta kTimestampB = base::Microseconds(1234);

  auto buffer = base::MakeRefCounted<DataBuffer>(0);
  EXPECT_EQ(buffer->timestamp(), base::TimeDelta{});

  buffer->set_timestamp(kTimestampA);
  EXPECT_EQ(buffer->timestamp(), kTimestampA);

  buffer->set_timestamp(kTimestampB);
  EXPECT_EQ(buffer->timestamp(), kTimestampB);
}

TEST(DataBufferTest, Duration) {
  constexpr base::TimeDelta kDurationA = base::Microseconds(1337);
  constexpr base::TimeDelta kDurationB = base::Microseconds(1234);

  scoped_refptr<DataBuffer> buffer = base::MakeRefCounted<DataBuffer>(0);
  EXPECT_EQ(buffer->duration(), base::TimeDelta{});

  buffer->set_duration(kDurationA);
  EXPECT_EQ(buffer->duration(), kDurationA);

  buffer->set_duration(kDurationB);
  EXPECT_EQ(buffer->duration(), kDurationB);
}

TEST(DataBufferTest, ReadingWriting) {
  constexpr const char kData[] = "hello";
  constexpr const char kNewData[] = "chromium";
  const auto kDataSpan = base::byte_span_from_cstring(kData);
  const auto kNewDataSpan = base::byte_span_from_cstring(kNewData);

  // Create a DataBuffer.
  auto buffer = base::MakeRefCounted<DataBuffer>(kDataSpan.size());
  buffer->Append(kDataSpan);
  EXPECT_EQ(buffer->writable_data(), buffer->data());
  EXPECT_EQ(buffer->data(), kDataSpan);

  constexpr size_t kBufferTwoSize = kNewDataSpan.size() + 10;
  auto buffer_two = base::MakeRefCounted<DataBuffer>(kBufferTwoSize);
  buffer_two->Append(kNewDataSpan);

  EXPECT_EQ(kNewDataSpan.size(), buffer_two->size());
  EXPECT_EQ(buffer_two->data(), kNewDataSpan);
  EXPECT_EQ(buffer_two->data().first(buffer_two->size()), kNewDataSpan);

  // NOTE: the writable_data() method returns the uninitialized portion of the
  // buffer as well. Only compare the portion that has been written to.
  EXPECT_EQ(buffer_two->writable_data().size(), kBufferTwoSize);
  EXPECT_EQ(buffer_two->writable_data().first(buffer_two->size()),
            kNewDataSpan);
}

}  // namespace media
