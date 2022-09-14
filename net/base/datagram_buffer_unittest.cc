// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/datagram_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

const size_t kMaxBufferSize = 1024;

class DatagramBufferTest : public testing::Test {
 public:
  DatagramBufferTest() : pool_(kMaxBufferSize) {}

  DatagramBufferPool pool_;
};

TEST_F(DatagramBufferTest, EnqueueCopiesData) {
  DatagramBuffers buffers;
  const char data[] = "foo";
  pool_.Enqueue(data, sizeof(data), &buffers);
  EXPECT_EQ(sizeof(data), buffers.front()->length());
  EXPECT_NE(data, buffers.front()->data());
  EXPECT_EQ(0, memcmp(data, buffers.front()->data(), sizeof(data)));
}

TEST_F(DatagramBufferTest, DatgramBufferPoolRecycles) {
  DatagramBuffers buffers;
  const char data1[] = "foo";
  pool_.Enqueue(data1, sizeof(data1), &buffers);
  DatagramBuffer* buffer1_ptr = buffers.back().get();
  EXPECT_EQ(1u, buffers.size());
  const char data2[] = "bar";
  pool_.Enqueue(data2, sizeof(data2), &buffers);
  DatagramBuffer* buffer2_ptr = buffers.back().get();
  EXPECT_EQ(2u, buffers.size());
  pool_.Dequeue(&buffers);
  EXPECT_EQ(0u, buffers.size());
  const char data3[] = "baz";
  pool_.Enqueue(data3, sizeof(data3), &buffers);
  EXPECT_EQ(1u, buffers.size());
  EXPECT_EQ(buffer1_ptr, buffers.back().get());
  const char data4[] = "bag";
  pool_.Enqueue(data4, sizeof(data4), &buffers);
  EXPECT_EQ(2u, buffers.size());
  EXPECT_EQ(buffer2_ptr, buffers.back().get());
}

}  // namespace net::test
