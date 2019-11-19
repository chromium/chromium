// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_controller.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SelectionControllerTest : public EditingTestBase {
 protected:
  using AppendTrailingWhitespace =
      SelectionController::AppendTrailingWhitespace;
  using SelectInputEventType = SelectionController::SelectInputEventType;

  SelectionControllerTest() = default;

  SelectionController& Controller() {
    return GetFrame().GetEventHandler().GetSelectionController();
  }

  HitTestResult HitTestResultAtLocation(const HitTestLocation& location) {
    return GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  }

  static PositionWithAffinity GetPositionFromHitTestResult(
      const HitTestResult& hit_test_result) {
    return hit_test_result.InnerNode()->GetLayoutObject()->PositionForPoint(
        hit_test_result.LocalPoint());
  }

  PositionWithAffinity GetPositionAtLocation(const IntPoint& point) {
    HitTestLocation location(point);
    HitTestResult result = HitTestResultAtLocation(location);
    return GetPositionFromHitTestResult(result);
  }

  VisibleSelection VisibleSelectionInDOMTree() const {
    return Selection().ComputeVisibleSelectionInDOMTree();
  }

  VisibleSelectionInFlatTree GetVisibleSelectionInFlatTree() const {
    return Selection().GetSelectionInFlatTree();
  }

  bool SelectClosestWordFromHitTestResult(
      const HitTestResult& result,
      AppendTrailingWhitespace append_trailing_whitespace,
      SelectInputEventType select_input_event_type);
  void SetCaretAtHitTestResult(const HitTestResult&);
  void SetNonDirectionalSelectionIfNeeded(const SelectionInFlatTree&,
                                          TextGranularity);

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionControllerTest);
};

bool SelectionControllerTest::SelectClosestWordFromHitTestResult(
    const HitTestResult& result,
    AppendTrailingWhitespace append_trailing_whitespace,
    SelectInputEventType select_input_event_type) {
  return Controller().SelectClosestWordFromHitTestResult(
      result, append_trailing_whitespace, select_input_event_type);
}

void SelectionControllerTest::SetCaretAtHitTestResult(
    const HitTestResult& hit_test_result) {
  GetFrame().GetEventHandler().GetSelectionController().SetCaretAtHitTestResult(
      hit_test_result);
}

void SelectionControllerTest::SetNonDirectionalSelectionIfNeeded(
    const SelectionInFlatTree& new_selection,
    TextGranularity granularity) {
  GetFrame()
      .GetEventHandler()
      .GetSelectionController()
      .SetNonDirectionalSelectionIfNeeded(
          new_selection,
          SetSelectionOptions::Builder().SetGranularity(granularity).Build(),
          SelectionController::kDoNotAdjustEndpoints);
}

class ParameterizedSelectionControllerTest
    : public SelectionControllerTest,
      public testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest {
 public:
  ParameterizedSelectionControllerTest() : ScopedLayoutNGForTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(SelectionControllerTest,
                         ParameterizedSelectionControllerTest,
                         testing::Bool());

TEST_F(SelectionControllerTest, setNonDirectionalSelectionIfNeeded) {
  const char* body_content = "<span id=top>top</span><span id=host></span>";
  const char* shadow_content = "<span id=bottom>bottom</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* top = GetDocument().getElementById("top")->firstChild();
  Node* bottom = shadow_root->getElementById("bottom")->firstChild();

  // top to bottom
  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(bottom, 3))
                                         .Build(),
                                     TextGranularity::kCharacter);
  EXPECT_EQ(VisibleSelectionInDOMTree().Start(),
            VisibleSelectionInDOMTree().Base());
  EXPECT_EQ(VisibleSelectionInDOMTree().End(),
            VisibleSelectionInDOMTree().Extent());
  EXPECT_EQ(Position(top, 1), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(top, 3), VisibleSelectionInDOMTree().End());

  EXPECT_EQ(PositionInFlatTree(top, 1), GetVisibleSelectionInFlatTree().Base());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().Extent());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().End());

  // bottom to top
  SetNonDirectionalSelectionIfNeeded(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree(bottom, 3))
          .Extend(PositionInFlatTree(top, 1))
          .Build(),
      TextGranularity::kCharacter);
  EXPECT_EQ(VisibleSelectionInDOMTree().End(),
            VisibleSelectionInDOMTree().Base());
  EXPECT_EQ(VisibleSelectionInDOMTree().Start(),
            VisibleSelectionInDOMTree().Extent());
  EXPECT_EQ(Position(bottom, 0), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(bottom, 3), VisibleSelectionInDOMTree().End());

  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().Base());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Extent());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().End());
}

TEST_F(SelectionControllerTest, setCaretAtHitTestResult) {
  const char* body_content = "<div id='sample' contenteditable>sample</div>";
  SetBodyContent(body_content);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->SetInnerHTMLFromString(
      "var sample = document.getElementById('sample');"
      "sample.addEventListener('onselectstart', "
      "  event => elem.parentNode.removeChild(elem));");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();
  HitTestLocation location((IntPoint(8, 8)));
  GetFrame().GetEventHandler().GetSelectionController().HandleGestureLongPress(
      GetFrame().GetEventHandler().HitTestResultAtLocation(location));
}

// For http://crbug.com/704827
TEST_F(SelectionControllerTest, setCaretAtHitTestResultWithNullPosition) {
  SetBodyContent(
      "<style>"
      "#sample:before {content: '&nbsp;'}"
      "#sample { user-select: none; }"
      "</style>"
      "<div id=sample></div>");
  UpdateAllLifecyclePhasesForTest();

  // Hit "&nbsp;" in before pseudo element of "sample".
  HitTestLocation location((IntPoint(10, 10)));
  SetCaretAtHitTestResult(
      GetFrame().GetEventHandler().HitTestResultAtLocation(location));

  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
}

