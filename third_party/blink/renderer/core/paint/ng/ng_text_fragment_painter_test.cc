// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class NGTextFragmentPainterTest : public PaintControllerPaintTest,
                                  private ScopedLayoutNGForTest {
 public:
  NGTextFragmentPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(NGTextFragmentPainterTest);

TEST_P(NGTextFragmentPainterTest, TestTextStyle) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="container">Hello World!</div>
    </body>
  )HTML");

  LayoutObject& container = *GetLayoutObjectByElementId("container");
  const LayoutNGBlockFlow& block_flow = ToLayoutNGBlockFlow(container);
  NGInlineCursor cursor;
  cursor.MoveTo(*block_flow.FirstChild());
  const DisplayItemClient& text_fragment =
      *cursor.Current().GetDisplayItemClient();
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&text_fragment, kForegroundType)));
}

TEST_P(NGTextFragmentPainterTest, LineBreak) {
  SetBodyInnerHTML("<span style='font-size: 20px'>A<br>B<br>C</span>");
  // 0: view background, 1: A, 2: B, 3: C
  EXPECT_EQ(4u, ContentDisplayItems().size());

  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  // 0: view background, 1: A, 2: <br>, 3: B, 4: <br>, 5: C
  EXPECT_EQ(6u, ContentDisplayItems().size());
}

TEST_P(NGTextFragmentPainterTest, DegenerateUnderlineIntercepts) {
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

}  // namespace blink
