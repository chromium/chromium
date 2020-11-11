// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/compute_layer_selection.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

class ComputeLayerSelectionTest : public EditingTestBase {
 public:
  void SetUp() override {
    EnableCompositing();
    EditingTestBase::SetUp();
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    LoadAhem();
  }

  void FocusAndSelectAll(Element* focus, const Node& select) {
    DCHECK(focus);
    focus->focus();
    Selection().SetSelection(
        SelectionInDOMTree::Builder().SelectAllChildren(select).Build(),
        SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
    UpdateAllLifecyclePhasesForTest();
  }

  void FocusAndSelectAll(TextControlElement* target) {
    FocusAndSelectAll(target, *target->InnerEditorElement());
  }
};

TEST_F(ComputeLayerSelectionTest, ComputeLayerSelection) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
        input {
          font: 10px/1 Ahem;
          padding: 0;
          border: 0;
        }
      </style>
      <input id=target width=20 value='test test test test test tes tes test'
      style='width: 100px; height: 20px;'>
  )HTML");

  FocusAndSelectAll(
      To<HTMLInputElement>(GetDocument().getElementById("target")));

  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_FALSE(composited_selection.start.hidden);
  EXPECT_TRUE(composited_selection.end.hidden);
}

TEST_F(ComputeLayerSelectionTest, DontCrashOnLayerCreation) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
        input {
          font: 10px/1 Ahem;
          padding: 0;
          border: 0;
          width: 100px; height: 20px;
          position: relative;
        }
      </style>
      <input id=target width=20 value='test test test test test tes tes test'>
  )HTML");
  Element* target = GetDocument().getElementById("target");

  FocusAndSelectAll(To<HTMLInputElement>(target));

  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_FALSE(composited_selection.start.hidden);
  EXPECT_TRUE(composited_selection.end.hidden);

  target->setAttribute(html_names::kStyleAttr, "will-change: transform");

  UpdateAllLifecyclePhasesForTest();
  // Passes if no crash.
}

TEST_F(ComputeLayerSelectionTest, PositionInScrollableRoot) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
           margin: 0;
           height: 2000px;
           width: 2000px;
        }
        input {
          font: 10px/1 Ahem;
          padding: 0;
          border: 0;
          width: 100px;
          height: 20px;
          position: absolute;
          top: 900px;
          left: 1000px;
        }
      </style>
      <input id=target width=20 value='test test test test test tes tes test'>
  )HTML");

  FocusAndSelectAll(
      To<HTMLInputElement>(GetDocument().getElementById("target")));

  ScrollableArea* root_scroller = GetDocument().View()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(800, 500),
                                 mojom::blink::ScrollType::kProgrammatic);
  ASSERT_EQ(ScrollOffset(800, 500), root_scroller->GetScrollOffset());

  UpdateAllLifecyclePhasesForTest();

  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());

  // Top-left corner should be around (1000, 905) - 10px centered in 20px
  // height.
  EXPECT_EQ(gfx::Point(1000, 905), composited_selection.start.edge_start);
  EXPECT_EQ(gfx::Point(1000, 915), composited_selection.start.edge_end);
  EXPECT_EQ(gfx::Point(1369, 905), composited_selection.end.edge_start);
  EXPECT_EQ(gfx::Point(1369, 915), composited_selection.end.edge_end);
}

