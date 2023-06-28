// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/simplified_backwards_text_iterator.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace simplified_backwards_text_iterator_test {

TextIteratorBehavior EmitsSmallXForTextSecurityBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsSmallXForTextSecurity(true)
      .Build();
}

class SimplifiedBackwardsTextIteratorTest : public EditingTestBase {
 protected:
  std::string ExtractStringInRange(
      const std::string selection_text,
      const TextIteratorBehavior& behavior = TextIteratorBehavior()) {
    const SelectionInDOMTree selection = SetSelectionTextToBody(selection_text);
    StringBuilder builder;
    bool is_first = true;
    for (SimplifiedBackwardsTextIterator iterator(selection.ComputeRange(),
                                                  behavior);
         !iterator.AtEnd(); iterator.Advance()) {
      if (!is_first)
        builder.Append(", ", 2);
      is_first = false;
      builder.Append(iterator.GetTextState().GetTextForTesting());
    }
    return builder.ToString().Utf8();
  }
};

template <typename Strategy>
static String ExtractString(const Element& element) {
  const EphemeralRangeTemplate<Strategy> range =
      EphemeralRangeTemplate<Strategy>::RangeOfContents(element);
  String result;
  for (SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(range);
       !it.AtEnd(); it.Advance()) {
    result = it.GetTextState().GetTextForTesting() + result;
  }
  return result;
}

TEST_F(SimplifiedBackwardsTextIteratorTest, IterateWithFirstLetterPart) {
  InsertStyleElement("p::first-letter {font-size: 200%}");
  // TODO(editing-dev): |SimplifiedBackwardsTextIterator| should not account
  // collapsed whitespace (http://crbug.com/760428)

  // Simulate PreviousBoundary()
  EXPECT_EQ(" , \n", ExtractStringInRange("^<p> |[(3)]678</p>"));
  EXPECT_EQ(" [, \n", ExtractStringInRange("^<p> [|(3)]678</p>"));
  EXPECT_EQ(" [(, \n", ExtractStringInRange("^<p> [(|3)]678</p>"));
  EXPECT_EQ(" [(3, \n", ExtractStringInRange("^<p> [(3|)]678</p>"));
  EXPECT_EQ(" [(3), \n", ExtractStringInRange("^<p> [(3)|]678</p>"));
  EXPECT_EQ(" [(3)], \n", ExtractStringInRange("^<p> [(3)]|678</p>"));

  EXPECT_EQ("6,  [(3)], \n, ab", ExtractStringInRange("^ab<p> [(3)]6|78</p>"))
      << "From remaining part to outside";

  EXPECT_EQ("(3)", ExtractStringInRange("<p> [^(3)|]678</p>"))
      << "Iterate in first-letter part";

  EXPECT_EQ("67, (3)]", ExtractStringInRange("<p> [^(3)]67|8</p>"))
      << "From remaining part to first-letter part";

  EXPECT_EQ("789", ExtractStringInRange("<p> [(3)]6^789|a</p>"))
      << "Iterate in remaining part";

  EXPECT_EQ("9, \n, 78", ExtractStringInRange("<p> [(3)]6^78</p>9|a"))
      << "Enter into remaining part and stop in remaining part";

  EXPECT_EQ("9, \n, 678, (3)]", ExtractStringInRange("<p> [^(3)]678</p>9|a"))
      << "Enter into remaining part and stop in first-letter part";
}

