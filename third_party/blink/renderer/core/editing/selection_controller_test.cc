// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_controller.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class SelectionControllerTest : public EditingTestBase {
 public:
  SelectionControllerTest(const SelectionControllerTest&) = delete;
  SelectionControllerTest& operator=(const SelectionControllerTest&) = delete;

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

  HitTestResult HitTestResultAtLocation(int x, int y) {
    HitTestLocation location(gfx::Point(x, y));
    return HitTestResultAtLocation(location);
  }

  static PositionWithAffinity GetPositionFromHitTestResult(
      const HitTestResult& hit_test_result) {
    return hit_test_result.GetPosition();
  }

  VisibleSelection VisibleSelectionInDOMTree() const {
    return Selection().ComputeVisibleSelectionInDOMTree();
  }

  VisibleSelectionInFlatTree GetVisibleSelectionInFlatTree() const {
    return Selection().ComputeVisibleSelectionInFlatTree();
  }

  bool SelectClosestWordFromHitTestResult(
      const HitTestResult& result,
      AppendTrailingWhitespace append_trailing_whitespace,
      SelectInputEventType select_input_event_type);
  void SetCaretAtHitTestResult(const HitTestResult&);
  void SetNonDirectionalSelectionIfNeeded(const SelectionInFlatTree&,
                                          TextGranularity);
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

TEST_F(SelectionControllerTest, setNonDirectionalSelectionIfNeeded) {
  const char* body_content = "<span id=top>top</span><span id=host></span>";
  const char* shadow_content = "<span id=bottom>bottom</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* top = GetDocument().getElementById(AtomicString("top"))->firstChild();
  Node* bottom =
      shadow_root->getElementById(AtomicString("bottom"))->firstChild();

  // top to bottom
  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(bottom, 3))
                                         .Build(),
                                     TextGranularity::kCharacter);
  EXPECT_EQ(VisibleSelectionInDOMTree().Start(),
            VisibleSelectionInDOMTree().Anchor());
  EXPECT_EQ(VisibleSelectionInDOMTree().End(),
            VisibleSelectionInDOMTree().Focus());
  EXPECT_EQ(Position(top, 1), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(top, 3), VisibleSelectionInDOMTree().End());

  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Anchor());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().Focus());
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
            VisibleSelectionInDOMTree().Anchor());
  EXPECT_EQ(VisibleSelectionInDOMTree().Start(),
            VisibleSelectionInDOMTree().Focus());
  EXPECT_EQ(Position(bottom, 0), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(bottom, 3), VisibleSelectionInDOMTree().End());

  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().Anchor());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Focus());
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
  script->setInnerHTML(
      "var sample = document.getElementById('sample');"
      "sample.addEventListener('onselectstart', "
      "  event => elem.parentNode.removeChild(elem));");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();
  HitTestLocation location((gfx::Point(8, 8)));
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
  HitTestLocation location((gfx::Point(10, 10)));
  SetCaretAtHitTestResult(
      GetFrame().GetEventHandler().HitTestResultAtLocation(location));

  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
}

// For http://crbug.com/759971
TEST_F(SelectionControllerTest,
       SetCaretAtHitTestResultWithDisconnectedPosition) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
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
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kIsCompatibilityEventForTouch,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  mouse_event.SetFrameScale(1);
  HitTestLocation location((gfx::Point(0, 0)));
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
  Element* const input = GetDocument().QuerySelector(AtomicString("input"));

  const SelectionInFlatTree& selection = ExpandWithGranularity(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::BeforeNode(*input))
          .Extend(PositionInFlatTree::AfterNode(*input))
          .Build(),
      TextGranularity::kWord);
  const SelectionInFlatTree& result =
      AdjustSelectionWithTrailingWhitespace(selection);

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
  HitTestLocation location(gfx::Point(40, 10));
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
  EXPECT_EQ("<pre>(1)^\n|(2)</pre>", GetSelectionTextFromBody());

  // Select word by tap
  EXPECT_FALSE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kTouch));
  EXPECT_EQ("<pre>(1)^\n|(2)</pre>", GetSelectionTextFromBody())
      << "selection isn't changed";
}

