// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_fragment_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class TextFragmentPainterTest : public PaintControllerPaintTest {
 public:
  explicit TextFragmentPainterTest(
      LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(TextFragmentPainterTest);

TEST_P(TextFragmentPainterTest, TestTextStyle) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="container">Hello World!</div>
    </body>
  )HTML");

  LayoutObject& container = *GetLayoutObjectByElementId("container");
  const auto& block_flow = To<LayoutBlockFlow>(container);
  InlineCursor cursor;
  cursor.MoveTo(*block_flow.FirstChild());
  const DisplayItemClient& text_fragment =
      *cursor.Current().GetDisplayItemClient();
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_fragment.Id(), kForegroundType)));
}

TEST_P(TextFragmentPainterTest, LineBreak) {
  SetBodyInnerHTML("<span style='font-size: 20px'>A<br>B<br>C</span>");
  // 0: view background, 1: A, 2: B, 3: C
  EXPECT_EQ(4u, ContentDisplayItems().size());

  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  // 0: view background, 1: A, 2: <br>, 3: B, 4: <br>, 5: C
  EXPECT_EQ(6u, ContentDisplayItems().size());
}

TEST_P(TextFragmentPainterTest, LineBreaksInLongDocument) {
  SetBodyInnerHTML(
      "<div id='div' style='font-size: 100px; width: 300px'><div>");
  auto* div = GetDocument().getElementById(AtomicString("div"));
  for (int i = 0; i < 1000; i++) {
    div->appendChild(GetDocument().createTextNode("XX"));
    div->appendChild(
        GetDocument().CreateRawElement(QualifiedName(AtomicString("br"))));
  }
  UpdateAllLifecyclePhasesForTest();
  Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();

  PaintContents(gfx::Rect(0, 0, 800, 600));
  EXPECT_LE(ContentDisplayItems().size(), 100u);
}

