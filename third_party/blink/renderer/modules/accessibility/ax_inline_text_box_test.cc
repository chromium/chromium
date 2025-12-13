// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/modules/accessibility/ax_block_flow_iterator.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/accessibility_features.h"

using AXIntListAttribute = ax::mojom::blink::IntListAttribute;
using AXMarkerType = ax::mojom::blink::MarkerType;
using AXHighlightType = ax::mojom::blink::HighlightType;

namespace blink {
namespace test {

class AXInlineTextBoxTest : public AccessibilityTest,
                            public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    AccessibilityTest::SetUp();
    bool feature_on = GetParam();
    if (feature_on) {
      list.InitWithFeatures({::features::kAccessibilityBlockFlowIterator}, {});
    } else {
      list.InitWithFeatures({}, {::features::kAccessibilityBlockFlowIterator});
    }
  }

 protected:
  base::test::ScopedFeatureList list;
};

INSTANTIATE_TEST_SUITE_P(BoolSequence, AXInlineTextBoxTest, testing::Bool());

TEST_P(AXInlineTextBoxTest, GetWordBoundaries) {
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

  VectorOf<int> expected_word_starts{0,  1,  5,  7,  9,  11, 14, 18, 19,
                                     25, 29, 31, 32, 33, 34, 38, 40, 41};
  VectorOf<int> expected_word_ends{1,  5,  6,  8,  10, 13, 17, 19, 25,
                                   29, 30, 32, 33, 34, 38, 40, 41, 43};
  VectorOf<int> word_starts, word_ends;
  ax_inline_text_box->GetWordBoundaries(word_starts, word_ends);
  EXPECT_EQ(expected_word_starts, word_starts);
  EXPECT_EQ(expected_word_ends, word_ends);
}

