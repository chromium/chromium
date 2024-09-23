// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using AXIntListAttribute = ax::mojom::blink::IntListAttribute;
using AXMarkerType = ax::mojom::blink::MarkerType;

namespace blink {
namespace test {

TEST_F(AccessibilityTest, GetWordBoundaries) {
  // &#9728; is the sun emoji symbol.
  // &#2460; is circled digit one.
  // Full string: "This, ☀ জ is ... a---+++test. <p>word</p>"
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph">
        &quot;This, &#9728; &#2460; is ... a---+++test. &lt;p&gt;word&lt;/p&gt;&quot;
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  VectorOf<int> expected_word_starts{0,  1,  5,  7,  9,  11, 14, 18, 19, 22, 23,
                                     24, 25, 29, 31, 32, 33, 34, 38, 40, 41};
  VectorOf<int> expected_word_ends{1,  5,  6,  8,  10, 13, 17, 19, 22, 23, 24,
                                   25, 29, 30, 32, 33, 34, 38, 40, 41, 43};
  VectorOf<int> word_starts, word_ends;
  ax_inline_text_box->GetWordBoundaries(word_starts, word_ends);
  EXPECT_EQ(expected_word_starts, word_starts);
  EXPECT_EQ(expected_word_ends, word_ends);
}

TEST_F(AccessibilityTest, GetDocumentMarkers) {
  // There should be four inline text boxes in the following paragraph.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph" style="width: 10ch;">
        Misspelled text with a grammar error.
      </p>)HTML");

  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  // Mark the part of the paragraph that says "Misspelled text" with a spelling
  // marker, and the part that says "a grammar error" with a grammar marker.
  //
  // Note that the inline text boxes and the markers do not occupy the same text
  // range. The ranges simply overlap. Also note that the marker ranges include
  // non-collapsed white space found in the DOM.
  DocumentMarkerController& marker_controller = GetDocument().Markers();
  const EphemeralRange misspelled_range(Position(text, 9), Position(text, 24));
  marker_controller.AddSpellingMarker(misspelled_range);
  const EphemeralRange grammar_range(Position(text, 30), Position(text, 45));
  marker_controller.AddGrammarMarker(grammar_range);

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  // kStaticText: "Misspelled text with a grammar error.".
  const AXObject* ax_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  ASSERT_EQ(4, ax_text->ChildCountIncludingIgnored());

  // For each inline text box, angle brackets indicate where the marker starts
  // and ends respectively.

  // kInlineTextBox: "<Misspelled >".
  AXObject* ax_inline_text_box = ax_text->ChildAtIncludingIgnored(0);
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);

