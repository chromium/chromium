// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/server/http_connection.h"

#include <string>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

std::string GetTestString(int size) {
  std::string test_string;
  for (int i = 0; i < size; ++i) {
    test_string.push_back('A' + (i % 26));
  }
  return test_string;
}

TEST(HttpConnectionTest, ReadIOBuffer_SetCapacity) {
  scoped_refptr<HttpConnection::ReadIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::ReadIOBuffer>();
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize + 0,
            buffer->GetCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize + 0,
            buffer->RemainingCapacity());
  EXPECT_EQ(0, buffer->GetSize());

  const int kNewCapacity = HttpConnection::ReadIOBuffer::kInitialBufSize + 128;
  buffer->SetCapacity(kNewCapacity);
  EXPECT_EQ(kNewCapacity, buffer->GetCapacity());
  EXPECT_EQ(kNewCapacity, buffer->RemainingCapacity());
  EXPECT_EQ(0, buffer->GetSize());
}

TEST(HttpConnectionTest, ReadIOBuffer_SetCapacity_WithData) {
  scoped_refptr<HttpConnection::ReadIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::ReadIOBuffer>();
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize + 0,
            buffer->GetCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize + 0,
            buffer->RemainingCapacity());

  // Write arbitrary data up to kInitialBufSize.
  const std::string kReadData(
      GetTestString(HttpConnection::ReadIOBuffer::kInitialBufSize));
  memcpy(buffer->data(), kReadData.data(), kReadData.size());
  buffer->DidRead(kReadData.size());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize + 0,
            buffer->GetCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize -
                static_cast<int>(kReadData.size()),
            buffer->RemainingCapacity());
  EXPECT_EQ(static_cast<int>(kReadData.size()), buffer->GetSize());
  EXPECT_EQ(kReadData,
            std::string_view(buffer->StartOfBuffer(), buffer->GetSize()));

  // Check if read data in the buffer is same after SetCapacity().
  const int kNewCapacity = HttpConnection::ReadIOBuffer::kInitialBufSize + 128;
  buffer->SetCapacity(kNewCapacity);
  EXPECT_EQ(kNewCapacity, buffer->GetCapacity());
  EXPECT_EQ(kNewCapacity - static_cast<int>(kReadData.size()),
            buffer->RemainingCapacity());
  EXPECT_EQ(static_cast<int>(kReadData.size()), buffer->GetSize());
  EXPECT_EQ(kReadData,
            std::string_view(buffer->StartOfBuffer(), buffer->GetSize()));
}

TEST(HttpConnectionTest, ReadIOBuffer_IncreaseCapacity) {
  scoped_refptr<HttpConnection::ReadIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::ReadIOBuffer>();
  EXPECT_TRUE(buffer->IncreaseCapacity());
  const int kExpectedInitialBufSize =
      HttpConnection::ReadIOBuffer::kInitialBufSize *
      HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor;
  EXPECT_EQ(kExpectedInitialBufSize, buffer->GetCapacity());
  EXPECT_EQ(kExpectedInitialBufSize, buffer->RemainingCapacity());
  EXPECT_EQ(0, buffer->GetSize());

  // Increase capacity until it fails.
  while (buffer->IncreaseCapacity());
  EXPECT_FALSE(buffer->IncreaseCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize + 0,
            buffer->max_buffer_size());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize + 0,
            buffer->GetCapacity());

  // Enlarge capacity limit.
  buffer->set_max_buffer_size(buffer->max_buffer_size() * 2);
  EXPECT_TRUE(buffer->IncreaseCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize *
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor,
            buffer->GetCapacity());

  // Shrink capacity limit. It doesn't change capacity itself.
  buffer->set_max_buffer_size(
      HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize / 2);
  EXPECT_FALSE(buffer->IncreaseCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize *
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor,
            buffer->GetCapacity());
}

TEST(HttpConnectionTest, ReadIOBuffer_IncreaseCapacity_WithData) {
  scoped_refptr<HttpConnection::ReadIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::ReadIOBuffer>();
  EXPECT_TRUE(buffer->IncreaseCapacity());
  const int kExpectedInitialBufSize =
      HttpConnection::ReadIOBuffer::kInitialBufSize *
      HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor;
  EXPECT_EQ(kExpectedInitialBufSize, buffer->GetCapacity());
  EXPECT_EQ(kExpectedInitialBufSize, buffer->RemainingCapacity());
  EXPECT_EQ(0, buffer->GetSize());

  // Write arbitrary data up to kExpectedInitialBufSize.
  std::string kReadData(GetTestString(kExpectedInitialBufSize));
  memcpy(buffer->data(), kReadData.data(), kReadData.size());
  buffer->DidRead(kReadData.size());
  EXPECT_EQ(kExpectedInitialBufSize, buffer->GetCapacity());
  EXPECT_EQ(kExpectedInitialBufSize - static_cast<int>(kReadData.size()),
            buffer->RemainingCapacity());
  EXPECT_EQ(static_cast<int>(kReadData.size()), buffer->GetSize());
  EXPECT_EQ(kReadData,
            std::string_view(buffer->StartOfBuffer(), buffer->GetSize()));

  // Increase capacity until it fails and check if read data in the buffer is
  // same.
  while (buffer->IncreaseCapacity());
  EXPECT_FALSE(buffer->IncreaseCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize + 0,
            buffer->max_buffer_size());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize + 0,
            buffer->GetCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kDefaultMaxBufferSize -
                static_cast<int>(kReadData.size()),
            buffer->RemainingCapacity());
  EXPECT_EQ(static_cast<int>(kReadData.size()), buffer->GetSize());
  EXPECT_EQ(kReadData,
            std::string_view(buffer->StartOfBuffer(), buffer->GetSize()));
}

