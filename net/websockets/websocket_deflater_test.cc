// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_deflater.h"

#include <string>

#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string ToString(IOBufferWithSize* buffer) {
  return std::string(buffer->data(), buffer->size());
}

TEST(WebSocketDeflaterTest, Construct) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  deflater.Initialize(8);
  ASSERT_EQ(0u, deflater.CurrentOutputSize());
  ASSERT_TRUE(deflater.Finish());
  scoped_refptr<IOBufferWithSize> actual =
      deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\00", 1), ToString(actual.get()));
  ASSERT_EQ(0u, deflater.CurrentOutputSize());
}

TEST(WebSocketDeflaterTest, DeflateHelloTakeOverContext) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  deflater.Initialize(15);
  scoped_refptr<IOBufferWithSize> actual1, actual2;

  ASSERT_TRUE(deflater.AddBytes("Hello", 5));
  ASSERT_TRUE(deflater.Finish());
  actual1 = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(actual1.get()));

  ASSERT_TRUE(deflater.AddBytes("Hello", 5));
  ASSERT_TRUE(deflater.Finish());
  actual2 = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\xf2\x00\x11\x00\x00", 5), ToString(actual2.get()));
}

TEST(WebSocketDeflaterTest, DeflateHelloDoNotTakeOverContext) {
  WebSocketDeflater deflater(WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT);
  deflater.Initialize(15);
  scoped_refptr<IOBufferWithSize> actual1, actual2;

  ASSERT_TRUE(deflater.AddBytes("Hello", 5));
  ASSERT_TRUE(deflater.Finish());
  actual1 = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(actual1.get()));

  ASSERT_TRUE(deflater.AddBytes("Hello", 5));
  ASSERT_TRUE(deflater.Finish());
  actual2 = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(actual2.get()));
}

TEST(WebSocketDeflaterTest, MultipleAddBytesCalls) {
  WebSocketDeflater deflater(WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT);
  deflater.Initialize(15);
  std::string input(32, 'a');
  scoped_refptr<IOBufferWithSize> actual;

  for (char& c : input) {
    ASSERT_TRUE(deflater.AddBytes(&c, 1));
  }
  ASSERT_TRUE(deflater.Finish());
  actual = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\x4a\x4c\xc4\x0f\x00\x00", 6), ToString(actual.get()));
}

TEST(WebSocketDeflaterTest, GetMultipleDeflatedOutput) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  deflater.Initialize(15);
  scoped_refptr<IOBufferWithSize> actual;

  ASSERT_TRUE(deflater.AddBytes("Hello", 5));
  ASSERT_TRUE(deflater.Finish());
  deflater.PushSyncMark();
  ASSERT_TRUE(deflater.Finish());
  deflater.PushSyncMark();
  ASSERT_TRUE(deflater.AddBytes("Hello", 5));
  ASSERT_TRUE(deflater.Finish());

  actual = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00\x00\x00\xff\xff"
                        "\x00\x00\x00\xff\xff"
                        "\xf2\x00\x11\x00\x00", 21),
            ToString(actual.get()));
  ASSERT_EQ(0u, deflater.CurrentOutputSize());
}

TEST(WebSocketDeflaterTest, WindowBits8) {
  WebSocketDeflater deflater(WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT);
  deflater.Initialize(8);
  // Set the head and tail of |input| so that back-reference
  // can be used if the window size is sufficiently-large.
  const std::string word = "Chromium";
  std::string input = word + std::string(256, 'a') + word;
  scoped_refptr<IOBufferWithSize> actual;

  ASSERT_TRUE(deflater.AddBytes(input.data(), input.size()));
  ASSERT_TRUE(deflater.Finish());
  actual = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(std::string("r\xce(\xca\xcf\xcd,\xcdM\x1c\xe1\xc0\x39\xa3"
                        "(?7\xb3\x34\x17\x00", 21),
            ToString(actual.get()));
}

TEST(WebSocketDeflaterTest, WindowBits10) {
  WebSocketDeflater deflater(WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT);
  deflater.Initialize(10);
  // Set the head and tail of |input| so that back-reference
  // can be used if the window size is sufficiently-large.
  const std::string word = "Chromium";
  std::string input = word + std::string(256, 'a') + word;
  scoped_refptr<IOBufferWithSize> actual;

  ASSERT_TRUE(deflater.AddBytes(input.data(), input.size()));
  ASSERT_TRUE(deflater.Finish());
  actual = deflater.GetOutput(deflater.CurrentOutputSize());
  EXPECT_EQ(
      std::string("r\xce(\xca\xcf\xcd,\xcdM\x1c\xe1\xc0\x19\x1a\x0e\0\0", 17),
      ToString(actual.get()));
}

}  // namespace

}  // namespace net
