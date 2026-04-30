/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/position_units.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"

namespace blink {

class PositionUnitsTest : public EditingTestBase {};

// ---------- NextPositionOf ----------

TEST_F(PositionUnitsTest, NextPositionOfBasic) {
  SetBodyContent("<p id='p'>abc</p>");
  Node* text = GetElementById("p")->firstChild();

  // From offset 0 → should advance to offset 1.
  Position result = NextPositionOf(Position(text, 0));
  EXPECT_EQ(Position(text, 1), result);

  // From offset 1 → should advance to offset 2.
  result = NextPositionOf(Position(text, 1));
  EXPECT_EQ(Position(text, 2), result);
}

TEST_F(PositionUnitsTest, NextPositionOfAtEnd) {
  SetBodyContent("<p id='p'>x</p>");
  Node* text = GetElementById("p")->firstChild();

  // At the end of the only text in the document body, there is no next
  // visually distinct candidate, so NextPositionOf returns null.
  Position result = NextPositionOf(Position(text, 1));
  EXPECT_TRUE(result.IsNull());
}

TEST_F(PositionUnitsTest, NextPositionOfFlatTree) {
  SetBodyContent("<p id='p'>abc</p>");
  Node* text = GetElementById("p")->firstChild();

  PositionInFlatTree result = NextPositionOf(PositionInFlatTree(text, 0));
  EXPECT_EQ(PositionInFlatTree(text, 1), result);
}

TEST_F(PositionUnitsTest, NextPositionOfCannotCrossEditingBoundary) {
  SetBodyContent(
      "<div contenteditable id='e'>abc</div>"
      "<div id='outside'>def</div>");
  Node* text = GetElementById("e")->firstChild();

  // At the end of the editable region, navigating forward with
  // kCannotCrossEditingBoundary must return null — the boundary prevents
  // leaving the editable root.
  Position pos(text, 3);
  Position result = NextPositionOf(pos, kCannotCrossEditingBoundary);
  EXPECT_TRUE(result.IsNull());
}

TEST_F(PositionUnitsTest, NextPositionOfCanSkipOverEditingBoundary) {
  SetBodyContent(
      "<div contenteditable id='e'>abc<span contenteditable='false' "
      "id='ne'>NE</span>xyz</div>");
  Element* editable = GetElementById("e");
  Node* abc_text = editable->firstChild();  // "abc"
  Node* xyz_text = editable->lastChild();   // "xyz"

  // From the end of "abc", kCanSkipOverEditingBoundary should skip over the
  // non-editable <span> and land at the start of "xyz".
  Position pos(abc_text, 3);
  Position result = NextPositionOf(pos, kCanSkipOverEditingBoundary);
  ASSERT_TRUE(result.IsNotNull());
  EXPECT_EQ(Position(xyz_text, 0), result);
}

TEST_F(PositionUnitsTest, NextPositionOfConsistentWithVisiblePosition) {
  SetBodyContent("<p id='p'>abc</p>");
  Node* text = GetElementById("p")->firstChild();

  Position pos(text, 1);
  // The Position-based overload should produce a position that, when
  // canonicalized, matches what the VisiblePosition overload returns.
  Position pos_result = NextPositionOf(pos);
  VisiblePosition vp_result = NextPositionOf(CreateVisiblePosition(pos));

  EXPECT_EQ(CreateVisiblePosition(pos_result).DeepEquivalent(),
            vp_result.DeepEquivalent());
}

// ---------- PreviousPositionOf ----------

TEST_F(PositionUnitsTest, PreviousPositionOfBasic) {
  SetBodyContent("<p id='p'>abc</p>");
  Node* text = GetElementById("p")->firstChild();

  // From offset 3 → should move back to offset 2.
  Position result = PreviousPositionOf(Position(text, 3));
  EXPECT_EQ(Position(text, 2), result);

  // From offset 1 → should move back to offset 0.
  result = PreviousPositionOf(Position(text, 1));
  EXPECT_EQ(Position(text, 0), result);
}

TEST_F(PositionUnitsTest, PreviousPositionOfAtStart) {
  SetBodyContent("<p id='p'>x</p>");
  Node* text = GetElementById("p")->firstChild();

  // At the very beginning of the document body content, PreviousPositionOf
  // should return null — there is no earlier visually distinct candidate.
  Position result = PreviousPositionOf(Position(text, 0));
  EXPECT_TRUE(result.IsNull());
}

TEST_F(PositionUnitsTest, PreviousPositionOfReturnsNullWhenStuck) {
  // Use the very first position in the document's <html> element.  At this
  // point PreviousVisuallyDistinctCandidate may return a position equal to
  // the input, triggering the prev == position early-return.
  SetBodyContent("<p id='p'>abc</p>");
  Element* html = GetDocument().documentElement();
  Position pos = Position::FirstPositionInNode(*html);

  Position result = PreviousPositionOf(pos);
  EXPECT_TRUE(result.IsNull());
}

TEST_F(PositionUnitsTest, PreviousPositionOfFlatTree) {
  SetBodyContent("<p id='p'>abc</p>");
  Node* text = GetElementById("p")->firstChild();

  PositionInFlatTree result = PreviousPositionOf(PositionInFlatTree(text, 3));
  EXPECT_EQ(PositionInFlatTree(text, 2), result);
}

TEST_F(PositionUnitsTest, PreviousPositionOfCannotCrossEditingBoundary) {
  SetBodyContent(
      "<div id='outside'>abc</div>"
      "<div contenteditable id='e'>def</div>");
  Node* text = GetElementById("e")->firstChild();

  // At the start of the editable region, going backward with
  // kCannotCrossEditingBoundary must return null — the boundary prevents
  // leaving the editable root.
  Position pos(text, 0);
  Position result = PreviousPositionOf(pos, kCannotCrossEditingBoundary);
  EXPECT_TRUE(result.IsNull());
}

TEST_F(PositionUnitsTest, PreviousPositionOfCanSkipOverEditingBoundary) {
  SetBodyContent(
      "<div contenteditable id='e'>abc<span contenteditable='false' "
      "id='ne'>NE</span>xyz</div>");
  Element* editable = GetElementById("e");
  Node* abc_text = editable->firstChild();  // "abc"
  Node* xyz_text = editable->lastChild();   // "xyz"

  // From the start of "xyz", kCanSkipOverEditingBoundary should skip over
  // the non-editable <span> and land at the end of "abc".
  Position pos(xyz_text, 0);
  Position result = PreviousPositionOf(pos, kCanSkipOverEditingBoundary);
  ASSERT_TRUE(result.IsNotNull());
  EXPECT_EQ(Position(abc_text, 3), result);
}

TEST_F(PositionUnitsTest, PreviousPositionOfConsistentWithVisiblePosition) {
  SetBodyContent("<p id='p'>abc</p>");
  Node* text = GetElementById("p")->firstChild();

  Position pos(text, 2);
  Position pos_result = PreviousPositionOf(pos);
  VisiblePosition vp_result = PreviousPositionOf(CreateVisiblePosition(pos));

  EXPECT_EQ(CreateVisiblePosition(pos_result).DeepEquivalent(),
            vp_result.DeepEquivalent());
}

// ---------- CharacterAfter ----------

TEST_F(PositionUnitsTest, CharacterAfterBasic) {
  SetBodyContent("<p id='p'>Hello</p>");
  Node* text = GetElementById("p")->firstChild();

  EXPECT_EQ('H', CharacterAfter(Position(text, 0)));
  EXPECT_EQ('e', CharacterAfter(Position(text, 1)));
  EXPECT_EQ('o', CharacterAfter(Position(text, 4)));
}

TEST_F(PositionUnitsTest, CharacterAfterAtEnd) {
  SetBodyContent("<p id='p'>ab</p>");
  Node* text = GetElementById("p")->firstChild();

  // Past the last character → should return 0.
  EXPECT_EQ(0u, CharacterAfter(Position(text, 2)));
}

TEST_F(PositionUnitsTest, CharacterAfterNonText) {
  SetBodyContent("<div id='d'><br></div>");
  Element* div = GetElementById("d");

  // Position before <br> is not an offset-in-anchor into a text node.
  EXPECT_EQ(0u, CharacterAfter(Position(div, 0)));

  // An empty contenteditable element also yields 0.
  SetBodyContent("<div contenteditable id='e'></div>");
  Element* editable = GetElementById("e");
  EXPECT_EQ(0u, CharacterAfter(Position(editable, 0)));
}

TEST_F(PositionUnitsTest, CharacterAfterConsistentWithVisiblePosition) {
  SetBodyContent("<p id='p'>Hello</p>");
  Node* text = GetElementById("p")->firstChild();

  Position pos(text, 2);
  // Position-based CharacterAfter should agree with the VisiblePosition-based
  // one for simple text.
  UChar32 pos_char = CharacterAfter(pos);
  UChar32 vp_char = CharacterAfter(CreateVisiblePosition(pos));
  EXPECT_EQ(pos_char, vp_char);
}

// ---------- StartOfDocument / EndOfDocument ----------

TEST_F(PositionUnitsTest, StartOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  Position result = StartOfDocument(Position(*text, 3));
  EXPECT_EQ(Position::FirstPositionInNode(*GetDocument().documentElement()),
            result);
}

TEST_F(PositionUnitsTest, StartOfDocumentWithShadowDOM) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<p><slot name='#two'></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* one = GetElementById("one")->firstChild();
  Node* two = GetElementById("two")->firstChild();