TEST_F(ComputeLayerSelectionTest, PositionInScroller) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
           margin: 0;
           height: 2000px;
           width: 2000px;
        }
        input {
          font: 10px/1 Ahem;
          padding: 0;
          border: 0;
          width: 100px;
          height: 20px;
          position: absolute;
          top: 900px;
          left: 1000px;
        }

        #scroller {
          width: 300px;
          height: 300px;
          position: absolute;
          left: 300px;
          top: 400px;
          overflow: scroll;
          border: 200px;
          will-change: transform;
        }

        #space {
          width: 2000px;
          height: 2000px;
        }
      </style>
      <div id="scroller">
        <div id="space"></div>
        <input id=target width=20 value='test test test test test tes tes test'>
      </div>
  )HTML");

  FocusAndSelectAll(
      To<HTMLInputElement>(GetDocument().getElementById("target")));

  Element* e = GetDocument().getElementById("scroller");
  PaintLayerScrollableArea* scroller = e->GetLayoutBox()->GetScrollableArea();
  scroller->SetScrollOffset(ScrollOffset(900, 800),
                            mojom::blink::ScrollType::kProgrammatic);
  ASSERT_EQ(ScrollOffset(900, 800), scroller->GetScrollOffset());

  UpdateAllLifecyclePhasesForTest();

  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());

  // Top-left corner should be around (1000, 905) - 10px centered in 20px
  // height.
  EXPECT_EQ(gfx::Point(1000, 905), composited_selection.start.edge_start);
  EXPECT_EQ(gfx::Point(1000, 915), composited_selection.start.edge_end);
  EXPECT_EQ(gfx::Point(1369, 905), composited_selection.end.edge_start);
  EXPECT_EQ(gfx::Point(1369, 915), composited_selection.end.edge_end);
}

