// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/addition_overlaps_union_find.h"

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace {

TEST(AdditionOverlapsUnionFindUnittest, InvalidNumSets) {
  EXPECT_CHECK_DEATH(AdditionOverlapsUnionFind(-1));
}

TEST(AdditionOverlapsUnionFindUnittest, EmptyUnionFind_Union_BoundsCheckFails) {
  AdditionOverlapsUnionFind union_find(0);
  EXPECT_CHECK_DEATH(union_find.Union(0, 0));
}

TEST(AdditionOverlapsUnionFindUnittest, Union_BoundsCheckFails) {
  AdditionOverlapsUnionFind union_find(3);

  // Test lower bound of [0, |num_sets|)
  EXPECT_CHECK_DEATH(union_find.Union(-1, 0));
  EXPECT_CHECK_DEATH(union_find.Union(0, -1));

  // Test upper bound of [0, |num_sets|)
  EXPECT_CHECK_DEATH(union_find.Union(0, 3));
  EXPECT_CHECK_DEATH(union_find.Union(3, 0));
}

TEST(AdditionOverlapsUnionFindUnittest, SetsAreTheirInitRepresentatives) {
  EXPECT_THAT(
      AdditionOverlapsUnionFind(4).SetsMapping(),
      AdditionOverlapsUnionFind::SetsMap({{0, {}}, {1, {}}, {2, {}}, {3, {}}}));
}

TEST(AdditionOverlapsUnionFindUnittest, Union_ChoosesLesserSetIndex) {
  AdditionOverlapsUnionFind union_find(3);

  union_find.Union(1, 2);
  EXPECT_THAT(union_find.SetsMapping(),
              AdditionOverlapsUnionFind::SetsMap({{0, {}}, {1, {2}}}));

  union_find.Union(0, 1);
  EXPECT_THAT(union_find.SetsMapping(), AdditionOverlapsUnionFind::SetsMap({
                                            {0, {1, 2}},
                                        }));
}

TEST(AdditionOverlapsUnionFindUnittest, Union_NoOp_SameSet) {
  AdditionOverlapsUnionFind uf(4);
  for (int i = 0; i < 4; i++) {
    uf.Union(i, i);
  }
  EXPECT_THAT(
      AdditionOverlapsUnionFind(4).SetsMapping(),
      AdditionOverlapsUnionFind::SetsMap({{0, {}}, {1, {}}, {2, {}}, {3, {}}}));
}

TEST(AdditionOverlapsUnionFindUnittest, Union_NoOp_SharedRepresentative) {
  AdditionOverlapsUnionFind union_find(4);

  union_find.Union(0, 2);
  EXPECT_THAT(union_find.SetsMapping(),
              AdditionOverlapsUnionFind::SetsMap({{0, {2}}, {1, {}}, {3, {}}}));

  union_find.Union(0, 2);
  EXPECT_THAT(union_find.SetsMapping(),
              AdditionOverlapsUnionFind::SetsMap({{0, {2}}, {1, {}}, {3, {}}}));

  union_find.Union(2, 0);
  EXPECT_THAT(union_find.SetsMapping(),
              AdditionOverlapsUnionFind::SetsMap({{0, {2}}, {1, {}}, {3, {}}}));
}

}  // namespace
}  // namespace net
