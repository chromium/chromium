// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_buffer.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kData[] = "hello!\0hi.";
const size_t kDataSize = std::size(kData);

class SpdyBufferTest : public ::testing::Test {};

// Make a string from the data remaining in |buffer|.
std::string BufferToString(const SpdyBuffer& buffer) {
  return std::string(buffer.GetRemainingData(), buffer.GetRemainingSize());
}

// Construct a SpdyBuffer from a spdy::SpdySerializedFrame and make sure its
// data is same as the original data.
TEST_F(SpdyBufferTest, FrameConstructor) {
  SpdyBuffer buffer(std::make_unique<spdy::SpdySerializedFrame>(
      spdy::test::MakeSerializedFrame(const_cast<char*>(kData), kDataSize)));

  EXPECT_EQ(kDataSize, buffer.GetRemainingSize());
  EXPECT_EQ(
      std::string_view(kData, kDataSize),
      std::string_view(buffer.GetRemainingData(), buffer.GetRemainingSize()));
}

// Construct a SpdyBuffer from a const char*/size_t pair and make sure
// it makes a copy of the data.
TEST_F(SpdyBufferTest, DataConstructor) {
  std::string data(kData, kDataSize);
  SpdyBuffer buffer(data.data(), data.size());
  // This mutation shouldn't affect |buffer|'s data.
  data[0] = 'H';

  EXPECT_NE(kData, buffer.GetRemainingData());
  EXPECT_EQ(kDataSize, buffer.GetRemainingSize());
  EXPECT_EQ(std::string(kData, kDataSize), BufferToString(buffer));
}

void IncrementBy(size_t* x,
                 SpdyBuffer::ConsumeSource expected_consume_source,
                 size_t delta,
                 SpdyBuffer::ConsumeSource consume_source) {
  EXPECT_EQ(expected_consume_source, consume_source);
  *x += delta;
}

// Construct a SpdyBuffer and call Consume() on it, which should
// update the remaining data pointer and size appropriately, as well
// as calling the consume callbacks.
TEST_F(SpdyBufferTest, Consume) {
  SpdyBuffer buffer(kData, kDataSize);

  size_t x1 = 0;
  size_t x2 = 0;
  buffer.AddConsumeCallback(
      base::BindRepeating(&IncrementBy, &x1, SpdyBuffer::CONSUME));
  buffer.AddConsumeCallback(
      base::BindRepeating(&IncrementBy, &x2, SpdyBuffer::CONSUME));

  EXPECT_EQ(std::string(kData, kDataSize), BufferToString(buffer));

  buffer.Consume(5);
  EXPECT_EQ(std::string(kData + 5, kDataSize - 5), BufferToString(buffer));
  EXPECT_EQ(5u, x1);
  EXPECT_EQ(5u, x2);

  buffer.Consume(kDataSize - 5);
  EXPECT_EQ(0u, buffer.GetRemainingSize());
  EXPECT_EQ(kDataSize, x1);
  EXPECT_EQ(kDataSize, x2);
}

// Construct a SpdyBuffer and attach a ConsumeCallback to it. The
// callback should be called when the SpdyBuffer is destroyed.
TEST_F(SpdyBufferTest, ConsumeOnDestruction) {
  size_t x = 0;

  {
    SpdyBuffer buffer(kData, kDataSize);
    buffer.AddConsumeCallback(
        base::BindRepeating(&IncrementBy, &x, SpdyBuffer::DISCARD));
  }

  EXPECT_EQ(kDataSize, x);
}

// Make sure the IOBuffer returned by GetIOBufferForRemainingData()
// points to the buffer's remaining data and isn't updated by
// Consume().
TEST_F(SpdyBufferTest, GetIOBufferForRemainingData) {
  SpdyBuffer buffer(kData, kDataSize);

  buffer.Consume(5);
  scoped_refptr<IOBuffer> io_buffer = buffer.GetIOBufferForRemainingData();
  size_t io_buffer_size = buffer.GetRemainingSize();
  const std::string expectedData(kData + 5, kDataSize - 5);
  EXPECT_EQ(expectedData, std::string(io_buffer->data(), io_buffer_size));

  buffer.Consume(kDataSize - 5);
  EXPECT_EQ(expectedData, std::string(io_buffer->data(), io_buffer_size));
}

// Make sure the IOBuffer returned by GetIOBufferForRemainingData()
// outlives the buffer itself.
TEST_F(SpdyBufferTest, IOBufferForRemainingDataOutlivesBuffer) {
  auto buffer = std::make_unique<SpdyBuffer>(kData, kDataSize);
  scoped_refptr<IOBuffer> io_buffer = buffer->GetIOBufferForRemainingData();
  buffer.reset();

  // This will cause a use-after-free error if |io_buffer| doesn't
  // outlive |buffer|.
  std::memcpy(io_buffer->data(), kData, kDataSize);
}

}  // namespace

}  // namespace net
