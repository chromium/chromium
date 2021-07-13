// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

using testing::ElementsAre;

namespace blink {

using InlineTextBoxPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(InlineTextBoxPainterTest);

TEST_P(InlineTextBoxPainterTest, LineBreak) {
  SetBodyInnerHTML("<span style='font-size: 20px'>A<br>B<br>C</span>");
  // 0: view background, 1: A, 2: B, 3: C
  EXPECT_EQ(4u, ContentDisplayItems().size());

  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  // 0: view background, 1: A, 2: <br>, 3: B, 4: <br>, 5: C
  EXPECT_EQ(6u, ContentDisplayItems().size());
}

class InlineTextBoxPainterNonNGTest : public PaintControllerPaintTest,
                                      public ScopedLayoutNGForTest {
 public:
  InlineTextBoxPainterNonNGTest() : ScopedLayoutNGForTest(false) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(InlineTextBoxPainterNonNGTest);

TEST_P(InlineTextBoxPainterNonNGTest, RecordedSelectionAll) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SetBodyInnerHTML("<span>A<br>B<br>C</span>");

  LocalFrame* local_frame = GetDocument().GetFrame();
  local_frame->Selection().SetHandleVisibleForTesting();
  local_frame->GetPage()->GetFocusController().SetFocusedFrame(local_frame);
  local_frame->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();

  auto chunks = ContentPaintChunks();
  EXPECT_EQ(chunks.size(), 1u);
  EXPECT_TRUE(chunks.begin()->layer_selection_data->start.has_value());
  EXPECT_TRUE(chunks.begin()->layer_selection_data->end.has_value());
  PaintedSelectionBound start =
      chunks.begin()->layer_selection_data->start.value();
  EXPECT_EQ(start.type, gfx::SelectionBound::LEFT);
  EXPECT_EQ(start.edge_start, IntPoint(8, 8));
  EXPECT_EQ(start.edge_end, IntPoint(8, 9));

  PaintedSelectionBound end = chunks.begin()->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  EXPECT_EQ(end.edge_start, IntPoint(9, 10));
  EXPECT_EQ(end.edge_end, IntPoint(9, 11));
}

TEST_P(InlineTextBoxPainterNonNGTest, RecordedSelectionMultiline) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  LocalFrame* local_frame = GetDocument().GetFrame();
  local_frame->Selection().SetSelectionAndEndTyping(
      SelectionSample::SetSelectionText(
          GetDocument().body(),
          "<div style='white-space:pre'>f^oo\nbar\nb|az</div>"));
  local_frame->Selection().SetHandleVisibleForTesting();
  local_frame->GetPage()->GetFocusController().SetFocusedFrame(local_frame);
  UpdateAllLifecyclePhasesForTest();

  auto chunks = ContentPaintChunks();
  EXPECT_EQ(chunks.size(), 1u);
  EXPECT_TRUE(chunks.begin()->layer_selection_data->start.has_value());
  EXPECT_TRUE(chunks.begin()->layer_selection_data->end.has_value());
  PaintedSelectionBound start =
      chunks.begin()->layer_selection_data->start.value();
  EXPECT_EQ(start.type, gfx::SelectionBound::LEFT);
  EXPECT_EQ(start.edge_start, IntPoint(8, 8));
  EXPECT_EQ(start.edge_end, IntPoint(8, 9));

  PaintedSelectionBound end = chunks.begin()->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  EXPECT_EQ(end.edge_start, IntPoint(9, 10));
  EXPECT_EQ(end.edge_end, IntPoint(9, 11));
}

}  // namespace blink
