// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/capabilities.h"

#include <stddef.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct HasCapabilityTestData {
  const char* capabilities;
  const char* key;
  bool result;
};

struct IntersectTestData {
  const char* left;
  const char* right;
  const char* result;
};

}  // namespace

namespace remoting {

TEST(CapabilitiesTest, Empty) {
  // Expect that nothing can be found in an empty set.
  EXPECT_FALSE(HasCapability("", "a"));
  EXPECT_FALSE(HasCapability(" ", "a"));
  EXPECT_FALSE(HasCapability("  ", "a"));

  // Expect that nothing can be found in an empty set, event when the key is
  // empty.
  EXPECT_FALSE(HasCapability("", ""));
  EXPECT_FALSE(HasCapability(" ", ""));
  EXPECT_FALSE(HasCapability("  ", ""));
}

TEST(CapabilitiesTest, HasCapability) {
  HasCapabilityTestData data[] = {
    { "", "", false },
    { "a", "", false },
    { "a", "a", true },
    { "a a", "", false },
    { "a a", "a", true },
    { "a a", "z", false },
    { "a b", "", false },
    { "a b", "a", true },
    { "a b", "b", true },
    { "a b", "z", false },
    { "a b c", "", false },
    { "a b c", "a", true },
    { "a b c", "b", true },
    { "a b c", "z", false }
  };

  // Verify that HasCapability(|capabilities|, |key|) returns |result|.
  // |result|.
  for (size_t i = 0; i < std::size(data); ++i) {
    std::vector<std::string_view> caps =
        base::SplitStringPiece(data[i].capabilities, " ", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    do {
      EXPECT_EQ(data[i].result,
                HasCapability(base::JoinString(caps, " "), data[i].key));
    } while (std::next_permutation(caps.begin(), caps.end()));
  }
}

TEST(CapabilitiesTest, Intersect) {
  EXPECT_EQ(IntersectCapabilities("a", "a"), "a");

  IntersectTestData data[] = {
    { "", "", "" },
    { "a", "", "" },
    { "a", "a", "a" },
    { "a", "b", "" },
    { "a b", "", "" },
    { "a b", "a", "a" },
    { "a b", "b", "b" },
    { "a b", "z", "" },
    { "a b c", "a", "a" },
    { "a b c", "b", "b" },
    { "a b c", "a b", "a b" },
    { "a b c", "b a", "a b" },
    { "a b c", "z", "" }
  };

  // Verify that intersection of |right| with all permutations of |left| yields
  // |result|.
  for (size_t i = 0; i < std::size(data); ++i) {
    std::vector<std::string_view> caps = base::SplitStringPiece(
        data[i].left, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    do {
      EXPECT_EQ(data[i].result,
                IntersectCapabilities(base::JoinString(caps, " "),
                                      data[i].right));
    } while (std::next_permutation(caps.begin(), caps.end()));
  }
}

}  // namespace remoting
