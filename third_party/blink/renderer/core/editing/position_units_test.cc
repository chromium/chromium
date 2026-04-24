// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_units.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PositionUnitsTest : public EditingTestBase {};

// ----- Document tests -----

TEST_F(PositionUnitsTest, StartOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text =
      GetElementById("sample")->firstChild();
  Position result = StartOfDocument(Position(*text, 3));
  EXPECT_EQ(Position::FirstPositionInNode(*GetDocument().documentElement()),
            result);
}

TEST_F(PositionUnitsTest, EndOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text =
      GetElementById("sample")->firstChild();
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

TEST_F(PositionUnitsTest, IsStartOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text =
      GetElementById("sample")->firstChild();
  Position doc_start = StartOfDocument(Position(*text, 0));
  EXPECT_TRUE(IsStartOfDocument(doc_start));
  EXPECT_FALSE(IsStartOfDocument(Position(*text, 3)));
}

TEST_F(PositionUnitsTest, IsEndOfDocumentBasic) {
  SetBodyContent("<div id=sample>hello</div>");
  Node* text =
      GetElementById("sample")->firstChild();
  Position doc_end = EndOfDocument(Position(*text, 0));
  EXPECT_TRUE(IsEndOfDocument(doc_end));
  EXPECT_FALSE(IsEndOfDocument(Position(*text, 0)));
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

// ----- Editable content tests -----

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
  EXPECT_FALSE(
      IsEndOfEditableOrNonEditableContent(PositionInFlatTree()));
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
