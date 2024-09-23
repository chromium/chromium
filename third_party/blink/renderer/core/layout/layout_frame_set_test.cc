// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutFrameSetTest : public RenderingTest {};

TEST_F(LayoutFrameSetTest, GetCursor) {
  SetHtmlInnerHTML(R"HTML(
    <frameset id='f' rows='50%,50%' cols='50%,50%' border='20'>
    <frame src=""></frame>
    <frame src=""></frame>
    <frame src=""></frame>
    <frame src=""></frame>
    </frame>)HTML");

  LayoutBox* box = GetLayoutBoxByElementId("f");
  ui::Cursor cursor;
  EXPECT_EQ(kSetCursorBasedOnStyle, box->GetCursor({100, 100}, cursor));

  EXPECT_EQ(kSetCursor, box->GetCursor({100, 300}, cursor));
  EXPECT_EQ(RowResizeCursor(), cursor);

  EXPECT_EQ(kSetCursor, box->GetCursor({400, 100}, cursor));
  EXPECT_EQ(ColumnResizeCursor(), cursor);
}

TEST_F(LayoutFrameSetTest, HitTestingCrash) {
  SetBodyInnerHTML(R"HTML(<hgroup id="container">a
<style>frameset {  transform-style: preserve-3d; }</style></hgroup>)HTML");
  auto& doc = GetDocument();
  Element* outer_frameset = doc.CreateRawElement(html_names::kFramesetTag);
  GetElementById("container")->appendChild(outer_frameset);
  // `outer_frameset` has no `rows` and `cols` attributes. So it shows at most
  // one child, and other children don't have physical fragments.
  outer_frameset->appendChild(doc.CreateRawElement(html_names::kFramesetTag));
  outer_frameset->appendChild(doc.CreateRawElement(html_names::kFramesetTag));
  UpdateAllLifecyclePhasesForTest();

  HitTestLocation location(gfx::PointF(400, 300));
  HitTestResult result;
  GetLayoutView().HitTestNoLifecycleUpdate(location, result);
  // Pass if no crashes in PaintLayer.
}

}  // namespace blink