TEST_F(SelectionControllerTest,
       SelectClosestWordFromHitTestResultAtEndOfLine2) {
  InsertStyleElement("body { margin: 0; padding: 0; font: 10px monospace; }");
  SetBodyContent("<pre>ab:\ncd</pre>");

  // Click/Tap after "(1)"
  HitTestLocation location(gfx::Point(40, 10));
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
  EXPECT_EQ("<pre>ab:^\n|cd</pre>", GetSelectionTextFromBody());

  // Select word by tap
  EXPECT_FALSE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kTouch));
  EXPECT_EQ("<pre>ab:^\n|cd</pre>", GetSelectionTextFromBody())
      << "selection isn't changed";
}

// For http://crbug.com/1092554
TEST_F(SelectionControllerTest, SelectWordToEndOfLine) {
  LoadAhem();
  InsertStyleElement("body { margin: 0; padding: 0; font: 10px/10px Ahem; }");
  SetBodyContent("<div>abc def<br/>ghi</div>");

  // Select foo
  blink::WebMouseEvent double_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  double_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  double_click.button = blink::WebMouseEvent::Button::kLeft;
  double_click.click_count = 2;
  HitTestResult result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  GetFrame().GetEventHandler().GetSelectionController().HandleMousePressEvent(
      MouseEventWithHitTestResults(double_click, location, result));
  ASSERT_EQ("<div>ab|c def<br>ghi</div>",
            GetSelectionTextFromBody(
                SelectionInDOMTree::Builder()
                    .Collapse(GetPositionFromHitTestResult(result))
                    .Build()));

  // Select word by mouse
  EXPECT_TRUE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kMouse));
  EXPECT_EQ("<div>^abc| def<br>ghi</div>", GetSelectionTextFromBody());

  // Select to end of line
  blink::WebMouseEvent single_shift_click(
      blink::WebMouseEvent::Type::kMouseDown,
      blink::WebInputEvent::Modifiers::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  single_shift_click.SetFrameScale(1);
  HitTestLocation single_click_location((gfx::Point(400, 5)));
  single_shift_click.button = blink::WebMouseEvent::Button::kLeft;
  single_shift_click.click_count = 1;
  HitTestResult single_click_result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(
          single_click_location);
  GetFrame().GetEventHandler().GetSelectionController().HandleMousePressEvent(
      MouseEventWithHitTestResults(single_shift_click, single_click_location,
                                   single_click_result));
  EXPECT_EQ("<div>^abc def<br>|ghi</div>", GetSelectionTextFromBody());
}

