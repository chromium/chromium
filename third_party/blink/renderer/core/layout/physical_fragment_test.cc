// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/physical_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class PhysicalFragmentTest : public RenderingTest {
 public:
  String DumpAll(const PhysicalFragment* target = nullptr) const {
    return PhysicalFragment::DumpFragmentTree(
        *GetDocument().GetLayoutView(), PhysicalFragment::DumpAll, target);
  }
};

class StickyFragmentPropagationTest : public RenderingTest {
 public:
  struct ExpectedStickyAxes {
    PhysicalAxes consumed = kPhysicalAxesNone;
    PhysicalAxes pending = kPhysicalAxesNone;
  };

  wtf_size_t CountStickyDescendants(const char* scroller_id) {
    return GetLayoutBoxByElementId(scroller_id)
        ->GetPhysicalFragment(0)
        ->StickyDescendants()
        .size();
  }

  void ExpectStickyDescendant(const char* scroller_id,
                              const char* sticky_id,
                              ExpectedStickyAxes expected) {
    const auto* fragment =
        GetLayoutBoxByElementId(scroller_id)->GetPhysicalFragment(0);
    const auto* sticky_obj = GetLayoutObjectByElementId(sticky_id);

    for (const auto& item : fragment->StickyDescendants()) {
      if (item.GetIfConsumed() == sticky_obj ||
          item.GetIfPending() == sticky_obj) {
        EXPECT_EQ(expected.consumed, item.ConsumedAxes()) << sticky_id;
        EXPECT_EQ(expected.pending, item.PendingAxes()) << sticky_id;
        return;
      }
    }
    ADD_FAILURE() << sticky_id << " not found in " << scroller_id;
  }

  void BuildDOM() {
    SetBodyInnerHTML(R"HTML(
      <style>
        #outer { overflow: scroll; width: 100px; height: 100px; }
        #inner { overflow-y: scroll; overflow-x: clip; width: 100px; height: 100px; }
        #sticky-y { position: sticky; top: 0; height: 10px; }
        #sticky-x { position: sticky; left: 0; width: 10px; }
        #sticky-both { position: sticky; top: 0; left: 0; width: 10px; height: 10px; }
        .spacer { width: 200px; height: 200px; }
      </style>
      <div id="outer">
        <div id="inner">
          <div id="sticky-y"></div>
          <div id="sticky-x"></div>
          <div id="sticky-both"></div>
          <div class="spacer"></div>
        </div>
        <div class="spacer"></div>
      </div>
    )HTML");
  }
};

TEST_F(StickyFragmentPropagationTest, StickyAxisEnabled) {
  ScopedSingleAxisScrollContainersForTest feature(true);
  BuildDOM();

  EXPECT_EQ(3u, CountStickyDescendants("inner"));

  ExpectStickyDescendant(
      "inner", "sticky-y",
      {.consumed = kPhysicalAxesVertical, .pending = kPhysicalAxesNone});

  ExpectStickyDescendant(
      "inner", "sticky-x",
      {.consumed = kPhysicalAxesNone, .pending = kPhysicalAxesHorizontal});

  ExpectStickyDescendant(
      "inner", "sticky-both",
      {.consumed = kPhysicalAxesVertical, .pending = kPhysicalAxesHorizontal});

  EXPECT_EQ(2u, CountStickyDescendants("outer"));

  ExpectStickyDescendant(
      "outer", "sticky-x",
      {.consumed = kPhysicalAxesHorizontal, .pending = kPhysicalAxesNone});

  ExpectStickyDescendant(
      "outer", "sticky-both",
      {.consumed = kPhysicalAxesHorizontal, .pending = kPhysicalAxesNone});
}

TEST_F(StickyFragmentPropagationTest, StickyAxisDisabled) {
  ScopedSingleAxisScrollContainersForTest feature(false);
  BuildDOM();

  EXPECT_EQ(3u, CountStickyDescendants("inner"));

  ExpectStickyDescendant(
      "inner", "sticky-y",
      {.consumed = kPhysicalAxesBoth, .pending = kPhysicalAxesNone});

  ExpectStickyDescendant(
      "inner", "sticky-x",
      {.consumed = kPhysicalAxesBoth, .pending = kPhysicalAxesNone});

  ExpectStickyDescendant(
      "inner", "sticky-both",
      {.consumed = kPhysicalAxesBoth, .pending = kPhysicalAxesNone});

  EXPECT_EQ(0u, CountStickyDescendants("outer"));
}

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
    <div id="multicol" style="columns:3; gap:10px; width:320px;">
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
        Box (block-flow-root block-flow) offset:0,0 size:320x50 LayoutBlockFlow (multicol) DIV id='multicol'
          Box (column block-flow) offset:0,0 size:100x50 sequence:0 (seen all children) consumed:50px
            Box (block-flow) offset:0,0 size:100x50 LayoutBlockFlow DIV id='child' sequence:0 (seen all children) consumed:50px
          Box (column block-flow)(resumed) offset:110,0 size:100x50 sequence:1 (seen all children) consumed:100px
