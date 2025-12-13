// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_style_constants.h"

#include <set>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PseudoIdFlagsTest, DefaultConstructor) {
  PseudoIdFlags flags;
  EXPECT_FALSE(flags.HasAny());
}

TEST(PseudoIdFlagsTest, ListConstructor) {
  PseudoIdFlags flags({kPseudoIdBefore, kPseudoIdSelection});
  std::set<int> expected = {kPseudoIdBefore, kPseudoIdSelection};
  for (int i = PseudoIdFlags::kFirstValid; i <= PseudoIdFlags::kLastValid;
       ++i) {
    EXPECT_EQ(expected.count(i), flags.Has(static_cast<PseudoId>(i)));
  }
}

TEST(PseudoIdFlagsTest, Or) {
  PseudoIdFlags flags_a({kPseudoIdBefore, kPseudoIdSelection});
  PseudoIdFlags flags_b({kPseudoIdAfter, kPseudoIdSelection});
  PseudoIdFlags flags = flags_a;
  flags |= flags_b;

  std::set<int> expected = {kPseudoIdBefore, kPseudoIdAfter,
                            kPseudoIdSelection};
  for (int i = PseudoIdFlags::kFirstValid; i <= PseudoIdFlags::kLastValid;
       ++i) {
    EXPECT_EQ(expected.count(i), flags.Has(static_cast<PseudoId>(i)));
  }
}

TEST(PseudoIdFlagsTest, Set) {
  PseudoIdFlags flags;

  flags.Set(kPseudoIdBefore);
  flags.Set(kPseudoIdAfter);
  flags.Set(kPseudoIdSelection);

  std::set<int> expected = {kPseudoIdBefore, kPseudoIdAfter,
                            kPseudoIdSelection};
  for (int i = PseudoIdFlags::kFirstValid; i <= PseudoIdFlags::kLastValid;
       ++i) {
    EXPECT_EQ(expected.count(i), flags.Has(static_cast<PseudoId>(i)));
  }
}

TEST(PseudoIdFlagsTest, Reset_Assignment) {
  PseudoIdFlags flags;
  EXPECT_FALSE(flags.HasAny());

  flags.Set(kPseudoIdBefore);
  flags.Set(kPseudoIdAfter);
  flags.Set(kPseudoIdSelection);
  EXPECT_TRUE(flags.HasAny());

  flags = PseudoIdFlags();
  EXPECT_FALSE(flags.HasAny());
}

TEST(PseudoIdFlagsTest, Equality) {
  PseudoIdFlags before1({kPseudoIdBefore});
  PseudoIdFlags before2({kPseudoIdBefore});
  PseudoIdFlags after({kPseudoIdAfter});
  PseudoIdFlags both({kPseudoIdBefore, kPseudoIdAfter});

  EXPECT_EQ(before1, before2);
  EXPECT_EQ(before2, before1);
  EXPECT_NE(before1, after);
  EXPECT_NE(after, before1);
  EXPECT_NE(before1, both);
  EXPECT_NE(after, both);
}

}  // namespace blink