// For http://crbug.com/892750
TEST_F(SelectionControllerTest, SelectWordToEndOfTableCell) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0; padding: 0; font: 10px/10px Ahem; } td {width: "
      "200px}");
  SetBodyContent("<table><td>foo</td><td>bar</td></table>");

  // Select foo
  blink::WebMouseEvent double_click(
      blink::WebMouseEvent::Type::kMouseDown, 0,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  double_click.SetFrameScale(1);
  HitTestLocation location((gfx::Point(20, 5)));
  double_click.button = WebMouseEvent::Button::kLeft;
  double_click.click_count = 2;
  HitTestResult result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  GetFrame().GetEventHandler().GetSelectionController().HandleMousePressEvent(
      MouseEventWithHitTestResults(double_click, location, result));
  ASSERT_EQ("<table><tbody><tr><td>fo|o</td><td>bar</td></tr></tbody></table>",
            GetSelectionTextFromBody(
                SelectionInDOMTree::Builder()
                    .Collapse(GetPositionFromHitTestResult(result))
                    .Build()));
  // Select word by mouse
  EXPECT_TRUE(SelectClosestWordFromHitTestResult(
      result, AppendTrailingWhitespace::kDontAppend,
      SelectInputEventType::kMouse));
  EXPECT_EQ("<table><tbody><tr><td>^foo|</td><td>bar</td></tr></tbody></table>",
            GetSelectionTextFromBody());

  // Select to end of cell 1
  blink::WebMouseEvent cell1_single_shift_click(
      blink::WebMouseEvent::Type::kMouseDown,
      blink::WebInputEvent::Modifiers::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  cell1_single_shift_click.SetFrameScale(1);
  HitTestLocation cell1_single_click_location((gfx::Point(175, 5)));
  cell1_single_shift_click.button = blink::WebMouseEvent::Button::kLeft;
  cell1_single_shift_click.click_count = 1;
  HitTestResult cell1_single_click_result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(
          cell1_single_click_location);
  GetFrame().GetEventHandler().GetSelectionController().HandleMousePressEvent(
      MouseEventWithHitTestResults(cell1_single_shift_click,
                                   cell1_single_click_location,
                                   cell1_single_click_result));
  EXPECT_EQ("<table><tbody><tr><td>^foo|</td><td>bar</td></tr></tbody></table>",
            GetSelectionTextFromBody());

  // Select to end of cell 2
  blink::WebMouseEvent cell2_single_shift_click(
      blink::WebMouseEvent::Type::kMouseDown,
      blink::WebInputEvent::Modifiers::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  // Frame scale defaults to 0, which would cause a divide-by-zero problem.
  cell2_single_shift_click.SetFrameScale(1);
  HitTestLocation cell2_single_click_location((gfx::Point(375, 5)));
  cell2_single_shift_click.button = blink::WebMouseEvent::Button::kLeft;
  cell2_single_shift_click.click_count = 1;
  HitTestResult cell2_single_click_result =
      GetFrame().GetEventHandler().HitTestResultAtLocation(
          cell2_single_click_location);
  GetFrame().GetEventHandler().GetSelectionController().HandleMousePressEvent(
      MouseEventWithHitTestResults(cell2_single_shift_click,
                                   cell2_single_click_location,
                                   cell2_single_click_result));
  EXPECT_EQ("<table><tbody><tr><td>^foo</td><td>bar|</td></tr></tbody></table>",
            GetSelectionTextFromBody());
}

TEST_F(SelectionControllerTest, Scroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-size: 50px;
      line-height: 2;
    }
    #scroller {
      width: 400px;
      height: 600px;
      overflow: scroll;
    }
    </style>
    <div id="scroller">
      <span>line1</span><br>
      <span>line2</span><br>
      <span>line3</span><br>
      <span>line4</span><br>
      <span style="padding-left: 100px">line5</span><br>
      <span style="border-left: 100px solid blue">line6</span><br>
      <span style="margin-left: 100px">line7</span><br>
      <span style="display: inline-block; width: 100px; height: 1em; line-height: 1">x</span>line8<br>
      <span>line9</span><br>
    </div>
  )HTML");

  // Scroll #scroller by 2 lines. "line3" should be at the top.
  Element* scroller = GetElementById("scroller");
  scroller->setScrollTop(200);

  // Hit-test on the first visible line. This should be "line3".
  HitTestResult line3_result = HitTestResultAtLocation(5, 50);
  EXPECT_EQ(line3_result.LocalPoint(), PhysicalOffset(5, 50));
  PositionWithAffinity line3 = line3_result.GetPosition();
  Node* line3_node = line3.AnchorNode();
  EXPECT_EQ(line3_node->nodeName(), "#text");
  EXPECT_EQ(line3_node->textContent(), "line3");

  // Then hit-test beyond the end of the first visible line. This should snap to
  // the end of the "line3".
  //
  // +------------
  // |line3   x <-- Click here
  // |line4
  HitTestResult line3_end_result = HitTestResultAtLocation(300, 50);
  EXPECT_EQ(line3_end_result.LocalPoint(), PhysicalOffset(300, 50));
  PositionWithAffinity line3_end = line3_end_result.GetPosition();
  Node* line3_end_node = line3_end.AnchorNode();
  EXPECT_EQ(line3_end_node->nodeName(), "#text");
  EXPECT_EQ(line3_end_node->textContent(), "line3");

  // At the line-gap between line3 and line4.
  // There is no |LayoutText| here, but it should snap to line4.
  HitTestResult line4_over_result = HitTestResultAtLocation(5, 101);
  EXPECT_EQ(line4_over_result.LocalPoint(), PhysicalOffset(5, 101));
  PositionWithAffinity line4_over = line4_over_result.GetPosition();
  Node* line4_over_node = line4_over.AnchorNode();
  EXPECT_EQ(line4_over_node->nodeName(), "#text");
  EXPECT_EQ(line4_over_node->textContent(), "line4");

  // At the padding of an inline box.
  HitTestResult line5_result = HitTestResultAtLocation(5, 250);
  EXPECT_EQ(line5_result.LocalPoint(), PhysicalOffset(5, 250));
  PositionWithAffinity line5 = line5_result.GetPosition();
  Node* line5_node = line5.AnchorNode();
  EXPECT_EQ(line5_node->nodeName(), "#text");
  EXPECT_EQ(line5_node->textContent(), "line5");

  // At the border of an inline box.
  HitTestResult line6_result = HitTestResultAtLocation(5, 350);
  EXPECT_EQ(line6_result.LocalPoint(), PhysicalOffset(5, 350));
  PositionWithAffinity line6 = line6_result.GetPosition();
  Node* line6_node = line6.AnchorNode();
  EXPECT_EQ(line6_node->nodeName(), "#text");
  EXPECT_EQ(line6_node->textContent(), "line6");

  // At the margin of an inline box.
  HitTestResult line7_result = HitTestResultAtLocation(5, 450);
  EXPECT_EQ(line7_result.LocalPoint(), PhysicalOffset(5, 450));
  PositionWithAffinity line7 = line7_result.GetPosition();
  Node* line7_node = line7.AnchorNode();
  EXPECT_EQ(line7_node->nodeName(), "#text");
  EXPECT_EQ(line7_node->textContent(), "line7");

  // At the inline-block.
  HitTestResult line8_result = HitTestResultAtLocation(5, 550);
  EXPECT_EQ(line8_result.LocalPoint(), PhysicalOffset(5, 25));
  PositionWithAffinity line8 = line8_result.GetPosition();
  Node* line8_node = line8.AnchorNode();
  EXPECT_EQ(line8_node->nodeName(), "#text");
  EXPECT_EQ(line8_node->textContent(), "x");
}

