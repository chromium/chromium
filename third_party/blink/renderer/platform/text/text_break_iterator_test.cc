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
      const Vector<int> expected_break_positions,
      LineBreakType line_break_type = LineBreakType::kNormal,
      BreakSpaceType break_space = BreakSpaceType::kAfterSpaceRun) {
    if (test_string_.Is8Bit()) {
      test_string_ = String::Make16BitFrom8BitSource(test_string_.Span8());
    }
    LazyLineBreakIterator lazy_break_iterator(test_string_, locale_.get());
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
    for (unsigned i = 0; i <= test_string_.length(); i++) {
      if (break_iterator.IsBreakable(i)) {
        break_positions.push_back(i);
      }
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

 protected:
  String test_string_;
  scoped_refptr<LayoutLocale> locale_;
};

TEST_F(TextBreakIteratorTest, PooledBreakIterator) {
  const AtomicString locale{"en"};
  const String str{"a"};
  PooledBreakIterator it1 = AcquireLineBreakIterator(str, locale);

  // Get another and release. It should be a different instance than `it1`.
  TextBreakIterator* ptr2;
  {
    PooledBreakIterator it2 = AcquireLineBreakIterator(str, locale);
    EXPECT_NE(it2.get(), it1.get());
    ptr2 = it2.get();
  }

  // Because `it2` is released, `it3` should be the same instance as `it2`.
  PooledBreakIterator it3 = AcquireLineBreakIterator(str, locale);
  EXPECT_EQ(it3.get(), ptr2);
}

static const LineBreakType all_break_types[] = {
    LineBreakType::kNormal, LineBreakType::kBreakAll,
    LineBreakType::kBreakCharacter, LineBreakType::kKeepAll,
    LineBreakType::kPhrase};

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

TEST_F(TextBreakIteratorTest, Strictness) {
  scoped_refptr<LayoutLocale> locale =
      LayoutLocale::CreateForTesting(AtomicString("ja"));
  LazyLineBreakIterator iterator(String(u"あーあ"), locale.get());
  EXPECT_EQ(iterator.NextBreakOpportunity(0), 1u);
  EXPECT_EQ(iterator.LocaleWithKeyword(), "ja");

  iterator.SetStrictness(LineBreakStrictness::kStrict);
  EXPECT_EQ(iterator.NextBreakOpportunity(0), 2u);
  EXPECT_EQ(iterator.LocaleWithKeyword(), "ja@lb=strict");

  iterator.SetLocale(nullptr);
  EXPECT_EQ(iterator.NextBreakOpportunity(0), 1u);
  EXPECT_EQ(iterator.LocaleWithKeyword(), "");
}

TEST_F(TextBreakIteratorTest, Basic) {
  SetTestString("a b  c");
  MatchLineBreaks({2, 5, 6});
}

TEST_F(TextBreakIteratorTest, Newline) {
  SetTestString("a\nb\n\nc\n d");
  MatchLineBreaks({2, 5, 8, 9});
}

TEST_F(TextBreakIteratorTest, Tab) {
  SetTestString("a\tb\t\tc");
  MatchLineBreaks({2, 5, 6}, LineBreakType::kNormal);
}

TEST_F(TextBreakIteratorTest, LatinPunctuation) {
  SetTestString("(ab) cd.");
  MatchLineBreaks({5, 8}, LineBreakType::kNormal);
  MatchLineBreaks({2, 5, 6, 8}, LineBreakType::kBreakAll);
  MatchLineBreaks({1, 2, 3, 4, 5, 6, 7, 8}, LineBreakType::kBreakCharacter);
  MatchLineBreaks({5, 8}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, Chinese) {
  SetTestString("標準萬國碼");
  MatchLineBreaks({1, 2, 3, 4, 5}, LineBreakType::kNormal);
  MatchLineBreaks({1, 2, 3, 4, 5}, LineBreakType::kBreakAll);
  MatchLineBreaks({1, 2, 3, 4, 5}, LineBreakType::kBreakCharacter);
  MatchLineBreaks({5}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, ChineseMixed) {
  SetTestString("標（準）萬ab國.碼");
  MatchLineBreaks({1, 4, 5, 7, 9, 10}, LineBreakType::kNormal);
  MatchLineBreaks({1, 4, 5, 6, 7, 9, 10}, LineBreakType::kBreakAll);
  MatchLineBreaks({1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                  LineBreakType::kBreakCharacter);
  MatchLineBreaks({1, 4, 9, 10}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, ChineseSpaces) {
  SetTestString("標  萬  a  國");
  MatchLineBreaks({3, 6, 9, 10}, LineBreakType::kNormal);
  MatchLineBreaks({3, 6, 9, 10}, LineBreakType::kBreakAll);
  MatchLineBreaks({1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                  LineBreakType::kBreakCharacter);
  MatchLineBreaks({3, 6, 9, 10}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJFamilyIsolate) {
  SetTestString("\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
  MatchLineBreaks({11}, LineBreakType::kNormal);
  MatchLineBreaks({11}, LineBreakType::kBreakAll);
  MatchLineBreaks({11}, LineBreakType::kBreakCharacter);
  MatchLineBreaks({11}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequenceIsolate) {
  SetTestString("\u261D\U0001F3FB");
  MatchLineBreaks({3}, LineBreakType::kNormal);
  MatchLineBreaks({3}, LineBreakType::kBreakAll);
  MatchLineBreaks({3}, LineBreakType::kBreakCharacter);
  MatchLineBreaks({3}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, KeepEmojiZWJSequence) {
  SetTestString(
      "abc \U0001F469\u200D\U0001F469\u200D\U0001F467\u200D\U0001F467 def");
  MatchLineBreaks({4, 16, 19}, LineBreakType::kNormal);
  MatchLineBreaks({1, 2, 4, 16, 17, 18, 19}, LineBreakType::kBreakAll);
  MatchLineBreaks({1, 2, 3, 4, 15, 16, 17, 18, 19},
                  LineBreakType::kBreakCharacter);
  MatchLineBreaks({4, 16, 19}, LineBreakType::kKeepAll);
}

TEST_F(TextBreakIteratorTest, KeepEmojiModifierSequence) {
  SetTestString("abc \u261D\U0001F3FB def");
  MatchLineBreaks({4, 8, 11}, LineBreakType::kNormal);
  MatchLineBreaks({1, 2, 4, 8, 9, 10, 11}, LineBreakType::kBreakAll);
  MatchLineBreaks({1, 2, 3, 4, 7, 8, 9, 10, 11},
                  LineBreakType::kBreakCharacter);
  MatchLineBreaks({4, 8, 11}, LineBreakType::kKeepAll);
}

TEST_P(BreakTypeTest, NextBreakOpportunityAtEnd) {
  const LineBreakType break_type = GetParam();
  LazyLineBreakIterator break_iterator(String("1"));
  break_iterator.SetBreakType(break_type);
  EXPECT_EQ(1u, break_iterator.NextBreakOpportunity(1));
}

TEST_F(TextBreakIteratorTest, Phrase) {
  locale_ = LayoutLocale::CreateForTesting(AtomicString("ja"));
  test_string_ = u"今日はよい天気です。";
  MatchLineBreaks({3, 5, 10}, LineBreakType::kPhrase);
  test_string_ = u"あなたに寄り添う最先端のテクノロジー。";
  MatchLineBreaks({4, 8, 12, 19}, LineBreakType::kPhrase);
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

TEST_F(TextBreakIteratorTest, SoftHyphen) {
  SetTestString("xy\u00ADxy\u00ADxy xy\u00ADxy");
  LazyLineBreakIterator break_iterator(test_string_);
  break_iterator.SetBreakSpace(BreakSpaceType::kAfterSpaceRun);
  TestNextBreakOpportunity({3, 6, 9, 12, 14}, break_iterator);
  break_iterator.EnableSoftHyphen(false);
  TestNextBreakOpportunity({9, 14}, break_iterator);
}

TEST_F(TextBreakIteratorTest, HyphenMinusBeforeHighLatin) {
  SetTestString("Lorem-úpsum");
  MatchLineBreaks({6, 11});
  SetTestString("Lorem-èpsum");
  MatchLineBreaks({6, 11});
}

}  // namespace blink
