// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class TextBreakIteratorTest : public testing::Test {
 protected:
  void SetTestString(const char* test_string) {
    test_string_ = String::FromUTF8(test_string);
  }

  void SetTestString16(Vector<UChar> input) {
    test_string_ = String(input.data(), static_cast<unsigned>(input.size()));
  }

  // The expected break positions must be specified UTF-16 character boundaries.
  void MatchLineBreaks(
      LineBreakType line_break_type,
      const Vector<int> expected_break_positions,
      BreakSpaceType break_space = BreakSpaceType::kBeforeEverySpace) {
    if (test_string_.Is8Bit()) {
      test_string_ = String::Make16BitFrom8BitSource(test_string_.Characters8(),
                                                     test_string_.length());
    }
    LazyLineBreakIterator lazy_break_iterator(test_string_);
    lazy_break_iterator.SetBreakType(line_break_type);
    lazy_break_iterator.SetBreakSpace(break_space);
    TestIsBreakable(expected_break_positions, lazy_break_iterator);
    TestNextBreakOpportunity(expected_break_positions, lazy_break_iterator);
  }

  // Test IsBreakable() by iterating all positions. BreakingContext uses this
  // interface.
  void TestIsBreakable(const Vector<int> expected_break_positions,
                       const LazyLineBreakIterator& break_iterator) {
    Vector<int> break_positions;
    int next_breakable = -1;
    for (unsigned i = 0; i <= test_string_.length(); i++) {
      if (break_iterator.IsBreakable(i, next_breakable))
        break_positions.push_back(i);
    }
    EXPECT_THAT(break_positions,
                testing::ElementsAreArray(expected_break_positions))
        << test_string_ << " " << break_iterator.BreakType() << " "
        << break_iterator.BreakSpace();
  }

  // Test NextBreakOpportunity() by iterating break opportunities.
  // ShapingLineBreaker uses this interface.
  void TestNextBreakOpportunity(const Vector<int> expected_break_positions,
                                const LazyLineBreakIterator& break_iterator) {
    Vector<int> break_positions;
    for (unsigned i = 0; i <= test_string_.length(); i++) {
      i = break_iterator.NextBreakOpportunity(i);
      break_positions.push_back(i);
    }
    EXPECT_THAT(break_positions,
                testing::ElementsAreArray(expected_break_positions))
        << test_string_ << " " << break_iterator.BreakType() << " "
        << break_iterator.BreakSpace();
  }

  unsigned TestLengthOfGraphemeCluster() {
    return LengthOfGraphemeCluster(test_string_);
  }

  Vector<unsigned> GraphemesClusterList(String input,
                                        unsigned start,
                                        unsigned length) {
    Vector<unsigned> result;
    ::blink::GraphemesClusterList(StringView(input, start, length), &result);
    return result;
  }

 private:
  String test_string_;
};

static const LineBreakType all_break_types[] = {
    LineBreakType::kNormal, LineBreakType::kBreakAll,
    LineBreakType::kBreakCharacter, LineBreakType::kKeepAll};

class BreakTypeTest : public TextBreakIteratorTest,
                      public testing::WithParamInterface<LineBreakType> {};

INSTANTIATE_TEST_SUITE_P(TextBreakIteratorTest,
                         BreakTypeTest,
                         testing::ValuesIn(all_break_types));

TEST_P(BreakTypeTest, EmptyString) {
  LazyLineBreakIterator iterator(g_empty_string);
  iterator.SetBreakType(GetParam());
  EXPECT_TRUE(iterator.IsBreakable(0));
}

TEST_P(BreakTypeTest, EmptyNullString) {
  LazyLineBreakIterator iterator(String{});
  iterator.SetBreakType(GetParam());
  EXPECT_TRUE(iterator.IsBreakable(0));
}

TEST_P(BreakTypeTest, EmptyDefaultConstructor) {
  LazyLineBreakIterator iterator;
  iterator.SetBreakType(GetParam());
  EXPECT_TRUE(iterator.IsBreakable(0));
}

TEST_F(TextBreakIteratorTest, Basic) {
  SetTestString("a b  c");
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6});
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 6},
                  BreakSpaceType::kBeforeSpaceRun);
}

TEST_F(TextBreakIteratorTest, Newline) {
  SetTestString("a\nb\n\nc\n d");
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6, 7, 9});
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 6, 9},
                  BreakSpaceType::kBeforeSpaceRun);
}

TEST_F(TextBreakIteratorTest, Tab) {
  SetTestString("a\tb\t\tc");
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 4, 6});
  MatchLineBreaks(LineBreakType::kNormal, {1, 3, 6},
                  BreakSpaceType::kBeforeSpaceRun);
}

TEST_F(TextBreakIteratorTest, LatinPunctuation) {
  SetTestString("(ab) cd.");
  MatchLineBreaks(LineBreakType::kNormal, {4, 8});
  MatchLineBreaks(LineBreakType::kBreakAll, {2, 4, 6, 8});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {1, 2, 3, 4, 5, 6, 7, 8});
  MatchLineBreaks(LineBreakType::kKeepAll, {4, 8});
}

TEST_F(TextBreakIteratorTest, Chinese) {
  SetTestString("標準萬國碼");
  MatchLineBreaks(LineBreakType::kNormal, {1, 2, 3, 4, 5});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 4, 5});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {1, 2, 3, 4, 5});
  MatchLineBreaks(LineBreakType::kKeepAll, {5});
}

TEST_F(TextBreakIteratorTest, ChineseMixed) {
  SetTestString("標（準）萬ab國.碼");
  MatchLineBreaks(LineBreakType::kNormal, {1, 4, 5, 7, 9, 10});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 4, 5, 6, 7, 9, 10});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  MatchLineBreaks(LineBreakType::kKeepAll, {1, 4, 9, 10});
}