TEST_F(SimplifiedBackwardsTextIteratorTest, Basic) {
  SetBodyContent("<p> [(3)]678</p>");
  const Element* const sample = GetDocument().QuerySelector(AtomicString("p"));
  SimplifiedBackwardsTextIterator iterator(EphemeralRange(
      Position(sample->firstChild(), 0), Position(sample->firstChild(), 9)));
  // TODO(editing-dev): |SimplifiedBackwardsTextIterator| should not account
  // collapsed whitespace (http://crbug.com/760428)
  EXPECT_EQ(9, iterator.length())
      << "We should have 8 as ignoring collapsed whitespace.";
  EXPECT_EQ(Position(sample->firstChild(), 0), iterator.StartPosition());
  EXPECT_EQ(Position(sample->firstChild(), 9), iterator.EndPosition());
  EXPECT_EQ(sample->firstChild(), iterator.StartContainer());
  EXPECT_EQ(9, iterator.EndOffset());
  EXPECT_EQ(sample->firstChild(), iterator.GetNode());
  EXPECT_EQ('8', iterator.CharacterAt(0));
  EXPECT_EQ('7', iterator.CharacterAt(1));
  EXPECT_EQ('6', iterator.CharacterAt(2));
  EXPECT_EQ(']', iterator.CharacterAt(3));
  EXPECT_EQ(')', iterator.CharacterAt(4));
  EXPECT_EQ('3', iterator.CharacterAt(5));
  EXPECT_EQ('(', iterator.CharacterAt(6));
  EXPECT_EQ('[', iterator.CharacterAt(7));
  EXPECT_EQ(' ', iterator.CharacterAt(8));

  EXPECT_FALSE(iterator.AtEnd());
  iterator.Advance();
  EXPECT_TRUE(iterator.AtEnd());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, NbspCharacter) {
  SetBodyContent("<p>123 456&nbsp;789</p>");
  const Element* const p = GetDocument().QuerySelector(AtomicString("p"));
  SimplifiedBackwardsTextIteratorInFlatTree iterator(
      EphemeralRangeInFlatTree(PositionInFlatTree(p->firstChild(), 0),
                               PositionInFlatTree(p->firstChild(), 11)));
  EXPECT_EQ(11, iterator.length());
  EXPECT_EQ('9', iterator.CharacterAt(0));
  EXPECT_EQ('8', iterator.CharacterAt(1));
  EXPECT_EQ('7', iterator.CharacterAt(2));
  EXPECT_EQ(kNoBreakSpaceCharacter, iterator.CharacterAt(3));
  EXPECT_EQ('6', iterator.CharacterAt(4));
  EXPECT_EQ('5', iterator.CharacterAt(5));
  EXPECT_EQ('4', iterator.CharacterAt(6));
  EXPECT_EQ(' ', iterator.CharacterAt(7));
  EXPECT_EQ('3', iterator.CharacterAt(8));
  EXPECT_EQ('2', iterator.CharacterAt(9));
  EXPECT_EQ('1', iterator.CharacterAt(10));

  EXPECT_FALSE(iterator.AtEnd());
  iterator.Advance();
  EXPECT_TRUE(iterator.AtEnd());

  TextIteratorBehavior behavior =
      TextIteratorBehavior::Builder().SetEmitsSpaceForNbsp(true).Build();
  SimplifiedBackwardsTextIteratorInFlatTree emits_space_iterator(
      EphemeralRangeInFlatTree(PositionInFlatTree(p->firstChild(), 0),
                               PositionInFlatTree(p->firstChild(), 11)),
      behavior);
  EXPECT_EQ(11, emits_space_iterator.length());
  EXPECT_EQ('9', emits_space_iterator.CharacterAt(0));
  EXPECT_EQ('8', emits_space_iterator.CharacterAt(1));
  EXPECT_EQ('7', emits_space_iterator.CharacterAt(2));
  EXPECT_EQ(' ', emits_space_iterator.CharacterAt(3));
  EXPECT_EQ('6', emits_space_iterator.CharacterAt(4));
  EXPECT_EQ('5', emits_space_iterator.CharacterAt(5));
  EXPECT_EQ('4', emits_space_iterator.CharacterAt(6));
  EXPECT_EQ(' ', emits_space_iterator.CharacterAt(7));
  EXPECT_EQ('3', emits_space_iterator.CharacterAt(8));
  EXPECT_EQ('2', emits_space_iterator.CharacterAt(9));
  EXPECT_EQ('1', emits_space_iterator.CharacterAt(10));

  EXPECT_FALSE(emits_space_iterator.AtEnd());
  emits_space_iterator.Advance();
  EXPECT_TRUE(emits_space_iterator.AtEnd());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, EmitsPunctuationForImage) {
  SetBodyContent("<img id='img'><p>1</p>");
  const Element* const p = GetDocument().QuerySelector(AtomicString("p"));
  const Element* const img = GetDocument().QuerySelector(AtomicString("img"));
  SimplifiedBackwardsTextIteratorInFlatTree iterator(EphemeralRangeInFlatTree(
      PositionInFlatTree(img, 0), PositionInFlatTree(p->firstChild(), 1)));
  EXPECT_EQ(1, iterator.length());
  EXPECT_EQ('1', iterator.CharacterAt(0));
  iterator.Advance();
  EXPECT_EQ(1, iterator.length());
  EXPECT_EQ('\n', iterator.CharacterAt(0));
  iterator.Advance();
  EXPECT_EQ(0, iterator.length());

  EXPECT_TRUE(iterator.AtEnd());

  TextIteratorBehavior behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsPunctuationForReplacedElements(true)
          .Build();
  SimplifiedBackwardsTextIteratorInFlatTree with_punctuation_iterator(
      EphemeralRangeInFlatTree(PositionInFlatTree(img, 0),
                               PositionInFlatTree(p->firstChild(), 1)),
      behavior);
  EXPECT_EQ(1, with_punctuation_iterator.length());
  EXPECT_EQ('1', with_punctuation_iterator.CharacterAt(0));
  with_punctuation_iterator.Advance();
  EXPECT_EQ(1, with_punctuation_iterator.length());
  EXPECT_EQ('\n', with_punctuation_iterator.CharacterAt(0));
  with_punctuation_iterator.Advance();
  EXPECT_EQ(1, with_punctuation_iterator.length());
  EXPECT_EQ(',', with_punctuation_iterator.CharacterAt(0));

  EXPECT_FALSE(with_punctuation_iterator.AtEnd());
  with_punctuation_iterator.Advance();
  EXPECT_TRUE(with_punctuation_iterator.AtEnd());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, FirstLetter) {
  SetBodyContent(
      "<style>p::first-letter {font-size: 200%}</style>"
      "<p> [(3)]678</p>");
  const Element* const sample = GetDocument().QuerySelector(AtomicString("p"));
  SimplifiedBackwardsTextIterator iterator(EphemeralRange(
      Position(sample->firstChild(), 0), Position(sample->firstChild(), 9)));
  EXPECT_EQ(3, iterator.length());
  EXPECT_EQ(Position(sample->firstChild(), 6), iterator.StartPosition());
  EXPECT_EQ(Position(sample->firstChild(), 9), iterator.EndPosition());
  EXPECT_EQ(sample->firstChild(), iterator.StartContainer());
  EXPECT_EQ(9, iterator.EndOffset());
  EXPECT_EQ(sample->firstChild(), iterator.GetNode());
  EXPECT_EQ('8', iterator.CharacterAt(0));
  EXPECT_EQ('7', iterator.CharacterAt(1));
  EXPECT_EQ('6', iterator.CharacterAt(2));

  iterator.Advance();
  // TODO(editing-dev): |SimplifiedBackwardsTextIterator| should not account
  // collapsed whitespace (http://crbug.com/760428)
  EXPECT_EQ(6, iterator.length())
      << "We should have 5 as ignoring collapsed whitespace.";
  EXPECT_EQ(Position(sample->firstChild(), 0), iterator.StartPosition());
  EXPECT_EQ(Position(sample->firstChild(), 6), iterator.EndPosition());
  EXPECT_EQ(sample->firstChild(), iterator.StartContainer());
  EXPECT_EQ(6, iterator.EndOffset());
  EXPECT_EQ(sample->firstChild(), iterator.GetNode());
  EXPECT_EQ(']', iterator.CharacterAt(0));
  EXPECT_EQ(')', iterator.CharacterAt(1));
  EXPECT_EQ('3', iterator.CharacterAt(2));
  EXPECT_EQ('(', iterator.CharacterAt(3));
  EXPECT_EQ('[', iterator.CharacterAt(4));
  EXPECT_EQ(' ', iterator.CharacterAt(5));

  EXPECT_FALSE(iterator.AtEnd());
  iterator.Advance();
  EXPECT_TRUE(iterator.AtEnd());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, SubrangeWithReplacedElements) {
  static const char* body_content =
      "<span id=host><b slot='#one' id=one>one</b> not appeared <b slot='#two' "
      "id=two>two</b></span>";
  const char* shadow_content =
      "three <slot name=#two></slot> <slot name=#one></slot> "
      "zero";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById(AtomicString("host"));

  // We should not apply DOM tree version to containing shadow tree in
  // general. To record current behavior, we have this test. even if it
  // isn't intuitive.
  EXPECT_EQ("onetwo", ExtractString<EditingStrategy>(*host));
  EXPECT_EQ("three two one zero",
            ExtractString<EditingInFlatTreeStrategy>(*host));
}

