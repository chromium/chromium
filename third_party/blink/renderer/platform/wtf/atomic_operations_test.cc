// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/atomic_operations.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class AtomicOperationsTest : public ::testing::Test {};

template <size_t buffer_size, size_t alignment, typename CopyMethod>
void TestCopyImpl(CopyMethod copy) {
  alignas(alignment) unsigned char src[buffer_size];
  for (size_t i = 0; i < buffer_size; ++i)
    src[i] = static_cast<char>(i + 1);
  // Allocating extra memory before and after the buffer to make sure the
  // atomic memcpy doesn't exceed the buffer in any direction.
  alignas(alignment) unsigned char tgt[buffer_size + (2 * sizeof(size_t))];
  memset(tgt, 0, buffer_size + (2 * sizeof(size_t)));
  copy(tgt + sizeof(size_t), src);
  // Check nothing before the buffer was changed
  size_t v;
  memcpy(&v, tgt, sizeof(size_t));
  EXPECT_EQ(0u, v);
  // Check buffer was copied correctly
  EXPECT_TRUE(!memcmp(src, tgt + sizeof(size_t), buffer_size));
  // Check nothing after the buffer was changed
  memcpy(&v, tgt + sizeof(size_t) + buffer_size, sizeof(size_t));
  EXPECT_EQ(0u, v);
}

// Tests for AtomicReadMemcpy
template <size_t buffer_size, size_t alignment>
void TestAtomicReadMemcpy() {
  TestCopyImpl<buffer_size, alignment>(
      AtomicReadMemcpy<buffer_size, alignment>);
}

TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT8T) {
  TestAtomicReadMemcpy<sizeof(uint8_t), sizeof(uint32_t)>();
  TestAtomicReadMemcpy<sizeof(uint8_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT16T) {
  TestAtomicReadMemcpy<sizeof(uint16_t), sizeof(uint32_t)>();
  TestAtomicReadMemcpy<sizeof(uint16_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT32T) {
  TestAtomicReadMemcpy<sizeof(uint32_t), sizeof(uint32_t)>();
  TestAtomicReadMemcpy<sizeof(uint32_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_UINT64T) {
  TestAtomicReadMemcpy<sizeof(uint64_t), sizeof(uint32_t)>();
  TestAtomicReadMemcpy<sizeof(uint64_t), sizeof(uintptr_t)>();
}

TEST_F(AtomicOperationsTest, AtomicReadMemcpy_17Bytes) {
  TestAtomicReadMemcpy<17, sizeof(uint32_t)>();
  TestAtomicReadMemcpy<17, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_34Bytes) {
  TestAtomicReadMemcpy<34, sizeof(uint32_t)>();
  TestAtomicReadMemcpy<34, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_68Bytes) {
  TestAtomicReadMemcpy<68, sizeof(uint32_t)>();
  TestAtomicReadMemcpy<68, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicReadMemcpy_127Bytes) {
  TestAtomicReadMemcpy<127, sizeof(uint32_t)>();
  TestAtomicReadMemcpy<127, sizeof(uintptr_t)>();
}

// Tests for AtomicWriteMemcpy
template <size_t buffer_size, size_t alignment>
void TestAtomicWriteMemcpy() {
  TestCopyImpl<buffer_size, alignment>(
      AtomicWriteMemcpy<buffer_size, alignment>);
}

TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT8T) {
  TestAtomicWriteMemcpy<sizeof(uint8_t), sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<sizeof(uint8_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT16T) {
  TestAtomicWriteMemcpy<sizeof(uint16_t), sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<sizeof(uint16_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT32T) {
  TestAtomicWriteMemcpy<sizeof(uint32_t), sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<sizeof(uint32_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_UINT64T) {
  TestAtomicWriteMemcpy<sizeof(uint64_t), sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<sizeof(uint64_t), sizeof(uintptr_t)>();
}

TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_17Bytes) {
  TestAtomicWriteMemcpy<17, sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<17, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_34Bytes) {
  TestAtomicWriteMemcpy<34, sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<34, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_68Bytes) {
  TestAtomicWriteMemcpy<68, sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<68, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicWriteMemcpy_127Bytes) {
  TestAtomicWriteMemcpy<127, sizeof(uint32_t)>();
  TestAtomicWriteMemcpy<127, sizeof(uintptr_t)>();
}

// Tests for AtomicMemzero
template <size_t buffer_size, size_t alignment>
void TestAtomicMemzero() {
  // Allocating extra memory before and after the buffer to make sure the
  // AtomicMemzero doesn't exceed the buffer in any direction.
  alignas(alignment) unsigned char buf[buffer_size + (2 * sizeof(size_t))];
  memset(buf, ~uint8_t{0}, buffer_size + (2 * sizeof(size_t)));
  AtomicMemzero<buffer_size, alignment>(buf + sizeof(size_t));
  // Check nothing before the buffer was changed
  size_t v;
  memcpy(&v, buf, sizeof(size_t));
  EXPECT_EQ(~size_t{0}, v);
  // Check buffer was copied correctly
  static const unsigned char for_comparison[buffer_size] = {0};
  EXPECT_TRUE(!memcmp(buf + sizeof(size_t), for_comparison, buffer_size));
  // Check nothing after the buffer was changed
  memcpy(&v, buf + sizeof(size_t) + buffer_size, sizeof(size_t));
  EXPECT_EQ(~size_t{0}, v);
}

TEST_F(AtomicOperationsTest, AtomicMemzero_UINT8T) {
  TestAtomicMemzero<sizeof(uint8_t), sizeof(uint32_t)>();
  TestAtomicMemzero<sizeof(uint8_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_UINT16T) {
  TestAtomicMemzero<sizeof(uint16_t), sizeof(uint32_t)>();
  TestAtomicMemzero<sizeof(uint16_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_UINT32T) {
  TestAtomicMemzero<sizeof(uint32_t), sizeof(uint32_t)>();
  TestAtomicMemzero<sizeof(uint32_t), sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_UINT64T) {
  TestAtomicMemzero<sizeof(uint64_t), sizeof(uint32_t)>();
  TestAtomicMemzero<sizeof(uint64_t), sizeof(uintptr_t)>();
}

TEST_F(AtomicOperationsTest, AtomicMemzero_17Bytes) {
  TestAtomicMemzero<17, sizeof(uint32_t)>();
  TestAtomicMemzero<17, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_34Bytes) {
  TestAtomicMemzero<34, sizeof(uint32_t)>();
  TestAtomicMemzero<34, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_68Bytes) {
  TestAtomicMemzero<68, sizeof(uint32_t)>();
  TestAtomicMemzero<68, sizeof(uintptr_t)>();
}
TEST_F(AtomicOperationsTest, AtomicMemzero_127Bytes) {
  TestAtomicMemzero<127, sizeof(uint32_t)>();
  TestAtomicMemzero<127, sizeof(uintptr_t)>();
}

}  // namespace WTF
