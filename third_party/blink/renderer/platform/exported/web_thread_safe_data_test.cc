// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_thread_safe_data.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(WebThreadSafeDataTest, Construction) {
  {
    // Null construction.
    WebThreadSafeData d;
    EXPECT_EQ(d.size(), 0u);
    EXPECT_EQ(d.data(), nullptr);
  }

  {
    // Construction from a data block.
    WebThreadSafeData d("abc", 4);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_STREQ(d.data(), "abc");
  }

  {
    // Construction explicitly from a null pointer.
    WebThreadSafeData d(nullptr, 0);
    EXPECT_EQ(d.size(), 0u);
    EXPECT_EQ(d.data(), nullptr);
  }

  {
    // Copy construction.
    WebThreadSafeData d1("abc", 4);
    WebThreadSafeData d2(d1);
    EXPECT_EQ(d2.size(), 4u);
    EXPECT_STREQ(d2.data(), "abc");
  }
}

TEST(WebThreadSafeDataTest, Modification) {
  WebThreadSafeData d1("abc", 4);
  WebThreadSafeData d2;

  // Copy d1 to d2.
  d2 = d1;
  EXPECT_EQ(d2.size(), 4u);
  EXPECT_STREQ(d2.data(), "abc");

  // d1 should not have been modified.
  EXPECT_EQ(d1.size(), 4u);
  EXPECT_STREQ(d1.data(), "abc");

  // Reset d1.
  d1.Reset();
  EXPECT_EQ(d1.size(), 0u);
  EXPECT_EQ(d1.data(), nullptr);

  // d2 should not have been modified.
  EXPECT_EQ(d2.size(), 4u);
  EXPECT_STREQ(d2.data(), "abc");

  // Try copying again, this time with Assign().
  d1.Assign(d2);
  EXPECT_EQ(d1.size(), 4u);
  EXPECT_STREQ(d1.data(), "abc");

  // d2 should not have been modified.
  EXPECT_EQ(d2.size(), 4u);
  EXPECT_STREQ(d2.data(), "abc");

  // Reset both. No double-free should occur.
  d1.Reset();
  EXPECT_EQ(d1.size(), 0u);
  EXPECT_EQ(d1.data(), nullptr);
  d2.Reset();
  EXPECT_EQ(d2.size(), 0u);
  EXPECT_EQ(d2.data(), nullptr);
}

TEST(WebThreadSafeDataTest, Access) {
  // Explicit, via begin()/end().
  WebThreadSafeData d1("abc", 3);
  EXPECT_FALSE(d1.IsEmpty());
  for (auto it = d1.begin(); it != d1.end(); ++it) {
    EXPECT_EQ(*it, base::span_from_cstring("abc")[it - d1.begin()]);
  }

  // Implicit, via range-for.
  char expected = 'a';
  for (char c : d1) {
    EXPECT_EQ(c, expected++);
  }

  // Implicit, via span.
  base::span<const char> s1(d1);
  EXPECT_EQ(s1, base::span_from_cstring("abc"));

  // Try again with an empty obj.
  WebThreadSafeData d2;
  EXPECT_TRUE(d2.IsEmpty());
  for (auto it = d2.begin(); it != d2.end(); ++it) {
    ADD_FAILURE();  // Should not reach here.
  }
  for ([[maybe_unused]] char c : d2) {
    ADD_FAILURE();  // Or here.
  }
  base::span<const char> s2(d2);
  EXPECT_EQ(s2, base::span<const char>());
}

}  // namespace blink
