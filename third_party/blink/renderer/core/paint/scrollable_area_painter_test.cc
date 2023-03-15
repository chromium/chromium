// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

using testing::_;
using testing::ElementsAre;

namespace blink {

using ScrollableAreaPainterTest = PaintControllerPaintTestBase;

TEST_F(ScrollableAreaPainterTest, OverlayScrollbars) {
  SetPreferCompositingToLCDText(true);
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="overflow: scroll; width: 50px; height: 50px">
      <div style="width: 200px; height: 200px"></div>
    </div>
  )HTML");

  ASSERT_TRUE(
      GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());
  const auto* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  const auto* properties =
      GetLayoutObjectByElementId("target")->FirstFragment().PaintProperties();
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->HorizontalScrollbarEffect());
  ASSERT_TRUE(properties->VerticalScrollbarEffect());

  PaintChunk::Id horizontal_id(
      target->GetScrollableArea()->HorizontalScrollbar()->Id(),
      DisplayItem::kScrollbarHorizontal);
  auto horizontal_state = target->FirstFragment().LocalBorderBoxProperties();
  horizontal_state.SetEffect(*properties->HorizontalScrollbarEffect());

  PaintChunk::Id vertical_id(
      target->GetScrollableArea()->VerticalScrollbar()->Id(),
      DisplayItem::kScrollbarVertical);
  auto vertical_state = target->FirstFragment().LocalBorderBoxProperties();
  vertical_state.SetEffect(*properties->VerticalScrollbarEffect());

  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON, _, _, _,
                          IsPaintChunk(1, 2, horizontal_id, horizontal_state),
                          IsPaintChunk(2, 3, vertical_id, vertical_state)));
}

}  // namespace blink