TEST_P(AXInlineTextBoxTest, GetDocumentMarkers) {
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

TEST_P(AXInlineTextBoxTest, AriaInvalidAndHighlightMarkers) {
  // There should be four inline text boxes in the following paragraph.
  SetBodyInnerHTML(
      R"HTML(<p id="paragraph"><span aria-invalid="spelling" id="span1">Misspelled</span> text with a <span aria-invalid="grammar" id="span2">grammar error</span>.</p>)HTML");

  Node* paragraph = GetElementById("paragraph");
  ASSERT_NE(nullptr, paragraph);
  Node* text = GetElementById("span1")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  auto* dom_window = GetDocument().domWindow();
  HighlightRegistry* registry = HighlightRegistry::From(*dom_window);
  HeapVector<Member<AbstractRange>> range_vector;
  auto* range1 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 10);
  range_vector.push_back(range1);

  text = GetElementById("span2")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  auto* range2 = MakeGarbageCollected<Range>(GetDocument(), text, 0, text, 13);
  range_vector.push_back(range2);

  Highlight* highlight = Highlight::Create(range_vector);
  AtomicString name("highlight-name");
  registry->SetForTesting(name, highlight);
  registry->ScheduleRepaint();
  registry->ValidateHighlightMarkers();

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  ax_paragraph->LoadInlineTextBoxes();

  AXObject* ax_container = ax_paragraph->ChildAtIncludingIgnored(0);
  AXObject* ax_text = ax_container->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  ASSERT_EQ(1, ax_text->ChildCountIncludingIgnored());

  // For each inline text box, angle brackets indicate where the marker starts
  // and ends respectively.
  // kInlineTextBox: "<Misspelled>".
  AXObject* ax_inline_text_box = ax_text->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);
    EXPECT_EQ(std::vector<int32_t>({int32_t(AXMarkerType::kSpelling),
                                    int32_t(AXMarkerType::kHighlight)}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ(
        std::vector<int32_t>({int32_t(AXHighlightType::kNone),
                              int32_t(AXHighlightType::kHighlight)}),
        node_data.GetIntListAttribute(AXIntListAttribute::kHighlightTypes));
    EXPECT_EQ(std::vector<int32_t>({0, 0}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ(std::vector<int32_t>({10, 10}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }

  // kInlineTextBox: " text with a ".
  ax_text = ax_paragraph->ChildAtIncludingIgnored(1);
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  ASSERT_EQ(1, ax_text->ChildCountIncludingIgnored());
  ax_inline_text_box = ax_text->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);
    EXPECT_EQ(std::vector<int32_t>(),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ(std::vector<int32_t>(), node_data.GetIntListAttribute(
                                          AXIntListAttribute::kHighlightTypes));
    EXPECT_EQ(std::vector<int32_t>(),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ(std::vector<int32_t>(),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }

  // kInlineTextBox: "<grammar error>".
  ax_container = ax_paragraph->ChildAtIncludingIgnored(2);
  ax_text = ax_container->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  ASSERT_EQ(1, ax_text->ChildCountIncludingIgnored());
  ax_inline_text_box = ax_text->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  {
    ScopedFreezeAXCache freeze(ax_inline_text_box->AXObjectCache());
    ui::AXNodeData node_data;
    ax_inline_text_box->Serialize(&node_data, ui::kAXModeComplete);
    EXPECT_EQ(std::vector<int32_t>({int32_t(AXMarkerType::kGrammar),
                                    int32_t(AXMarkerType::kHighlight)}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerTypes));
    EXPECT_EQ(
        std::vector<int32_t>({int32_t(AXHighlightType::kNone),
                              int32_t(AXHighlightType::kHighlight)}),
        node_data.GetIntListAttribute(AXIntListAttribute::kHighlightTypes));
    EXPECT_EQ(std::vector<int32_t>({0, 0}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerStarts));
    EXPECT_EQ(std::vector<int32_t>({13, 13}),
              node_data.GetIntListAttribute(AXIntListAttribute::kMarkerEnds));
  }
}

TEST_P(AXInlineTextBoxTest, TextOffsetInContainerWithASpan) {
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

TEST_P(AXInlineTextBoxTest, TextOffsetInContainerWithMultipleInlineTextBoxes) {
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

TEST_P(AXInlineTextBoxTest, TextOffsetInContainerWithLineBreak) {
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

TEST_P(AXInlineTextBoxTest, TextOffsetInContainerWithBreakWord) {
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

TEST_P(AXInlineTextBoxTest, GetTextDirection) {
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

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_Simple) {
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph">Hello <em>World</em></p>)HTML");

  // id#=3249 rootWebArea
  // ++id#=3250 genericContainer ignored
  // ++++id#=3251 genericContainer ignored
  // ++++++id#=3252 paragraph
  // ++++++++id#=3247 staticText name='Hello '
  //     nextOnLineId=inlineTextBox:"World"
  // ++++++++++id#=-1000002414 inlineTextBox name='Hello '
  //     nextOnLineId=inlineTextBox:"World"
  // ++++++++id#=3257 emphasis
  // ++++++++++id#=3248 staticText name='World'
  // ++++++++++++id#=-1000002415 inlineTextBox name='World'

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  const AXObject* ax_static_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("Hello ", it.GetText());
  std::optional<AXBlockFlowIterator::MapKey> previous_on_line =
      it.PreviousOnLine();
  ASSERT_FALSE(previous_on_line.has_value());
  std::optional<AXBlockFlowIterator::MapKey> next_on_line = it.NextOnLine();
  ASSERT_TRUE(next_on_line.has_value());
  ASSERT_EQ("World", it.GetTextForTesting(next_on_line.value()));

  // 'World' is not part of the same LayoutText object.
  ASSERT_FALSE(it.Next());
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_SoftLinebreak) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      p, textarea {
        width: 16ch;
        font: 16px/16px Ahem;
      }
      textarea {
        height: 100px;
      }
    </style>
    <p id="paragraph">
      Antidisestablishmentarianism is a really long English word!
    </p>
    <textarea id="textarea">Antidisestablishmentarianism is really long!
    </textarea>)HTML");

  // id#=1673 rootWebArea
  // ++id#=1674 genericContainer ignored
  // ++++id#=1675 genericContainer ignored
  // ++++++id#=1676 paragraph
  // ++++++++id#=1670 staticText
  //    name='Antidisestablishmentarianism is a really long English word!'
  // ++++++++++id#=-1000001399 inlineTextBox
  //    name='Antidisestablishmentarianism '
  // ++++++++++id#=-1000001400 inlineTextBox name='is a really long '
  // ++++++++++id#=-1000001401 inlineTextBox name='English word!'
  // ++++++id#=812 textField
  // ++++++++id#=816 genericContainer
  // ++++++++++id#=809 staticText
  //     name='Antidisestablishmentarianism is really long!<newline>'
  // ++++++++++++id#=-1000000613 inlineTextBox name='Antidisestablish'
  // ++++++++++++id#=-1000000614 inlineTextBox
  //     name='mentarianism is' nextOnLineId=inlineTextBox:" "
  // ++++++++++++id#=-1000000615 inlineTextBox name=' '
  // ++++++++++++id#=-1000000616 inlineTextBox
  //     name='really long!' ...
  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  const AXObject* ax_static_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);

  ASSERT_TRUE(it.Next());
  // Trailing whitespace expected since a soft linebreak.
  ASSERT_EQ("Antidisestablishmentarianism ", it.GetText());
  std::optional<AXBlockFlowIterator::MapKey> previous_on_line =
      it.PreviousOnLine();
  ASSERT_FALSE(previous_on_line.has_value());
  // Wraps to next line so no next on line fragment.
  std::optional<AXBlockFlowIterator::MapKey> next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("is a really long ", it.GetText());
  previous_on_line = it.PreviousOnLine();
  ASSERT_FALSE(previous_on_line.has_value());
  next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("English word!", it.GetText());
  previous_on_line = it.PreviousOnLine();
  ASSERT_FALSE(previous_on_line.has_value());
  next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());

  ASSERT_FALSE(it.Next());

  AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());
  const AXObject* ax_textarea_container =
      ax_textarea->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_textarea_container);
  const AXObject* ax_text_box_static_text =
      ax_textarea_container->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_box_static_text);
  it = AXBlockFlowIterator(ax_text_box_static_text);

  ASSERT_TRUE(it.Next());
  // No trailing whitespace since breaking mid word.
  ASSERT_EQ("Antidisestablish", it.GetText());
  previous_on_line = it.PreviousOnLine();
  ASSERT_FALSE(previous_on_line.has_value());
  next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("mentarianism is", it.GetText());
  previous_on_line = it.PreviousOnLine();
  ASSERT_FALSE(previous_on_line.has_value());
  next_on_line = it.NextOnLine();
  ASSERT_TRUE(next_on_line.has_value());

  // trailing whitespace on the second line is in its own fragment.
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(" ", it.GetText());
  previous_on_line = it.PreviousOnLine();
  ASSERT_TRUE(previous_on_line.has_value());
  ASSERT_EQ("mentarianism is", it.GetTextForTesting(previous_on_line.value()));
  next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("really long!", it.GetText());

  // Trailing whitespace at the end due to test formatting.
  // Skipping further assertion checking since not really pertinent to
  // testing soft-line-breaking.
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_HardtLinebreak) {
  SetBodyInnerHTML(R"HTML(
    <p id="paragraph">Hello <br>World!</p>)HTML");

  // id#=1604 rootWebArea name='Forced linebreak'
  // ++id#=1605 genericContainer ignored
  // ++++id#=1606 genericContainer ignored
  // ++++++id#=1607 paragraph
  // ++++++++id#=1601 staticText name='Hello'
  //                  nextOnLineId=inlineTextBox:"<newline>"
  // ++++++++++id#=-1000001191 inlineTextBox name='Hello'
  //                           nextOnLineId=inlineTextBox:"<newline>"
  // ++++++++id#=1602 lineBreak name='<newline>'
  // ++++++++++id#=-1000001187 inlineTextBox name='<newline>'
  // ++++++++id#=1603 staticText name='World!'
  // ++++++++++id#=-1000001192 inlineTextBox name='World!'

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  const AXObject* ax_static_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);
  ASSERT_TRUE(it.Next());
  // Trailing whitespace suppressed even though there is whitespace before the
  // linebreak since using a hard linebreak.
  ASSERT_EQ("Hello", it.GetText());
  // Hard linebreak token is on the same line.
  std::optional<AXBlockFlowIterator::MapKey> next_on_line = it.NextOnLine();
  ASSERT_TRUE(next_on_line.has_value());
  ASSERT_EQ("\n", it.GetTextForTesting(next_on_line.value()));

  // Linebreak token is not part of the text node. Subsequent text is in a
  // separate node.
  ASSERT_FALSE(it.Next());
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_Ellipsis) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
        width: 400px;
      }

      div, span {
        outline: 1px solid;
        font-size: 40px;
        font-family: Ahem;
      }
    </style>
    <div><span id="span">SPAN NESTED INSIDE DIV</span> CONTAINER DIV</div>
 )HTML");

  // id#=1621 rootWebArea
  // ++id#=1622 genericContainer ignored
  // ++++id#=1623 genericContainer ignored
  // ++++++id#=1624 genericContainer
  // ++++++++id#=1619 staticText name='SPAN NESTED INSIDE DIV'
  // ++++++++++id#=-1000001207 inlineTextBox name='SPAN NESTED INSIDE DIV'
  //    nextOnLineId=inlineTextBox:"SPAN NESTED INS"
  // ++++++++++id#=-1000001208 inlineTextBox name='SPAN NEST'
  //    nextOnLineId=inlineTextBox:" CONTAINER DIV"
  // ++++++++++id#=-1000001209 inlineTextBox name='…'
  // ++++++++id#=1620 staticText name=' CONTAINER DIV'
  // ++++++++++id#=-1000001210 inlineTextBox name=' CONTAINER DIV'

  AXObject* ax_span = GetAXObjectByElementId("span");
  ASSERT_NE(nullptr, ax_span);
  const AXObject* ax_static_text = ax_span->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);
  ASSERT_TRUE(it.Next());
  ASSERT_EQ("SPAN NESTED INSIDE DIV", it.GetText());

  // Though the following behavior is correct insofar as it agrees with
  // our AbstractInlineTextBox based algorithm, it breaks detection of
  // bounding boxes for text selections, since that code assumes that the
  // sum of the lengths of the inline text boxes aligns with the length
  // of the parent static text object.
  // TODO (accessibility): Revisit how inline text boxes are computed for
  // ellipsis.
  std::optional<AXBlockFlowIterator::MapKey> next_on_line = it.NextOnLine();
  ASSERT_TRUE(next_on_line.has_value());
  ASSERT_EQ("SPAN NEST", it.GetTextForTesting(next_on_line.value()));

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("SPAN NEST", it.GetText());

  // Next on line skips over the ellipsis.
  next_on_line = it.NextOnLine();
  ASSERT_TRUE(next_on_line.has_value());
  ASSERT_EQ(" CONTAINER DIV", it.GetTextForTesting(next_on_line.value()));

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("\u2026", it.GetText().Utf8());  // Horizontal ellipsis.

  // " CONTAINER DIV" is outside of the span.
  ASSERT_FALSE(it.Next());
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_Ruby) {
  SetBodyInnerHTML(R"HTML(
    <ruby id="ruby">Ruby base<rt>ruby text</rt></ruby>)HTML");

  // id#=1 rootWebArea
  // ++id#=4 genericContainer ignored
  // ++++id#=5 genericContainer ignored
  // ++++++id#=6 paragraph
  // ++++++++id#=8 ruby description="ruby text"
  // ++++++++++id#=2 staticText name='ruby base' nextOnLineId=staticText
  // ++++++++++++id#=-1000000004 inlineTextBox name='ruby base'
  //                             nextOnLineId=staticText
  // ++++++++++id#=9 rubyAnnotation ignored
  // ++++++++++++id#=3 staticText ignored

  AXObject* ax_ruby = GetAXObjectByElementId("ruby");
  ASSERT_NE(nullptr, ax_ruby);
  ASSERT_EQ(ax::mojom::Role::kRuby, ax_ruby->RoleValue());
  const AXObject* ax_static_text = ax_ruby->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);
  ASSERT_TRUE(it.Next());
  ASSERT_EQ("Ruby base", it.GetText());

  std::optional<AXBlockFlowIterator::MapKey> next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());

  ASSERT_FALSE(it.Next());
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_Ruby2) {
  SetBodyInnerHTML(R"HTML(
    <p style="font-family:monospace; width:5ch;">
      <ruby id="ruby">ruby base<rt>ruby text</rt></ruby>
    </p>)HTML");

  // id#=798 rootWebArea
  // ++id#=799 genericContainer ignored
  // ++++id#=800 genericContainer ignored
  // ++++++id#=801 paragraph
  // ++++++++id#=803 ruby description='ruby text'
  // ++++++++++id#=796 staticText name='ruby base' nextOnLineId=staticText
  // ++++++++++++id#=-1000000986 inlineTextBox name='ruby '
  // ++++++++++++id#=-1000000987 inlineTextBox name='base'
  //                             nextOnLineId=staticText
  // ++++++++++id#=804 rubyAnnotation ignored
  // ++++++++++++id#=797 staticText ignored

  AXObject* ax_ruby = GetAXObjectByElementId("ruby");
  ASSERT_NE(nullptr, ax_ruby);
  ASSERT_EQ(ax::mojom::Role::kRuby, ax_ruby->RoleValue());
  const AXObject* ax_static_text = ax_ruby->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);
  ASSERT_TRUE(it.Next());
  ASSERT_EQ("ruby ", it.GetText());

  std::optional<AXBlockFlowIterator::MapKey> next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());
  ASSERT_TRUE(it.Next());
  ASSERT_EQ("base", it.GetText());
  next_on_line = it.NextOnLine();
  ASSERT_FALSE(next_on_line.has_value());
  ASSERT_FALSE(it.Next());
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_CharacterOffsets) {
  // Then Ahem font has consistent font metrics across platforms.
  LoadAhem();

  SetBodyInnerHTML(R"HTML(
    <style>
      p, textarea {
        width: 5ch;
        font: 16px/16px Ahem;
      }
      textarea {
        height: 100px;
      }
    </style>
    <p id="paragraph">Hello world!</p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  const AXObject* ax_static_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);
  ASSERT_TRUE(it.Next());
  ASSERT_EQ("Hello ", it.GetText());

  // The trailing whitespace in "Hello " is not part of the actual text fragment
  // since not rendered. When extracting the glyphs, the length of the vector
  // padded to include the trailing space a zero-width glyph.
  Vector<int> expected_character_offsets = {16, 32, 48, 64, 80, 80};
  Vector<int> result;
  it.GetCharacterLayoutPixelOffsets(result);
  ASSERT_EQ(expected_character_offsets, result);

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("world!", it.GetText());
  expected_character_offsets = {16, 32, 48, 64, 80, 96};
  it.GetCharacterLayoutPixelOffsets(result);
  ASSERT_EQ(expected_character_offsets, result);

  ASSERT_FALSE(it.Next());
}

