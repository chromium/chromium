// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

#define EXPECT_EQ_SELECTED_TEXT(text) \
  EXPECT_EQ(text, Selection().SelectedText().Utf8())

gfx::Point VisiblePositionToContentsPoint(const VisiblePosition& pos) {
  gfx::Point result = AbsoluteSelectionBoundsOf(pos).bottom_left();
  // Need to move the point at least by 1 - caret's minXMaxYCorner is not
  // evaluated to the same line as the text by hit testing.
  result.Offset(0, -1);
  return result;
}

using TextNodeVector = HeapVector<Member<Text>>;

class GranularityStrategyTest : public EditingTestBase {
 protected:
  void SetUp() override;

  Text* AppendTextNode(const String& data);
  void SetInnerHTML(const char*);
  // Parses the text node, appending the info to letter_pos_ and word_middles_.
  void ParseText(Text*);
  void ParseText(const TextNodeVector&);

  Text* SetupTranslateZ(String);
  Text* SetupTransform(String);
  Text* SetupRotate(String);
  void SetupTextSpan(String str1,
                     String str2,
                     String str3,
                     wtf_size_t sel_begin,
                     wtf_size_t sel_end);
  void SetupVerticalAlign(String str1,
                          String str2,
                          String str3,
                          wtf_size_t sel_begin,
                          wtf_size_t sel_end);
  void SetupFontSize(String str1,
                     String str2,
                     String str3,
                     wtf_size_t sel_begin,
                     wtf_size_t sel_end);

  void TestDirectionExpand();
  void TestDirectionShrink();
  void TestDirectionSwitchSide();

  // Pixel coordinates of the positions for each letter within the text being
  // tested.
  Vector<gfx::Point> letter_pos_;
  // Pixel coordinates of the middles of the words in the text being tested.
  // (y coordinate is based on y coordinates of letter_pos_)
  Vector<gfx::Point> word_middles_;
};

void GranularityStrategyTest::SetUp() {
  PageTestBase::SetUp();
  GetFrame().GetSettings()->SetDefaultFontSize(12);
  GetFrame().GetSettings()->SetSelectionStrategy(SelectionStrategy::kDirection);
}

Text* GranularityStrategyTest::AppendTextNode(const String& data) {
  Text* text = GetDocument().createTextNode(data);
  GetDocument().body()->AppendChild(text);
  return text;
}

void GranularityStrategyTest::SetInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

void GranularityStrategyTest::ParseText(Text* text) {
  TextNodeVector text_nodes;
  text_nodes.push_back(text);
  ParseText(text_nodes);
}

void GranularityStrategyTest::ParseText(const TextNodeVector& text_nodes) {
  bool word_started = false;
  int word_start_index = 0;
  for (auto& text : text_nodes) {
    wtf_size_t word_start_index_offset = letter_pos_.size();
    String str = text->wholeText();
    for (wtf_size_t i = 0; i < str.length(); i++) {
      letter_pos_.push_back(VisiblePositionToContentsPoint(
          CreateVisiblePosition(Position(text, i))));
      char c = str[i];
      if (IsASCIIAlphanumeric(c) && !word_started) {
        word_start_index = i + word_start_index_offset;
        word_started = true;
      } else if (!IsASCIIAlphanumeric(c) && word_started) {
        gfx::Point word_middle((letter_pos_[word_start_index].x() +
                                letter_pos_[i + word_start_index_offset].x()) /
                                   2,
                               letter_pos_[word_start_index].y());
        word_middles_.push_back(word_middle);
        word_started = false;
      }
    }
  }
  if (word_started) {
    const auto& last_node = text_nodes.back();
    int x_end = VisiblePositionToContentsPoint(
                    CreateVisiblePosition(
                        Position(last_node, last_node->wholeText().length())))
                    .x();
    gfx::Point word_middle((letter_pos_[word_start_index].x() + x_end) / 2,
                           letter_pos_[word_start_index].y());
    word_middles_.push_back(word_middle);
  }
}

Text* GranularityStrategyTest::SetupTranslateZ(String str) {
  SetInnerHTML(
      "<html>"
      "<head>"
      "<style>"
      "div {"
      "transform: translateZ(0);"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<div id='mytext'></div>"
      "</body>"
      "</html>");

  Text* text = GetDocument().createTextNode(str);
  Element* div = GetDocument().getElementById(AtomicString("mytext"));
  div->AppendChild(text);

  UpdateAllLifecyclePhasesForTest();

  ParseText(text);
  return text;
}

