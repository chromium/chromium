// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class AtomicOperationsTest : public ::testing::Test {};

template <size_t buffer_size, typename CopyMethod>
void TestCopyImpl(CopyMethod copy) {
  alignas(sizeof(size_t)) unsigned char src[buffer_size];
  for (size_t i = 0; i < buffer_size; ++i)
    src[i] = static_cast<char>(i + 1);
  // Allocating extra memory before and after the buffer to make sure the
  // atomic memcpy doesn't exceed the buffer in any direction.
  alignas(sizeof(size_t)) unsigned char tgt[buffer_size + (2 * sizeof(size_t))];
  memset(tgt, 0, buffer_size + (2 * sizeof(size_t)));
  copy(tgt + sizeof(size_t), src);
  // Check nothing before the buffer was changed
  EXPECT_EQ(0u, *reinterpret_cast<size_t*>(&tgt[0]));
  // Check buffer was copied correctly
  EXPECT_TRUE(!memcmp(src, tgt + sizeof(size_t), buffer_size));
  // Check nothing after the buffer was changed
  EXPECT_EQ(0u, *reinterpret_cast<size_t*>(&tgt[sizeof(size_t) + buffer_size]));
}

// Tests for AtomicReadMemcpy
template <size_t buffer_size>
void TestAtomicReadMemcpy() {
  TestCopyImpl<buffer_size>(AtomicReadMemcpy<buffer_size>);
}

TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT8T) {
  TestAtomicReadMemcpy<sizeof(uint8_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT16T) {
  TestAtomicReadMemcpy<sizeof(uint16_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT32T) {
  TestAtomicReadMemcpy<sizeof(uint32_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT64T) {
  TestAtomicReadMemcpy<sizeof(uint64_t)>();
}

TEST_F(AtomicOperationsTest, AtomicReadMemcpy_17Bytes) {
  TestAtomicReadMemcpy<17>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_34Bytes) {
  TestAtomicReadMemcpy<34>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_68Bytes) {
  TestAtomicReadMemcpy<68>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_127Bytes) {
  TestAtomicReadMemcpy<127>();
}

// Tests for AtomicWriteMemcpy
template <size_t buffer_size>
void TestAtomicWriteMemcpy() {
  TestCopyImpl<buffer_size>(AtomicWriteMemcpy<buffer_size>);
}

TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT8T) {
  TestAtomicWriteMemcpy<sizeof(uint8_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT16T) {
  TestAtomicWriteMemcpy<sizeof(uint16_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT32T) {
  TestAtomicWriteMemcpy<sizeof(uint32_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT64T) {
  TestAtomicWriteMemcpy<sizeof(uint64_t)>();
}

TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_17Bytes) {
  TestAtomicWriteMemcpy<17>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_34Bytes) {
  TestAtomicWriteMemcpy<34>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_68Bytes) {
  TestAtomicWriteMemcpy<68>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_127Bytes) {
  TestAtomicWriteMemcpy<127>();
}

// Tests for AtomicMemzero
template <size_t buffer_size>
void TestAtomicMemzero() {
  // Allocating extra memory before and after the buffer to make sure the
  // AtomicMemzero doesn't exceed the buffer in any direction.
  alignas(sizeof(size_t)) unsigned char buf[buffer_size + (2 * sizeof(size_t))];
  memset(buf, ~uint8_t{0}, buffer_size + (2 * sizeof(size_t)));
  AtomicMemzero<buffer_size>(buf + sizeof(size_t));
  // Check nothing before the buffer was changed
  EXPECT_EQ(~size_t{0}, *reinterpret_cast<size_t*>(&buf[0]));
  // Check buffer was copied correctly
  static const unsigned char for_comparison[buffer_size] = {0};
  EXPECT_TRUE(!memcmp(buf + sizeof(size_t), for_comparison, buffer_size));
  // Check nothing after the buffer was changed
  EXPECT_EQ(~size_t{0},
            *reinterpret_cast<size_t*>(&buf[sizeof(size_t) + buffer_size]));
}

TEST_F(AtomicOperationsTest, AtomicMemzero_UINT8T) {
  TestAtomicMemzero<sizeof(uint8_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_UINT16T) {
  TestAtomicMemzero<sizeof(uint16_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_UINT32T) {
  TestAtomicMemzero<sizeof(uint32_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_UINT64T) {
  TestAtomicMemzero<sizeof(uint64_t)>();
}

TEST_F(AtomicOperationsTest, AtomicMemzero_17Bytes) {
  TestAtomicMemzero<17>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_34Bytes) {
  TestAtomicMemzero<34>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_68Bytes) {
  TestAtomicMemzero<68>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_127Bytes) {
  TestAtomicMemzero<127>();
}

}  // namespace WTF