TEST_P(AXInlineTextBoxTest, AXBlockFlowIteratorAPI_CharacterWidths_Ligature) {
  // Google Sans supports ligatures (e.g. "fi" being rendered as a single glyph.
  LoadFontFromFile(GetFrame(), test::CoreTestDataPath("GoogleSans-Regular.ttf"),
                   AtomicString("Google Sans"));

  SetBodyInnerHTML(R"HTML(
    <style>
      p {
       font: 16px Google Sans;
      }
      span {
        color: red;
      }
    </style>
    <p id="paragraph">f<span id="span">ire</span></p>)HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());
  const AXObject* ax_static_text = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  AXBlockFlowIterator it(ax_static_text);
  ASSERT_TRUE(it.Next());
  ASSERT_EQ("f", it.GetText());
  Vector<int> result;
  it.GetCharacterLayoutPixelOffsets(result);
  ASSERT_EQ(1u, result.size());

  ASSERT_TRUE(it.NextOnLine().has_value());
  ASSERT_FALSE(it.Next());

  const AXObject* ax_span = GetAXObjectByElementId("span");
  ax_static_text = ax_span->FirstChildIncludingIgnored();
  it = AXBlockFlowIterator(ax_static_text);

  ASSERT_TRUE(it.Next());
  ASSERT_EQ("ire", it.GetText());

  it.GetCharacterLayoutPixelOffsets(result);
  ASSERT_EQ(3u, result.size());
  // "i"  was rendered as part of the "fi" ligature and is a reported as a
  // zero width glyph, to preserve character alignment.
  ASSERT_EQ(0, result[0]);

  ASSERT_FALSE(it.Next());
}

}  // namespace test

TEST_F(AccessibilityTest, LoadInlineTextBoxesCrashsOnAndroid) {
  SetBodyInnerHTML(R"HTML(
    <p id="paragraph"></p>
      )HTML");

  AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);

  // In lieu of a repro snippet, we force this paragraph, which has a
  // LayoutBlock, to be a static text role.
  ax_paragraph->role_ = ax::mojom::Role::kStaticText;

  // Then, force a life cycle change.
  ax_paragraph->AXObjectCache().CommitAXUpdates(*(ax_paragraph->GetDocument()),
                                                true);

  // Finally, this enables us to request a load of inline text boxes and trigger
  // the CHECK for the node to be a LayoutText. This once crashed because
  // Android had a slightly different codepath.
  ax_paragraph->LoadInlineTextBoxes();
}

}  // namespace blink
