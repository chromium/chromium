// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/memory_allocator.h"

#include <stddef.h>

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

TEST(MemoryAllocatorTest, NoThrowBuffer) {
  const size_t size_list[] = {0U, 1U, 2U, 11U, 15U, 16U};

  // Repeat test for different sizes.
  for (size_t idx = 0; idx < std::size(size_list); ++idx) {
    size_t size = size_list[idx];

    courgette::NoThrowBuffer<size_t> buf1;
    EXPECT_EQ(0U, buf1.size());
    EXPECT_TRUE(buf1.empty());

    // Ensure reserve() should not affect size.
    EXPECT_TRUE(buf1.reserve(size / 2));
    EXPECT_EQ(0U, buf1.size());
    EXPECT_TRUE(buf1.empty());

    // Populate with integers from |size| - 1 to 0.
    for (size_t i = 0; i < size; ++i) {
      size_t new_value = size - 1 - i;
      EXPECT_TRUE(buf1.push_back(new_value));
      EXPECT_EQ(new_value, buf1.back());
      EXPECT_EQ(i + 1, buf1.size());
      EXPECT_FALSE(buf1.empty());
    }

    // Sort, and verify that list is indeed sorted.
    std::sort(buf1.begin(), buf1.end());
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(i, buf1[i]);

    // Test operator[] for read and write.
    for (size_t i = 0; i < size; ++i)
      buf1[i] = buf1[i] * 2;

    // Test append().
    courgette::NoThrowBuffer<size_t> buf2;

    if (size > 0) {
      EXPECT_TRUE(buf2.append(&buf1[0], size));
      EXPECT_EQ(size, buf2.size());
      for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(buf1[i], buf2[i]);
    }

    // Test shrinking by resize().
    const size_t kNewValue = 137;
    size_t new_size = size / 2;
    EXPECT_TRUE(buf2.resize(new_size, kNewValue));
    EXPECT_EQ(new_size, buf2.size());
    for (size_t i = 0; i < new_size; ++i)
      EXPECT_EQ(buf1[i], buf2[i]);

    // Test expanding by resize().
    EXPECT_TRUE(buf2.resize(size, kNewValue));
    EXPECT_EQ(size, buf2.size());
    for (size_t i = 0; i < new_size; ++i)
      EXPECT_EQ(buf1[i], buf2[i]);
    for (size_t i = new_size; i < size; ++i)
      EXPECT_EQ(kNewValue, buf2[i]);

    // Test clear().
    buf2.clear();
    EXPECT_EQ(0U, buf2.size());
    EXPECT_TRUE(buf2.empty());
  }
}
