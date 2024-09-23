// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_inflater.h"

#include <string>
#include <vector>

#include "net/base/io_buffer.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string ToString(IOBufferWithSize* buffer) {
  return std::string(buffer->data(), buffer->size());
}

TEST(WebSocketInflaterTest, Construct) {
  WebSocketInflater inflater;
  ASSERT_TRUE(inflater.Initialize(15));

  EXPECT_EQ(0u, inflater.CurrentOutputSize());
}

TEST(WebSocketInflaterTest, InflateHelloTakeOverContext) {
  WebSocketInflater inflater;
  ASSERT_TRUE(inflater.Initialize(15));
  scoped_refptr<IOBufferWithSize> actual1, actual2;

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  actual1 = inflater.GetOutput(inflater.CurrentOutputSize());
  ASSERT_TRUE(actual1.get());
  EXPECT_EQ("Hello", ToString(actual1.get()));
  EXPECT_EQ(0u, inflater.CurrentOutputSize());

  ASSERT_TRUE(inflater.AddBytes("\xf2\x00\x11\x00\x00", 5));
  ASSERT_TRUE(inflater.Finish());
  actual2 = inflater.GetOutput(inflater.CurrentOutputSize());
  ASSERT_TRUE(actual2.get());
  EXPECT_EQ("Hello", ToString(actual2.get()));
  EXPECT_EQ(0u, inflater.CurrentOutputSize());
}

TEST(WebSocketInflaterTest, InflateHelloSmallCapacity) {
  WebSocketInflater inflater(1, 1);
  ASSERT_TRUE(inflater.Initialize(15));
  std::string actual;

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  for (size_t i = 0; i < 5; ++i) {
    ASSERT_EQ(1u, inflater.CurrentOutputSize());
    scoped_refptr<IOBufferWithSize> buffer = inflater.GetOutput(1);
    ASSERT_TRUE(buffer.get());
    ASSERT_EQ(1, buffer->size());
    actual += ToString(buffer.get());
  }
  EXPECT_EQ("Hello", actual);
  EXPECT_EQ(0u, inflater.CurrentOutputSize());
}

TEST(WebSocketInflaterTest, InflateHelloSmallCapacityGetTotalOutput) {
  WebSocketInflater inflater(1, 1);
  ASSERT_TRUE(inflater.Initialize(15));
  scoped_refptr<IOBufferWithSize> actual;

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  ASSERT_EQ(1u, inflater.CurrentOutputSize());
  actual = inflater.GetOutput(1024);
  EXPECT_EQ("Hello", ToString(actual.get()));
  EXPECT_EQ(0u, inflater.CurrentOutputSize());
}

TEST(WebSocketInflaterTest, InflateInvalidData) {
  WebSocketInflater inflater;
  ASSERT_TRUE(inflater.Initialize(15));
  EXPECT_FALSE(inflater.AddBytes("\xf2\x48\xcd\xc9INVALID DATA", 16));
}

TEST(WebSocketInflaterTest, ChokedInvalidData) {
  WebSocketInflater inflater(1, 1);
  ASSERT_TRUE(inflater.Initialize(15));

  EXPECT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9INVALID DATA", 16));
  EXPECT_TRUE(inflater.Finish());
  EXPECT_EQ(1u, inflater.CurrentOutputSize());
  EXPECT_FALSE(inflater.GetOutput(1024).get());
}

TEST(WebSocketInflaterTest, MultipleAddBytesCalls) {
  WebSocketInflater inflater;
  ASSERT_TRUE(inflater.Initialize(15));
  std::string input("\xf2\x48\xcd\xc9\xc9\x07\x00", 7);
  scoped_refptr<IOBufferWithSize> actual;

  for (char& c : input) {
    ASSERT_TRUE(inflater.AddBytes(&c, 1));
  }
  ASSERT_TRUE(inflater.Finish());
  actual = inflater.GetOutput(5);
  ASSERT_TRUE(actual.get());
  EXPECT_EQ("Hello", ToString(actual.get()));
}

