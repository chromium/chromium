// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/spdy/core/array_output_buffer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace spdy {
namespace test {

// This test verifies that ArrayOutputBuffer is initialized properly.
TEST(ArrayOutputBufferTest, InitializedFromArray) {
  char array[100];
  ArrayOutputBuffer buffer(array, sizeof(array));
  EXPECT_EQ(sizeof(array), buffer.BytesFree());
  EXPECT_EQ(0u, buffer.Size());
  EXPECT_EQ(array, buffer.Begin());
}

// This test verifies that Reset() causes an ArrayOutputBuffer's capacity and
// size to be reset to the initial state.
TEST(ArrayOutputBufferTest, WriteAndReset) {
  char array[100];
  ArrayOutputBuffer buffer(array, sizeof(array));

  // Let's write some bytes.
  char* dst;
  int size;
  buffer.Next(&dst, &size);
  ASSERT_GT(size, 1);
  ASSERT_NE(nullptr, dst);
  const int64_t written = size / 2;
  memset(dst, 'x', written);
  buffer.AdvanceWritePtr(written);

  // The buffer should be partially used.
  EXPECT_EQ(static_cast<uint64_t>(size) - written, buffer.BytesFree());
  EXPECT_EQ(static_cast<uint64_t>(written), buffer.Size());

  buffer.Reset();

  // After a reset, the buffer should regain its full capacity.
  EXPECT_EQ(sizeof(array), buffer.BytesFree());
  EXPECT_EQ(0u, buffer.Size());
}

}  // namespace test
}  // namespace spdy
