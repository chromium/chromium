// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_fragment_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class NGTextFragmentPainterTest : public PaintControllerPaintTest,
                                  private ScopedLayoutNGForTest {
 public:
  NGTextFragmentPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_PAINT_TEST_CASE_P(NGTextFragmentPainterTest);

TEST_P(NGTextFragmentPainterTest, TestTextStyle) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <body>
      <div id="container">Hello World!</div>
    </body>
  )HTML");

  LayoutObject& container = *GetLayoutObjectByElementId("container");

  const LayoutNGBlockFlow& block_flow = ToLayoutNGBlockFlow(container);

  InvalidateAll(RootPaintController());
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect interest_rect(0, 0, 640, 480);
  Paint(&interest_rect);

  const NGPaintFragment& root_fragment = *block_flow.PaintFragment();
  EXPECT_EQ(1u, root_fragment.Children().size());
  const NGPaintFragment& line_box_fragment = *root_fragment.FirstChild();
  EXPECT_EQ(1u, line_box_fragment.Children().size());
  const NGPaintFragment& text_fragment = *line_box_fragment.FirstChild();

  EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 2,
                      TestDisplayItem(ViewScrollingBackgroundClient(),
                                      DisplayItem::kDocumentBackground),
                      TestDisplayItem(text_fragment, kForegroundType));
}

}  // namespace blink
