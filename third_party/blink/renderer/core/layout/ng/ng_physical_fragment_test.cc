// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class NGPhysicalFragmentTest : public NGLayoutTest {
 public:
  String DumpAll() const {
    return NGPhysicalFragment::DumpFragmentTree(*GetDocument().GetLayoutView(),
                                                NGPhysicalFragment::DumpAll);
  }
};

TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeBasic) {
  SetBodyInnerHTML(R"HTML(
    <div id="block"></div>
  )HTML");
  String dump = DumpAll();
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='block'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// LayoutView is the containing block of an absolutely positioned descendant.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithAbspos) {
  SetBodyInnerHTML(R"HTML(
    <div id="abs" style="position:absolute;"></div>
  )HTML");

  String dump = DumpAll();
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// An NG object is the containing block of an absolutely positioned descendant.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithAbsposInRelpos) {
  SetBodyInnerHTML(R"HTML(
    <div id="rel" style="position:relative;">
      <div id="abs" style="position:absolute; left:10px; top:20px;"></div>
    </div>
  )HTML");

  String dump = DumpAll();
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow)(self paint) offset:0,0 size:784x0 LayoutNGBlockFlow (relative positioned) DIV id='rel'
          Box (out-of-flow-positioned block-flow)(self paint) offset:10,20 size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// A legacy table is the containing block of an absolutely positioned
// descendant.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithTableWithAbspos) {
  if (RuntimeEnabledFeatures::LayoutNGTableEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <table style="position:relative;">
      <td>
        <div id="abs" style="position:absolute; left:10px; top:20px;"></div>
        <div id="inflow">
          <div id="inflowchild"></div>
        </div>
      </td>
    </table>
  )HTML");

  String dump = DumpAll();
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x22 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x6 LayoutNGBlockFlow BODY
        Box (block-flow-root legacy-layout-root)(self paint) offset:0,0 size:6x6 LayoutTable (relative positioned) TABLE
          (NG fragment root inside legacy subtree:)
            Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:0x0 LayoutNGBlockFlow (positioned) DIV id='abs'
          (NG fragment root inside legacy subtree:)
            Box (block-flow-root block-flow) offset:unplaced size:2x2 LayoutNGTableCell TD
              Box (block-flow) offset:1,1 size:0x0 LayoutNGBlockFlow DIV id='inflow'
                Box (block-flow) offset:0,0 size:0x0 LayoutNGBlockFlow DIV id='inflowchild'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// LayoutView is the containing block of an absolutely positioned legacy
// table. The table has no fragment, so it won't show up in the fragment dump.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithAbsposTable) {
  if (RuntimeEnabledFeatures::LayoutNGTableEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <div id="abs" style="display:table; position:absolute;"></div>
  )HTML");

  String dump = DumpAll();
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// LayoutView is the containing block of an absolutely positioned legacy table
// with a child. The table has no fragment, so it won't show up in the fragment
// dump, but its NG descendants will.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithAbsposTableWithChild) {
  if (RuntimeEnabledFeatures::LayoutNGTableEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <div id="abs" style="display:table; position:absolute;">
      <div id="child"></div>
    </div>
  )HTML");

  String dump = DumpAll();
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow) offset:unplaced size:0x0 LayoutNGTableCell (anonymous)
      Box (block-flow) offset:0,0 size:0x0 LayoutNGBlockFlow DIV id='child'
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x8 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
)DUMP";
  EXPECT_EQ(expectation, dump);
}

// A legacy grid with another legacy grid inside, and some NG objects, too.
TEST_F(NGPhysicalFragmentTest, DumpFragmentTreeWithGrid) {
  if (RuntimeEnabledFeatures::LayoutNGGridEnabled())
    return;
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
  String expectation =
      R"DUMP(.:: LayoutNG Physical Fragment Tree at legacy root LayoutView #document ::.
  (NG fragment root inside legacy subtree:)
    Box (block-flow-root block-flow)(self paint) offset:unplaced size:800x16 LayoutNGBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x0 LayoutNGBlockFlow BODY
        Box (block-flow-root legacy-layout-root) offset:0,0 size:784x0 LayoutGrid DIV id='outer-grid'
          (NG fragment root inside legacy subtree:)
            Box (block-flow-root block-flow) offset:unplaced size:784x0 LayoutNGBlockFlow DIV id='inner-grid-item'
              Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='foo'
          (NG fragment root inside legacy subtree:)
            Box (block-flow-root block-flow) offset:unplaced size:784x0 LayoutNGBlockFlow DIV id='block-container-item'
              Box (block-flow) offset:0,0 size:784x0 LayoutNGBlockFlow DIV id='bar'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // namespace blink