*           Box (block-flow)(resumed) offset:0,0 size:100x50 LayoutBlockFlow DIV id='child' sequence:1 (seen all children) consumed:100px
          Box (column block-flow)(resumed) offset:220,0 size:100x50
            Box (block-flow)(resumed) offset:0,0 size:100x50 LayoutBlockFlow DIV id='child'
)DUMP";
  EXPECT_EQ(expectation, dump);
}

TEST_F(PhysicalFragmentTest, DumpFragmentTreeWithRepeatedContent) {
  SetBodyInnerHTML(R"HTML(
<div style="columns:2; width:100px; height:100px; gap:0; column-fill:auto;">
  <div style="display:table;">
    <div style="display:table-header-group; break-inside:avoid;">
      <div style="columns:2; gap:0; column-fill:auto; height:20px;">
        <div style="height:40px;"></div>
      </div>
    </div>
    <div style="height:100px;"></div>
  </div>
</div>
  )HTML");

  String dump = DumpAll();
  String expectation = R"DUMP(.:: LayoutNG Physical Fragment Tree ::.
  Box (out-of-flow-positioned block-flow)(self paint) offset:unplaced size:800x600 LayoutView #document
    Box (block-flow-root block-flow)(self paint) offset:0,0 size:800x116 LayoutBlockFlow HTML
      Box (block-flow) offset:8,8 size:784x100 LayoutBlockFlow BODY
        Box (block-flow-root block-flow) offset:0,0 size:100x100 LayoutBlockFlow (multicol) DIV
          Box (column block-flow) offset:0,0 size:50x100 sequence:0 (seen all children) consumed:100px
            Box (block-flow-root) offset:0,0 size:0x100 LayoutTable DIV sequence:0 (seen all children) consumed:100px
              Box (block-flow-root) offset:0,0 size:0x20 LayoutTableSection DIV sequence:0 (repeated) consumed:0px
                Box (block-flow-root) offset:0,0 size:0x20 LayoutTableRow (anonymous) sequence:0 (repeated) consumed:0px
                  Box (block-flow-root block-flow) offset:0,0 size:0x20 LayoutTableCell (anonymous) sequence:0 (repeated) consumed:0px
                    Box (block-flow-root block-flow) offset:0,0 size:0x20 LayoutBlockFlow (multicol) DIV sequence:0 (repeated) consumed:0px
                      Box (column block-flow) offset:0,0 size:0x20 sequence:0 (seen all children) consumed:20px
                        Box (block-flow) offset:0,0 size:0x20 LayoutBlockFlow DIV sequence:0 (seen all children) consumed:20px
                      Box (column block-flow)(resumed) offset:0,0 size:0x20 sequence:1 (repeated) consumed:0px
                        Box (block-flow)(resumed) offset:0,0 size:0x20 LayoutBlockFlow DIV sequence:1 (repeated) consumed:0px
              Box (block-flow-root) offset:0,20 size:0x80 LayoutTableSection (anonymous) sequence:0 (seen all children) consumed:80px
                Box (block-flow-root) offset:0,0 size:0x80 LayoutTableRow (anonymous) sequence:0 (seen all children) consumed:80px
                  Box (block-flow-root block-flow) offset:0,0 size:0x80 LayoutTableCell (anonymous) sequence:0 (seen all children) consumed:80px
                    Box (block-flow) offset:0,0 size:0x80 LayoutBlockFlow DIV sequence:0 (seen all children) consumed:80px
          Box (column block-flow)(resumed) offset:50,0 size:50x100
            Box (block-flow-root)(resumed) offset:0,0 size:0x40 LayoutTable DIV
              Box (block-flow-root)(resumed) offset:0,0 size:0x20 LayoutTableSection DIV
                Box (block-flow-root)(resumed) offset:0,0 size:0x20 LayoutTableRow (anonymous)
                  Box (block-flow-root block-flow)(resumed) offset:0,0 size:0x20 LayoutTableCell (anonymous)
                    Box (block-flow-root block-flow)(resumed) offset:0,0 size:0x20 LayoutBlockFlow (multicol) DIV
                      Box (column block-flow) offset:0,0 size:0x20 sequence:2 consumed:20px
                        Box (block-flow)(resumed) offset:0,0 size:0x20 LayoutBlockFlow DIV sequence:2 consumed:20px
                      Box (column block-flow)(resumed) offset:0,0 size:0x20
                        Box (block-flow)(resumed) offset:0,0 size:0x20 LayoutBlockFlow DIV
              Box (block-flow-root)(resumed) offset:0,20 size:0x20 LayoutTableSection (anonymous)
                Box (block-flow-root)(resumed) offset:0,0 size:0x20 LayoutTableRow (anonymous)
                  Box (block-flow-root block-flow)(resumed) offset:0,0 size:0x20 LayoutTableCell (anonymous)
                    Box (block-flow)(resumed) offset:0,0 size:0x20 LayoutBlockFlow DIV
)DUMP";
  EXPECT_EQ(expectation, dump);
}

}  // namespace blink
