// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_units.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PositionUnitsParagraphTest : public EditingTestBase {};

TEST_F(PositionUnitsParagraphTest, StartOfParagraphWithPosition) {
  SetBodyContent("<div id=sample>1ab\nde</div>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 0), StartOfParagraph(Position(*text, 0)));
  EXPECT_EQ(Position(text, 0), StartOfParagraph(Position(*text, 3)));
  EXPECT_EQ(Position(text, 0), StartOfParagraph(Position(*text, 6)));
}

TEST_F(PositionUnitsParagraphTest, StartOfParagraphWithPositionPre) {
  SetBodyContent("<pre id=sample>1ab\nde</pre>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 0), StartOfParagraph(Position(*text, 0)));
  EXPECT_EQ(Position(text, 0), StartOfParagraph(Position(*text, 2)));
  EXPECT_EQ(Position(text, 4), StartOfParagraph(Position(*text, 4)));
  EXPECT_EQ(Position(text, 4), StartOfParagraph(Position(*text, 5)));
}

TEST_F(PositionUnitsParagraphTest, EndOfParagraphWithPositionSimple) {
  SetBodyContent("<div id=sample>1ab\nde</div>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 6), EndOfParagraph(Position(*text, 0)));
  EXPECT_EQ(Position(text, 6), EndOfParagraph(Position(*text, 3)));
  EXPECT_EQ(Position(text, 6), EndOfParagraph(Position(*text, 6)));
}

TEST_F(PositionUnitsParagraphTest, EndOfParagraphWithPositionPre) {
  SetBodyContent("<pre id=sample>1ab\nde</pre>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 3), EndOfParagraph(Position(*text, 0)));
  EXPECT_EQ(Position(text, 3), EndOfParagraph(Position(*text, 2)));
  EXPECT_EQ(Position(text, 3), EndOfParagraph(Position(*text, 3)));
  EXPECT_EQ(Position(text, 6), EndOfParagraph(Position(*text, 4)));
  EXPECT_EQ(Position(text, 6), EndOfParagraph(Position(*text, 6)));
}

TEST_F(PositionUnitsParagraphTest, IsStartOfParagraphWithPosition) {
  SetBodyContent("<pre id=sample>abc\ndef</pre>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_TRUE(IsStartOfParagraph(Position(*text, 0)));
  EXPECT_FALSE(IsStartOfParagraph(Position(*text, 1)));
  EXPECT_FALSE(IsStartOfParagraph(Position(*text, 3)));
  EXPECT_TRUE(IsStartOfParagraph(Position(*text, 4)));
  EXPECT_FALSE(IsStartOfParagraph(Position(*text, 5)));
}

TEST_F(PositionUnitsParagraphTest, IsEndOfParagraphWithPosition) {
  SetBodyContent("<pre id=sample>abc\ndef</pre>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_FALSE(IsEndOfParagraph(Position(*text, 0)));
  EXPECT_FALSE(IsEndOfParagraph(Position(*text, 2)));
  EXPECT_TRUE(IsEndOfParagraph(Position(*text, 3)));
  EXPECT_FALSE(IsEndOfParagraph(Position(*text, 4)));
  EXPECT_TRUE(IsEndOfParagraph(Position(*text, 7)));
}

TEST_F(PositionUnitsParagraphTest, InSameParagraphWithPosition) {
  SetBodyContent("<pre id=sample>abc\ndef</pre>");

  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_TRUE(InSameParagraph(Position(*text, 0), Position(*text, 2)));
  EXPECT_TRUE(InSameParagraph(Position(*text, 4), Position(*text, 6)));
  EXPECT_FALSE(InSameParagraph(Position(*text, 0), Position(*text, 5)));
}

TEST_F(PositionUnitsParagraphTest, NullPositionHandling) {
  SetBodyContent("<div id=sample>abc</div>");
  Node* text = GetElementById("sample")->firstChild();

  EXPECT_TRUE(StartOfParagraph(Position()).IsNull());
  EXPECT_TRUE(EndOfParagraph(Position()).IsNull());
  EXPECT_FALSE(IsStartOfParagraph(Position()));
  EXPECT_FALSE(IsEndOfParagraph(Position()));
  EXPECT_FALSE(InSameParagraph(Position(), Position(*text, 0)));
}

TEST_F(PositionUnitsParagraphTest, ParagraphBoundariesFlatTree) {
  SetBodyContent("<pre id=sample>abc\ndef</pre>");
  Node* sample = GetElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(PositionInFlatTree(text, 0),
            StartOfParagraph(PositionInFlatTree(*text, 2)));
  EXPECT_EQ(PositionInFlatTree(text, 4),
            StartOfParagraph(PositionInFlatTree(*text, 5)));
  EXPECT_EQ(PositionInFlatTree(text, 3),
            EndOfParagraph(PositionInFlatTree(*text, 0)));
  EXPECT_EQ(PositionInFlatTree(text, 7),
            EndOfParagraph(PositionInFlatTree(*text, 5)));
}

TEST_F(PositionUnitsParagraphTest, ExpandToParagraphBoundarySimple) {
  SetBodyContent("<div id=sample>hello world</div>");
  Node* text = GetElementById("sample")->firstChild();

  // A range in the middle should expand to the full paragraph.
  EphemeralRange input(Position(text, 2), Position(text, 5));
  EphemeralRange expanded = ExpandToParagraphBoundary(input);
  EXPECT_EQ(Position(text, 0), expanded.StartPosition());
  EXPECT_EQ(Position(text, 11), expanded.EndPosition());
}

TEST_F(PositionUnitsParagraphTest, EndOfParagraphWithPositionHiddenElement) {
  SetBodyContent(
      "<div contenteditable='true'><div id='first'>First block</div>"
      "<div id='second'>Second block<select style='visibility:hidden'>"
      "<b contenteditable='false'>Non-editable</b>"
      "</select></div></div>");

  Node* second = GetElementById("second");
  Node* second_block = second->firstChild();

  Element* select = QuerySelector("select");
  EXPECT_EQ(Position::AfterNode(*select),
            EndOfParagraph(Position(*second_block, 0)));
}

TEST_F(PositionUnitsParagraphTest, ParagraphBoundariesWithHrAndImg) {
  // IsRenderedAsNonInlineTableImageOrHR early return for StartOfParagraph
  // and EndOfParagraph.
  SetBodyContent("<div id=sample><p>abc</p><hr id=hr><p>def</p></div>");
  Element* hr = GetElementById("hr");

  EXPECT_EQ(Position::BeforeNode(*hr), StartOfParagraph(Position(*hr, 0)));
  EXPECT_EQ(Position::AfterNode(*hr), EndOfParagraph(Position(*hr, 0)));

  // Same behavior for <img> (also treated as non-inline replaced).
  SetBodyContent("<div id=sample><p>abc</p><img id=img><p>def</p></div>");
  Element* img = GetElementById("img");

  EXPECT_EQ(Position::BeforeNode(*img), StartOfParagraph(Position(*img, 0)));
  EXPECT_EQ(Position::AfterNode(*img), EndOfParagraph(Position(*img, 0)));
}

TEST_F(PositionUnitsParagraphTest, ParagraphBoundariesWithReplacedAndTable) {
  // EditingIgnoresContent branch (e.g., <img> inline inside a paragraph).
  SetBodyContent("<div id=sample>abc<img id=img>def</div>");
  Node* sample = GetElementById("sample");
  Element* img = GetElementById("img");
  Node* text_abc = sample->firstChild();
  Node* text_def = img->nextSibling();

  // StartOfParagraph from after <img> should find start of text "abc".
  EXPECT_EQ(Position(text_abc, 0),
            StartOfParagraph(Position::AfterNode(*img)));
  // EndOfParagraph from before <img> should find end of text "def".
  EXPECT_EQ(Position(text_def, 3),
            EndOfParagraph(Position::BeforeNode(*img)));

  // IsDisplayInsideTable branch.
  SetBodyContent(
      "<div id=sample>text<table id=tbl><tr><td>cell</td></tr></table>"
      "more</div>");
  Element* tbl = GetElementById("tbl");

  // StartOfParagraph from after a table returns the table's before-anchor
  // position, since the table is treated as a paragraph boundary.
  EXPECT_EQ(Position::BeforeNode(*tbl),
            StartOfParagraph(Position::AfterNode(*tbl)));
}

TEST_F(PositionUnitsParagraphTest, ParagraphBoundariesWithBR) {
  // IsBR() break condition — <br> acts as paragraph separator.
  SetBodyContent("<div id=sample>abc<br id=br>def</div>");
  Node* sample = GetElementById("sample");
  Node* text_abc = sample->firstChild();
  Element* br = GetElementById("br");
  Node* text_def = br->nextSibling();

  // EndOfParagraph from "abc" stops before <br>.
  EXPECT_EQ(Position(text_abc, 3), EndOfParagraph(Position(*text_abc, 0)));
  // StartOfParagraph from "def" stops after <br>.
  EXPECT_EQ(Position(text_def, 0), StartOfParagraph(Position(*text_def, 1)));
}

TEST_F(PositionUnitsParagraphTest, BoundaryCrossingWithMixedEditability) {
  // kCannotCrossEditingBoundary with mixed editable/non-editable content.
  SetBodyContent(
      "<div contenteditable id=sample>aaa"
      "<span contenteditable='false' id=non>bbb</span>"
      "ccc</div>");
  Element* sample = GetElementById("sample");
  Node* text_aaa = sample->firstChild();
  Element* non = GetElementById("non");
  Node* text_ccc = non->nextSibling();

  // From editable "ccc", StartOfParagraph should not cross into
  // non-editable "bbb" when kCannotCrossEditingBoundary.
  Position start = StartOfParagraph(Position(*text_ccc, 1),
                                    kCannotCrossEditingBoundary);
  // The boundary stops at the editable/non-editable transition.
  EXPECT_NE(Position(text_aaa, 0), start);

  // kCanSkipOverEditingBoundary should skip non-editable spans.
  Position start_skip = StartOfParagraph(Position(*text_ccc, 1),
                                         kCanSkipOverEditingBoundary);
  EXPECT_EQ(Position(text_aaa, 0), start_skip);
}

TEST_F(PositionUnitsParagraphTest, ExpandToParagraphBoundaryMultiBlock) {
  // ExpandToParagraphBoundary across two blocks.
  SetBodyContent("<div id=a>hello</div><div id=b>world</div>");
  Node* text_a = GetElementById("a")->firstChild();
  Node* text_b = GetElementById("b")->firstChild();

  // Range spanning across blocks — each endpoint expands to its own paragraph.
  EphemeralRange input(Position(text_a, 2), Position(text_b, 3));
  EphemeralRange expanded = ExpandToParagraphBoundary(input);
  EXPECT_EQ(Position(text_a, 0), expanded.StartPosition());
  EXPECT_EQ(Position(text_b, 5), expanded.EndPosition());
}

TEST_F(PositionUnitsParagraphTest, StartOfParagraphSkipsHiddenElements) {
  // Visibility:hidden elements should be skipped (no layout object path).
  SetBodyContent(
      "<div contenteditable id=sample>aaa"
      "<span style='display:none'>hidden</span>"
      "<span id=vis>bbb</span></div>");
  Element* sample = GetElementById("sample");
  Node* text_aaa = sample->firstChild();
  Node* text_bbb = GetElementById("vis")->firstChild();

  // StartOfParagraph from "bbb" should skip the hidden span and reach "aaa".
  EXPECT_EQ(Position(text_aaa, 0), StartOfParagraph(Position(*text_bbb, 1)));
}

TEST_F(PositionUnitsParagraphTest, EndOfParagraphStartNodeIsBlock) {
  // When start_node == start_block, EndOfParagraph advances to first child
  // before looping (line 207-209).
  SetBodyContent("<div id=block>text inside</div>");
  Element* block = GetElementById("block");
  Node* text = block->firstChild();

  // Position at the block element itself.
  EXPECT_EQ(Position(text, 11),
            EndOfParagraph(Position::FirstPositionInNode(*block)));
}

TEST_F(PositionUnitsParagraphTest, EndOfParagraphSkipEditingBoundary) {
  // kCanSkipOverEditingBoundary in EndOfParagraph should skip non-editable
  // content and find end in next editable region.
  SetBodyContent(
      "<div contenteditable id=sample>aaa"
      "<span contenteditable='false' id=non>bbb</span>"
      "ccc</div>");
  Element* sample = GetElementById("sample");
  Node* text_aaa = sample->firstChild();
  Element* non = GetElementById("non");
  Node* text_ccc = non->nextSibling();

  Position end = EndOfParagraph(Position(*text_aaa, 1),
                                kCanSkipOverEditingBoundary);
  EXPECT_EQ(Position(text_ccc, 3), end);
}

TEST_F(PositionUnitsParagraphTest, ParagraphBoundariesWithEnclosingBlock) {
  // IsEnclosingBlock break condition — nested block elements form paragraph
  // boundaries.
  SetBodyContent(
      "<div id=outer>before<div id=inner>inside</div>after</div>");
  Element* inner = GetElementById("inner");
  Node* text_before = inner->previousSibling();
  Node* text_after = inner->nextSibling();

  // EndOfParagraph from "before" should stop at the block boundary.
  EXPECT_EQ(Position(text_before, 6),
            EndOfParagraph(Position(*text_before, 0)));
  // StartOfParagraph from "after" should stop at the block boundary.
  EXPECT_EQ(Position(text_after, 0),
            StartOfParagraph(Position(*text_after, 3)));
}

}  // namespace blink
