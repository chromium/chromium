// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/find_length_of_declaration_list-inl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#if defined(__SSE2__) || defined(__ARM_NEON__)

static bool BlockAccepted(const String& str) {
  // Close the block, then add various junk afterwards to make sure
  // that it doesn't affect the parsing. (We also need a fair bit of
  // padding since the SIMD code needs there to be room after the end
  // of the block.)
  String test_str = str + "}abcdefghi jkl!{}\\\"\\#/*[]                 ";
  return FindLengthOfDeclarationList(test_str) == str.length();
}

TEST(FindLengthOfDeclarationListTest, Basic) {
  EXPECT_TRUE(BlockAccepted("color: red;"));
}

TEST(FindLengthOfDeclarationListTest, Variable) {
  EXPECT_TRUE(BlockAccepted("color: var(--color);"));
  EXPECT_TRUE(BlockAccepted("color: var(--variable-name-that-spans-blocks);"));
}

TEST(FindLengthOfDeclarationListTest, UnbalancedVariable) {
  // The closing brace here should be ignored as an unbalanced block-end
  // token, so we should hit the junk afterwards and stop with an error.
  EXPECT_FALSE(BlockAccepted("color: var("));

  // An underflow; we could ignore them, but it's easier to throw an error.
  EXPECT_FALSE(BlockAccepted("color: var()) red green blue"));

  // There are 200 of these; they will cause an overflow. That is just a
  // limitation, but we need to at least detect it.
  EXPECT_FALSE(
      BlockAccepted("color: var"
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "))))))))))))))))))))))))))))))))))))))))))))))))))"
                    "))))))))))))))))))))))))))))))))))))))))))))))))))"
                    "))))))))))))))))))))))))))))))))))))))))))))))))))"
                    "))))))))))))))))))))))))))))))))))))))))))))))))))"));

  // If we did not have overflow detection, this (256 left-parens)
  // would seem acceptable.
  EXPECT_FALSE(
      BlockAccepted("color: var"
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((((((((((((((((((((((((((((((((((((((((((((((("
                    "(((((("));

  // Parens after the end must not be counted.
  EXPECT_EQ(0u, FindLengthOfDeclarationList("a:(()})paddingpaddingpadding"));
}

TEST(FindLengthOfDeclarationListTest, NoSubBlocksAccepted) {
  // Some of these are by design, some of these are just because of
  // limitations in the algorithm.
  EXPECT_FALSE(BlockAccepted(".a { --nested-rule: nope; }"));
  EXPECT_FALSE(BlockAccepted("--foo: []"));
  EXPECT_FALSE(BlockAccepted("--foo: {}"));
}

TEST(FindLengthOfDeclarationListTest, NoCommentsAccepted) {
  // This is also just a limitation in the algorithm.
  // The second example demonstrates the peril.
  EXPECT_FALSE(BlockAccepted("color: black /* any color */"));
  EXPECT_FALSE(BlockAccepted("color: black /* } */"));

  // However, / and * on themselves are useful and should
  // not stop the block from being accepted.
  EXPECT_TRUE(BlockAccepted("z-index: calc(2 * 3 / 4)"));
}

TEST(FindLengthOfDeclarationListTest, String) {
  EXPECT_TRUE(BlockAccepted("--foo: \"some string\""));
  EXPECT_TRUE(BlockAccepted("--foo: \"(\""));
  EXPECT_TRUE(BlockAccepted("--foo: \"}\""));
  EXPECT_TRUE(BlockAccepted("--foo: \"[]\""));
  EXPECT_TRUE(BlockAccepted("--foo: \"/* comment */\""));

  EXPECT_TRUE(BlockAccepted("--foo: 'some string'"));
  EXPECT_TRUE(BlockAccepted("--foo: '('"));
  EXPECT_TRUE(BlockAccepted("--foo: '}'"));
  EXPECT_TRUE(BlockAccepted("--foo: '[]'"));
  EXPECT_TRUE(BlockAccepted("--foo: '/* comment */'"));

  EXPECT_TRUE(BlockAccepted("--foo: \"this is fine\" 'it really is'"));
  EXPECT_FALSE(BlockAccepted("--foo: \"don't\" } \"accept'this!\""));

  // We don't support escapes (this is just a limitation).
  EXPECT_FALSE(BlockAccepted("--foo: \"\\n\""));
  EXPECT_FALSE(BlockAccepted("--foo: \"\\\""));

  // We don't support nested quotes (this is also just a limitation).
  EXPECT_FALSE(BlockAccepted("--foo: \"it's OK\""));
  EXPECT_FALSE(BlockAccepted("--foo: '1\" = 2.54cm'"));
}

TEST(FindLengthOfDeclarationListTest, IgnoringDangerousAfterBlock) {
  EXPECT_EQ(
      0u,
      FindLengthOfDeclarationList(
          "a:b[selector containing difficult stuff]}paddingpaddingpadding"));
  EXPECT_EQ(
      3u,
      FindLengthOfDeclarationList(
          "a:b}[selector containing difficult stuff]paddingpaddingpadding"));
}

TEST(FindLengthOfDeclarationListTest, NonASCII) {
  // Non-ASCII long after the block should not matter.
  EXPECT_EQ(10u, FindLengthOfDeclarationList(
                     String::FromUTF8("--foo: bar}                   ❤️")));

  // We should also support these characters inside the block itself.
  EXPECT_TRUE(BlockAccepted(String::FromUTF8("--foo: \"❤️\"")));
  EXPECT_TRUE(BlockAccepted(String::FromUTF8("font-family: 😊")));

  // Also make sure we don't simply _ignore_ the top UTF-16 byte;
  // these two characters become 01 7B and 7B 01 depending on
  // endianness, and should _not_ match as { (which is 0x7B).
  EXPECT_TRUE(BlockAccepted(String::FromUTF8("--fooŻ笁: value")));
}

#endif  // SIMD

}  // namespace blink