TEST(HttpConnectionTest, ReadIOBuffer_DidRead_DidConsume) {
  scoped_refptr<HttpConnection::ReadIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::ReadIOBuffer>();
  const char* start_of_buffer = buffer->StartOfBuffer();
  EXPECT_EQ(start_of_buffer, buffer->data());

  // Read data.
  const int kReadLength = 128;
  const std::string kReadData(GetTestString(kReadLength));
  memcpy(buffer->data(), kReadData.data(), kReadLength);
  buffer->DidRead(kReadLength);
  // No change in total capacity.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize + 0,
            buffer->GetCapacity());
  // Change in unused capacity because of read data.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize - kReadLength,
            buffer->RemainingCapacity());
  EXPECT_EQ(kReadLength, buffer->GetSize());
  // No change in start pointers of read data.
  EXPECT_EQ(start_of_buffer, buffer->StartOfBuffer());
  // Change in start pointer of unused buffer.
  EXPECT_EQ(start_of_buffer + kReadLength, buffer->data());
  // Test read data.
  EXPECT_EQ(kReadData, std::string(buffer->StartOfBuffer(), buffer->GetSize()));

  // Consume data partially.
  const int kConsumedLength = 32;
  ASSERT_LT(kConsumedLength, kReadLength);
  buffer->DidConsume(kConsumedLength);
  // Capacity reduced because read data was too small comparing to capacity.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor,
            buffer->GetCapacity());
  // Change in unused capacity because of read data.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor -
                kReadLength + kConsumedLength,
            buffer->RemainingCapacity());
  // Change in read size.
  EXPECT_EQ(kReadLength - kConsumedLength, buffer->GetSize());
  // Start data could be changed even when capacity is reduced.
  start_of_buffer = buffer->StartOfBuffer();
  // Change in start pointer of unused buffer.
  EXPECT_EQ(start_of_buffer + kReadLength - kConsumedLength, buffer->data());
  // Change in read data.
  EXPECT_EQ(kReadData.substr(kConsumedLength),
            std::string(buffer->StartOfBuffer(), buffer->GetSize()));

  // Read more data.
  const int kReadLength2 = 64;
  buffer->DidRead(kReadLength2);
  // No change in total capacity.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor,
            buffer->GetCapacity());
  // Change in unused capacity because of read data.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor -
                kReadLength + kConsumedLength - kReadLength2,
            buffer->RemainingCapacity());
  // Change in read size
  EXPECT_EQ(kReadLength - kConsumedLength + kReadLength2, buffer->GetSize());
  // No change in start pointer of read part.
  EXPECT_EQ(start_of_buffer, buffer->StartOfBuffer());
  // Change in start pointer of unused buffer.
  EXPECT_EQ(start_of_buffer + kReadLength - kConsumedLength + kReadLength2,
            buffer->data());

  // Consume data fully.
  buffer->DidConsume(kReadLength - kConsumedLength + kReadLength2);
  // Capacity reduced again because read data was too small.
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor,
            buffer->GetCapacity());
  EXPECT_EQ(HttpConnection::ReadIOBuffer::kInitialBufSize /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor /
                HttpConnection::ReadIOBuffer::kCapacityIncreaseFactor,
            buffer->RemainingCapacity());
  // All reverts to initial because no data is left.
  EXPECT_EQ(0, buffer->GetSize());
  // Start data could be changed even when capacity is reduced.
  start_of_buffer = buffer->StartOfBuffer();
  EXPECT_EQ(start_of_buffer, buffer->data());
}

