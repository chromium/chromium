// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

}  // namespace blink