    EXPECT_EQ(std::vector<int32_t>{int32_t(AXMarkerType::kSpelling)},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ(std::vector<int32_t>{0},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ(std::vector<int32_t>{11},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }

  // kInlineTextBox: "<text> with <a >".
  ax_inline_text_box = ax_text->ChildAtIncludingIgnored(1);
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);
    EXPECT_EQ((std::vector<int32_t>{int32_t(AXMarkerType::kSpelling),
                                    int32_t(AXMarkerType::kGrammar)}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ((std::vector<int32_t>{0, 10}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ((std::vector<int32_t>{4, 12}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }

  // kInlineTextBox: "<grammar >".
  ax_inline_text_box = ax_text->ChildAtIncludingIgnored(2);
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);
    EXPECT_EQ(std::vector<int32_t>{int32_t(AXMarkerType::kGrammar)},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ(std::vector<int32_t>{0},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ(std::vector<int32_t>{8},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }

  // kInlineTextBox: "<error>.".
  ax_inline_text_box = ax_text->ChildAtIncludingIgnored(3);
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);
    EXPECT_EQ(std::vector<int32_t>{int32_t(AXMarkerType::kGrammar)},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ(std::vector<int32_t>{0},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ(std::vector<int32_t>{5},
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }
}

TEST_F(AccessibilityTest, TextOffsetInContainerWithASpan) {
  // There should be three inline text boxes in the following paragraph. The
  // span should reset the text start offset of all of them to 0.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph">
        Hello <span>world </span>there.
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(2, ax_inline_text_box->TextOffsetInContainer(2));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(3, ax_inline_text_box->TextOffsetInContainer(3));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextInPreOrderIncludingIgnored());
}

TEST_F(AccessibilityTest, TextOffsetInContainerWithMultipleInlineTextBoxes) {
  // There should be four inline text boxes in the following paragraph. The span
  // should not affect the text start offset of the text outside the span.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph" style="width: 5ch;">
        <span>Offset</span>Hello world there.
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(6, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(8, ax_inline_text_box->TextOffsetInContainer(2));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(12, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(15, ax_inline_text_box->TextOffsetInContainer(3));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextInPreOrderIncludingIgnored());
}

TEST_F(AccessibilityTest, TextOffsetInContainerWithLineBreak) {
  // There should be three inline text boxes in the following paragraph. The
  // line break should reset the text start offset to 0 of both the inline text
  // box inside the line break, as well as the text start ofset of the second
  // line.
  SetBodyInnerHTML(R"HTML(
      <style>* { font-size: 10px; }</style>
      <p id="paragraph">
        Line one.<br>
        Line two.
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(1, ax_inline_text_box->TextOffsetInContainer(1));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));

  ax_inline_text_box = ax_inline_text_box->NextInPreOrderIncludingIgnored()
                           ->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());
  EXPECT_EQ(0, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(2, ax_inline_text_box->TextOffsetInContainer(2));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextInPreOrderIncludingIgnored());
}

TEST_F(AccessibilityTest, TextOffsetInContainerWithBreakWord) {
  // There should be three inline text boxes in the following paragraph because
  // of the narrow width and the long word, coupled with the CSS "break-word"
  // property. Each inline text box should have a different offset in container.
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
      <style>* { font: 10px/10px Ahem; }</style>
      <p id="paragraph" style="width: 5ch; word-wrap: break-word;">
        VeryLongWord
      </p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_inline_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  int text_start_offset = 0;
  int text_end_offset = ax_inline_text_box->TextLength();
  EXPECT_EQ(text_start_offset, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(text_end_offset, ax_inline_text_box->TextOffsetInContainer(
                                 ax_inline_text_box->TextLength()));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  text_start_offset = text_end_offset;
  text_end_offset = text_start_offset + ax_inline_text_box->TextLength();
  EXPECT_EQ(text_start_offset, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(text_end_offset, ax_inline_text_box->TextOffsetInContainer(
                                 ax_inline_text_box->TextLength()));

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_inline_text_box->RoleValue());

  text_start_offset = text_end_offset;
  text_end_offset = text_start_offset + ax_inline_text_box->TextLength();
  EXPECT_EQ(text_start_offset, ax_inline_text_box->TextOffsetInContainer(0));
  EXPECT_EQ(text_end_offset, ax_inline_text_box->TextOffsetInContainer(
                                 ax_inline_text_box->TextLength()));

  ASSERT_EQ(nullptr, ax_inline_text_box->NextSiblingIncludingIgnored());
}

TEST_F(AccessibilityTest, GetTextDirection) {
  using WritingDirection = ax::mojom::blink::WritingDirection;
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph" style="writing-mode:sideways-lr;">
        Text.
      </p>)HTML");
  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ax_paragraph->LoadInlineTextBoxes();

  const AXObject* ax_text_box =
      ax_paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox, ax_text_box->RoleValue());
  // AXInlineTextBox::GetTextDirection() is used.
  EXPECT_EQ(WritingDirection::kBtt, ax_text_box->GetTextDirection());

  const AXObject* ax_static_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());
  // AXNodeObject::GetTextDirection() is used.
  EXPECT_EQ(WritingDirection::kBtt, ax_static_text->GetTextDirection());
}

}  // namespace test
}  // namespace blink
