// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/physical_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class PhysicalFragmentTest : public RenderingTest {
 public:
  String DumpAll(const PhysicalFragment* target = nullptr) const {
    return PhysicalFragment::DumpFragmentTree(
        *GetDocument().GetLayoutView(), PhysicalFragment::DumpAll, target);
  }
};

TEST_F(PhysicalFragmentTest, DumpFragmentTreeBasic) {
  SetBodyInnerHTML(R"HTML(
    <div id="block"></div>
  )HTML");
  String dump = DumpAll();
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x8 LayoutBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutBlockFlow BODY
        Box (block-flow) offset:0,0 size:784x0 LayoutBlockFlow DIV id='block'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// LayoutView is the containing block of an absolutely positioned descendant.
TEST_F(PhysicalFragmentTest, DumpFragmentTreeWithAbspos) {
  SetBodyInnerHTML(R"HTML(
    <div id="abs" style="position:absolute;"></div>
  )HTML");

  String dump = DumpAll();
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x8 LayoutBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutBlockFlow (children-inline) BODY
    Box (out-of-flow-positioned block-flow)(self paint) offset:8,8 size:0x0 LayoutBlockFlow (positioned) DIV id='abs'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// An NG object is the containing block of an absolutely positioned descendant.
TEST_F(PhysicalFragmentTest, DumpFragmentTreeWithAbsposInRelpos) {
  SetBodyInnerHTML(R"HTML(
    <div id="rel" style="position:relative;">
      <div id="abs" style="position:absolute; left:10px; top:20px;"></div>
    </div>
  )HTML");

  String dump = DumpAll();
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x8 LayoutBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutBlockFlow BODY
        Box (block-flow)(self paint) offset:0,0 size:784x0 LayoutBlockFlow (relative positioned, children-inline) DIV id='rel'
          Box (out-of-flow-positioned block-flow)(self paint) offset:10,20 size:0x0 LayoutBlockFlow (positioned) DIV id='abs'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// A legacy grid with another legacy grid inside, and some NG objects, too.
TEST_F(PhysicalFragmentTest, DumpFragmentTreeWithGrid) {
  SetBodyInnerHTML(R"HTML(
    <div id="outer-grid" style="display:grid;">
      <div id="grid-as-item" style="display:grid;">
        <div id="inner-grid-item">
          <div id="foo"></div>
        </div>
      </div>
      <div id="block-container-item">
        <div id="bar"></div>
      </div>
    </div>
  )HTML");

  String dump = DumpAll();
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x16 LayoutBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutBlockFlow BODY
        Box (block-flow-root) offset:0,0 size:784x0 LayoutGrid DIV id='outer-grid'
          Box (block-flow-root) offset:0,0 size:784x0 LayoutGrid DIV id='grid-as-item'
            Box (block-flow-root block-flow) offset:0,0 size:784x0 LayoutBlockFlow DIV id='inner-grid-item'
              Box (block-flow) offset:0,0 size:784x0 LayoutBlockFlow DIV id='foo'
          Box (block-flow-root block-flow) offset:0,0 size:784x0 LayoutBlockFlow DIV id='block-container-item'
            Box (block-flow) offset:0,0 size:784x0 LayoutBlockFlow DIV id='bar'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(PhysicalFragmentTest, DumpFragmentTreeWithTargetInsideColumn) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns:3;">
      <div id="child" style="height:150px;"></div>
    </div>
  )HTML");

  const LayoutObject* child_object = GetLayoutObjectByElementId("child");
  ASSERT_TRUE(child_object);
  ASSERT_TRUE(child_object->IsBox());
  const LayoutBox& box = To<LayoutBox>(*child_object);
  ASSERT_EQ(box.PhysicalFragmentCount(), 3u);
  const PhysicalBoxFragment* second_child_fragment = box.GetPhysicalFragment(1);

  String dump = DumpAll(second_child_fragment);
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x66 LayoutBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x50 LayoutBlockFlow BODY
        Box (block-flow-root block-flow) offset:0,0 size:784x50 LayoutBlockFlow DIV id='multicol'
          Box (column block-flow) offset:0,0 size:260.65625x50
            Box (block-flow) offset:0,0 size:260.65625x50 LayoutBlockFlow DIV id='child'
          Box (column block-flow) offset:261.65625,0 size:260.65625x50
*           Box (block-flow) offset:0,0 size:260.65625x50 LayoutBlockFlow DIV id='child'
          Box (column block-flow) offset:523.3125,0 size:260.65625x50
            Box (block-flow) offset:0,0 size:260.65625x50 LayoutBlockFlow DIV id='child'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // namespace blink
