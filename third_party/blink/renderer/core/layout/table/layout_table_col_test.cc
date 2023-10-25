// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutTableColTest = RenderingTest;

TEST_F(LayoutTableColTest, LocalVisualRect) {
  SetBodyInnerHTML(R"HTML(
    <table style='width: 200px; height: 200px'>
      <col id='col1' style='visibility: hidden'>
      <col id='col2' style='visibility: collapse'>
      <col id='col3'>
      <tr><td></td><td></td></tr>
    </table>
  )HTML");

  // TablesNG hidden columns get geometry, because they paint their background
  // into cells.
  EXPECT_FALSE(GetLayoutObjectByElementId("col1")->LocalVisualRect().IsEmpty());
  EXPECT_TRUE(GetLayoutObjectByElementId("col2")->LocalVisualRect().IsEmpty());
  EXPECT_TRUE(GetLayoutObjectByElementId("col3")->LocalVisualRect().IsEmpty());
}

}  // namespace blink
