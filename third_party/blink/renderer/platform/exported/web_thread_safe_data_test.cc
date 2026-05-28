// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_thread_safe_data.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(WebThreadSafeDataTest, Construction) {
  const auto kAbcNul = base::byte_span_with_nul_from_cstring("abc");

  {
    // Null construction.
    WebThreadSafeData d;
    EXPECT_EQ(d.size(), 0u);
    EXPECT_EQ(d.data(), nullptr);
  }

  {
    // Construction from a data block.
    WebThreadSafeData d(kAbcNul);
    EXPECT_EQ(d.size(), 4u);
    EXPECT_EQ(base::span(d), kAbcNul);
  }

  {
    // Construction explicitly from a null pointer.
    WebThreadSafeData d(base::span<const uint8_t>{});
    EXPECT_EQ(d.size(), 0u);
    EXPECT_EQ(d.data(), nullptr);
  }

  {
    // Copy construction.
    WebThreadSafeData d1(kAbcNul);
    WebThreadSafeData d2(d1);
    EXPECT_EQ(d2.size(), 4u);
    EXPECT_EQ(base::span(d2), kAbcNul);
  }
}

TEST(WebThreadSafeDataTest, Modification) {
  const auto kAbcNul = base::byte_span_with_nul_from_cstring("abc");
  WebThreadSafeData d1(kAbcNul);
  WebThreadSafeData d2;

  // Copy d1 to d2.
  d2 = d1;
  EXPECT_EQ(d2.size(), 4u);
  EXPECT_EQ(base::span(d2), kAbcNul);

  // d1 should not have been modified.
  EXPECT_EQ(d1.size(), 4u);
  EXPECT_EQ(base::span(d1), kAbcNul);

  // Reset d1.
  d1.Reset();
  EXPECT_EQ(d1.size(), 0u);
  EXPECT_EQ(d1.data(), nullptr);

  // d2 should not have been modified.
  EXPECT_EQ(d2.size(), 4u);
  EXPECT_EQ(base::span(d2), kAbcNul);

  // Try copying again, this time with Assign().
  d1.Assign(d2);
  EXPECT_EQ(d1.size(), 4u);
  EXPECT_EQ(base::span(d1), kAbcNul);

  // d2 should not have been modified.
  EXPECT_EQ(d2.size(), 4u);
  EXPECT_EQ(base::span(d2), kAbcNul);

  // Reset both. No double-free should occur.
  d1.Reset();
  EXPECT_EQ(d1.size(), 0u);
  EXPECT_EQ(d1.data(), nullptr);
  d2.Reset();
  EXPECT_EQ(d2.size(), 0u);
  EXPECT_EQ(d2.data(), nullptr);
}

TEST(WebThreadSafeDataTest, Access) {
  const auto kAbc = base::byte_span_from_cstring("abc");
  // Explicit, via begin()/end().
  WebThreadSafeData d1(kAbc);
  EXPECT_FALSE(d1.IsEmpty());
  for (auto it = d1.begin(); it != d1.end(); ++it) {
    EXPECT_EQ(*it, kAbc[static_cast<size_t>(it - d1.begin())]);
  }

  // Implicit, via range-for.
  uint8_t expected = 'a';
  for (uint8_t c : d1) {
    EXPECT_EQ(c, expected++);
  }

  // Implicit, via span.
  base::span<const uint8_t> s1(d1);
  EXPECT_EQ(s1, base::span_from_cstring("abc"));

  // Try again with an empty obj.
  WebThreadSafeData d2;
  EXPECT_TRUE(d2.IsEmpty());
  for (auto it = d2.begin(); it != d2.end(); ++it) {
    ADD_FAILURE();  // Should not reach here.
  }
  for ([[maybe_unused]] uint8_t c : d2) {
    ADD_FAILURE();  // Or here.
  }
  base::span<const uint8_t> s2(d2);
  EXPECT_EQ(s2, base::span<const uint8_t>());
}

}  // namespace blink