TEST_F(TextBreakIteratorTest, ChineseSpaces) {
  SetTestString("標  萬  a  國");
  MatchLineBreaks(LineBreakType::kNormal, {1, 2, 4, 5, 7, 8, 10});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 4, 5, 7, 8, 10});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  MatchLineBreaks(LineBreakType::kKeepAll, {1, 2, 4, 5, 7, 8, 10});
  MatchLineBreaks(LineBreakType::kNormal, {1, 4, 7, 10},
                  BreakSpaceType::kBeforeSpaceRun);
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJFamilyIsolate) {
  SetTestString("\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  MatchLineBreaks(LineBreakType::kNormal, {11});
  MatchLineBreaks(LineBreakType::kBreakAll, {11});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {11});
  MatchLineBreaks(LineBreakType::kKeepAll, {11});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequenceIsolate) {
  SetTestString("\u261D\U0001F3FB");
  MatchLineBreaks(LineBreakType::kNormal, {3});
  MatchLineBreaks(LineBreakType::kBreakAll, {3});
  MatchLineBreaks(LineBreakType::kBreakCharacter, {3});
  MatchLineBreaks(LineBreakType::kKeepAll, {3});
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJSequence) {
  SetTestString(
      "abc \U0001F469\u200D\U0001F469\u200D\U0001F467\u200D\U0001F467 def");
  MatchLineBreaks(LineBreakType::kNormal, {3, 15, 19});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 15, 17, 18, 19});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 15, 16, 17, 18, 19});
  MatchLineBreaks(LineBreakType::kKeepAll, {3, 15, 19});
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequence) {
  SetTestString("abc \u261D\U0001F3FB def");
  MatchLineBreaks(LineBreakType::kNormal, {3, 7, 11});
  MatchLineBreaks(LineBreakType::kBreakAll, {1, 2, 3, 7, 9, 10, 11});
  MatchLineBreaks(LineBreakType::kBreakCharacter,
                  {1, 2, 3, 4, 7, 8, 9, 10, 11});
  MatchLineBreaks(LineBreakType::kKeepAll, {3, 7, 11});
}

TEST_F(TextBreakIteratorTest, NextBreakOpportunityAtEnd) {
  LineBreakType break_types[] = {
      LineBreakType::kNormal, LineBreakType::kBreakAll,
      LineBreakType::kBreakCharacter, LineBreakType::kKeepAll};
  for (const auto break_type : break_types) {
    LazyLineBreakIterator break_iterator(String("1"));
    break_iterator.SetBreakType(break_type);
    EXPECT_EQ(1u, break_iterator.NextBreakOpportunity(1));
  }
}

TEST_F(TextBreakIteratorTest, LengthOfGraphemeCluster) {
  SetTestString("");
  EXPECT_EQ(0u, TestLengthOfGraphemeCluster());

  SetTestString16({});
  EXPECT_EQ(0u, TestLengthOfGraphemeCluster());

  SetTestString("a");
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());
  SetTestString("\n");
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());
  SetTestString("\r");
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());
  SetTestString16({'a'});
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());
  SetTestString16({'\n'});
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());
  SetTestString16({'\r'});
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());

  SetTestString("abc");
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());

  SetTestString16({'a', 'b', 'c'});
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());

  SetTestString("\r\n");
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());

  SetTestString16({'\r', '\n'});
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());

  SetTestString("\n\r");
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());

  SetTestString16({'\n', '\r'});
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());

  SetTestString("\r\n\r");
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());

  SetTestString16({'\r', '\n', '\r'});
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());

  SetTestString16({'g', 0x308});
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());
  SetTestString16({0x1100, 0x1161, 0x11A8});
  EXPECT_EQ(3u, TestLengthOfGraphemeCluster());
  SetTestString16({0x0BA8, 0x0BBF});
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());

  SetTestString16({0x308, 'g'});
  EXPECT_EQ(1u, TestLengthOfGraphemeCluster());

  SetTestString("\r\nbc");
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());

  SetTestString16({'g', 0x308, 'b', 'c'});
  EXPECT_EQ(2u, TestLengthOfGraphemeCluster());
}

TEST_F(TextBreakIteratorTest, GraphemesClusterListTest) {
  EXPECT_EQ(GraphemesClusterList(u"hello", 0, 5),
            Vector<unsigned>({0, 1, 2, 3, 4}));
  EXPECT_EQ(GraphemesClusterList(u"hello", 2, 2), Vector<unsigned>({0, 1}));
  EXPECT_EQ(GraphemesClusterList(u"voila\u0300!", 0, 7),
            Vector<unsigned>({0, 1, 2, 3, 4, 4, 5}));
  EXPECT_EQ(GraphemesClusterList(u"di\u0303\u031c\u0337!", 0, 6),
            Vector<unsigned>({0, 1, 1, 1, 1, 2}));
  EXPECT_EQ(GraphemesClusterList(u"🇨🇦", 0, 4), Vector<unsigned>({0, 0, 0, 0}));

  EXPECT_EQ(GraphemesClusterList(u"🏳️‍🌈", 0, 6),
            Vector<unsigned>({0, 0, 0, 0, 0, 0}));
  // NO ZWJ on this sequence.
  EXPECT_EQ(GraphemesClusterList(u"🏳🌈", 0, 4),
            Vector<unsigned>({0, 0, 1, 1}));

  // ARABIC LETTER MEEM + ARABIC FATHA
  EXPECT_EQ(GraphemesClusterList(u"\u0645\u064E", 0, 2),
            Vector<unsigned>({0, 0}));
}

}  // namespace blink
