// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_buffer.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_view_util.h"
#include "net/base/io_buffer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const auto kData = base::byte_span_with_nul_from_cstring("hello!\0hi.");

class SpdyBufferTest : public ::testing::Test {};

// Make a string from the data remaining in |buffer|.
std::string BufferToString(const SpdyBuffer& buffer) {
  return std::string(base::as_string_view(buffer.GetRemaining()));
}

// Construct a SpdyBuffer from a spdy::SpdySerializedFrame and make sure its
// data is same as the original data.
TEST_F(SpdyBufferTest, FrameConstructor) {
  SpdyBuffer buffer(std::make_unique<spdy::SpdySerializedFrame>(
      spdy::test::MakeSerializedFrame(
          reinterpret_cast<const char*>(kData.data()), kData.size())));

  EXPECT_EQ(kData.size(), buffer.GetRemainingSize());
  EXPECT_EQ(kData, buffer.GetRemaining());
}

// Construct a SpdyBuffer from a const char*/size_t pair and make sure
// it makes a copy of the data.
TEST_F(SpdyBufferTest, DataConstructor) {
  std::string data(base::as_string_view(kData));
  SpdyBuffer buffer(base::as_byte_span(data));
  // This mutation shouldn't affect |buffer|'s data.
  data[0] = 'H';

  EXPECT_NE(kData.data(), buffer.GetRemaining().data());
  EXPECT_EQ(kData.size(), buffer.GetRemainingSize());
  EXPECT_EQ(base::as_string_view(kData), BufferToString(buffer));
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
  SpdyBuffer buffer(kData);

  size_t x1 = 0;
  size_t x2 = 0;
  buffer.AddConsumeCallback(
      base::BindRepeating(&IncrementBy, &x1, SpdyBuffer::CONSUME));
  buffer.AddConsumeCallback(
      base::BindRepeating(&IncrementBy, &x2, SpdyBuffer::CONSUME));

  EXPECT_EQ(base::as_string_view(kData), BufferToString(buffer));

  buffer.Consume(5);
  EXPECT_EQ(std::string(base::as_string_view(kData.subspan(5u))),
            BufferToString(buffer));
  EXPECT_EQ(5u, x1);
  EXPECT_EQ(5u, x2);

  buffer.Consume(kData.size() - 5);
  EXPECT_EQ(0u, buffer.GetRemainingSize());
  EXPECT_EQ(kData.size(), x1);
  EXPECT_EQ(kData.size(), x2);
}

// Construct a SpdyBuffer and attach a ConsumeCallback to it. The
// callback should be called when the SpdyBuffer is destroyed.
TEST_F(SpdyBufferTest, ConsumeOnDestruction) {
  size_t x = 0;

  {
    SpdyBuffer buffer(kData);
    buffer.AddConsumeCallback(
        base::BindRepeating(&IncrementBy, &x, SpdyBuffer::DISCARD));
  }

  EXPECT_EQ(kData.size(), x);
}

// Make sure the IOBuffer returned by GetIOBufferForRemainingData()
// points to the buffer's remaining data and isn't updated by
// Consume().
TEST_F(SpdyBufferTest, GetIOBufferForRemainingData) {
  SpdyBuffer buffer(kData);

  buffer.Consume(5);
  scoped_refptr<IOBuffer> io_buffer = buffer.GetIOBufferForRemainingData();
  size_t io_buffer_size = buffer.GetRemainingSize();
  const std::string expectedData(base::as_string_view(kData.subspan(5u)));
  EXPECT_EQ(expectedData,
            base::as_string_view(io_buffer->first(io_buffer_size)));

  buffer.Consume(kData.size() - 5);
  EXPECT_EQ(expectedData,
            base::as_string_view(io_buffer->first(io_buffer_size)));
}

// Make sure the IOBuffer returned by GetIOBufferForRemainingData()
// outlives the buffer itself.
TEST_F(SpdyBufferTest, IOBufferForRemainingDataOutlivesBuffer) {
  auto buffer = std::make_unique<SpdyBuffer>(kData);
  scoped_refptr<IOBuffer> io_buffer = buffer->GetIOBufferForRemainingData();
  buffer.reset();

  // This will cause a use-after-free error if |io_buffer| doesn't
  // outlive |buffer|.
  io_buffer->span().copy_prefix_from(kData);
}

}  // namespace

}  // namespace net
