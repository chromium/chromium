// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

using testing::ElementsAre;

namespace blink {

class SelectionBoundsRecorderTest : public PaintControllerPaintTestBase,
                                    public testing::WithParamInterface<bool>,
                                    public ScopedLayoutNGForTest {
 public:
  SelectionBoundsRecorderTest() : ScopedLayoutNGForTest(GetParam()) {}
};

struct SelectionBoundsRecorderTestPassToString {
  std::string operator()(const testing::TestParamInfo<bool> b) const {
    return b.param ? "LayoutNG" : "LegacyLayout";
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SelectionBoundsRecorderTest,
                         ::testing::Bool(),
                         SelectionBoundsRecorderTestPassToString());

TEST_P(SelectionBoundsRecorderTest, SelectAll) {
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
  EXPECT_EQ(start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(start.edge_end, gfx::Point(8, 9));

  PaintedSelectionBound end = chunks.begin()->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  EXPECT_EQ(end.edge_start, gfx::Point(9, 10));
  EXPECT_EQ(end.edge_end, gfx::Point(9, 11));
}

TEST_P(SelectionBoundsRecorderTest, SelectMultiline) {
  LocalFrame* local_frame = GetDocument().GetFrame();
  LoadAhem(*local_frame);

  local_frame->Selection().SetSelectionAndEndTyping(
      SelectionSample::SetSelectionText(GetDocument().body(),
                                        R"HTML(
          <style>
            div { white-space:pre; font-family: Ahem; }
          </style>
          <div>f^oo\nbar\nb|az</div>
      )HTML"));

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
  EXPECT_EQ(start.edge_start, gfx::Point(9, 8));
  EXPECT_EQ(start.edge_end, gfx::Point(9, 9));

  PaintedSelectionBound end = chunks.begin()->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  EXPECT_EQ(end.edge_start, gfx::Point(19, 8));
  EXPECT_EQ(end.edge_end, gfx::Point(19, 9));
}

TEST_P(SelectionBoundsRecorderTest, SelectMultilineEmptyStartEnd) {
  LocalFrame* local_frame = GetDocument().GetFrame();
  LoadAhem(*local_frame);
  local_frame->Selection().SetSelectionAndEndTyping(
      SelectionSample::SetSelectionText(GetDocument().body(),
                                        R"HTML(
          <style>
            body { margin: 0; }
            * { font: 10px/1 Ahem; }
          </style>
          <div>foo^<br>bar<br>|baz</div>
      )HTML"));
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
  EXPECT_EQ(start.edge_start, gfx::Point(30, 0));
  EXPECT_EQ(start.edge_end, gfx::Point(30, 10));

  PaintedSelectionBound end = chunks.begin()->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  EXPECT_EQ(end.edge_start, gfx::Point(0, 20));
  EXPECT_EQ(end.edge_end, gfx::Point(0, 30));
}

TEST_P(SelectionBoundsRecorderTest, InvalidationForEmptyBounds) {
  LocalFrame* local_frame = GetDocument().GetFrame();
  LoadAhem(*local_frame);

  // Set a selection that has empty start and end in separate paint chunks.
  // We'll move these empty endpoints into the middle div and make sure
  // everything is invalidated/re-painted/recorded correctly.
  local_frame->Selection().SetSelectionAndEndTyping(
      SelectionSample::SetSelectionText(GetDocument().body(),
                                        R"HTML(
          <style>
            body { margin: 0; }
            div { will-change: transform; }
            * { font: 10px/1 Ahem; }
          </style>
          <div>foo^</div><div id=target>bar</div><div>|baz</div>
      )HTML"));
  local_frame->Selection().SetHandleVisibleForTesting();
  local_frame->GetPage()->GetFocusController().SetFocusedFrame(local_frame);
  UpdateAllLifecyclePhasesForTest();

  auto chunks = ContentPaintChunks();
  EXPECT_EQ(chunks.size(), 4u);

  PaintChunkSubset::Iterator chunk_iterator = chunks.begin();
  // Skip the root chunk to get to the first div.
  ++chunk_iterator;
  EXPECT_TRUE(chunk_iterator->layer_selection_data->start.has_value());
  PaintedSelectionBound start =
      chunk_iterator->layer_selection_data->start.value();
  EXPECT_EQ(start.type, gfx::SelectionBound::LEFT);
  EXPECT_EQ(start.edge_start, gfx::Point(30, 0));
  EXPECT_EQ(start.edge_end, gfx::Point(30, 10));

  // Skip the middle div as well to get to the third div where the end of the
  // selection is.
  ++chunk_iterator;
  ++chunk_iterator;

  EXPECT_TRUE(chunk_iterator->layer_selection_data->end.has_value());
  PaintedSelectionBound end = chunk_iterator->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  // Coordinates are chunk-relative, so they should start at 0 y coordinate.
  EXPECT_EQ(end.edge_start, gfx::Point(0, 0));
  EXPECT_EQ(end.edge_end, gfx::Point(0, 10));

  // Move the selection around the start and end of the second div.
  local_frame->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetElementById("target")->firstChild(), 0))
          .Extend(Position(GetElementById("target")->firstChild(), 3))
          .Build());

  // Ensure the handle will be visible for the next paint (previous call to
  // SetSelectionAndEndTyping will clear the bit).
  local_frame->Selection().SetHandleVisibleForTesting();

  UpdateAllLifecyclePhasesForTest();

  chunks = ContentPaintChunks();
  chunk_iterator = chunks.begin();
  EXPECT_EQ(chunks.size(), 4u);

  // Skip the root chunk to get to the first div, which should no longer have
  // a recorded value.
  ++chunk_iterator;
  EXPECT_FALSE(chunk_iterator->layer_selection_data);

  // Validate start/end in second div.
  ++chunk_iterator;
  EXPECT_TRUE(chunk_iterator->layer_selection_data->start.has_value());
  EXPECT_TRUE(chunk_iterator->layer_selection_data->end.has_value());
  start = chunk_iterator->layer_selection_data->start.value();
  EXPECT_EQ(start.type, gfx::SelectionBound::LEFT);
  EXPECT_EQ(start.edge_start, gfx::Point(0, 0));
  EXPECT_EQ(start.edge_end, gfx::Point(0, 10));

  end = chunk_iterator->layer_selection_data->end.value();
  EXPECT_EQ(end.type, gfx::SelectionBound::RIGHT);
  EXPECT_EQ(end.edge_start, gfx::Point(30, 0));
  EXPECT_EQ(end.edge_end, gfx::Point(30, 10));

  // Third div's chunk should no longer have an end value.
  ++chunk_iterator;
  EXPECT_FALSE(chunk_iterator->layer_selection_data);
}

}  // namespace blink