Text* GranularityStrategyTest::SetupTransform(String str) {
  SetInnerHTML(
      "<html>"
      "<head>"
      "<style>"
      "div {"
      "transform: scale(1,-1) translate(0,-100px);"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<div id='mytext'></div>"
      "</body>"
      "</html>");

  Text* text = GetDocument().createTextNode(str);
  Element* div = GetDocument().getElementById(AtomicString("mytext"));
  div->AppendChild(text);

  UpdateAllLifecyclePhasesForTest();

  ParseText(text);
  return text;
}

Text* GranularityStrategyTest::SetupRotate(String str) {
  SetInnerHTML(
      "<html>"
      "<head>"
      "<style>"
      "div {"
      "transform: translate(0px,600px) rotate(90deg);"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<div id='mytext'></div>"
      "</body>"
      "</html>");

  Text* text = GetDocument().createTextNode(str);
  Element* div = GetDocument().getElementById(AtomicString("mytext"));
  div->AppendChild(text);

  UpdateAllLifecyclePhasesForTest();

  ParseText(text);
  return text;
}

void GranularityStrategyTest::SetupTextSpan(String str1,
                                            String str2,
                                            String str3,
                                            wtf_size_t sel_begin,
                                            wtf_size_t sel_end) {
  Text* text1 = GetDocument().createTextNode(str1);
  Text* text2 = GetDocument().createTextNode(str2);
  Text* text3 = GetDocument().createTextNode(str3);
  auto* span = MakeGarbageCollected<HTMLSpanElement>(GetDocument());
  Element* div = GetDocument().getElementById(AtomicString("mytext"));
  div->AppendChild(text1);
  div->AppendChild(span);
  span->AppendChild(text2);
  div->AppendChild(text3);

  UpdateAllLifecyclePhasesForTest();

  Vector<gfx::Point> letter_pos;
  Vector<gfx::Point> word_middle_pos;

  TextNodeVector text_nodes;
  text_nodes.push_back(text1);
  text_nodes.push_back(text2);
  text_nodes.push_back(text3);
  ParseText(text_nodes);

  Position p1;
  Position p2;
  if (sel_begin < str1.length())
    p1 = Position(text1, sel_begin);
  else if (sel_begin < str1.length() + str2.length())
    p1 = Position(text2, sel_begin - str1.length());
  else
    p1 = Position(text3, sel_begin - str1.length() - str2.length());
  if (sel_end < str1.length())
    p2 = Position(text1, sel_end);
  else if (sel_end < str1.length() + str2.length())
    p2 = Position(text2, sel_end - str1.length());
  else
    p2 = Position(text3, sel_end - str1.length() - str2.length());

  Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(p1, p2).Build(),
      SetSelectionOptions());
}

void GranularityStrategyTest::SetupVerticalAlign(String str1,
                                                 String str2,
                                                 String str3,
                                                 wtf_size_t sel_begin,
                                                 wtf_size_t sel_end) {
  SetInnerHTML(
      "<html>"
      "<head>"
      "<style>"
      "span {"
      "vertical-align:20px;"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<div id='mytext'></div>"
      "</body>"
      "</html>");

  SetupTextSpan(str1, str2, str3, sel_begin, sel_end);
}

void GranularityStrategyTest::SetupFontSize(String str1,
                                            String str2,
                                            String str3,
                                            wtf_size_t sel_begin,
                                            wtf_size_t sel_end) {
  SetInnerHTML(
      "<html>"
      "<head>"
      "<style>"
      "span {"
      "font-size: 200%;"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<div id='mytext'></div>"
      "</body>"
      "</html>");

  SetupTextSpan(str1, str2, str3, sel_begin, sel_end);
}