  Element* doc_element = GetDocument().documentElement();
  EXPECT_EQ(Position::FirstPositionInNode(*doc_element),
            StartOfDocument(Position(*one, 0)));
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*doc_element),
            StartOfDocument(PositionInFlatTree(*one, 0)));

  EXPECT_EQ(Position::FirstPositionInNode(*doc_element),
            StartOfDocument(Position(*two, 1)));
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*doc_element),
            StartOfDocument(PositionInFlatTree(*two, 1)));
}

TEST_F(PositionUnitsTest, IsStartOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  Position doc_start = StartOfDocument(Position(*text, 0));
  EXPECT_TRUE(IsStartOfDocument(doc_start));
  EXPECT_FALSE(IsStartOfDocument(Position(*text, 3)));
}

TEST_F(PositionUnitsTest, EndOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  Position result = EndOfDocument(Position(*text, 0));
  EXPECT_EQ(Position::LastPositionInNode(*GetDocument().documentElement()),
            result);
}

TEST_F(PositionUnitsTest, EndOfDocumentWithShadowDOM) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<p><slot name='#two'></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetElementById("one");
  Element* two = GetElementById("two");

  Element* doc_element = GetDocument().documentElement();
  EXPECT_EQ(Position::LastPositionInNode(*doc_element),
            EndOfDocument(Position(*one->firstChild(), 0)));
  EXPECT_EQ(Position::LastPositionInNode(*doc_element),
            EndOfDocument(Position(*two->firstChild(), 1)));
}

