/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace text_iterator_test {

TextIteratorBehavior CollapseTrailingSpaceBehavior() {
  return TextIteratorBehavior::Builder().SetCollapseTrailingSpace(true).Build();
}

TextIteratorBehavior EmitsImageAltTextBehavior() {
  return TextIteratorBehavior::Builder().SetEmitsImageAltText(true).Build();
}

TextIteratorBehavior EntersTextControlsBehavior() {
  return TextIteratorBehavior::Builder().SetEntersTextControls(true).Build();
}

TextIteratorBehavior EntersOpenShadowRootsBehavior() {
  return TextIteratorBehavior::Builder().SetEntersOpenShadowRoots(true).Build();
}

TextIteratorBehavior EmitsObjectReplacementCharacterBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .Build();
}

TextIteratorBehavior EmitsSmallXForTextSecurityBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsSmallXForTextSecurity(true)
      .Build();
}

TextIteratorBehavior EmitsCharactersBetweenAllVisiblePositionsBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsCharactersBetweenAllVisiblePositions(true)
      .Build();
}

TextIteratorBehavior EmitsSpaceForNbspBehavior() {
  return TextIteratorBehavior::Builder().SetEmitsSpaceForNbsp(true).Build();
}

struct DOMTree : NodeTraversal {
  using PositionType = Position;
  using TextIteratorType = TextIterator;
};

struct FlatTree : FlatTreeTraversal {
  using PositionType = PositionInFlatTree;
  using TextIteratorType = TextIteratorInFlatTree;
};

class TextIteratorTest : public testing::WithParamInterface<bool>,
                         private ScopedLayoutNGForTest,
                         public EditingTestBase {
 protected:
  TextIteratorTest() : ScopedLayoutNGForTest(GetParam()) {}

  bool LayoutNGEnabled() const { return GetParam(); }

  template <typename Tree>
  std::string Iterate(const TextIteratorBehavior& = TextIteratorBehavior());

  template <typename Tree>
  std::string IteratePartial(
      const typename Tree::PositionType& start,
      const typename Tree::PositionType& end,
      const TextIteratorBehavior& = TextIteratorBehavior());

  Range* GetBodyRange() const;

  int TestRangeLength(const std::string& selection_text) {
    return TextIterator::RangeLength(
        SetSelectionTextToBody(selection_text).ComputeRange());
  }

 private:
  template <typename Tree>
  std::string IterateWithIterator(typename Tree::TextIteratorType&);
};

template <typename Tree>
std::string TextIteratorTest::Iterate(
    const TextIteratorBehavior& iterator_behavior) {
  Element* body = GetDocument().body();
  using PositionType = typename Tree::PositionType;
  auto start = PositionType(body, 0);
  auto end = PositionType(body, Tree::CountChildren(*body));
  typename Tree::TextIteratorType iterator(start, end, iterator_behavior);
  return IterateWithIterator<Tree>(iterator);
}

template <typename Tree>
std::string TextIteratorTest::IteratePartial(
    const typename Tree::PositionType& start,
    const typename Tree::PositionType& end,
    const TextIteratorBehavior& iterator_behavior) {
  typename Tree::TextIteratorType iterator(start, end, iterator_behavior);
  return IterateWithIterator<Tree>(iterator);
}

template <typename Tree>
std::string TextIteratorTest::IterateWithIterator(
    typename Tree::TextIteratorType& iterator) {
  StringBuilder text_chunks;
  for (; !iterator.AtEnd(); iterator.Advance()) {
    text_chunks.Append('[');
    text_chunks.Append(iterator.GetText().GetTextForTesting());
    text_chunks.Append(']');
  }
  return text_chunks.ToString().Utf8();
}

Range* TextIteratorTest::GetBodyRange() const {
  Range* range = Range::Create(GetDocument());
  range->selectNode(GetDocument().body());
  return range;
}

INSTANTIATE_TEST_SUITE_P(All, TextIteratorTest, testing::Bool());

TEST_P(TextIteratorTest, BitStackOverflow) {
  const unsigned kBitsInWord = sizeof(unsigned) * 8;
  BitStack bs;

  for (unsigned i = 0; i < kBitsInWord + 1u; i++)
    bs.Push(true);

  bs.Pop();

  EXPECT_TRUE(bs.Top());
}