// Tests expanding selection on text "abcdef ghij kl mno^p|>qr stuvwi inm mnii,"
// (^ means base, | means extent, < means start, and > means end). Text needs to
// be laid out on a single line with no rotation.
void GranularityStrategyTest::TestDirectionExpand() {
  // Expand selection using character granularity until the end of the word
  // is reached.
  // "abcdef ghij kl mno^pq|>r stuvwi inm  mnii,"
  Selection().MoveRangeSelectionExtent(letter_pos_[20]);
  EXPECT_EQ_SELECTED_TEXT("pq");
  // Move to the same postion shouldn't change anything.
  Selection().MoveRangeSelectionExtent(letter_pos_[20]);
  EXPECT_EQ_SELECTED_TEXT("pq");
  // "abcdef ghij kl mno^pqr|> stuvwi inm  mnii,"
  Selection().MoveRangeSelectionExtent(letter_pos_[21]);
  EXPECT_EQ_SELECTED_TEXT("pqr");
  // Selection should stay the same until the middle of the word is passed.
  // "abcdef ghij kl mno^pqr |>stuvwi inm  mnii," -
  Selection().MoveRangeSelectionExtent(letter_pos_[22]);
  EXPECT_EQ_SELECTED_TEXT("pqr ");
  // "abcdef ghij kl mno^pqr >st|uvwi inm  mnii,"
  Selection().MoveRangeSelectionExtent(letter_pos_[24]);
  EXPECT_EQ_SELECTED_TEXT("pqr ");
  gfx::Point p = word_middles_[4];
  p.Offset(-1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr ");
  p.Offset(1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi");
  // Selection should stay the same until the end of the word is reached.
  // "abcdef ghij kl mno^pqr stuvw|i> inm  mnii,"
  Selection().MoveRangeSelectionExtent(letter_pos_[27]);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi");
  // "abcdef ghij kl mno^pqr stuvwi|> inm  mnii,"
  Selection().MoveRangeSelectionExtent(letter_pos_[28]);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi");
  // "abcdef ghij kl mno^pqr stuvwi |>inm  mnii,"
  Selection().MoveRangeSelectionExtent(letter_pos_[29]);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi ");
  // Now expand slowly to the middle of word #5.
  int y = letter_pos_[29].y();
  for (int x = letter_pos_[29].x() + 1; x < word_middles_[5].x(); x++) {
    Selection().MoveRangeSelectionExtent(gfx::Point(x, y));
    Selection().MoveRangeSelectionExtent(gfx::Point(x, y));
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi ");
  }
  Selection().MoveRangeSelectionExtent(word_middles_[5]);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi inm");
  // Jump over quickly to just before the middle of the word #6 and then
  // move over it.
  p = word_middles_[6];
  p.Offset(-1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi inm ");
  p.Offset(1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr stuvwi inm mnii");
}

// Tests shrinking selection on text "abcdef ghij kl mno^pqr|> iiinmni, abc"
// (^ means base, | means extent, < means start, and > means end).
// Text needs to be laid out on a single line with no rotation.
void GranularityStrategyTest::TestDirectionShrink() {
  // Move to the middle of word #4 to it and then move back, confirming
  // that the selection end is moving with the extent. The offset between the
  // extent and the selection end will be equal to half the width of "iiinmni".
  Selection().MoveRangeSelectionExtent(word_middles_[4]);
  EXPECT_EQ_SELECTED_TEXT("pqr iiinmni");
  gfx::Point p = word_middles_[4];
  p.Offset(letter_pos_[28].x() - letter_pos_[29].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr iiinmn");
  p.Offset(letter_pos_[27].x() - letter_pos_[28].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr iiinm");
  p.Offset(letter_pos_[26].x() - letter_pos_[27].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr iiin");
  // Move right by the width of char 30 ('m'). Selection shouldn't change,
  // but offset should be reduced.
  p.Offset(letter_pos_[27].x() - letter_pos_[26].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr iiin");
  // Move back a couple of character widths and confirm the selection still
  // updates accordingly.
  p.Offset(letter_pos_[25].x() - letter_pos_[26].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr iii");
  p.Offset(letter_pos_[24].x() - letter_pos_[25].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr ii");
  // "Catch up" with the handle - move the extent to where the handle is.
  // "abcdef ghij kl mno^pqr ii|>inmni, abc"
  Selection().MoveRangeSelectionExtent(letter_pos_[24]);
  EXPECT_EQ_SELECTED_TEXT("pqr ii");
  // Move ahead and confirm the selection expands accordingly
  // "abcdef ghij kl mno^pqr iii|>nmni, abc"
  Selection().MoveRangeSelectionExtent(letter_pos_[25]);
  EXPECT_EQ_SELECTED_TEXT("pqr iii");

  // Confirm we stay in character granularity if the user moves within a word.
  // "abcdef ghij kl mno^pqr |>iiinmni, abc"
  Selection().MoveRangeSelectionExtent(letter_pos_[22]);
  EXPECT_EQ_SELECTED_TEXT("pqr ");
  // It's possible to get a move when position doesn't change.
  // It shouldn't affect anything.
  p = letter_pos_[22];
  p.Offset(1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("pqr ");
  // "abcdef ghij kl mno^pqr i|>iinmni, abc"
  Selection().MoveRangeSelectionExtent(letter_pos_[23]);
  EXPECT_EQ_SELECTED_TEXT("pqr i");
}

// Tests moving selection extent over to the other side of the base
// on text "abcd efgh ijkl mno^pqr|> iiinmni, abc"
// (^ means base, | means extent, < means start, and > means end).
// Text needs to be laid out on a single line with no rotation.
void GranularityStrategyTest::TestDirectionSwitchSide() {
  // Move to the middle of word #4, selecting it - this will set the offset to
  // be half the width of "iiinmni.
  Selection().MoveRangeSelectionExtent(word_middles_[4]);
  EXPECT_EQ_SELECTED_TEXT("pqr iiinmni");
  // Move back leaving only one letter selected.
  gfx::Point p = word_middles_[4];
  p.Offset(letter_pos_[19].x() - letter_pos_[29].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("p");
  // Confirm selection doesn't change if extent is positioned at base.
  p.Offset(letter_pos_[18].x() - letter_pos_[19].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("p");
  // Move over to the other side of the base. Confirm the offset is preserved.
  // (i.e. the selection start stays on the right of the extent)
  // Confirm we stay in character granularity until the beginning of the word
  // is passed.
  p.Offset(letter_pos_[17].x() - letter_pos_[18].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("o");
  p.Offset(letter_pos_[16].x() - letter_pos_[17].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("no");
  p.Offset(letter_pos_[14].x() - letter_pos_[16].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT(" mno");
  // Move to just one pixel on the right before the middle of the word #2.
  // We should switch to word granularity, so the selection shouldn't change.
  p.Offset(word_middles_[2].x() - letter_pos_[14].x() + 1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT(" mno");
  // Move over the middle of the word. The word should get selected.
  // This should reduce the offset, but it should still stay greated than 0,
  // since the width of "iiinmni" is greater than the width of "ijkl".
  p.Offset(-2, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("ijkl mno");
  // Move to just one pixel on the right of the middle of word #1.
  // The selection should now include the space between the words.
  p.Offset(word_middles_[1].x() - letter_pos_[10].x() + 1, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT(" ijkl mno");
  // Move over the middle of the word. The word should get selected.
  p.Offset(-2, 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("efgh ijkl mno");
}

// Test for the default CharacterGranularityStrategy
TEST_F(GranularityStrategyTest, Character) {
  GetDummyPageHolder().GetFrame().GetSettings()->SetSelectionStrategy(
      SelectionStrategy::kCharacter);
  GetDummyPageHolder().GetFrame().GetSettings()->SetDefaultFontSize(12);
  // "Foo Bar Baz,"
  Text* text = AppendTextNode("Foo Bar Baz,");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // "Foo B^a|>r Baz," (^ means base, | means extent, , < means start, and >
  // means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 5), Position(text, 6))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("a");
  // "Foo B^ar B|>az,"
  Selection().MoveRangeSelectionExtent(
      VisiblePositionToContentsPoint(CreateVisiblePosition(Position(text, 9))));
  EXPECT_EQ_SELECTED_TEXT("ar B");
  // "F<|oo B^ar Baz,"
  Selection().MoveRangeSelectionExtent(
      VisiblePositionToContentsPoint(CreateVisiblePosition(Position(text, 1))));
  EXPECT_EQ_SELECTED_TEXT("oo B");
}

// DirectionGranularityStrategy strategy on rotated text should revert to the
// same behavior as CharacterGranularityStrategy
TEST_F(GranularityStrategyTest, DirectionRotate) {
  Text* text = SetupRotate("Foo Bar Baz,");
  // "Foo B^a|>r Baz," (^ means base, | means extent, , < means start, and >
  // means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 5), Position(text, 6))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("a");
  gfx::Point p = letter_pos_[9];
  // Need to move by one pixel, otherwise this point is not evaluated
  // to the same line as the text by hit testing.
  p.Offset(1, 0);
  // "Foo B^ar B|>az,"
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("ar B");
  p = letter_pos_[1];
  p.Offset(1, 0);
  // "F<|oo B^ar Baz,"
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("oo B");
}

TEST_F(GranularityStrategyTest, DirectionExpandTranslateZ) {
  Text* text = SetupTranslateZ("abcdef ghij kl mnopqr stuvwi inm mnii,");
  // "abcdef ghij kl mno^p|>qr stuvwi inm  mnii," (^ means base, | means extent,
  // < means start, and > means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 19))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("p");
  TestDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionExpandTransform) {
  Text* text = SetupTransform("abcdef ghij kl mnopqr stuvwi inm mnii,");
  // "abcdef ghij kl mno^p|>qr stuvwi inm  mnii," (^ means base, | means extent,
  // < means start, and > means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 19))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("p");
  TestDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionExpandVerticalAlign) {
  // "abcdef ghij kl mno^p|>qr stuvwi inm  mnii," (^ means base, | means extent,
  // < means start, and > means end).
  SetupVerticalAlign("abcdef ghij kl m", "nopq", "r stuvwi inm mnii,", 18, 19);
  EXPECT_EQ_SELECTED_TEXT("p");
  TestDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionExpandFontSizes) {
  SetupFontSize("abcdef ghij kl mnopqr st", "uv", "wi inm mnii,", 18, 19);
  EXPECT_EQ_SELECTED_TEXT("p");
  TestDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionShrinkTranslateZ) {
  Text* text = SetupTranslateZ("abcdef ghij kl mnopqr iiinmni, abc");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 21))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionShrinkTransform) {
  Text* text = SetupTransform("abcdef ghij kl mnopqr iiinmni, abc");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 21))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionShrinkVerticalAlign) {
  SetupVerticalAlign("abcdef ghij kl mnopqr ii", "inm", "ni, abc", 18, 21);
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionShrinkFontSizes) {
  SetupFontSize("abcdef ghij kl mnopqr ii", "inm", "ni, abc", 18, 21);
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideTranslateZ) {
  Text* text = SetupTranslateZ("abcd efgh ijkl mnopqr iiinmni, abc");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 21))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionSwitchSide();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideTransform) {
  Text* text = SetupTransform("abcd efgh ijkl mnopqr iiinmni, abc");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 21))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionSwitchSide();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideVerticalAlign) {
  SetupVerticalAlign("abcd efgh ijkl", " mnopqr", " iiinmni, abc", 18, 21);
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionSwitchSide();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideFontSizes) {
  SetupFontSize("abcd efgh i", "jk", "l mnopqr iiinmni, abc", 18, 21);
  EXPECT_EQ_SELECTED_TEXT("pqr");
  TestDirectionSwitchSide();
}

// Tests moving extent over to the other side of the vase and immediately
// passing the word boundary and going into word granularity.
TEST_F(GranularityStrategyTest, DirectionSwitchSideWordGranularityThenShrink) {
  GetDummyPageHolder().GetFrame().GetSettings()->SetDefaultFontSize(12);
  String str = "ab cd efghijkl mnopqr iiin, abc";
  Text* text = GetDocument().createTextNode(str);
  GetDocument().body()->AppendChild(text);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDummyPageHolder().GetFrame().GetSettings()->SetSelectionStrategy(
      SelectionStrategy::kDirection);

  ParseText(text);

  // "abcd efgh ijkl mno^pqr|> iiin, abc" (^ means base, | means extent, < means
  // start, and > means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 18), Position(text, 21))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("pqr");
  // Move to the middle of word #4 selecting it - this will set the offset to
  // be half the width of "iiin".
  Selection().MoveRangeSelectionExtent(word_middles_[4]);
  EXPECT_EQ_SELECTED_TEXT("pqr iiin");
  // Move to the middle of word #2 - extent will switch over to the other
  // side of the base, and we should enter word granularity since we pass
  // the word boundary. The offset should become negative since the width
  // of "efghjkkl" is greater than that of "iiin".
  int offset = letter_pos_[26].x() - word_middles_[4].x();
  gfx::Point p =
      gfx::Point(word_middles_[2].x() - offset - 1, word_middles_[2].y());
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("efghijkl mno");
  p.Offset(letter_pos_[7].x() - letter_pos_[6].x(), 0);
  Selection().MoveRangeSelectionExtent(p);
  EXPECT_EQ_SELECTED_TEXT("fghijkl mno");
}

// Make sure we switch to word granularity right away when starting on a
// word boundary and extending.
TEST_F(GranularityStrategyTest, DirectionSwitchStartOnBoundary) {
  GetDummyPageHolder().GetFrame().GetSettings()->SetDefaultFontSize(12);
  String str = "ab cd efghijkl mnopqr iiin, abc";
  Text* text = GetDocument().createTextNode(str);
  GetDocument().body()->AppendChild(text);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDummyPageHolder().GetFrame().GetSettings()->SetSelectionStrategy(
      SelectionStrategy::kDirection);

  ParseText(text);

  // "ab cd efghijkl ^mnopqr |>stuvwi inm," (^ means base and | means extent,
  // > means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 15), Position(text, 22))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("mnopqr ");
  Selection().MoveRangeSelectionExtent(word_middles_[4]);
  EXPECT_EQ_SELECTED_TEXT("mnopqr iiin");
}

// For http://crbug.com/704529
TEST_F(GranularityStrategyTest, UpdateExtentWithNullPositionForCharacter) {
  GetDummyPageHolder().GetFrame().GetSettings()->SetSelectionStrategy(
      SelectionStrategy::kCharacter);
  GetDocument().body()->setInnerHTML(
      "<div id=host></div><div id=sample>ab</div>");
  // Simulate VIDEO element which has a RANGE as slider of video time.
  Element* const host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<input type=range>");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  const SelectionInDOMTree& selection_in_dom_tree =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 2))
          .Build();
  Selection().SetSelection(selection_in_dom_tree,
                           SetSelectionOptions::Builder()
                               .SetShouldCloseTyping(true)
                               .SetShouldClearTypingStyle(true)
                               .SetShouldShowHandle(true)
                               .SetIsDirectional(true)
                               .Build());

  // Since, it is not obvious that
  // |PositionForContentsPointRespectingEditingBoundary()| returns null
  // position, we verify here.
  ASSERT_EQ(Position(), CreateVisiblePosition(
                            PositionForContentsPointRespectingEditingBoundary(
                                gfx::Point(0, 0), &GetFrame()))
                            .DeepEquivalent())
      << "This test requires null position.";

  // Point to RANGE inside shadow root to get null position from
  // |visiblePositionForContentsPoint()|.
  Selection().MoveRangeSelectionExtent(gfx::Point(0, 0));
  EXPECT_EQ(selection_in_dom_tree, Selection().GetSelectionInDOMTree());
}

// For http://crbug.com/704529
TEST_F(GranularityStrategyTest, UpdateExtentWithNullPositionForDirectional) {
  GetDocument().body()->setInnerHTML(
      "<div id=host></div><div id=sample>ab</div>");
  // Simulate VIDEO element which has a RANGE as slider of video time.
  Element* const host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<input type=range>");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  const SelectionInDOMTree& selection_in_dom_tree =
      SelectionInDOMTree::Builder()
          .Collapse(Position(sample->firstChild(), 2))
          .Build();
  Selection().SetSelection(selection_in_dom_tree,
                           SetSelectionOptions::Builder()
                               .SetShouldCloseTyping(true)
                               .SetShouldClearTypingStyle(true)
                               .SetShouldShowHandle(true)
                               .SetIsDirectional(true)
                               .Build());

  // Since, it is not obvious that
  // |PositionForContentsPointRespectingEditingBoundary()| returns null
  // position, we verify here.
  ASSERT_EQ(Position(), CreateVisiblePosition(
                            PositionForContentsPointRespectingEditingBoundary(
                                gfx::Point(0, 0), &GetFrame()))
                            .DeepEquivalent())
      << "This test requires null position.";

  // Point to RANGE inside shadow root to get null position from
  // |visiblePositionForContentsPoint()|.
  Selection().MoveRangeSelectionExtent(gfx::Point(0, 0));

  EXPECT_EQ(selection_in_dom_tree, Selection().GetSelectionInDOMTree());
}

// For http://crbug.com/974728
TEST_F(GranularityStrategyTest, UpdateExtentWithNullNextWordBound) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      "<style>body { margin: 0; padding: 0; font: 10px monospace; }</style>"
      "<div contenteditable id=target></div>|def^");
  Selection().SetSelection(selection, SetSelectionOptions());

  // Move inside content editable
  ASSERT_EQ(
      Position(*GetDocument().getElementById(AtomicString("target")), 0),
      CreateVisiblePosition(PositionForContentsPointRespectingEditingBoundary(
                                gfx::Point(0, 0), &GetFrame()))
          .DeepEquivalent())
      << "We extend selection inside content editable.";
  Selection().MoveRangeSelectionExtent(gfx::Point(0, 0));

  EXPECT_EQ(selection, Selection().GetSelectionInDOMTree());
}

}  // namespace blink