TEST_F(PositionUnitsTest, IsEndOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  Position doc_end = EndOfDocument(Position(*text, 0));
  EXPECT_TRUE(IsEndOfDocument(doc_end));
  EXPECT_FALSE(IsEndOfDocument(Position(*text, 0)));
}

// ---------- Editable content ----------

TEST_F(PositionUnitsTest, StartOfEditableContentBasic) {
  SetBodyContent("<div contenteditable id=editor>hello <b>world</b></div>");
  Element* editor = GetElementById("editor");
  Node* text = editor->firstChild();
  PositionInFlatTree result =
      StartOfEditableContent(PositionInFlatTree(*text, 3));
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*editor), result);
}

TEST_F(PositionUnitsTest, StartOfEditableContentNonEditable) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  EXPECT_EQ(PositionInFlatTree(),
            StartOfEditableContent(PositionInFlatTree(*text, 3)));
  EXPECT_EQ(Position(), StartOfEditableContent(Position(*text, 3)));
}

TEST_F(PositionUnitsTest, EndOfEditableContentBasic) {
  SetBodyContent("<div contenteditable id=editor>hello <b>world</b></div>");
  Element* editor = GetElementById("editor");
  Node* text = editor->firstChild();
  PositionInFlatTree result =
      EndOfEditableContent(PositionInFlatTree(*text, 3));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*editor), result);
}

TEST_F(PositionUnitsTest, EndOfEditableContentNonEditable) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  EXPECT_EQ(PositionInFlatTree(),
            EndOfEditableContent(PositionInFlatTree(*text, 3)));
  EXPECT_EQ(Position(), EndOfEditableContent(Position(*text, 3)));
}

TEST_F(PositionUnitsTest, IsEndOfEditableOrNonEditableContentWithPosition) {
  SetBodyContent("<div contenteditable id=editor>hello</div>");
  Element* editor = GetElementById("editor");
  Node* text = editor->firstChild();
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      Position::LastPositionInNode(*editor)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(Position(*text, 0)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(Position()));
}

TEST_F(PositionUnitsTest,
       IsEndOfEditableOrNonEditableContentWithFlatTreePosition) {
  SetBodyContent("<div contenteditable id=editor>hello</div>");
  Element* editor = GetElementById("editor");
  Node* text = editor->firstChild();
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      PositionInFlatTree::LastPositionInNode(*editor)));
  EXPECT_FALSE(
      IsEndOfEditableOrNonEditableContent(PositionInFlatTree(*text, 0)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(PositionInFlatTree()));
}

TEST_F(PositionUnitsTest,
       IsEndOfEditableOrNonEditableContentNonEditable) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  // In non-editable content, falls back to IsEndOfDocument.
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      Position::LastPositionInNode(*GetDocument().documentElement())));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(Position(*text, 0)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(Position(*text, 3)));
}

TEST_F(PositionUnitsTest,
       IsEndOfEditableOrNonEditableContentNonEditableWithFlatTreePosition) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text = GetElementById("sample")->firstChild();
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      PositionInFlatTree::LastPositionInNode(*GetDocument().documentElement())));
  EXPECT_FALSE(
      IsEndOfEditableOrNonEditableContent(PositionInFlatTree(*text, 0)));
  EXPECT_FALSE(
      IsEndOfEditableOrNonEditableContent(PositionInFlatTree(*text, 3)));
}

}  // namespace blink
