// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_markup.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// --- Tests for IsCSSTokenizerIdentifier (existing function) ---

TEST(CSSMarkupTest, IdentifierSingleWord) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("Times")));
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("Verdana")));
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("a")));
}

TEST(CSSMarkupTest, IdentifierHyphenPrefix) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("-webkit-body")));
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("-foo")));
}

TEST(CSSMarkupTest, IdentifierUnderscore) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("_private")));
  EXPECT_TRUE(IsCSSTokenizerIdentifier(StringView("__double")));
}

TEST(CSSMarkupTest, IdentifierRejectsEmpty) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentifier(StringView("")));
}

TEST(CSSMarkupTest, IdentifierRejectsDigitStart) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentifier(StringView("34J")));
  EXPECT_FALSE(IsCSSTokenizerIdentifier(StringView("0abc")));
}

TEST(CSSMarkupTest, IdentifierRejectsSpaces) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentifier(StringView("Twisty Tie")));
  EXPECT_FALSE(IsCSSTokenizerIdentifier(StringView("A B C")));
}

// --- Tests for IsCSSTokenizerIdentSequence (new function) ---

TEST(CSSMarkupTest, IdentSequenceSingleWord) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("Times")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("Verdana")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("a")));
}

TEST(CSSMarkupTest, IdentSequenceMultiWord) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("Twisty Tie")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("Times New Roman")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("A B C")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("Gill Sans")));
}

TEST(CSSMarkupTest, IdentSequenceHyphenatedWords) {
  test::TaskEnvironment task_environment;
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("-foo")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("-foo bar")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("foo -bar")));
  EXPECT_TRUE(IsCSSTokenizerIdentSequence(StringView("-foo -bar")));
}

TEST(CSSMarkupTest, IdentSequenceNonASCII) {
  test::TaskEnvironment task_environment;
  // Non-ASCII characters are valid ident-start code points.
  EXPECT_TRUE(
      IsCSSTokenizerIdentSequence(StringView(String::FromUTF8("日本語"))));
  EXPECT_TRUE(
      IsCSSTokenizerIdentSequence(StringView(String::FromUTF8("Ñoño"))));
  EXPECT_TRUE(
      IsCSSTokenizerIdentSequence(StringView(String::FromUTF8("Avenir Näxt"))));
}

TEST(CSSMarkupTest, IdentSequenceRejectsEmpty) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsLeadingSpace) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView(" Times")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView(" ")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsTrailingSpace) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("Times ")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsConsecutiveSpaces) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("A  B")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("Times   New")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsDigitStart) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("34J")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo 34J")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("34J bar")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsSpecialChars) {
  test::TaskEnvironment task_environment;
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo+bar")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo,bar")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo\"bar")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo\\bar")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo/bar")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsLoneHyphen) {
  test::TaskEnvironment task_environment;
  // A lone hyphen is not a valid ident.
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("-")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("foo -")));
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("- bar")));
}

TEST(CSSMarkupTest, IdentSequenceRejectsHyphenDigit) {
  test::TaskEnvironment task_environment;
  // -9 is not a valid ident (hyphen followed by digit, not nmstart).
  EXPECT_FALSE(IsCSSTokenizerIdentSequence(StringView("-9abc")));
}

// --- Tests for SerializeFontFamily ---

class SerializeFontFamilyTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(SerializeFontFamilyTest, SingleWordNoQuotes) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("Verdana")), "Verdana");
}

TEST_F(SerializeFontFamilyTest, MultiWordUnquoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("Twisty Tie")), "Twisty Tie");
  EXPECT_EQ(SerializeFontFamily(AtomicString("Times New Roman")),
            "Times New Roman");
  EXPECT_EQ(SerializeFontFamily(AtomicString("Gill Sans")), "Gill Sans");
}

TEST_F(SerializeFontFamilyTest, GenericFamilyAlwaysQuoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("serif")), "\"serif\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("sans-serif")), "\"sans-serif\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("monospace")), "\"monospace\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("cursive")), "\"cursive\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("fantasy")), "\"fantasy\"");
}

TEST_F(SerializeFontFamilyTest, CSSWideKeywordsAlwaysQuoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("initial")), "\"initial\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("inherit")), "\"inherit\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("unset")), "\"unset\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("revert")), "\"revert\"");
}

TEST_F(SerializeFontFamilyTest, DefaultKeywordAlwaysQuoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("default")), "\"default\"");
}

TEST_F(SerializeFontFamilyTest, DigitStartAlwaysQuoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("34J")), "\"34J\"");
  EXPECT_EQ(SerializeFontFamily(AtomicString("1")), "\"1\"");
}

TEST_F(SerializeFontFamilyTest, DoubleSpaceQuoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("A  B")), "\"A  B\"");
}

TEST_F(SerializeFontFamilyTest, SpecialCharsQuoted) {
  EXPECT_EQ(SerializeFontFamily(AtomicString("foo+bar")), "\"foo+bar\"");
}

}  // namespace blink