TEST_P(TextIteratorTest, BasicIteration) {
  static const char* input = "<p>Hello, \ntext</p><p>iterator.</p>";
  SetBodyContent(input);
  EXPECT_EQ("[Hello, ][text][\n][\n][iterator.]", Iterate<DOMTree>());
  EXPECT_EQ("[Hello, ][text][\n][\n][iterator.]", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, EmitsSmallXForTextSecurity) {
  InsertStyleElement("s {-webkit-text-security:disc;}");
  SetBodyContent("abc<s>foo</s>baz");
  // E2 80 A2 is U+2022 BULLET
  EXPECT_EQ("[abc][xxx][baz]",
            Iterate<DOMTree>(EmitsSmallXForTextSecurityBehavior()));
  EXPECT_EQ("[abc][\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2][baz]",
            Iterate<DOMTree>(TextIteratorBehavior()));
  EXPECT_EQ("[abc][xxx][baz]",
            Iterate<FlatTree>(EmitsSmallXForTextSecurityBehavior()));
  EXPECT_EQ("[abc][\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2][baz]",
            Iterate<FlatTree>(TextIteratorBehavior()));
}

TEST_P(TextIteratorTest, IgnoreAltTextInTextControls) {
  static const char* input = "<p>Hello <input type='text' value='value'>!</p>";
  SetBodyContent(input);
  EXPECT_EQ("[Hello ][][!]", Iterate<DOMTree>(EmitsImageAltTextBehavior()));
  EXPECT_EQ("[Hello ][][!]", Iterate<FlatTree>(EmitsImageAltTextBehavior()));
}

TEST_P(TextIteratorTest, DisplayAltTextInImageControls) {
  static const char* input = "<p>Hello <input type='image' alt='alt'>!</p>";
  SetBodyContent(input);
  EXPECT_EQ("[Hello ][alt][!]", Iterate<DOMTree>(EmitsImageAltTextBehavior()));
  EXPECT_EQ("[Hello ][alt][!]", Iterate<FlatTree>(EmitsImageAltTextBehavior()));
}

TEST_P(TextIteratorTest, NotEnteringTextControls) {
  static const char* input = "<p>Hello <input type='text' value='input'>!</p>";
  SetBodyContent(input);
  EXPECT_EQ("[Hello ][][!]", Iterate<DOMTree>());
  EXPECT_EQ("[Hello ][][!]", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, EnteringTextControlsWithOption) {
  static const char* input = "<p>Hello <input type='text' value='input'>!</p>";
  SetBodyContent(input);
  EXPECT_EQ("[Hello ][\n][input][!]",
            Iterate<DOMTree>(EntersTextControlsBehavior()));
  EXPECT_EQ("[Hello ][\n][input][\n][!]",
            Iterate<FlatTree>(EntersTextControlsBehavior()));
}

TEST_P(TextIteratorTest, EnteringTextControlsWithOptionComplex) {
  static const char* input =
      "<input type='text' value='Beginning of range'><div><div><input "
      "type='text' value='Under DOM nodes'></div></div><input type='text' "
      "value='End of range'>";
  SetBodyContent(input);
  EXPECT_EQ("[\n][Beginning of range][\n][Under DOM nodes][\n][End of range]",
            Iterate<DOMTree>(EntersTextControlsBehavior()));
  EXPECT_EQ("[Beginning of range][\n][Under DOM nodes][\n][End of range]",
            Iterate<FlatTree>(EntersTextControlsBehavior()));
}

TEST_P(TextIteratorTest, NotEnteringShadowTree) {
  static const char* body_content =
      "<div>Hello, <span id='host'>text</span> iterator.</div>";
  static const char* shadow_content = "<span>shadow</span>";
  SetBodyContent(body_content);
  CreateShadowRootForElementWithIDAndSetInnerHTML(GetDocument(), "host",
                                                  shadow_content);
  // TextIterator doesn't emit "text" since its layoutObject is not created. The
  // shadow tree is ignored.
  EXPECT_EQ("[Hello, ][ iterator.]", Iterate<DOMTree>());
  EXPECT_EQ("[Hello, ][shadow][ iterator.]", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, NotEnteringShadowTreeWithNestedShadowTrees) {
  static const char* body_content =
      "<div>Hello, <span id='host-in-document'>text</span> iterator.</div>";
  static const char* shadow_content1 =
      "<span>first <span id='host-in-shadow'>shadow</span></span>";
  static const char* shadow_content2 = "<span>second shadow</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root1 = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), "host-in-document", shadow_content1);
  CreateShadowRootForElementWithIDAndSetInnerHTML(
      *shadow_root1, "host-in-shadow", shadow_content2);
  EXPECT_EQ("[Hello, ][ iterator.]", Iterate<DOMTree>());
  EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]",
            Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, NotEnteringShadowTreeWithContentInsertionPoint) {
  static const char* body_content =
      "<div>Hello, <span id='host'>text</span> iterator.</div>";
  static const char* shadow_content =
      "<span>shadow <content>content</content></span>";
  SetBodyContent(body_content);
  CreateShadowRootForElementWithIDAndSetInnerHTML(GetDocument(), "host",
                                                  shadow_content);
  // In this case a layoutObject for "text" is created, so it shows up here.
  EXPECT_EQ("[Hello, ][text][ iterator.]", Iterate<DOMTree>());
  EXPECT_EQ("[Hello, ][shadow ][text][ iterator.]", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, EnteringShadowTreeWithOption) {
  static const char* body_content =
      "<div>Hello, <span id='host'>text</span> iterator.</div>";
  static const char* shadow_content = "<span>shadow</span>";
  SetBodyContent(body_content);
  CreateShadowRootForElementWithIDAndSetInnerHTML(GetDocument(), "host",
                                                  shadow_content);
  // TextIterator emits "shadow" since entersOpenShadowRootsBehavior() is
  // specified.
  EXPECT_EQ("[Hello, ][shadow][ iterator.]",
            Iterate<DOMTree>(EntersOpenShadowRootsBehavior()));
  EXPECT_EQ("[Hello, ][shadow][ iterator.]",
            Iterate<FlatTree>(EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest, EnteringShadowTreeWithNestedShadowTreesWithOption) {
  static const char* body_content =
      "<div>Hello, <span id='host-in-document'>text</span> iterator.</div>";
  static const char* shadow_content1 =
      "<span>first <span id='host-in-shadow'>shadow</span></span>";
  static const char* shadow_content2 = "<span>second shadow</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root1 = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), "host-in-document", shadow_content1);
  CreateShadowRootForElementWithIDAndSetInnerHTML(
      *shadow_root1, "host-in-shadow", shadow_content2);
  EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]",
            Iterate<DOMTree>(EntersOpenShadowRootsBehavior()));
  EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]",
            Iterate<FlatTree>(EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest,
       EnteringShadowTreeWithContentInsertionPointWithOption) {
  static const char* body_content =
      "<div>Hello, <span id='host'>text</span> iterator.</div>";
  static const char* shadow_content =
      "<span><content>content</content> shadow</span>";
  // In this case a layoutObject for "text" is created, and emitted AFTER any
  // nodes in the shadow tree. This order does not match the order of the
  // rendered texts, but at this moment it's the expected behavior.
  // FIXME: Fix this. We probably need pure-renderer-based implementation of
  // TextIterator to achieve this.
  SetBodyContent(body_content);
  CreateShadowRootForElementWithIDAndSetInnerHTML(GetDocument(), "host",
                                                  shadow_content);
  EXPECT_EQ("[Hello, ][ shadow][text][ iterator.]",
            Iterate<DOMTree>(EntersOpenShadowRootsBehavior()));
  EXPECT_EQ("[Hello, ][text][ shadow][ iterator.]",
            Iterate<FlatTree>(EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest, StartingAtNodeInShadowRoot) {
  static const char* body_content =
      "<div id='outer'>Hello, <span id='host'>text</span> iterator.</div>";
  static const char* shadow_content =
      "<span><content>content</content> shadow</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), "host", shadow_content);
  Node* outer_div = GetDocument().getElementById("outer");
  Node* span_in_shadow = shadow_root->firstChild();
  Position start(span_in_shadow, PositionAnchorType::kBeforeChildren);
  Position end(outer_div, PositionAnchorType::kAfterChildren);
  EXPECT_EQ(
      "[ shadow][text][ iterator.]",
      IteratePartial<DOMTree>(start, end, EntersOpenShadowRootsBehavior()));

  PositionInFlatTree start_in_flat_tree(span_in_shadow,
                                        PositionAnchorType::kBeforeChildren);
  PositionInFlatTree end_in_flat_tree(outer_div,
                                      PositionAnchorType::kAfterChildren);
  EXPECT_EQ("[text][ shadow][ iterator.]",
            IteratePartial<FlatTree>(start_in_flat_tree, end_in_flat_tree,
                                     EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest, FinishingAtNodeInShadowRoot) {
  static const char* body_content =
      "<div id='outer'>Hello, <span id='host'>text</span> iterator.</div>";
  static const char* shadow_content =
      "<span><content>content</content> shadow</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = CreateShadowRootForElementWithIDAndSetInnerHTML(
      GetDocument(), "host", shadow_content);
  Node* outer_div = GetDocument().getElementById("outer");
  Node* span_in_shadow = shadow_root->firstChild();
  Position start(outer_div, PositionAnchorType::kBeforeChildren);
  Position end(span_in_shadow, PositionAnchorType::kAfterChildren);
  EXPECT_EQ(
      "[Hello, ][ shadow]",
      IteratePartial<DOMTree>(start, end, EntersOpenShadowRootsBehavior()));

  PositionInFlatTree start_in_flat_tree(outer_div,
                                        PositionAnchorType::kBeforeChildren);
  PositionInFlatTree end_in_flat_tree(span_in_shadow,
                                      PositionAnchorType::kAfterChildren);
  EXPECT_EQ("[Hello, ][text][ shadow]",
            IteratePartial<FlatTree>(start_in_flat_tree, end_in_flat_tree,
                                     EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest, FullyClipsContents) {
  static const char* body_content =
      "<div style='overflow: hidden; width: 200px; height: 0;'>"
      "I'm invisible"
      "</div>";
  SetBodyContent(body_content);
  EXPECT_EQ("", Iterate<DOMTree>());
  EXPECT_EQ("", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, IgnoresContainerClip) {
  static const char* body_content =
      "<div style='overflow: hidden; width: 200px; height: 0;'>"
      "<div>I'm not visible</div>"
      "<div style='position: absolute; width: 200px; height: 200px; top: 0; "
      "right: 0;'>"
      "but I am!"
      "</div>"
      "</div>";
  SetBodyContent(body_content);
  EXPECT_EQ("[but I am!]", Iterate<DOMTree>());
  EXPECT_EQ("[but I am!]", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, FullyClippedContentsDistributed) {
  static const char* body_content =
      "<div id='host'>"
      "<div>Am I visible?</div>"
      "</div>";
  static const char* shadow_content =
      "<div style='overflow: hidden; width: 200px; height: 0;'>"
      "<content></content>"
      "</div>";
  SetBodyContent(body_content);
  CreateShadowRootForElementWithIDAndSetInnerHTML(GetDocument(), "host",
                                                  shadow_content);
  // FIXME: The text below is actually invisible but TextIterator currently
  // thinks it's visible.
  EXPECT_EQ("[\n][Am I visible?]",
            Iterate<DOMTree>(EntersOpenShadowRootsBehavior()));
  EXPECT_EQ("", Iterate<FlatTree>(EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest, IgnoresContainersClipDistributed) {
  static const char* body_content =
      "<div id='host' style='overflow: hidden; width: 200px; height: 0;'>"
      "<div>Nobody can find me!</div>"
      "</div>";
  static const char* shadow_content =
      "<div style='position: absolute; width: 200px; height: 200px; top: 0; "
      "right: 0;'>"
      "<content></content>"
      "</div>";
  SetBodyContent(body_content);
  CreateShadowRootForElementWithIDAndSetInnerHTML(GetDocument(), "host",
                                                  shadow_content);
  // FIXME: The text below is actually visible but TextIterator currently thinks
  // it's invisible.
  // [\n][Nobody can find me!]
  EXPECT_EQ("", Iterate<DOMTree>(EntersOpenShadowRootsBehavior()));
  EXPECT_EQ("[Nobody can find me!]",
            Iterate<FlatTree>(EntersOpenShadowRootsBehavior()));
}

TEST_P(TextIteratorTest, EmitsReplacementCharForInput) {
  static const char* body_content =
      "<div contenteditable='true'>"
      "Before"
      "<img src='foo.png'>"
      "After"
      "</div>";
  SetBodyContent(body_content);
  EXPECT_EQ("[Before][\xEF\xBF\xBC][After]",
            Iterate<DOMTree>(EmitsObjectReplacementCharacterBehavior()));
  EXPECT_EQ("[Before][\xEF\xBF\xBC][After]",
            Iterate<FlatTree>(EmitsObjectReplacementCharacterBehavior()));
}

TEST_P(TextIteratorTest, RangeLengthWithReplacedElements) {
  static const char* body_content =
      "<div id='div' contenteditable='true'>1<img src='foo.png'>3</div>";
  SetBodyContent(body_content);
  UpdateAllLifecyclePhasesForTest();

  Node* div_node = GetDocument().getElementById("div");
  const EphemeralRange range(Position(div_node, 0), Position(div_node, 3));

  EXPECT_EQ(3, TextIterator::RangeLength(range));
}

TEST_P(TextIteratorTest, RangeLengthInMultilineSpan) {
  static const char* body_content =
      "<table style='width:5em'>"
      "<tbody>"
      "<tr>"
      "<td>"
      "<span id='span1'>one two three four five</span>"
      "</td>"
      "</tr>"
      "</tbody>"
      "</table>";

  SetBodyContent(body_content);
  UpdateAllLifecyclePhasesForTest();

  Node* span_node = GetDocument().getElementById("span1");
  Node* text_node = span_node->firstChild();

  // Select the word "two", this is the last word on the line.

  const EphemeralRange range(Position(text_node, 4), Position(text_node, 7));

  EXPECT_EQ(LayoutNGEnabled() ? 3 : 4, TextIterator::RangeLength(range));
  EXPECT_EQ(3, TextIterator::RangeLength(
                   range,
                   TextIteratorBehavior::NoTrailingSpaceRangeLengthBehavior()));
}

TEST_P(TextIteratorTest, RangeLengthBasic) {
  EXPECT_EQ(0, TestRangeLength("<p>^| (1) abc def</p>"));
  EXPECT_EQ(0, TestRangeLength("<p>^ |(1) abc def</p>"));
  EXPECT_EQ(1, TestRangeLength("<p>^ (|1) abc def</p>"));
  EXPECT_EQ(2, TestRangeLength("<p>^ (1|) abc def</p>"));
  EXPECT_EQ(3, TestRangeLength("<p>^ (1)| abc def</p>"));
  EXPECT_EQ(4, TestRangeLength("<p>^ (1) |abc def</p>"));
  EXPECT_EQ(5, TestRangeLength("<p>^ (1) a|bc def</p>"));
  EXPECT_EQ(6, TestRangeLength("<p>^ (1) ab|c def</p>"));
  EXPECT_EQ(7, TestRangeLength("<p>^ (1) abc| def</p>"));
  EXPECT_EQ(8, TestRangeLength("<p>^ (1) abc |def</p>"));
  EXPECT_EQ(9, TestRangeLength("<p>^ (1) abc d|ef</p>"));
  EXPECT_EQ(10, TestRangeLength("<p>^ (1) abc de|f</p>"));
  EXPECT_EQ(11, TestRangeLength("<p>^ (1) abc def|</p>"));
}

TEST_P(TextIteratorTest, RangeLengthWithFirstLetter) {
  InsertStyleElement("p::first-letter {font-size:200%;}");
  // Expectation should be as same as |RangeLengthBasic|
  EXPECT_EQ(0, TestRangeLength("<p>^| (1) abc def</p>"));
  EXPECT_EQ(0, TestRangeLength("<p>^ |(1) abc def</p>"));
  EXPECT_EQ(1, TestRangeLength("<p>^ (|1) abc def</p>"));
  EXPECT_EQ(2, TestRangeLength("<p>^ (1|) abc def</p>"));
  EXPECT_EQ(3, TestRangeLength("<p>^ (1)| abc def</p>"));
  EXPECT_EQ(4, TestRangeLength("<p>^ (1) |abc def</p>"));
  EXPECT_EQ(5, TestRangeLength("<p>^ (1) a|bc def</p>"));
  EXPECT_EQ(6, TestRangeLength("<p>^ (1) ab|c def</p>"));
  EXPECT_EQ(7, TestRangeLength("<p>^ (1) abc| def</p>"));
  EXPECT_EQ(8, TestRangeLength("<p>^ (1) abc |def</p>"));
  EXPECT_EQ(9, TestRangeLength("<p>^ (1) abc d|ef</p>"));
  EXPECT_EQ(10, TestRangeLength("<p>^ (1) abc de|f</p>"));
  EXPECT_EQ(11, TestRangeLength("<p>^ (1) abc def|</p>"));
}

TEST_P(TextIteratorTest, RangeLengthWithFirstLetterMultipleLeadingSpaces) {
  InsertStyleElement("p::first-letter {font-size:200%;}");
  EXPECT_EQ(0, TestRangeLength("<p>^|   foo</p>"));
  EXPECT_EQ(0, TestRangeLength("<p>^ |  foo</p>"));
  EXPECT_EQ(0, TestRangeLength("<p>^  | foo</p>"));
  EXPECT_EQ(0, TestRangeLength("<p>^   |foo</p>"));
  EXPECT_EQ(1, TestRangeLength("<p>^   f|oo</p>"));
  EXPECT_EQ(2, TestRangeLength("<p>^   fo|o</p>"));
  EXPECT_EQ(3, TestRangeLength("<p>^   foo|</p>"));
}

TEST_P(TextIteratorTest, WhitespaceCollapseForReplacedElements) {
  static const char* body_content =
      "<span>Some text </span> <input type='button' value='Button "
      "text'/><span>Some more text</span>";
  SetBodyContent(body_content);
  EXPECT_EQ("[Some text ][][Some more text]",
            Iterate<DOMTree>(CollapseTrailingSpaceBehavior()));
  // <input type=button> is not text control element
  EXPECT_EQ("[Some text ][][Button text][Some more text]",
            Iterate<FlatTree>(CollapseTrailingSpaceBehavior()));
}

TEST_P(TextIteratorTest, characterAt) {
  const char* body_content =
      "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
  const char* shadow_content =
      "three <content select=#two></content> <content select=#one></content> "
      "zero";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById("host");

  EphemeralRangeTemplate<EditingStrategy> range1(
      EphemeralRangeTemplate<EditingStrategy>::RangeOfContents(*host));
  TextIteratorAlgorithm<EditingStrategy> iter1(range1.StartPosition(),
                                               range1.EndPosition());
  const char* message1 = "|iter1| should emit 'one' and 'two'.";
  EXPECT_EQ('o', iter1.CharacterAt(0)) << message1;
  EXPECT_EQ('n', iter1.CharacterAt(1)) << message1;
  EXPECT_EQ('e', iter1.CharacterAt(2)) << message1;
  iter1.Advance();
  EXPECT_EQ('t', iter1.CharacterAt(0)) << message1;
  EXPECT_EQ('w', iter1.CharacterAt(1)) << message1;
  EXPECT_EQ('o', iter1.CharacterAt(2)) << message1;

  EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(
      EphemeralRangeTemplate<EditingInFlatTreeStrategy>::RangeOfContents(
          *host));
  TextIteratorAlgorithm<EditingInFlatTreeStrategy> iter2(range2.StartPosition(),
                                                         range2.EndPosition());
  const char* message2 =
      "|iter2| should emit 'three ', 'two', ' ', 'one' and ' zero'.";
  EXPECT_EQ('t', iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('h', iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('r', iter2.CharacterAt(2)) << message2;
  EXPECT_EQ('e', iter2.CharacterAt(3)) << message2;
  EXPECT_EQ('e', iter2.CharacterAt(4)) << message2;
  EXPECT_EQ(' ', iter2.CharacterAt(5)) << message2;
  iter2.Advance();
  EXPECT_EQ('t', iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('w', iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('o', iter2.CharacterAt(2)) << message2;
  iter2.Advance();
  EXPECT_EQ(' ', iter2.CharacterAt(0)) << message2;
  iter2.Advance();
  EXPECT_EQ('o', iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('n', iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('e', iter2.CharacterAt(2)) << message2;
  iter2.Advance();
  EXPECT_EQ(' ', iter2.CharacterAt(0)) << message2;
  EXPECT_EQ('z', iter2.CharacterAt(1)) << message2;
  EXPECT_EQ('e', iter2.CharacterAt(2)) << message2;
  EXPECT_EQ('r', iter2.CharacterAt(3)) << message2;
  EXPECT_EQ('o', iter2.CharacterAt(4)) << message2;
}

// Regression test for crbug.com/630921
TEST_P(TextIteratorTest, EndingConditionWithDisplayNone) {
  SetBodyContent(
      "<div style='display: none'><span>hello</span>world</div>Lorem ipsum "
      "dolor sit amet.");
  Position start(&GetDocument(), 0);
  Position end(GetDocument().QuerySelector("span"), 0);
  TextIterator iter(start, end);
  EXPECT_TRUE(iter.AtEnd());
}

// Trickier regression test for crbug.com/630921
TEST_P(TextIteratorTest, EndingConditionWithDisplayNoneInShadowTree) {
  const char* body_content =
      "<div style='display: none'><span id=host><a></a></span>world</div>Lorem "
      "ipsum dolor sit amet.";
  const char* shadow_content = "<i><b id=end>he</b></i>llo";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  ShadowRoot* shadow_root =
      GetDocument().getElementById("host")->OpenShadowRoot();
  Node* b_in_shadow_tree = shadow_root->getElementById("end");

  Position start(&GetDocument(), 0);
  Position end(b_in_shadow_tree, 0);
  TextIterator iter(start, end);
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, PreserveLeadingSpace) {
  SetBodyContent("<div style='width: 2em;'><b><i>foo</i></b> bar</div>");
  Element* div = GetDocument().QuerySelector("div");
  Position start(div->firstChild()->firstChild()->firstChild(), 0);
  Position end(div->lastChild(), 4);
  EXPECT_EQ("foo bar",
            PlainText(EphemeralRange(start, end), EmitsImageAltTextBehavior()));
}

// We used to have a bug where the leading space was duplicated if we didn't
// emit alt text, this tests for that bug
TEST_P(TextIteratorTest, PreserveLeadingSpaceWithoutEmittingAltText) {
  SetBodyContent("<div style='width: 2em;'><b><i>foo</i></b> bar</div>");
  Element* div = GetDocument().QuerySelector("div");
  Position start(div->firstChild()->firstChild()->firstChild(), 0);
  Position end(div->lastChild(), 4);
  EXPECT_EQ("foo bar", PlainText(EphemeralRange(start, end)));
}

TEST_P(TextIteratorTest, PreserveOnlyLeadingSpace) {
  SetBodyContent(
      "<div style='width: 2em;'><b><i id='foo'>foo </i></b> bar</div>");
  Element* div = GetDocument().QuerySelector("div");
  Position start(GetDocument().getElementById("foo")->firstChild(), 0);
  Position end(div->lastChild(), 4);
  EXPECT_EQ("foo bar",
            PlainText(EphemeralRange(start, end), EmitsImageAltTextBehavior()));
}

TEST_P(TextIteratorTest, StartAtFirstLetter) {
  SetBodyContent("<style>div:first-letter {color:red;}</style><div>Axyz</div>");

  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();
  Position start(text, 0);
  Position end(text, 4);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("A", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 0), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("xyz", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 4), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartInMultiCharFirstLetterWithCollapsedSpace) {
  SetBodyContent(
      "<style>div:first-letter {color:red;}</style><div>  (A)  xyz</div>");

  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();
  Position start(text, 3);
  Position end(text, 10);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("A)", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 3), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 5), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ(" ", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 5), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 6), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("xyz", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 7), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 10), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartAndEndInMultiCharFirstLetterWithCollapsedSpace) {
  SetBodyContent(
      "<style>div:first-letter {color:red;}</style><div>  (A)  xyz</div>");

  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();
  Position start(text, 3);
  Position end(text, 4);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("A", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 3), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 4), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartAtRemainingText) {
  SetBodyContent("<style>div:first-letter {color:red;}</style><div>Axyz</div>");

  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();
  Position start(text, 1);
  Position end(text, 4);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("xyz", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 4), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartAtFirstLetterInPre) {
  SetBodyContent("<style>pre:first-letter {color:red;}</style><pre>Axyz</pre>");

  Element* pre = GetDocument().QuerySelector("pre");
  Node* text = pre->firstChild();
  Position start(text, 0);
  Position end(text, 4);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("A", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 0), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("xyz", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 4), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartInMultiCharFirstLetterInPre) {
  SetBodyContent(
      "<style>pre:first-letter {color:red;}</style><pre>(A)xyz</pre>");

  Element* pre = GetDocument().QuerySelector("pre");
  Node* text = pre->firstChild();
  Position start(text, 1);
  Position end(text, 6);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("A)", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 3), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("xyz", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 3), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 6), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartAndEndInMultiCharFirstLetterInPre) {
  SetBodyContent(
      "<style>pre:first-letter {color:red;}</style><pre>(A)xyz</pre>");

  Element* pre = GetDocument().QuerySelector("pre");
  Node* text = pre->firstChild();
  Position start(text, 1);
  Position end(text, 2);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("A", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 2), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, StartAtRemainingTextInPre) {
  SetBodyContent("<style>pre:first-letter {color:red;}</style><pre>Axyz</pre>");

  Element* pre = GetDocument().QuerySelector("pre");
  Node* text = pre->firstChild();
  Position start(text, 1);
  Position end(text, 4);
  TextIterator iter(start, end);

  EXPECT_FALSE(iter.AtEnd());
  EXPECT_EQ("xyz", iter.GetText().GetTextForTesting());
  EXPECT_EQ(text, iter.CurrentContainer());
  EXPECT_EQ(Position(text, 1), iter.StartPositionInCurrentContainer());
  EXPECT_EQ(Position(text, 4), iter.EndPositionInCurrentContainer());

  iter.Advance();
  EXPECT_TRUE(iter.AtEnd());
}

TEST_P(TextIteratorTest, VisitsDisplayContentsChildren) {
  SetBodyContent(
      "<p>Hello, \ntext</p><p style='display: contents'>iterator.</p>");

  EXPECT_EQ("[Hello, ][text][iterator.]", Iterate<DOMTree>());
  EXPECT_EQ("[Hello, ][text][iterator.]", Iterate<FlatTree>());
}

TEST_P(TextIteratorTest, BasicIterationEmptyContent) {
  SetBodyContent("");
  EXPECT_EQ("", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationSingleCharacter) {
  SetBodyContent("a");
  EXPECT_EQ("[a]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationSingleDiv) {
  SetBodyContent("<div>a</div>");
  EXPECT_EQ("[a]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationMultipleDivs) {
  SetBodyContent("<div>a</div><div>b</div>");
  EXPECT_EQ("[a][\n][b]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationMultipleDivsWithStyle) {
  SetBodyContent(
      "<div style='line-height: 18px; min-height: 436px; '>"
        "debugging this note"
      "</div>");
  EXPECT_EQ("[debugging this note]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationMultipleDivsWithChildren) {
  SetBodyContent("<div>Hello<div><br><span></span></div></div>");
  EXPECT_EQ("[Hello][\n][\n]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationOnChildrenWithStyle) {
  SetBodyContent(
      "<div style='left:22px'>"
      "</div>"
      "\t\t\n"
      "<div style='left:26px'>"
      "</div>"
      "\t\t\n\n"
      "<div>"
        "\t\t\t\n"
        "<div>"
          "\t\t\t\t\n"
          "<div>"
            "\t\t\t\t\t\n"
            "<div contenteditable style='line-height: 20px; min-height: 580px; '>"
              "hey"
            "</div>"
            "\t\t\t\t\n"
          "</div>"
          "\t\t\t\n"
        "</div>"
        "\t\t\n"
      "</div>"
      "\n\t\n");
  EXPECT_EQ("[hey]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, BasicIterationInput) {
  SetBodyContent("<input id='a' value='b'>");
  auto* input_element = ToTextControl(GetDocument().getElementById("a"));
  const ShadowRoot* shadow_root = input_element->UserAgentShadowRoot();
  const Position start = Position::FirstPositionInNode(*shadow_root);
  const Position end = Position::LastPositionInNode(*shadow_root);
  EXPECT_EQ("[b]", IteratePartial<DOMTree>(start, end));
}

TEST_P(TextIteratorTest, BasicIterationInputiWithBr) {
  SetBodyContent("<input id='a' value='b'>");
  auto* input_element = ToTextControl(GetDocument().getElementById("a"));
  Element* inner_editor = input_element->InnerEditorElement();
  Element* br = GetDocument().CreateRawElement(html_names::kBrTag);
  inner_editor->AppendChild(br);
  const ShadowRoot* shadow_root = input_element->UserAgentShadowRoot();
  const Position start = Position::FirstPositionInNode(*shadow_root);
  const Position end = Position::LastPositionInNode(*shadow_root);
  GetDocument().UpdateStyleAndLayout();
  EXPECT_EQ("[b]", IteratePartial<DOMTree>(start, end));
}

TEST_P(TextIteratorTest, FloatLeft) {
  SetBodyContent("abc<span style='float:left'>DEF</span>ghi");
  EXPECT_EQ("[abc][DEF][ghi]", Iterate<DOMTree>())
      << "float doesn't affect text iteration";
}

TEST_P(TextIteratorTest, FloatRight) {
  SetBodyContent("abc<span style='float:right'>DEF</span>ghi");
  EXPECT_EQ("[abc][DEF][ghi]", Iterate<DOMTree>())
      << "float doesn't affect text iteration";
}

TEST_P(TextIteratorTest, InlineBlock) {
  SetBodyContent("abc<span style='display:inline-block'>DEF<br>GHI</span>jkl");
  EXPECT_EQ("[abc][DEF][\n][GHI][jkl]", Iterate<DOMTree>())
      << "inline-block doesn't insert newline around itself.";
}

TEST_P(TextIteratorTest, NoZWSForSpaceAfterNoWrapSpace) {
  SetBodyContent("<span style='white-space: nowrap'>foo </span> bar");
  EXPECT_EQ("[foo ][bar]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, PositionInShadowTree) {
  // Flat Tree: <div id=host>A<slot name=c><img slot=c alt=C></slot></div>
  SetBodyContent("<div id=host><a></a><b></b><img slot=c alt=C></div>");
  Element& host = *GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host.AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString("A<slot name=c></slot>");
  GetDocument().UpdateStyleAndLayout();
  Element& body = *GetDocument().body();
  Node& text_a = *shadow_root.firstChild();
  Node& slot = *shadow_root.lastChild();
  ASSERT_EQ("[A][C]", Iterate<FlatTree>(EmitsImageAltTextBehavior()));

  TextIteratorInFlatTree it(EphemeralRangeInFlatTree::RangeOfContents(body));

  EXPECT_EQ(PositionInFlatTree(text_a, 0),
            it.StartPositionInCurrentContainer());
  EXPECT_EQ(PositionInFlatTree(text_a, 1), it.EndPositionInCurrentContainer());

  ASSERT_FALSE(it.AtEnd());
  it.Advance();
  EXPECT_EQ(PositionInFlatTree(slot, 0), it.StartPositionInCurrentContainer());
  EXPECT_EQ(PositionInFlatTree(slot, 1), it.EndPositionInCurrentContainer());

  ASSERT_FALSE(it.AtEnd());
  it.Advance();
  EXPECT_EQ(PositionInFlatTree(body, 1), it.StartPositionInCurrentContainer());
  EXPECT_EQ(PositionInFlatTree(body, 1), it.EndPositionInCurrentContainer());

  ASSERT_TRUE(it.AtEnd());
}

TEST_P(TextIteratorTest, HiddenFirstLetter) {
  InsertStyleElement("body::first-letter{visibility:hidden}");
  SetBodyContent("foo");
  EXPECT_EQ("[oo]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, HiddenFirstLetterInPre) {
  InsertStyleElement(
      "body::first-letter{visibility:hidden} body{white-space:pre}");
  SetBodyContent("foo");
  EXPECT_EQ("[oo]", Iterate<DOMTree>());
}

TEST_P(TextIteratorTest, TextOffsetMappingAndFlatTree) {
  // Tests that TextOffsetMapping should skip text control even though it runs
  // on flat tree.
  SetBodyContent("foo <input value='bla bla. bla bla.'> bar");
  EXPECT_EQ(
      "[foo ][,][ bar]",
      Iterate<FlatTree>(EmitsCharactersBetweenAllVisiblePositionsBehavior()));
}

TEST_P(TextIteratorTest, EmitsSpaceForNbsp) {
  SetBodyContent("foo &nbsp;bar");
  EXPECT_EQ("[foo  bar]", Iterate<DOMTree>(EmitsSpaceForNbspBehavior()));
}

TEST_P(TextIteratorTest, IterateWithLockedSubtree) {
  SetBodyContent("<div id='parent'>foo<div id='locked'>text</div>bar</div>");
  auto* locked = GetDocument().getElementById("locked");
  locked->setAttribute("rendersubtree", "invisible activatable");
  GetDocument().UpdateStyleAndLayout();
  auto* parent = GetDocument().getElementById("parent");
  const Position start_position = Position::FirstPositionInNode(*parent);
  const Position end_position = Position::LastPositionInNode(*parent);
  EXPECT_EQ(6, TextIterator::RangeLength(start_position, end_position));
}

}  // namespace text_iterator_test
}  // namespace blink