// For http://crbug.com/759971
TEST_F(SelectionControllerTest,
       SetCaretAtHitTestResultWithDisconnectedPosition) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->SetInnerHTMLFromString(
      "document.designMode = 'on';"
      "const selection = window.getSelection();"
      "const html = document.getElementsByTagName('html')[0];"
      "selection.collapse(html);"
      "const range = selection.getRangeAt(0);"

      "function selectstart() {"
      "  const body = document.getElementsByTagName('body')[0];"
      "  range.surroundContents(body);"
      "  range.deleteContents();"
      "}"
      "document.addEventListener('selectstart', selectstart);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Simulate a tap somewhere in the document
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown,
      blink::WebInputEvent::kIsCompatibilityEventForTouch,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  mouse_event.SetFrameScale(1);
  HitTestLocation location((IntPoint(0, 0)));
  GetFrame().GetEventHandler().GetSelectionController().HandleMousePressEvent(
      MouseEventWithHitTestResults(
          mouse_event, location,
          GetFrame().GetEventHandler().HitTestResultAtLocation(location)));

  // The original bug was that this test would cause
  // TextSuggestionController::HandlePotentialMisspelledWordTap() to crash. So
  // the primary thing this test cases tests is that we can get here without
  // crashing.

  // Verify no selection was set.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
}

// For http://crbug.com/700368
TEST_F(SelectionControllerTest, AdjustSelectionWithTrailingWhitespace) {
  SetBodyContent(
      "<input type=checkbox>"
      "<div style='user-select:none'>abc</div>");
  Element* const input = GetDocument().QuerySelector("input");

  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelectionWithGranularity(
          SelectionInFlatTree::Builder()
              .Collapse(PositionInFlatTree::BeforeNode(*input))
              .Extend(PositionInFlatTree::AfterNode(*input))
              .Build(),
          TextGranularity::kWord);
  const SelectionInFlatTree& result =
      AdjustSelectionWithTrailingWhitespace(selection.AsSelection());

  EXPECT_EQ(PositionInFlatTree::BeforeNode(*input),
            result.ComputeStartPosition());
  EXPECT_EQ(PositionInFlatTree::AfterNode(*input), result.ComputeEndPosition());
}

// For http://crbug.com/974569
TEST_F(SelectionControllerTest,
       SelectClosestWordFromHitTestResultAtEndOfLine1) {
  InsertStyleElement("body { margin: 0; padding: 0; font: 10px monospace; }");
  SetBodyContent("<pre>(1)\n(2)</pre>");

  // Click/Tap after "(1)"
  HitTestLocation location(IntPoint(40, 10));
  HitTestResult result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  ASSERT_EQ("<pre>(1)|\n(2)</pre>",
            GetSelectionTextFromBody(
                SelectionInDOMTree::Builder()
                    .Collapse(GetPositionFromHitTestResult(result))
                    .Build()));

  // Select word by mouse
  EXPECT_TRUE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kMouse));
  EXPECT_EQ("<pre>(1)^\n(|2)</pre>", GetSelectionTextFromBody());

  // Select word by tap
  EXPECT_FALSE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kTouch));
  EXPECT_EQ("<pre>(1)^\n(|2)</pre>", GetSelectionTextFromBody())
      << "selection isn't changed";
}

TEST_F(SelectionControllerTest,
       SelectClosestWordFromHitTestResultAtEndOfLine2) {
  InsertStyleElement("body { margin: 0; padding: 0; font: 10px monospace; }");
  SetBodyContent("<pre>ab:\ncd</pre>");

  // Click/Tap after "(1)"
  HitTestLocation location(IntPoint(40, 10));
  HitTestResult result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  ASSERT_EQ("<pre>ab:|\ncd</pre>",
            GetSelectionTextFromBody(
                SelectionInDOMTree::Builder()
                    .Collapse(GetPositionFromHitTestResult(result))
                    .Build()));

  // Select word by mouse
  EXPECT_TRUE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kMouse));
  // TODO(yosin): This should be "<pre>ab:^\ncd|</pre>"
  EXPECT_EQ("<pre>ab:^\nc|d</pre>", GetSelectionTextFromBody());

  // Select word by tap
  EXPECT_FALSE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kTouch));
  EXPECT_EQ("<pre>ab:^\nc|d</pre>", GetSelectionTextFromBody())
      << "selection isn't changed";
}

TEST_P(ParameterizedSelectionControllerTest, Scroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-size: 50px;
      line-height: 1;
    }
    #scroller {
      width: 400px;
      height: 5em;
      overflow: scroll;
    }
    </style>
    <div id="scroller">
      <span>line1</span><br>
      <span>line2</span><br>
      <span>line3</span><br>
      <span>line4</span><br>
      <span>line5</span><br>
      <span>line6</span><br>
      <span>line7</span><br>
      <span>line8</span><br>
      <span>line9</span><br>
    </div>
  )HTML");

  // Scroll #scroller by 2 lines. "line3" should be at the top.
  Element* scroller = GetElementById("scroller");
  scroller->setScrollTop(100);

  // Hit-test on the first visible line. This should be "line3".
  PositionWithAffinity line3 = GetPositionAtLocation(IntPoint(5, 5));
  EXPECT_EQ(line3.AnchorNode()->textContent(), "line3");

  // Then hit-test beyond the end of the first visible line. This should snap to
  // the end of the "line3".
  //
  // +------------
  // |line3   x <-- Click here
  // |line4
  PositionWithAffinity line3_end = GetPositionAtLocation(IntPoint(300, 5));
  EXPECT_EQ(line3_end.AnchorNode()->textContent(), "line3");
}

}  // namespace blink
