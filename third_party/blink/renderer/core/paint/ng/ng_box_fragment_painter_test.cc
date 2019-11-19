// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class NGBoxFragmentPainterTest : public PaintControllerPaintTest,
                                 private ScopedLayoutNGForTest {
 public:
  NGBoxFragmentPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedLayoutNGForTest(true) {}
};

using NGBoxFragmentPainterScrollHitTestTest = NGBoxFragmentPainterTest;
INSTANTIATE_SCROLL_HIT_TEST_SUITE_P(NGBoxFragmentPainterScrollHitTestTest);

TEST_P(NGBoxFragmentPainterScrollHitTestTest, ScrollHitTestOrder) {
  GetPage().GetSettings().SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #scroller {
        width: 40px;
        height: 40px;
        overflow: scroll;
        font-size: 500px;
      }
    </style>
    <div id='scroller'>TEXT</div>
  )HTML");
  auto& scroller = *GetLayoutObjectByElementId("scroller");

  const NGPaintFragment& root_fragment = *scroller.PaintFragment();
  const NGPaintFragment& line_box_fragment = *root_fragment.FirstChild();
  const NGPaintFragment& text_fragment = *line_box_fragment.FirstChild();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   DisplayItem::kDocumentBackground),
                          IsSameId(&root_fragment, DisplayItem::kScrollHitTest),
                          IsSameId(&text_fragment, kForegroundType)));
}

}  // namespace blink
