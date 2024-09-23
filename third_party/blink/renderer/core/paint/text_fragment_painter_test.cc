// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_fragment_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

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
    <div id="parent" style="width: 100px; height: 100px; position: absolute">
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

}  // namespace blink