// http://crbug.com/1372847
TEST_F(SelectionControllerTest, AdjustSelectionByUserSelectWithInput) {
  SetBodyContent(R"HTML(
    <div style="user-select: none;">
      <div id="one" style="user-select: text;">11</div>
      <input type="text" value="input"/>
    </div>
    <div id="two">22</div>)HTML");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  const SelectionInFlatTree& selection =
      ExpandWithGranularity(SelectionInFlatTree::Builder()
                                .Collapse(PositionInFlatTree(one, 0))
                                .Build(),
                            TextGranularity::kParagraph);
  SelectionInFlatTree adjust_selection =
      AdjustSelectionByUserSelect(one, selection);
  EXPECT_EQ(adjust_selection.Anchor(), selection.Anchor());
  EXPECT_EQ(adjust_selection.Focus(), PositionInFlatTree(one->parentNode(), 2));
}

// http://crbug.com/1410448
TEST_F(SelectionControllerTest, AdjustSelectionByUserSelectWithSpan) {
  SetBodyContent(R"HTML(
    <div id="div" style="user-select:none">
      <span id="one" style="user-select:text">
        <span style="user-select:text">Hel</span>lo
      </span>
      <span style="user-select:text"> lo </span>
      <span id="two" style="user-select:text">there</span></div>)HTML");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  const SelectionInFlatTree& selection =
      ExpandWithGranularity(SelectionInFlatTree::Builder()
                                .Collapse(PositionInFlatTree(one, 0))
                                .Build(),
                            TextGranularity::kParagraph);
  SelectionInFlatTree adjust_selection =
      AdjustSelectionByUserSelect(one, selection);
  EXPECT_EQ(adjust_selection.Anchor(), selection.Anchor());
  EXPECT_EQ(adjust_selection.Focus(),
            PositionInFlatTree::LastPositionInNode(*two->firstChild()));
}

// http://crbug.com/1487484
TEST_F(SelectionControllerTest, AdjustSelectionByUserSelectWithComment) {
  SetBodyContent(R"HTML(
    <div id="div">
      <span id="one">Hello World!</span>
      <b>before comment</b><!---->
      <span id="two">after comment Hello World!</span>
    </div>)HTML");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  const SelectionInFlatTree& selection =
      ExpandWithGranularity(SelectionInFlatTree::Builder()
                                .Collapse(PositionInFlatTree(one, 0))
                                .Build(),
                            TextGranularity::kParagraph);
  SelectionInFlatTree adjust_selection =
      AdjustSelectionByUserSelect(one, selection);
  EXPECT_EQ(adjust_selection.Anchor(), selection.Anchor());
  EXPECT_EQ(adjust_selection.Anchor(),
            PositionInFlatTree::FirstPositionInNode(*one->firstChild()));
  EXPECT_EQ(adjust_selection.Focus(), selection.Focus());
  EXPECT_EQ(adjust_selection.Focus(),
            PositionInFlatTree::LastPositionInNode(*two->firstChild()));
}

}  // namespace blink
