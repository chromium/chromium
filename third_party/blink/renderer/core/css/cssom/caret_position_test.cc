// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/caret_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_caret_position_from_point_options.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CaretPositionTest : public PageTestBase {
 public:
  CaretPositionTest() = default;

 protected:
  // Sweeps caretPositionFromPoint() across the width of |control| and returns
  // the distinct getClientRect() positions of the reported carets.
  Vector<gfx::PointF> DistinctCaretRectPositions(TextControlElement& control) {
    const DOMRect* bounds = control.GetBoundingClientRect();
    const float y = bounds->top() + 12;
    Vector<gfx::PointF> positions;
    for (float x = bounds->left() + 4; x <= bounds->right() - 4; x += 2) {
      CaretPosition* caret_position = GetDocument().caretPositionFromPoint(
          x, y, CaretPositionFromPointOptions::Create());
      if (!caret_position) {
        continue;
      }
      EXPECT_EQ(&control, caret_position->offsetNode());
      EXPECT_EQ(0u, caret_position->offset());
      DOMRect* rect = caret_position->getClientRect();
      if (!rect) {
        continue;
      }
      gfx::PointF position(rect->x(), rect->y());
      if (!positions.Contains(position)) {
        positions.push_back(position);
      }
    }
    return positions;
  }
};

TEST_F(CaretPositionTest, offsetNodeAndOffset) {
  SetBodyContent(
      "<div>"
      "<span id='s0'>s0</span>"
      "<span id='s1'>s1</span>"
      "<span id='s2'>s2</span>"
      "</div>");
  Element* const s0 = GetDocument().getElementById(AtomicString("s0"));
  auto* caret_position = MakeGarbageCollected<CaretPosition>(s0, 1);
  EXPECT_EQ(s0, caret_position->offsetNode());
  EXPECT_EQ(1u, caret_position->offset());
}

TEST_F(CaretPositionTest, offsetNodeAndOffsetInShadowDom) {
  SetBodyContent("<div id='host'></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div>div inside Shadow DOM.</div>");
  Node* text_in_shadow = shadow_root.childNodes()->item(0)->firstChild();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  auto* caret_position = MakeGarbageCollected<CaretPosition>(text_in_shadow, 6);
  EXPECT_EQ(text_in_shadow, caret_position->offsetNode());
  EXPECT_EQ(6u, caret_position->offset());
}

TEST_F(CaretPositionTest, offsetNodeAndOffsetInClosedShadowTree) {
  SetBodyContent("<div id='host'></div>");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kClosed);

  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<div>div inside closed Shadow DOM.</div>");
  Node* text_in_shadow = shadow_root.childNodes()->item(0)->firstChild();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  auto* caret_position = MakeGarbageCollected<CaretPosition>(text_in_shadow, 6);
  EXPECT_EQ(text_in_shadow, caret_position->offsetNode());
  EXPECT_EQ(6u, caret_position->offset());
}

TEST_F(CaretPositionTest, offsetNodeAndOffsetInInput) {
  SetBodyContent("<input value='text inside input'>");
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  Node* text_inside_input = input.InnerEditorElement()->firstChild();

  auto* caret_position =
      MakeGarbageCollected<CaretPosition>(text_inside_input, 6);
  EXPECT_EQ(&input, caret_position->offsetNode());
  EXPECT_EQ(6u, caret_position->offset());
}

TEST_F(CaretPositionTest, getClientRect) {
  SetBodyContent(
      "<div>"
      "<span id='s0'>s0</span>"
      "<span id='s1'>s1</span>"
      "<span id='s2'>s2</span>"
      "</div>");
  Element* const s0 = GetDocument().getElementById(AtomicString("s0"));
  auto* caret_position = MakeGarbageCollected<CaretPosition>(s0, 1);
  EXPECT_EQ(s0, caret_position->offsetNode());
  EXPECT_EQ(1u, caret_position->offset());
  auto* range = Range::Create(GetDocument());
  range->setStart(s0, 1);
  range->setEnd(s0, 1);
  auto* range_client_rect = range->getBoundingClientRect();
  auto* caret_position_client_rect = caret_position->getClientRect();
  EXPECT_NE(nullptr, range_client_rect);
  EXPECT_NE(nullptr, caret_position_client_rect);
  EXPECT_EQ(*range_client_rect, *caret_position_client_rect);
}

TEST_F(CaretPositionTest, getClientRectInInput) {
  SetBodyContent("<input value='text inside input'>");
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  Node* text_inside_input = input.InnerEditorElement()->firstChild();

  auto* caret_position =
      MakeGarbageCollected<CaretPosition>(text_inside_input, 6);
  EXPECT_EQ(&input, caret_position->offsetNode());
  EXPECT_EQ(6u, caret_position->offset());
  auto* range = Range::Create(text_inside_input->GetDocument());
  range->setStart(text_inside_input, 6);
  range->setEnd(text_inside_input, 6);
  auto* range_client_rect = range->getBoundingClientRect();
  auto* caret_position_client_rect = caret_position->getClientRect();
  EXPECT_NE(nullptr, range_client_rect);
  EXPECT_NE(nullptr, caret_position_client_rect);
  EXPECT_EQ(*range_client_rect, *caret_position_client_rect);
}

TEST_F(CaretPositionTest, caretPositionFromPointInInputWithSuggestedValue) {
  SetBodyContent("<input style='width: 300px; font-size: 16px'>");
  auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  input.SetSuggestedValue("suggested value 0123456789");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_LE(DistinctCaretRectPositions(input).size(), 1u);
}

TEST_F(CaretPositionTest, caretPositionFromPointInTextareaWithSuggestedValue) {
  SetBodyContent(
      "<textarea style='width: 300px; font-size: 16px' rows='2'></textarea>");
  auto& textarea =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("textarea")));
  textarea.SetSuggestedValue("suggested value 0123456789");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_LE(DistinctCaretRectPositions(textarea).size(), 1u);
}
}  // namespace blink