// crbug.com/807930
TEST_F(ComputeLayerSelectionTest, ContentEditableLinebreak) {
  SetBodyContent(
      "<div style='font: 10px/10px Ahem;' contenteditable>"
      "test<br><br></div>");
  Element* target = GetDocument().QuerySelector("div");
  FocusAndSelectAll(target, *target);
  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(composited_selection.start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(composited_selection.start.edge_end, gfx::Point(8, 18));
  EXPECT_EQ(composited_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(composited_selection.end.edge_end, gfx::Point(8, 28));
}

// crbug.com/807930
TEST_F(ComputeLayerSelectionTest, TextAreaLinebreak) {
  SetBodyContent(
      "<textarea style='font: 10px/10px Ahem;'>"
      "test\n</textarea>");
  FocusAndSelectAll(ToTextControl(GetDocument().QuerySelector("textarea")));
  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(composited_selection.start.edge_start, gfx::Point(11, 11));
  EXPECT_EQ(composited_selection.start.edge_end, gfx::Point(11, 21));
  EXPECT_EQ(composited_selection.end.edge_start, gfx::Point(11, 21));
  EXPECT_EQ(composited_selection.end.edge_end, gfx::Point(11, 31));
}

// crbug.com/815099
TEST_F(ComputeLayerSelectionTest, CaretBeforeSoftWrap) {
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width:20px;' "
      "contenteditable>foo</div>");
  Element* target = GetDocument().QuerySelector("div");
  target->focus();
  Node* text_foo = target->firstChild();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(
              PositionWithAffinity({text_foo, 2}, TextAffinity::kUpstream))
          .Build(),
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  UpdateAllLifecyclePhasesForTest();
  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(composited_selection.start.edge_start, gfx::Point(27, 8));
  EXPECT_EQ(composited_selection.start.edge_end, gfx::Point(27, 18));
  EXPECT_EQ(composited_selection.end.edge_start, gfx::Point(27, 8));
  EXPECT_EQ(composited_selection.end.edge_end, gfx::Point(27, 18));
}

TEST_F(ComputeLayerSelectionTest, CaretAfterSoftWrap) {
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width:20px;' "
      "contenteditable>foo</div>");
  Element* target = GetDocument().QuerySelector("div");
  target->focus();
  Node* text_foo = target->firstChild();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(
              PositionWithAffinity({text_foo, 2}, TextAffinity::kDownstream))
          .Build(),
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  UpdateAllLifecyclePhasesForTest();
  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(composited_selection.start.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(composited_selection.start.edge_end, gfx::Point(8, 28));
  EXPECT_EQ(composited_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(composited_selection.end.edge_end, gfx::Point(8, 28));
}

// crbug.com/834686
TEST_F(ComputeLayerSelectionTest, RangeBeginAtBlockEnd) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(
      "<div style='font: 10px/10px Ahem;'>"
      "<div>foo\n^</div><div>ba|r</div></div>");
  Selection().SetSelection(
      selection,
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  Element* target = GetDocument().QuerySelector("div");
  target->focus();
  UpdateAllLifecyclePhasesForTest();
  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(composited_selection.start.edge_start, gfx::Point(38, 8));
  EXPECT_EQ(composited_selection.start.edge_end, gfx::Point(38, 18));
  EXPECT_EQ(composited_selection.end.edge_start, gfx::Point(28, 18));
  EXPECT_EQ(composited_selection.end.edge_end, gfx::Point(28, 28));
}

TEST_F(ComputeLayerSelectionTest, BlockEndBR1) {
  // LayerSelection should be:
  // ^test<br>
  // |<br>
  SetBodyContent(
      "<div style='font: 10px/10px Ahem;'>"
      "test<br><br></div>");
  Element* target = GetDocument().QuerySelector("div");
  FocusAndSelectAll(target, *target);
  const cc::LayerSelection& layer_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(layer_selection.start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(layer_selection.start.edge_end, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_end, gfx::Point(8, 28));
}

TEST_F(ComputeLayerSelectionTest, BlockEndBR2) {
  // LayerSelection should be:
  // ^test<br>
  // |<br>
  SetBodyContent(
      "<div style='font: 10px/10px Ahem;'>"
      "<div><span>test<br></span><br></div>");
  Element* target = GetDocument().QuerySelector("div");
  FocusAndSelectAll(target, *target);
  const cc::LayerSelection& layer_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(layer_selection.start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(layer_selection.start.edge_end, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_end, gfx::Point(8, 28));
}

TEST_F(ComputeLayerSelectionTest, BlockEndBR3) {
  // LayerSelection should be:
  // ^test<br>
  // |<br>
  SetBodyContent(
      "<div style='font: 10px/10px Ahem;'>"
      "<div><div>test<br></div><br></div>");
  Element* target = GetDocument().QuerySelector("div");
  FocusAndSelectAll(target, *target);
  const cc::LayerSelection& layer_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(layer_selection.start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(layer_selection.start.edge_end, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_end, gfx::Point(8, 28));
}

// crbug.com/889799. Checking when edge_end on box boundary, bound is still
// visible.
TEST_F(ComputeLayerSelectionTest, SamplePointOnBoundary) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
      input {
        padding: 0px;
        border: 0px;
        font-size: 17px;
        line-height: 18px;
      }
      </style>
      <input id=target value='test test test test'>
  )HTML");
  GetDocument().GetFrame()->SetPageZoomFactor(2.625);

  FocusAndSelectAll(
      To<HTMLInputElement>(GetDocument().getElementById("target")));

  const cc::LayerSelection& composited_selection =
      ComputeLayerSelection(Selection());
  EXPECT_FALSE(composited_selection.start.hidden);
  EXPECT_FALSE(composited_selection.end.hidden);
}

// https://crbug.com/892584.
TEST_F(ComputeLayerSelectionTest, CrossingBlock1) {
  // TODO(yoichio): To support this case with ComputeLayoutSelection,
  // we may need to fix LocalCaretRectOfPosition(<after first br>).
  Selection().SetSelection(
      SetSelectionTextToBody("<div style='font: 10px/10px Ahem;'>"
                             "<div>^<br></div>"
                             "<div>|<br></div>"
                             "</div>"),
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  Selection().CommitAppearanceIfNeeded();
  const cc::LayerSelection& layer_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(layer_selection.start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(layer_selection.start.edge_end, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_end, gfx::Point(8, 28));
}

// https://crbug.com/892584.
TEST_F(ComputeLayerSelectionTest, CrossingBlock2) {
  // TODO(yoichio): To support this case with ComputeLayoutSelection,
  // we may need to fix LocalCaretRectOfPosition(<after first br>).
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable style='font: 10px/10px Ahem;'>"
          "<div>^<br></div>"
          "<div>|<br></div>"
          "</div>"),
      SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
  Selection().CommitAppearanceIfNeeded();
  const cc::LayerSelection& layer_selection =
      ComputeLayerSelection(Selection());
  EXPECT_EQ(layer_selection.start.edge_start, gfx::Point(8, 8));
  EXPECT_EQ(layer_selection.start.edge_end, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_start, gfx::Point(8, 18));
  EXPECT_EQ(layer_selection.end.edge_end, gfx::Point(8, 28));
}

}  // namespace blink
