// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class NGFlexLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {
 protected:
  DevtoolsFlexInfo LayoutForDevtools(const String& body_content) {
    SetBodyInnerHTML(body_content);
    UpdateAllLifecyclePhasesForTest();
    LayoutNGFlexibleBox* flex =
        To<LayoutNGFlexibleBox>(GetLayoutObjectByElementId("flexbox"));
    EXPECT_NE(flex, nullptr);
    return flex->LayoutForDevtools();
  }
};

TEST_F(NGFlexLayoutAlgorithmTest, DetailsFlexDoesntCrash) {
  SetBodyInnerHTML(R"HTML(
    <details style="display:flex"></details>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // No crash is good.
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsBasic) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px;" id=flexbox>
      <div style="flex-grow: 1; height: 50px;"></div>
      <div style="flex-grow: 1"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
  EXPECT_EQ(devtools.lines[0].items.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items[0], PhysicalRect(0, 0, 50, 50));
  EXPECT_EQ(devtools.lines[0].items[0], PhysicalRect(0, 0, 50, 50));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsWrap) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap;" id=flexbox>
      <div style="min-width: 100px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items.size(), 1u);
  EXPECT_EQ(devtools.lines[0].items[0], PhysicalRect(0, 0, 100, 50));
  EXPECT_EQ(devtools.lines[1].items.size(), 1u);
  EXPECT_EQ(devtools.lines[1].items[0], PhysicalRect(0, 50, 100, 90));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsCoordinates) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; flex-wrap: wrap; border-top: 2px solid; padding-top: 3px; border-left: 3px solid; padding-left: 5px; margin-left: 19px;" id=flexbox>
      <div style="margin-left: 5px; min-width: 95px; height: 50px;"></div>
      <div style="flex: 1 0 20px; height: 90px;"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 2u);
  EXPECT_EQ(devtools.lines[0].items.size(), 1u);
  EXPECT_EQ(devtools.lines[0].items[0], PhysicalRect(8, 5, 100, 50));
  EXPECT_EQ(devtools.lines[1].items.size(), 1u);
  EXPECT_EQ(devtools.lines[1].items[0], PhysicalRect(8, 55, 100, 90));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsOverflow) {
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
    <div style="display:flex; width: 100px; border-left: 1px solid; border-right: 3px solid;" id=flexbox>
      <div style="min-width: 150px; height: 75px;"></div>
    </div>
  )HTML");
  EXPECT_EQ(devtools.lines[0].items[0], PhysicalRect(1, 0, 150, 75));
}

TEST_F(NGFlexLayoutAlgorithmTest, DevtoolsWithRelPosItem) {
  // Devtools' heuristic algorithm shows two lines for this case, but layout
  // knows there's only one line.
  DevtoolsFlexInfo devtools = LayoutForDevtools(R"HTML(
  <style>
  .item {
    flex: 0 0 50px;
    height: 50px;
  }
  </style>
  <div style="display: flex;" id=flexbox>
    <div class=item></div>
    <div class=item style="position: relative; top: 60px; left: -10px"></div>
  </div>
  )HTML");
  EXPECT_EQ(devtools.lines.size(), 1u);
}

}  // namespace
}  // namespace blink