TEST_F(SimplifiedBackwardsTextIteratorTest, characterAt) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>one</b> not appeared <b slot='#two' "
      "id=two>two</b></span>";
  const char* shadow_content =
      "three <slot name=#two></slot> <slot name=#one></slot> "
      "zero";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById(AtomicString("host"));

  EphemeralRangeTemplate<EditingStrategy> range1(
      EphemeralRangeTemplate<EditingStrategy>::RangeOfContents(*host));
  SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> back_iter1(range1);
  const char* message1 =
      "|backIter1| should emit 'one' and 'two' in reverse order.";
  EXPECT_EQ('o', back_iter1.CharacterAt(0)) << message1;
  EXPECT_EQ('w', back_iter1.CharacterAt(1)) << message1;
  EXPECT_EQ('t', back_iter1.CharacterAt(2)) << message1;
  back_iter1.Advance();
  EXPECT_EQ('e', back_iter1.CharacterAt(0)) << message1;
  EXPECT_EQ('n', back_iter1.CharacterAt(1)) << message1;
  EXPECT_EQ('o', back_iter1.CharacterAt(2)) << message1;

  EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(
      EphemeralRangeTemplate<EditingInFlatTreeStrategy>::RangeOfContents(
          *host));
  SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy>
      back_iter2(range2);
  const char* message2 =
      "|backIter2| should emit 'three ', 'two', ' ', 'one' and ' zero' in "
      "reverse order.";
  EXPECT_EQ('o', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('r', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('e', back_iter2.CharacterAt(2)) << message2;
  EXPECT_EQ('z', back_iter2.CharacterAt(3)) << message2;
  EXPECT_EQ(' ', back_iter2.CharacterAt(4)) << message2;
  back_iter2.Advance();
  EXPECT_EQ('e', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('n', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('o', back_iter2.CharacterAt(2)) << message2;
  back_iter2.Advance();
  EXPECT_EQ(' ', back_iter2.CharacterAt(0)) << message2;
  back_iter2.Advance();
  EXPECT_EQ('o', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('w', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('t', back_iter2.CharacterAt(2)) << message2;
  back_iter2.Advance();
  EXPECT_EQ(' ', back_iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('e', back_iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('e', back_iter2.CharacterAt(2)) << message2;
  EXPECT_EQ('r', back_iter2.CharacterAt(3)) << message2;
  EXPECT_EQ('h', back_iter2.CharacterAt(4)) << message2;
  EXPECT_EQ('t', back_iter2.CharacterAt(5)) << message2;
}

TEST_F(SimplifiedBackwardsTextIteratorTest, TextSecurity) {
  InsertStyleElement("s {-webkit-text-security:disc;}");
  EXPECT_EQ("baz, xxx, abc",
            ExtractStringInRange("^abc<s>foo</s>baz|",
                                 EmitsSmallXForTextSecurityBehavior()));
  // E2 80 A2 is U+2022 BULLET
  EXPECT_EQ("baz, \xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2, abc",
            ExtractStringInRange("^abc<s>foo</s>baz|", TextIteratorBehavior()));
}

}  // namespace simplified_backwards_text_iterator_test
}  // namespace blink