TEST_P(TextFragmentPainterTest, DegenerateUnderlineIntercepts) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      span {
        font-size: 20px;
        text-decoration: underline;
      }
    </style>
    <span style="letter-spacing: -1e9999em;">a|b|c d{e{f{</span>
    <span style="letter-spacing: 1e9999em;">a|b|c d{e{f{</span>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Test for https://crbug.com/1043753: the underline intercepts are infinite
  // due to letter spacing and this test passes if that does not cause a crash.
}

TEST_P(TextFragmentPainterTest, SvgTextWithFirstLineTextDecoration) {
  SetBodyInnerHTML(R"HTML(
<!DOCTYPE html>
<style>
*::first-line {
  text-decoration: underline dashed;
}
</style>
<svg xmlns="http://www.w3.org/2000/svg">
  <text y="30">vX7 Image 2</text>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Test passes if no crashes.
}

TEST_P(TextFragmentPainterTest, SvgTextWithTextDecorationNotInFirstLine) {
  SetBodyInnerHTML(R"HTML(
    <style>text:first-line { fill: lime; }</style>
    <svg xmlns="http://www.w3.org/2000/svg">
    <text text-decoration="overline">foo</text>
    </svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Test passes if no crashes.
}

TEST_P(TextFragmentPainterTest, WheelEventListenerOnInlineElement) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <div id="parent" style="width: 100px; height: 100px; will-change: opacity">
      <span id="child" style="font: 50px Ahem">ABC</span>
    </div>
  )HTML");

  SetWheelEventListener("child");
  auto* hit_test_data = MakeGarbageCollected<HitTestData>();
  hit_test_data->wheel_event_rects = {gfx::Rect(0, 0, 150, 50)};
  auto* parent = GetLayoutBoxByElementId("parent");
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                  IsPaintChunk(1, 2,
                               PaintChunk::Id(parent->Layer()->Id(),
                                              DisplayItem::kLayerChunk),
                               parent->FirstFragment().ContentsProperties(),
                               hit_test_data, gfx::Rect(0, 0, 150, 100))));
}

TEST_P(TextFragmentPainterTest,
       WheelEventListenerOnInlineElementUnderBackgroundClipText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div style="background-clip: text; color: transparent;
                background-color: blue">
      <div style="overflow: hidden; height: 10px">
        <span id="child" style="font: 50px Ahem">ABC</span>
      </div>
    </div>
  )HTML");

  SetWheelEventListener("child");
  GetLayoutView().SetSubtreeShouldDoFullPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();

  int wheel_hit_test_data_count = 0;
  for (const auto& chunk : ContentPaintChunks()) {
    if (chunk.hit_test_data && chunk.hit_test_data->wheel_event_rects.size()) {
      wheel_hit_test_data_count++;
      EXPECT_THAT(chunk.hit_test_data->wheel_event_rects,
                  ElementsAre(gfx::Rect(8, 8, 150, 50)));
    }
  }
  EXPECT_EQ(1, wheel_hit_test_data_count);
}

// Painting tests on text color for block caret.
class BlockCaretTextColorPainterTest : public TextFragmentPainterTest {
 public:
  BlockCaretTextColorPainterTest() = default;

 protected:
  void SetUpBlockCaretEditable(const String& style) {
    GetFocusController().SetActive(true);
    GetFocusController().SetFocused(true);
    SetBodyInnerHTML(
        "<div id='target' contenteditable spellcheck='false' "
        "style='font: 16px monospace; " +
        style + "'>abc</div>");
    auto* target = GetElementById("target");
    target->Focus();
    UpdateAllLifecyclePhasesForTest();
  }

  // Move the caret to |dom_offset| inside the editable's first text node.
  void MoveCaretTo(unsigned dom_offset) {
    auto* target = GetElementById("target");
    auto* text = target->firstChild();
    auto& selection = Selection();
    selection.SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(text, dom_offset))
                               .Build(),
                           SetSelectionOptions());
    UpdateAllLifecyclePhasesForTest();
  }

  // Returns ComputeBlockCaretCharacterOffset() for the first text fragment of
  // the editable.
  std::optional<unsigned> CaretFragmentOffset() {
    auto* target = GetElementById("target");
    InlineCursor cursor;
    cursor.MoveTo(
        *To<LayoutBlockFlow>(*target->GetLayoutObject()).FirstChild());
    return Selection().ComputeBlockCaretCharacterOffset(cursor);
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(BlockCaretTextColorPainterTest);

// Default focus places the caret at offset 0.
TEST_P(BlockCaretTextColorPainterTest, OffsetForBlockCaretAtFirstCharacter) {
  ScopedCSSCaretColorWithOptionalSecondValueForTest scoped(true);
  SetUpBlockCaretEditable(
      "caret-color: black white; caret-shape: block; "
      "caret-animation: manual");

  EXPECT_EQ(CaretFragmentOffset(), 0u);
}

// Caret moved one character forward inside the same text node: the override
// must follow.
TEST_P(BlockCaretTextColorPainterTest, OffsetFollowsCaretMovement) {
  ScopedCSSCaretColorWithOptionalSecondValueForTest scoped(true);
  SetUpBlockCaretEditable(
      "caret-color: black white; caret-shape: block; "
      "caret-animation: manual");

  MoveCaretTo(1);
  EXPECT_EQ(CaretFragmentOffset(), 1u);

  MoveCaretTo(2);
  EXPECT_EQ(CaretFragmentOffset(), 2u);
}

// Caret past the last character: nothing is overlapped.
TEST_P(BlockCaretTextColorPainterTest, NoOffsetWhenCaretIsPastLastCharacter) {
  ScopedCSSCaretColorWithOptionalSecondValueForTest scoped(true);
  SetUpBlockCaretEditable(
      "caret-color: black white; caret-shape: block; "
      "caret-animation: manual");

  MoveCaretTo(3);
  // After 'c', no character to overlap.
  EXPECT_EQ(CaretFragmentOffset(), std::nullopt);
}

// For other caret shapes (bar, underscore), the second value of caret-color
// does not apply.
TEST_P(BlockCaretTextColorPainterTest, NoOffsetForNonBlockCaretShape) {
  ScopedCSSCaretColorWithOptionalSecondValueForTest scoped(true);
  SetUpBlockCaretEditable(
      "caret-color: black white; caret-shape: bar; "
      "caret-animation: manual");

  EXPECT_EQ(CaretFragmentOffset(), std::nullopt);
}

// Paint does not happen when the second value of caret-color is auto.
TEST_P(BlockCaretTextColorPainterTest, NoOverrideColorWhenSecondValueIsAuto) {
  ScopedCSSCaretColorWithOptionalSecondValueForTest scoped(true);
  SetUpBlockCaretEditable(
      "caret-color: black auto; caret-shape: block; "
      "caret-animation: manual");

  auto* target = GetElementById("target");

  // Still returns 0 from the offset help function. The painter is aware that
  // the color value is auto.
  EXPECT_TRUE(target->GetLayoutObject()->StyleRef().IsCaretTextColorAuto());
  EXPECT_EQ(CaretFragmentOffset(), 0u);
}

}  // namespace blink
