// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/suffix_tree.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SuffixTreeTest, EmptyString) {
  SuffixTree<ASCIICodebook> tree("", 16);
  EXPECT_TRUE(tree.MightContain(""));
  EXPECT_FALSE(tree.MightContain("potato"));
}

TEST(SuffixTreeTest, NormalString) {
  SuffixTree<ASCIICodebook> tree("banana", 16);
  EXPECT_TRUE(tree.MightContain(""));
  EXPECT_TRUE(tree.MightContain("a"));
  EXPECT_TRUE(tree.MightContain("na"));
  EXPECT_TRUE(tree.MightContain("ana"));
  EXPECT_TRUE(tree.MightContain("nana"));
  EXPECT_TRUE(tree.MightContain("anana"));
  EXPECT_TRUE(tree.MightContain("banana"));
  EXPECT_FALSE(tree.MightContain("ab"));
  EXPECT_FALSE(tree.MightContain("bananan"));
  EXPECT_FALSE(tree.MightContain("abanana"));
  EXPECT_FALSE(tree.MightContain("potato"));
}

}  // namespace blink