TEST(WebSocketInflaterTest, Reset) {
  WebSocketInflater inflater;
  ASSERT_TRUE(inflater.Initialize(15));
  scoped_refptr<IOBufferWithSize> actual1, actual2;

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  actual1 = inflater.GetOutput(inflater.CurrentOutputSize());
  ASSERT_TRUE(actual1.get());
  EXPECT_EQ("Hello", ToString(actual1.get()));
  EXPECT_EQ(0u, inflater.CurrentOutputSize());

  // Reset the stream with a block [BFINAL = 1, BTYPE = 00, LEN = 0]
  ASSERT_TRUE(inflater.AddBytes("\x01", 1));
  ASSERT_TRUE(inflater.Finish());
  ASSERT_EQ(0u, inflater.CurrentOutputSize());

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  actual2 = inflater.GetOutput(inflater.CurrentOutputSize());
  ASSERT_TRUE(actual2.get());
  EXPECT_EQ("Hello", ToString(actual2.get()));
  EXPECT_EQ(0u, inflater.CurrentOutputSize());
}

TEST(WebSocketInflaterTest, ResetAndLostContext) {
  WebSocketInflater inflater;
  scoped_refptr<IOBufferWithSize> actual1, actual2;
  ASSERT_TRUE(inflater.Initialize(15));

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  actual1 = inflater.GetOutput(inflater.CurrentOutputSize());
  ASSERT_TRUE(actual1.get());
  EXPECT_EQ("Hello", ToString(actual1.get()));
  EXPECT_EQ(0u, inflater.CurrentOutputSize());

  // Reset the stream with a block [BFINAL = 1, BTYPE = 00, LEN = 0]
  ASSERT_TRUE(inflater.AddBytes("\x01", 1));
  ASSERT_TRUE(inflater.Finish());
  ASSERT_EQ(0u, inflater.CurrentOutputSize());

  // The context is already reset.
  ASSERT_FALSE(inflater.AddBytes("\xf2\x00\x11\x00\x00", 5));
}

TEST(WebSocketInflaterTest, CallAddBytesAndFinishWithoutGetOutput) {
  WebSocketInflater inflater;
  scoped_refptr<IOBufferWithSize> actual1, actual2;
  ASSERT_TRUE(inflater.Initialize(15));

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  EXPECT_EQ(5u, inflater.CurrentOutputSize());

  // This is a test for memory leak detectors.
}

TEST(WebSocketInflaterTest, CallAddBytesAndFinishWithoutGetOutputChoked) {
  WebSocketInflater inflater(1, 1);
  scoped_refptr<IOBufferWithSize> actual1, actual2;
  ASSERT_TRUE(inflater.Initialize(15));

  ASSERT_TRUE(inflater.AddBytes("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ASSERT_TRUE(inflater.Finish());
  EXPECT_EQ(1u, inflater.CurrentOutputSize());

  // This is a test for memory leak detectors.
}

TEST(WebSocketInflaterTest, LargeRandomDeflateInflate) {
  const size_t size = 64 * 1024;
  LinearCongruentialGenerator generator(133);
  std::vector<char> input;
  std::vector<char> output;
  scoped_refptr<IOBufferWithSize> compressed;

  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  ASSERT_TRUE(deflater.Initialize(8));
  WebSocketInflater inflater(256, 256);
  ASSERT_TRUE(inflater.Initialize(8));

  for (size_t i = 0; i < size; ++i)
    input.push_back(static_cast<char>(generator.Generate()));

  ASSERT_TRUE(deflater.AddBytes(input.data(), input.size()));
  ASSERT_TRUE(deflater.Finish());

  compressed = deflater.GetOutput(deflater.CurrentOutputSize());

  ASSERT_TRUE(compressed.get());
  ASSERT_EQ(0u, deflater.CurrentOutputSize());

  ASSERT_TRUE(inflater.AddBytes(compressed->data(), compressed->size()));
  ASSERT_TRUE(inflater.Finish());

  while (inflater.CurrentOutputSize() > 0) {
    scoped_refptr<IOBufferWithSize> uncompressed =
        inflater.GetOutput(inflater.CurrentOutputSize());
    ASSERT_TRUE(uncompressed.get());
    output.insert(output.end(),
                  uncompressed->data(),
                  uncompressed->data() + uncompressed->size());
  }

  EXPECT_EQ(output, input);
}

}  // unnamed namespace

}  // namespace net