TEST(HttpConnectionTest, QueuedWriteIOBuffer_Append_DidConsume) {
  scoped_refptr<HttpConnection::QueuedWriteIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::QueuedWriteIOBuffer>();
  EXPECT_TRUE(buffer->IsEmpty());
  EXPECT_EQ(0, buffer->GetSizeToWrite());
  EXPECT_EQ(0, buffer->total_size());

  const std::string kData("data to write");
  EXPECT_TRUE(buffer->Append(kData));
  EXPECT_FALSE(buffer->IsEmpty());
  EXPECT_EQ(static_cast<int>(kData.size()), buffer->GetSizeToWrite());
  EXPECT_EQ(static_cast<int>(kData.size()), buffer->total_size());
  // First data to write is same to kData.
  EXPECT_EQ(kData, std::string_view(buffer->data(), buffer->GetSizeToWrite()));

  const std::string kData2("more data to write");
  EXPECT_TRUE(buffer->Append(kData2));
  EXPECT_FALSE(buffer->IsEmpty());
  // No change in size to write.
  EXPECT_EQ(static_cast<int>(kData.size()), buffer->GetSizeToWrite());
  // Change in total size.
  EXPECT_EQ(static_cast<int>(kData.size() + kData2.size()),
            buffer->total_size());
  // First data to write has not been changed. Same to kData.
  EXPECT_EQ(kData, std::string_view(buffer->data(), buffer->GetSizeToWrite()));

  // Consume data partially.
  const int kConsumedLength = kData.length() - 1;
  buffer->DidConsume(kConsumedLength);
  EXPECT_FALSE(buffer->IsEmpty());
  // Change in size to write.
  EXPECT_EQ(static_cast<int>(kData.size()) - kConsumedLength,
            buffer->GetSizeToWrite());
  // Change in total size.
  EXPECT_EQ(static_cast<int>(kData.size() + kData2.size()) - kConsumedLength,
            buffer->total_size());
  // First data to write has shrinked.
  EXPECT_EQ(kData.substr(kConsumedLength),
            std::string_view(buffer->data(), buffer->GetSizeToWrite()));

  // Consume first data fully.
  buffer->DidConsume(kData.size() - kConsumedLength);
  EXPECT_FALSE(buffer->IsEmpty());
  // Now, size to write is size of data added second.
  EXPECT_EQ(static_cast<int>(kData2.size()), buffer->GetSizeToWrite());
  // Change in total size.
  EXPECT_EQ(static_cast<int>(kData2.size()), buffer->total_size());
  // First data to write has changed to kData2.
  EXPECT_EQ(kData2, std::string_view(buffer->data(), buffer->GetSizeToWrite()));

  // Consume second data fully.
  buffer->DidConsume(kData2.size());
  EXPECT_TRUE(buffer->IsEmpty());
  EXPECT_EQ(0, buffer->GetSizeToWrite());
  EXPECT_EQ(0, buffer->total_size());
}

TEST(HttpConnectionTest, QueuedWriteIOBuffer_TotalSizeLimit) {
  scoped_refptr<HttpConnection::QueuedWriteIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::QueuedWriteIOBuffer>();
  EXPECT_EQ(HttpConnection::QueuedWriteIOBuffer::kDefaultMaxBufferSize + 0,
            buffer->max_buffer_size());

  // Set total size limit very small.
  buffer->set_max_buffer_size(10);

  const int kDataLength = 4;
  const std::string kData(kDataLength, 'd');
  EXPECT_TRUE(buffer->Append(kData));
  EXPECT_EQ(kDataLength, buffer->total_size());
  EXPECT_TRUE(buffer->Append(kData));
  EXPECT_EQ(kDataLength * 2, buffer->total_size());

  // Cannot append more data because it exceeds the limit.
  EXPECT_FALSE(buffer->Append(kData));
  EXPECT_EQ(kDataLength * 2, buffer->total_size());

  // Consume data partially.
  const int kConsumedLength = 2;
  buffer->DidConsume(kConsumedLength);
  EXPECT_EQ(kDataLength * 2 - kConsumedLength, buffer->total_size());

  // Can add more data.
  EXPECT_TRUE(buffer->Append(kData));
  EXPECT_EQ(kDataLength * 3 - kConsumedLength, buffer->total_size());

  // Cannot append more data because it exceeds the limit.
  EXPECT_FALSE(buffer->Append(kData));
  EXPECT_EQ(kDataLength * 3 - kConsumedLength, buffer->total_size());

  // Enlarge limit.
  buffer->set_max_buffer_size(20);
  // Can add more data.
  EXPECT_TRUE(buffer->Append(kData));
  EXPECT_EQ(kDataLength * 4 - kConsumedLength, buffer->total_size());
}

TEST(HttpConnectionTest, QueuedWriteIOBuffer_DataPointerStability) {
  // This is a regression test that makes sure that QueuedWriteIOBuffer deals
  // with base::queue's semantics differences vs. std::queue right, and still
  // makes sure our data() pointers are stable.
  scoped_refptr<HttpConnection::QueuedWriteIOBuffer> buffer =
      base::MakeRefCounted<HttpConnection::QueuedWriteIOBuffer>();

  // We append a short string to make it fit within any short string
  // optimization, so that if the underlying queue moves the std::string,
  // the data should change.
  buffer->Append("abcdefgh");

  // Read part of it, to make sure this handles the case of data() pointing
  // to something other than start of string right.
  buffer->DidConsume(3);
  const char* old_data = buffer->data();
  EXPECT_EQ("defgh", std::string_view(buffer->data(), 5));

  // Now append a whole bunch of other things to make the underlying queue
  // grow, and likely need to move stuff around in memory.
  for (int i = 0; i < 256; ++i)
    buffer->Append("some other string data");

  // data() should still be right.
  EXPECT_EQ("defgh", std::string_view(buffer->data(), 5));

  // ... it should also be bitwise the same, since the IOBuffer can get passed
  // to async calls and then have Append's come in.
  EXPECT_TRUE(buffer->data() == old_data);
}

}  // namespace
}  // namespace net
