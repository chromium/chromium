// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/typed_buffer.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

struct Data {
  // A variable size vector.
  int data[1];
};

}  // namespace

// Check that the default constructor does not allocate the buffer.
TEST(TypedBufferTest, Empty) {
  TypedBuffer<Data> buffer;
  EXPECT_FALSE(buffer.get());
  EXPECT_FALSE(buffer);
  EXPECT_EQ(buffer.length(), 0u);
}

// Check that allocating zero-size structure does not allocate the buffer.
TEST(TypedBufferTest, ZeroSize) {
  TypedBuffer<Data> buffer(0);
  EXPECT_FALSE(buffer.get());
  EXPECT_FALSE(buffer);
  EXPECT_EQ(buffer.length(), 0u);
}

// Test creation of a buffer and verify that the buffer accessors work.
TEST(TypedBufferTest, Basic) {
  TypedBuffer<Data> buffer(sizeof(int) * 10);
  EXPECT_TRUE(buffer.get());
  EXPECT_TRUE(buffer);
  EXPECT_EQ(buffer.length(), sizeof(int) * 10);

  // Make sure that operator*() syntax works.
  (*buffer).data[9] = 0x12345678;

  // Make sure that operator->() syntax works.
  EXPECT_EQ(buffer->data[9], 0x12345678);
}

// Test passing ownership.
TEST(TypedBufferTest, Pass) {
  TypedBuffer<Data> left;
  TypedBuffer<Data> right(sizeof(int));

  EXPECT_FALSE(left.get());
  EXPECT_EQ(left.length(), 0u);
  EXPECT_TRUE(right.get());
  EXPECT_EQ(right.length(), sizeof(int));

  Data* ptr = right.get();
  left = std::move(right);

  // Verify that passing ownership transfers both the buffer pointer and its
  // length.
  EXPECT_EQ(left.get(), ptr);
  EXPECT_EQ(left.length(), sizeof(int));

  // Verify that the original object was cleared.
  EXPECT_FALSE(right.get());
  EXPECT_EQ(right.length(), 0u);
}

// Test swapping ownership.
TEST(TypedBufferTest, Swap) {
  TypedBuffer<Data> left(sizeof(int));
  TypedBuffer<Data> right(sizeof(int) * 2);

  EXPECT_TRUE(left.get());
  EXPECT_EQ(left.length(), sizeof(int));
  EXPECT_TRUE(right.get());
  EXPECT_EQ(right.length(), sizeof(int) * 2);

  Data* raw_left = left.get();
  Data* raw_right = right.get();
  left.Swap(right);

  // Verify that swapping simply exchange contents of two objects.
  // length.
  EXPECT_EQ(left.get(), raw_right);
  EXPECT_EQ(left.length(), sizeof(int) * 2);
  EXPECT_EQ(right.get(), raw_left);
  EXPECT_EQ(right.length(), sizeof(int));
}

TEST(TypedBufferTest, GetAtOffset) {
  TypedBuffer<Data> buffer(sizeof(int) * 10);
  EXPECT_EQ(buffer.get(), buffer.GetAtOffset(0));
  EXPECT_EQ(reinterpret_cast<Data*>(&buffer->data[9]),
            buffer.GetAtOffset(sizeof(int) * 9));
}

}  // namespace remoting
