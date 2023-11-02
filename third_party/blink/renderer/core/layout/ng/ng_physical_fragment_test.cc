// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class NGPhysicalFragmentTest : public NGLayoutTest {
 public:
  String DumpAll(const NGPhysicalFragment* target = nullptr) const {
    return NGPhysicalFragment::DumpFragmentTree(
        *GetDocument().GetLayoutView(), NGPhysicalFragment::DumpAll, target);
  }

  static bool IsNGViewEnabled() {
    return RuntimeEnabledFeatures::LayoutNGPrintingEnabled();
  }

 private:
  ScopedLayoutNGBlockFragmentationForTest enable_block_fragmentation_{true};
};

TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeBasic) {
  SetBodyInnerHTML(R"HTML(
    <div id="block"></div>
  )HTML");
  String dump = DumpAll();
  if (IsNGViewEnabled()) {
    String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutNGView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='block'
)DUMP";
    EXPECT_EQ(expectation, dump);
  } else {
    String expectation =
        R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside fragment-less or legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='block'
)DUMP";
    EXPECT_EQ(expectation, dump);
  }
}

// LayoutView is the containing block of an absolutely positioned descendant.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithAbspos) {
  SetBodyInnerHTML(R"HTML(
    <div id="abs" style="position:absolute;"></div>
  )HTML");

  String dump = DumpAll();
  if (IsNGViewEnabled()) {
    String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutNGView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
    Box (out-of-flow-positioned block-flow)(self paint) offset:8,8 size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
)DUMP";
    EXPECT_EQ(expectation, dump);
  } else {
    String expectation =
        R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside fragment-less or legacy subtree:)
    Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
  (NG fragment root inside fragment-less or legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
)DUMP";
    EXPECT_EQ(expectation, dump);
  }
}

// An NG object is the containing block of an absolutely positioned descendant.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithAbsposInRelpos) {
  SetBodyInnerHTML(R"HTML(
    <div id="rel" style="position:relative;">
      <div id="abs" style="position:absolute; left:10px; top:20px;"></div>
    </div>
  )HTML");

  String dump = DumpAll();
  if (IsNGViewEnabled()) {
    String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutNGView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow)(self paint) offset:0,0 size:784x0 LayoutNGBlockFlow (relative positioned) DIV id='rel'
          Box (out-of-flow-positioned block-flow)(self paint) offset:10,20 size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
)DUMP";
    EXPECT_EQ(expectation, dump);
  } else {
    String expectation =
        R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside fragment-less or legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow)(self paint) offset:0,0 size:784x0 LayoutNGBlockFlow (relative positioned) DIV id='rel'
          Box (out-of-flow-positioned block-flow)(self paint) offset:10,20 size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
)DUMP";
    EXPECT_EQ(expectation, dump);
  }
}

// A legacy grid with another legacy grid inside, and some NG objects, too.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithGrid) {
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
  if (IsNGViewEnabled()) {
    String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutNGView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x16 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow-root) offset:0,0 size:784x0 LayoutNGGrid DIV id='outer-grid'
          Box (block-flow-root) offset:0,0 size:784x0 LayoutNGGrid DIV id='grid-as-item'
            Box (block-flow-root block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='inner-grid-item'
              Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='foo'
          Box (block-flow-root block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='block-container-item'
            Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='bar'
)DUMP";
    EXPECT_EQ(expectation, dump);
  } else {
    String expectation =
        R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside fragment-less or legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x16 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow-root) offset:0,0 size:784x0 LayoutNGGrid DIV id='outer-grid'
          Box (block-flow-root) offset:0,0 size:784x0 LayoutNGGrid DIV id='grid-as-item'
            Box (block-flow-root block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='inner-grid-item'
              Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='foo'
          Box (block-flow-root block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='block-container-item'
            Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='bar'
)DUMP";
    EXPECT_EQ(expectation, dump);
  }
}

TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithTargetInsideColumn) {
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
  const NGPhysicalBoxFragment* second_child_fragment =
      box.GetPhysicalFragment(1);

  String dump = DumpAll(second_child_fragment);
  if (IsNGViewEnabled()) {
    String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutNGView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x66 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x50 LayoutNGBlockFlow BODY
        Box (block-flow-root block-flow) offset:0,0 size:784x50 LayoutNGBlockFlow DIV id='multicol'
          Box (column block-flow) offset:0,0 size:260.656x50
            Box (block-flow) offset:0,0 size:260.656x50 LayoutNGBlockFlow DIV id='child'
          Box (column block-flow) offset:261.656,0 size:260.656x50
*           Box (block-flow) offset:0,0 size:260.656x50 LayoutNGBlockFlow DIV id='child'
          Box (column block-flow) offset:523.313,0 size:260.656x50
            Box (block-flow) offset:0,0 size:260.656x50 LayoutNGBlockFlow DIV id='child'
)DUMP";
    EXPECT_EQ(expectation, dump);
  } else {
    String expectation =
        R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside fragment-less or legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x66 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x50 LayoutNGBlockFlow BODY
        Box (block-flow-root block-flow) offset:0,0 size:784x50 LayoutNGBlockFlow DIV id='multicol'
          Box (column block-flow) offset:0,0 size:260.656x50
            Box (block-flow) offset:0,0 size:260.656x50 LayoutNGBlockFlow DIV id='child'
          Box (column block-flow) offset:261.656,0 size:260.656x50
*           Box (block-flow) offset:0,0 size:260.656x50 LayoutNGBlockFlow DIV id='child'
          Box (column block-flow) offset:523.313,0 size:260.656x50
            Box (block-flow) offset:0,0 size:260.656x50 LayoutNGBlockFlow DIV id='child'
)DUMP";
    EXPECT_EQ(expectation, dump);
  }
}

}  // namespace blink
