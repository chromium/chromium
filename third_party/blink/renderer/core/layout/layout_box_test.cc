// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_box.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/test/stub_image.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class LayoutBoxTest : public RenderingTest {
 public:
  LayoutBoxTest() = default;

 protected:
  bool ForegroundIsKnownToBeOpaqueInRect(const LayoutBox& box,
                                         const PhysicalRect& rect) {
    return box.ForegroundIsKnownToBeOpaqueInRect(rect, 10);
  }
};

TEST_F(LayoutBoxTest, BackgroundIsKnownToBeObscured) {
  SetBodyInnerHTML(R"HTML(
    <style>.column { width: 295.4px; padding-left: 10.4px; }
    .white-background { background: red; position: relative; overflow:
    hidden; border-radius: 1px; }
    .black-background { height: 100px; background: black; color: white; }
    </style>
    <div class='column'> <div> <div id='target' class='white-background'>
    <div class='black-background'></div> </div> </div> </div>
  )HTML");
  const auto* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->BackgroundIsKnownToBeObscured());
}

TEST_F(LayoutBoxTest, BackgroundNotObscuredWithCssClippedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        position: relative;
        width: 200px;
        height: 200px;
        background-color: green;
      }
      #child {
        position: absolute;
        width: 100%;
        height: 100%;
        background-color: blue;
        /* clip the 200x200 box to a centered, 100x100 square. */
        clip: rect(50px, 150px, 150px, 50px);
      }
    </style>
    <div id="parent">
      <div id="child"></div>
    </div>
  )HTML");
  auto* child = GetLayoutBoxByElementId("child");
  EXPECT_FALSE(child->BackgroundIsKnownToBeObscured());

  auto* parent = GetLayoutBoxByElementId("parent");
  EXPECT_FALSE(parent->BackgroundIsKnownToBeObscured());
}

TEST_F(LayoutBoxTest, BackgroundNotObscuredWithCssClippedGrandChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #parent {
        position: relative;
        width: 200px;
        height: 200px;
        background-color: green;
      }
      #child {
        position: absolute;
        width: 100%;
        height: 100%;
        /* clip the 200x200 box to a centered, 100x100 square. */
        clip: rect(50px, 150px, 150px, 50px);
      }
      #grandchild {
        position: absolute;
        width: 100%;
        height: 100%;
        background-color: blue;
      }
    </style>
    <div id="parent">
      <div id="child">
        <div id="grandchild"></div>
      </div>
    </div>
  )HTML");
  auto* grandchild = GetLayoutBoxByElementId("grandchild");
  EXPECT_FALSE(grandchild->BackgroundIsKnownToBeObscured());

  auto* child = GetLayoutBoxByElementId("child");
  EXPECT_FALSE(child->BackgroundIsKnownToBeObscured());

  auto* parent = GetLayoutBoxByElementId("parent");
  EXPECT_FALSE(parent->BackgroundIsKnownToBeObscured());
}

TEST_F(LayoutBoxTest, ForegroundIsKnownToBeOpaqueInRect) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 30px; height: 30px">
      <div style="width: 10px; height: 10px; background: blue"></div>
      <div>
        <div style="width: 10px; height: 10px; opacity: 0.5; background: red">
        </div>
        <div style="width: 10px; height: 10px; background: green;
                    position: relative; left: 20px">
      </div>
    </div>
  )HTML");

  auto& target = *GetLayoutBoxByElementId("target");
  // Covered by the first child which is opaque.
  EXPECT_TRUE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 0, 10, 10)));
  // Covered by the first child of the second child is translucent.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 10, 10, 10)));
  // Though covered by the second child of the second child which is opaque,
  // we ignore child layers.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(20, 20, 10, 10)));
  // Not covered by any child.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 20, 10, 10)));
  // Partly covered by opaque children.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 0, 30, 30)));
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 0, 10, 30)));
}

TEST_F(LayoutBoxTest, ForegroundIsKnownToBeOpaqueInRectVerticalRL) {
  SetBodyInnerHTML(R"HTML(
    <div id="target"
         style="width: 30px; height: 30px; writing-mode: vertical-rl">
      <div style="width: 10px; height: 10px; background: blue"></div>
      <div>
        <div style="width: 10px; height: 10px; opacity: 0.5; background: red">
        </div>
        <div style="width: 10px; height: 10px; background: green;
                    position: relative; top: 20px">
      </div>
    </div>
  )HTML");

  auto& target = *GetLayoutBoxByElementId("target");
  // Covered by the first child which is opaque.
  EXPECT_TRUE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(20, 0, 10, 10)));
  // Covered by the first child of the second child is translucent.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(10, 0, 10, 10)));
  // Covered by the second child of the second child which is opaque.
  // However, the algorithm is optimized for horizontal-tb writing mode and has
  // false-negative (which is allowed) in this case.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 20, 10, 10)));
  // Not covered by any child.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 0, 10, 10)));
  // Partly covered by opaque children.
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(0, 0, 30, 30)));
  EXPECT_FALSE(
      ForegroundIsKnownToBeOpaqueInRect(target, PhysicalRect(20, 0, 30, 10)));
}

TEST_F(LayoutBoxTest, BackgroundRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { position: absolute; width: 100px; height: 100px;
            padding: 10px; border: 10px solid black; overflow: scroll; }
      #target1a, #target7a { border: 10px dashed black; }
      #target1, #target1a {
        background:
            url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) border-box,
            green content-box;
      }
      #target1b {
        background:
            url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) border-box;
      }
      #target2 {
        background:
            url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) content-box,
            green local border-box;
      }
      #target2b {
        background:
            url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) content-box;
      }
      #target3 {
        background:
            url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) content-box,
            rgba(0, 255, 0, 0.5) border-box;
      }
      #target4 {
        background-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg),
                          none;
        background-clip: content-box, border-box;
        background-blend-mode: normal, multiply;
        background-color: green;
      }
      #target5 { background: none border-box, green content-box;}
      #target6 { background: green content-box local; }
      #target7, #target7a {
        background-color: green;
        -webkit-background-clip: text;
      }
      #target8 { background: transparent; }
      #target9 { background: none; }
    </style>
    <div id='target1'></div>
    <div id='target1a'></div>
    <div id='target1b'></div>
    <div id='target2'></div>
    <div id='target2b'></div>
    <div id='target3'></div>
    <div id='target4'></div>
    <div id='target5'></div>
    <div id='target6'></div>
    <div id='target7'></div>
    <div id='target7a'></div>
    <div id='target8'></div>
    <div id='target9'></div>
  )HTML");

  // #target1's opaque background color only fills the content box but its
  // translucent image extends to the borders.
  LayoutBox* layout_box = GetLayoutBoxByElementId("target1");
  EXPECT_EQ(PhysicalRect(20, 20, 100, 100),
            layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect));
  // The opaque border covers the translucent image outside of the padding box.
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target1a is the same as #target1 except that the border is not opaque.
  layout_box = GetLayoutBoxByElementId("target1a");
  EXPECT_EQ(PhysicalRect(20, 20, 100, 100),
            layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(PhysicalRect(0, 0, 140, 140),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target1b is the same as #target1 except no background color.
  layout_box = GetLayoutBoxByElementId("target1b");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target2's background color is opaque but only fills the padding-box
  // because it has local attachment. This eclipses the content-box image.
  layout_box = GetLayoutBoxByElementId("target2");
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target2b is the same as #target2 except no background color.
  layout_box = GetLayoutBoxByElementId("target2b");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(PhysicalRect(20, 20, 100, 100),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target3's background color is not opaque.
  layout_box = GetLayoutBoxByElementId("target3");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target4's background color has a blend mode so it isn't opaque.
  layout_box = GetLayoutBoxByElementId("target4");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target5's solid background only covers the content-box but it has a "none"
  // background covering the border box.
  layout_box = GetLayoutBoxByElementId("target5");
  EXPECT_EQ(PhysicalRect(20, 20, 100, 100),
            layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(PhysicalRect(20, 20, 100, 100),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // Because it can scroll due to local attachment, the opaque local background
  // in #target6 is treated as padding box for the clip rect, but remains the
  // content box for the known opaque rect.
  layout_box = GetLayoutBoxByElementId("target6");
  EXPECT_EQ(PhysicalRect(20, 20, 100, 100),
            layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect));
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target7 has background-clip:text. The background may extend to the border
  // box.
  layout_box = GetLayoutBoxByElementId("target7");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(PhysicalRect(10, 10, 120, 120),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // #target7a is the same as #target1 except that the border is not opaque.
  layout_box = GetLayoutBoxByElementId("target7a");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_EQ(PhysicalRect(0, 0, 140, 140),
            layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent));

  // background: none
  layout_box = GetLayoutBoxByElementId("target8");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent).IsEmpty());

  // background: transparent
  layout_box = GetLayoutBoxByElementId("target9");
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundKnownOpaqueRect).IsEmpty());
  EXPECT_TRUE(
      layout_box->PhysicalBackgroundRect(kBackgroundPaintedExtent).IsEmpty());
}

TEST_F(LayoutBoxTest, LocationContainer) {
  SetBodyInnerHTML(R"HTML(
    <div id='div'>
      <b>Inline content<img id='img'></b>
    </div>
    <table id='table'>
      <tbody id='tbody'>
        <tr id='row'>
          <td id='cell' style='width: 100px; height: 80px'></td>
        </tr>
      </tbody>
    </table>
  )HTML");

  const LayoutBox* body = GetDocument().body()->GetLayoutBox();
  const LayoutBox* div = GetLayoutBoxByElementId("div");
  const LayoutBox* img = GetLayoutBoxByElementId("img");
  const LayoutBox* table = GetLayoutBoxByElementId("table");
  const LayoutBox* tbody = GetLayoutBoxByElementId("tbody");
  const LayoutBox* row = GetLayoutBoxByElementId("row");
  const LayoutBox* cell = GetLayoutBoxByElementId("cell");

  EXPECT_EQ(body, div->LocationContainer());
  EXPECT_EQ(div, img->LocationContainer());
  EXPECT_EQ(body, table->LocationContainer());
  EXPECT_EQ(table, tbody->LocationContainer());
  EXPECT_EQ(tbody, row->LocationContainer());
  EXPECT_EQ(row, cell->LocationContainer());
}

TEST_F(LayoutBoxTest, TopLeftLocationFlipped) {
  SetBodyInnerHTML(R"HTML(
    <div style='width: 600px; height: 200px; writing-mode: vertical-rl'>
      <div id='box1' style='width: 100px'></div>
      <div id='box2' style='width: 200px'></div>
    </div>
  )HTML");

  const LayoutBox* box1 = GetLayoutBoxByElementId("box1");
  EXPECT_EQ(PhysicalOffset(500, 0), box1->PhysicalLocation());

  const LayoutBox* box2 = GetLayoutBoxByElementId("box2");
  EXPECT_EQ(PhysicalOffset(300, 0), box2->PhysicalLocation());
}

TEST_F(LayoutBoxTest, TableRowCellTopLeftLocationFlipped) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <div style='writing-mode: vertical-rl'>
      <table style='border-spacing: 0'>
        <thead><tr><td style='width: 50px'></td></tr></thead>
        <tbody>
          <tr id='row1'>
            <td id='cell1' style='width: 100px; height: 80px'></td>
          </tr>
          <tr id='row2'>
            <td id='cell2' style='width: 300px; height: 80px'></td>
          </tr>
        </tbody>
      </table>
    </div>
  )HTML");

  // location and physicalLocation of a table row or a table cell should be
  // relative to the containing section.

  const LayoutBox* row1 = GetLayoutBoxByElementId("row1");
  EXPECT_EQ(PhysicalOffset(300, 0), row1->PhysicalLocation());

  const LayoutBox* cell1 = GetLayoutBoxByElementId("cell1");
  EXPECT_EQ(PhysicalOffset(0, 0), cell1->PhysicalLocation());

  const LayoutBox* row2 = GetLayoutBoxByElementId("row2");
  EXPECT_EQ(PhysicalOffset(0, 0), row2->PhysicalLocation());

  const LayoutBox* cell2 = GetLayoutBoxByElementId("cell2");
  EXPECT_EQ(PhysicalOffset(0, 0), cell2->PhysicalLocation());
}

TEST_F(LayoutBoxTest, LocationContainerOfSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style='writing-mode:vertical-rl' width='500' height='500'>
      <foreignObject x='44' y='77' width='100' height='80' id='foreign'>
        <div id='child' style='width: 33px; height: 55px'>
        </div>
      </foreignObject>
    </svg>
  )HTML");
  const LayoutBox* svg_root = GetLayoutBoxByElementId("svg");
  const LayoutBox* foreign = GetLayoutBoxByElementId("foreign");
  const LayoutBox* child = GetLayoutBoxByElementId("child");

  EXPECT_EQ(GetDocument().body()->GetLayoutObject(),
            svg_root->LocationContainer());

  // The foreign object's location is not affected by SVGRoot's writing-mode.
  EXPECT_FALSE(foreign->LocationContainer());
  EXPECT_EQ(PhysicalSize(100, 80), foreign->Size());
  EXPECT_EQ(PhysicalOffset(44, 77), foreign->PhysicalLocation());
  // The writing mode style should be still be inherited.
  EXPECT_TRUE(foreign->HasFlippedBlocksWritingMode());

  // The child of the foreign object is affected by writing-mode.
  EXPECT_EQ(foreign, child->LocationContainer());
  EXPECT_EQ(PhysicalSize(33, 55), child->Size());
  EXPECT_EQ(PhysicalOffset(67, 0), child->PhysicalLocation());
  EXPECT_TRUE(child->HasFlippedBlocksWritingMode());
}

TEST_F(LayoutBoxTest, ControlClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #target {
        position: relative;
        width: 100px; height: 50px;
      }
    </style>
    <input id='target' type='button' value='some text'/>
  )HTML");
  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->HasControlClip());
  EXPECT_TRUE(target->HasClipRelatedProperty());
  EXPECT_TRUE(target->ShouldClipOverflowAlongEitherAxis());
  EXPECT_EQ(PhysicalRect(2, 2, 96, 46), target->ClippingRect(PhysicalOffset()));
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithBlockChild) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 100px; height: 100px; baground: blue'>
      <div style='width: 300px; height: 300px; background: green'></div>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 300, 300), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithLegacyChild) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 100px; height: 100px; baground: blue'>
      <svg height='300' width='300' style='display: block'></svg>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 300, 300), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithMask) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='-webkit-mask-image: url(#a);
         width: 100px; height: 100px; baground: blue'>
      <div style='width: 300px; height: 300px; background: green'></div>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->HasMask());
  EXPECT_FALSE(target->IsScrollContainer());
  EXPECT_FALSE(target->ShouldClipOverflowAlongEitherAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithMaskAndOverflowHidden) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='-webkit-mask-image: url(#a); overflow: hidden;
         width: 100px; height: 100px; background: blue'>
      <div style='width: 300px; height: 300px; background: green'></div>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->HasMask());
  EXPECT_TRUE(target->IsScrollContainer());
  EXPECT_TRUE(target->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithMaskWithOutset) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='-webkit-mask-box-image-source: url(#a);
    -webkit-mask-box-image-outset: 10px 20px;
         width: 100px; height: 100px; background: blue'>
      <div style='width: 300px; height: 300px; background: green'></div>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->HasMask());
  EXPECT_FALSE(target->IsScrollContainer());
  EXPECT_FALSE(target->ShouldClipOverflowAlongEitherAxis());
  EXPECT_EQ(PhysicalRect(-20, -10, 140, 120), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(-20, -10, 140, 120), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithMaskWithOutsetAndOverflowHidden) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='-webkit-mask-box-image-source: url(#a);
    -webkit-mask-box-image-outset: 10px 20px; overflow: hidden;
         width: 100px; height: 100px; background: blue'>
      <div style='width: 300px; height: 300px; background: green'></div>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->HasMask());
  EXPECT_TRUE(target->IsScrollContainer());
  EXPECT_TRUE(target->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(-20, -10, 140, 120), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(-20, -10, 140, 120), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectOverflowHidden) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='overflow: hidden;
         width: 100px; height: 100px; background: blue'>
      <div style='width: 300px; height: 300px; background: green'></div>
    </div>
  )HTML");

  LayoutBox* target = GetLayoutBoxByElementId("target");
  EXPECT_TRUE(target->IsScrollContainer());
  EXPECT_TRUE(target->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), target->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectOverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .parent { width: 100px; height: 50px; }
      .child { width: 300px; height: 300px; }
    </style>
    <div id="clip" style="overflow: clip" class="parent">
      <div class="child"></div>
    </div>
    <div id="clip-x" style="overflow-x: clip" class="parent">
      <div class="child"></div>
    </div>
    <div id="clip-y" style="overflow-y: clip" class="parent">
      <div class="child"></div>
    </div>
  )HTML");

  LayoutBox* clip = GetLayoutBoxByElementId("clip");
  EXPECT_FALSE(clip->IsScrollContainer());
  EXPECT_TRUE(clip->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 50), clip->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 50), clip->VisualOverflowRect());

  LayoutBox* clip_x = GetLayoutBoxByElementId("clip-x");
  EXPECT_FALSE(clip_x->IsScrollContainer());
  EXPECT_EQ(kOverflowClipX, clip_x->GetOverflowClipAxes());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 50), clip_x->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 300), clip_x->VisualOverflowRect());

  LayoutBox* clip_y = GetLayoutBoxByElementId("clip-y");
  EXPECT_FALSE(clip_y->IsScrollContainer());
  EXPECT_EQ(kOverflowClipY, clip_y->GetOverflowClipAxes());
  EXPECT_EQ(PhysicalRect(0, 0, 100, 50), clip_y->SelfVisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 300, 50), clip_y->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, VisualOverflowRectWithOverflowClipMargin) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .parent { width: 100px; height: 50px; overflow: clip; }
      .parent2 { width: 100px; height: 50px; contain: paint; }
      .child { width: 110px; height: 55px; }
    </style>
    <div id="clip1" style="overflow-clip-margin: 4px" class="parent">
      <div class="child"></div>
    </div>
    <div id="clip2" style="overflow-clip-margin: 11px" class="parent">
      <div class="child"></div>
    </div>
    <div id="clip3" style="overflow-clip-margin: 11px" class="parent2">
      <div class="child"></div>
    </div>
  )HTML");

  LayoutBox* clip1 = GetLayoutBoxByElementId("clip1");
  EXPECT_FALSE(clip1->IsScrollContainer());
  EXPECT_TRUE(clip1->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 104, 54), clip1->VisualOverflowRect());

  LayoutBox* clip2 = GetLayoutBoxByElementId("clip2");
  EXPECT_FALSE(clip2->IsScrollContainer());
  EXPECT_TRUE(clip2->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 110, 55), clip2->VisualOverflowRect());

  LayoutBox* clip3 = GetLayoutBoxByElementId("clip3");
  EXPECT_FALSE(clip3->IsScrollContainer());
  EXPECT_TRUE(clip3->ShouldClipOverflowAlongBothAxis());
  EXPECT_EQ(PhysicalRect(0, 0, 110, 55), clip3->VisualOverflowRect());
}

// |InkOverflow| stopped storing visual overflow contained by |BorderBoxRect|
// because they are not useful, and they are inconsistent when fully contained
// and partially contained.
// TODO(crbug.com/1144203): Change this to "if (NG)" when NG always use
// fragment-based ink overflow. Then, remove this when legacy is gone.
#define EXPECT_CONTENTS_VISUAL_OVERFLOW(rect, layout_box)           \
  if (layout_box->CanUseFragmentsForVisualOverflow()) {             \
    EXPECT_EQ(UnionRect(rect, layout_box->PhysicalBorderBoxRect()), \
              layout_box->ContentsVisualOverflowRect());            \
  } else {                                                          \
    EXPECT_EQ(rect, layout_box->ContentsVisualOverflowRect());      \
  }

TEST_F(LayoutBoxTest, ContentsVisualOverflowPropagation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div id='a'>
      <div style='height: 50px'></div>
      <div id='b' style='writing-mode: vertical-rl; margin-left: 60px'>
        <div style='width: 30px'></div>
        <div id='c' style='margin-top: 40px'>
          <div style='width: 10px'></div>
          <div style='margin-top: 20px; margin-left: 10px'></div>
        </div>
        <div id='d' style='writing-mode: vertical-lr; margin-top: 40px'>
          <div style='width: 10px'></div>
          <div style='margin-top: 20px'></div>
        </div>
      </div>
    </div>
  )HTML");

  const int kCContentsLeft = -10;
  auto* c = GetLayoutBoxByElementId("c");
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), c->SelfVisualOverflowRect());
  EXPECT_CONTENTS_VISUAL_OVERFLOW(PhysicalRect(kCContentsLeft, 20, 100, 100),
                                  c);
  EXPECT_EQ(PhysicalRect(kCContentsLeft, 0, 110, 120), c->VisualOverflowRect());

  auto* d = GetLayoutBoxByElementId("d");
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), d->SelfVisualOverflowRect());
  EXPECT_CONTENTS_VISUAL_OVERFLOW(PhysicalRect(10, 20, 100, 100), d);
  EXPECT_EQ(PhysicalRect(0, 0, 110, 120), d->VisualOverflowRect());

  auto* b = GetLayoutBoxByElementId("b");
  const int kBContentsLeft = -130;
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), b->SelfVisualOverflowRect());
  // Union of VisualOverflowRectForPropagations offset by locations of c and d.
  EXPECT_CONTENTS_VISUAL_OVERFLOW(PhysicalRect(kBContentsLeft, 40, 200, 120),
                                  b);
  EXPECT_EQ(PhysicalRect(kBContentsLeft, 0, 230, 160), b->VisualOverflowRect());

  auto* a = GetLayoutBoxByElementId("a");
  EXPECT_EQ(PhysicalRect(0, 0, 100, 100), a->SelfVisualOverflowRect());
  EXPECT_CONTENTS_VISUAL_OVERFLOW(PhysicalRect(-70, 50, 230, 160), a);
  EXPECT_EQ(PhysicalRect(-70, 0, 230, 210), a->VisualOverflowRect());
}

TEST_F(LayoutBoxTest, HitTestOverflowClipMargin) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="width: 200px; height: 200px; overflow: clip;
                               overflow-clip-margin: 50px">
      <div id="child" style="width: 300px; height: 100px"></div>
    </div>
  )HTML");

  auto* container = GetElementById("container");
  auto* child = GetElementById("child");
  // In child overflowing container but within the overflow clip.
  EXPECT_EQ(child, HitTest(230, 50));
  // Outside of the overflow clip, would be in child without the clip.
  EXPECT_EQ(GetDocument().body(), HitTest(280, 50));
  // In container's border box rect, not in child.
  EXPECT_EQ(container, HitTest(100, 150));
  // In the bottom clip margin, but there is nothing.
  EXPECT_EQ(GetDocument().documentElement(), HitTest(100, 230));
}

TEST_F(LayoutBoxTest, HitTestContainPaint) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='width: 100px; height: 200px; contain: paint'>
      <div id='child' style='width: 300px; height: 400px;'></div>
    </div>
  )HTML");

  auto* child = GetElementById("child");
  EXPECT_EQ(GetDocument().documentElement(), HitTest(1, 1));
  EXPECT_EQ(child, HitTest(10, 10));
  EXPECT_EQ(GetDocument().FirstBodyElement(), HitTest(150, 10));
  EXPECT_EQ(GetDocument().documentElement(), HitTest(10, 250));
}

TEST_F(LayoutBoxTest, OverflowRectsContainPaint) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='width: 100px; height: 200px; contain: paint;
                               border: 10px solid blue'>
      <div id='child' style='width: 300px; height: 400px;'></div>
    </div>
  )HTML");

  auto* container = GetLayoutBoxByElementId("container");
  EXPECT_TRUE(container->ShouldClipOverflowAlongEitherAxis());
  EXPECT_EQ(PhysicalRect(10, 10, 300, 400),
            container->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 120, 220), container->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 120, 220), container->SelfVisualOverflowRect());
  EXPECT_CONTENTS_VISUAL_OVERFLOW(PhysicalRect(10, 10, 300, 400), container);
  EXPECT_EQ(PhysicalRect(10, 10, 100, 200),
            container->OverflowClipRect(PhysicalOffset()));
}

TEST_F(LayoutBoxTest, OverflowRectsOverflowHidden) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='width: 100px; height: 200px; overflow: hidden;
                               border: 10px solid blue'>
      <div id='child' style='width: 300px; height: 400px;'></div>
    </div>
  )HTML");

  auto* container = GetLayoutBoxByElementId("container");
  EXPECT_TRUE(container->ShouldClipOverflowAlongEitherAxis());
  EXPECT_EQ(PhysicalRect(10, 10, 300, 400),
            container->ScrollableOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 120, 220), container->VisualOverflowRect());
  EXPECT_EQ(PhysicalRect(0, 0, 120, 220), container->SelfVisualOverflowRect());
  EXPECT_CONTENTS_VISUAL_OVERFLOW(PhysicalRect(10, 10, 300, 400), container);
  EXPECT_EQ(PhysicalRect(10, 10, 100, 200),
            container->OverflowClipRect(PhysicalOffset()));
}

TEST_F(LayoutBoxTest, SetTextFieldIntrinsicInlineSize) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
   <style>
     input { font: 10px Ahem; }
     #a::-webkit-inner-spin-button{ width: 50%; appearance: none; }
     #b::-webkit-inner-spin-button{ width: 50px; appearance: none; }
     #c::-webkit-inner-spin-button{ width: 100%; appearance: none; }
   </style>
   <input id='a' type='number' min='100' max='100' step='1'/>
   <input id='b' type='number' min='100' max='100' step='1'/>
   <input id='c' type='number' min='100' max='100' step='1'/>
  )HTML");

  LayoutBox* a = GetLayoutBoxByElementId("a");
  EXPECT_EQ(LayoutUnit(60), a->DefaultIntrinsicContentInlineSize());

  LayoutBox* b = GetLayoutBoxByElementId("b");
  EXPECT_EQ(LayoutUnit(80), b->DefaultIntrinsicContentInlineSize());

  LayoutBox* c = GetLayoutBoxByElementId("c");
  EXPECT_EQ(LayoutUnit(30), c->DefaultIntrinsicContentInlineSize());
}

class AnimatedImage : public StubImage {
 public:
  bool MaybeAnimated() override { return true; }
};

TEST_F(LayoutBoxTest, DelayedInvalidation) {
  SetBodyInnerHTML("<img id='image' style='width: 100px; height: 100px;'/>");
  auto* obj = GetLayoutBoxByElementId("image");
  ASSERT_TRUE(obj);

  // Inject an animated image since deferred invalidations are only done for
  // animated images.
  auto* image =
      ImageResourceContent::CreateLoaded(base::AdoptRef(new AnimatedImage()));
  To<LayoutImage>(obj)->ImageResource()->SetImageResource(image);
  ASSERT_TRUE(To<LayoutImage>(obj)->CachedImage()->GetImage()->MaybeAnimated());

  obj->ClearPaintInvalidationFlags();
  EXPECT_FALSE(obj->ShouldDoFullPaintInvalidation());
  EXPECT_EQ(obj->PaintInvalidationReasonForPrePaint(),
            PaintInvalidationReason::kNone);
  EXPECT_FALSE(obj->ShouldDelayFullPaintInvalidation());

  // CanDeferInvalidation::kYes results in a deferred invalidation.
  obj->ImageChanged(image, ImageResourceObserver::CanDeferInvalidation::kYes);
  EXPECT_FALSE(obj->ShouldDoFullPaintInvalidation());
  EXPECT_EQ(obj->PaintInvalidationReasonForPrePaint(),
            PaintInvalidationReason::kImage);
  EXPECT_TRUE(obj->ShouldDelayFullPaintInvalidation());

  // CanDeferInvalidation::kNo results in a immediate invalidation.
  obj->ImageChanged(image, ImageResourceObserver::CanDeferInvalidation::kNo);
  EXPECT_TRUE(obj->ShouldDoFullPaintInvalidation());
  EXPECT_EQ(obj->PaintInvalidationReasonForPrePaint(),
            PaintInvalidationReason::kImage);
  EXPECT_FALSE(obj->ShouldDelayFullPaintInvalidation());
}

TEST_F(LayoutBoxTest, DelayedInvalidationLayoutViewScrolled) {
  SetHtmlInnerHTML(R"HTML(
    <body style="
      background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      background-size: cover;
    ">
      <div style="height: 20000px"></div>
    </body>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* layout_view = GetDocument().GetLayoutView();
  EXPECT_FALSE(layout_view->ShouldDelayFullPaintInvalidation());

  // The background-image will be painted by the LayoutView. Get a reference to
  // it from there.
  auto* background_image =
      layout_view->StyleRef().BackgroundLayers().GetImage();
  ASSERT_TRUE(background_image);
  auto* image_resource_content = background_image->CachedImage();
  ASSERT_TRUE(image_resource_content);
  ASSERT_TRUE(image_resource_content->GetImage()->MaybeAnimated());

  // Simulate an image change notification.
  static_cast<ImageObserver*>(image_resource_content)
      ->Changed(image_resource_content->GetImage());
  EXPECT_TRUE(layout_view->MayNeedPaintInvalidationAnimatedBackgroundImage());

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(layout_view->ShouldDelayFullPaintInvalidation());

  static_cast<ImageObserver*>(image_resource_content)
      ->Changed(image_resource_content->GetImage());
  EXPECT_TRUE(layout_view->MayNeedPaintInvalidationAnimatedBackgroundImage());

  // Scroll down at least by a viewport height.
  GetDocument().domWindow()->scrollBy(0, 10000);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(layout_view->ShouldDelayFullPaintInvalidation());
}

TEST_F(LayoutBoxTest, MarkerContainerScrollableOverflowRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
    </style>
    <div id='target' style='display: list-item;'>
      <div style='overflow: hidden; line-height:100px;'>hello</div>
    </div>
  )HTML");

  auto* marker_container =
      To<LayoutBox>(GetLayoutObjectByElementId("target")->SlowFirstChild());
  EXPECT_GE(marker_container->PhysicalLocation().top +
                marker_container->ScrollableOverflowRect().Bottom(),
            LayoutUnit(50));
}

static String CommonStyleForGeometryWithScrollbarTests() {
  return R"HTML(
    <style>
      ::-webkit-scrollbar { width: 15px; height: 16px; background: yellow; }
      .rtl { direction: rtl; }
      .htb { writing-mode: horizontal-tb; }
      .vlr { writing-mode: vertical-lr; }
      .vrl { writing-mode: vertical-rl; }
      .container {
        overflow: scroll;
        width: 400px;
        height: 300px;
        padding: 10px 20px 30px 40px;
        border-width: 20px 30px 40px 50px;
        border-style: solid;
      }
      .child {
        width: 50px;
        height: 80px;
        border: 40px solid blue;
        padding: 10px;
      }
    </style>
  )HTML";
}

TEST_F(LayoutBoxTest, LocationOfAbsoluteChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>
      .container { position: relative; }
      .child { position: absolute; top: 70px; left: 100px; }
    </style>
    <div class="container">
      <div id="normal" class="child"></div>
    </div>
    <div class="container vlr">
      <div id="vlr" class="child"></div>
    </div>
    <div class="container vrl">
      <div id="vrl" class="child"></div>
    </div>
    <div class="container rtl">
      <div id="rtl" class="child"></div>
    </div>
    <div class="container rtl vlr">
      <div id="rtl-vlr" class="child"></div>
    </div>
    <div class="container rtl vrl">
      <div id="rtl-vrl" class="child"></div>
    </div>
  )HTML");

  const auto* normal = GetLayoutBoxByElementId("normal");
  // In non-flipped writing mode, child's Location is the location of the
  // top-left corner of its border box relative the top-left corner of its
  // containing box's border box.
  // 150 = absolute_left (100) + container_border_left (50)
  // 90 = absolute_top (70) + container_border_top (20)
  EXPECT_EQ(PhysicalOffset(150, 90), normal->PhysicalLocation());

  // Same as "normal".
  const auto* vlr = GetLayoutBoxByElementId("vlr");
  EXPECT_EQ(PhysicalOffset(150, 90), vlr->PhysicalLocation());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  // The physical location is still about the top-left corners.
  EXPECT_EQ(PhysicalOffset(150, 90), vrl->PhysicalLocation());

  // In horizontal rtl mode, there is scrollbar on the left, so the child is
  // shifted to the right by the width of the scrollbar.
  const auto* rtl = GetLayoutBoxByElementId("rtl");
  EXPECT_EQ(PhysicalOffset(165, 90), rtl->PhysicalLocation());

  // Same as "vlr".
  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  EXPECT_EQ(PhysicalOffset(150, 90), rtl_vlr->PhysicalLocation());

  // Same as "vrl".
  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  EXPECT_EQ(PhysicalOffset(150, 90), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest,
       LocationOfAbsoluteChildWithContainerScrollbarsDifferentWritingModes) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>
      .container { position: relative; }
      .child { position: absolute; top: 70px; left: 100px; }
    </style>
    <div class="container">
      <div id="vlr-in-htb" class="child vlr"></div>
    </div>
    <div class="container">
      <div id="vrl-in-htb" class="child vrl"></div>
    </div>
    <div class="container vlr">
      <div id="htb-in-vlr" class="child htb"></div>
    </div>
    <div class="container vlr">
      <div id="vrl-in-vlr" class="child vrl"></div>
    </div>
    <div class="container vrl">
      <div id="htb-in-vrl" class="child htb"></div>
    </div>
    <div class="container vrl">
      <div id="vlr-in-vrl" class="child vlr"></div>
    </div>
  )HTML");

  // The following expected values are just what the current system produces,
  // and we haven't fully verified their correctness.

  const auto* vlr_in_htb = GetLayoutBoxByElementId("vlr-in-htb");
  EXPECT_EQ(PhysicalOffset(150, 90), vlr_in_htb->PhysicalLocation());

  const auto* vrl_in_htb = GetLayoutBoxByElementId("vrl-in-htb");
  EXPECT_EQ(PhysicalOffset(150, 90), vrl_in_htb->PhysicalLocation());

  const auto* htb_in_vlr = GetLayoutBoxByElementId("htb-in-vlr");
  EXPECT_EQ(PhysicalOffset(150, 90), htb_in_vlr->PhysicalLocation());

  const auto* vrl_in_vlr = GetLayoutBoxByElementId("vrl-in-vlr");
  EXPECT_EQ(PhysicalOffset(150, 90), vrl_in_vlr->PhysicalLocation());

  const auto* htb_in_vrl = GetLayoutBoxByElementId("htb-in-vrl");
  EXPECT_EQ(PhysicalOffset(150, 90), htb_in_vrl->PhysicalLocation());

  const auto* vlr_in_vrl = GetLayoutBoxByElementId("vlr-in-vrl");
  EXPECT_EQ(PhysicalOffset(150, 90), vlr_in_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest,
       LocationOfAbsoluteAutoTopLeftChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>
      .container { position: relative; }
      .child { position: absolute; }
    </style>
    <div class="container">
      <div id="normal" class="child"></div>
    </div>
    <div class="container vlr">
      <div id="vlr" class="child"></div>
    </div>
    <div class="container vrl">
      <div id="vrl" class="child"></div>
    </div>
    <div class="container rtl">
      <div id="rtl" class="child"></div>
    </div>
    <div class="container rtl vlr">
      <div id="rtl-vlr" class="child"></div>
    </div>
    <div class="container rtl vrl">
      <div id="rtl-vrl" class="child"></div>
    </div>
  )HTML");

  const auto* normal = GetLayoutBoxByElementId("normal");
  // In non-flipped writing mode, child's Location is the location of the
  // top-left corner of its border box relative the top-left corner of its
  // containing box's border box.
  // 90 = container_border_left (50) + container_padding_left (40)
  // 30 = container_border_top (20) + container_padding_top (10)
  EXPECT_EQ(PhysicalOffset(90, 30), normal->PhysicalLocation());

  // Same as "normal".
  const auto* vlr = GetLayoutBoxByElementId("vlr");
  EXPECT_EQ(PhysicalOffset(90, 30), vlr->PhysicalLocation());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  // The physical location is still about the top-left corners.
  // 65 = container_border_right (30) + container_padding_right (20) +
  //      vertical_scrollbar_width (15)
  // 325 = total_container_width (540) - child_x (65) - total_child_width (150)
  EXPECT_EQ(PhysicalOffset(325, 30), vrl->PhysicalLocation());

  const auto* rtl = GetLayoutBoxByElementId("rtl");
  // 340 = total_container_width (540) - container_border_right (30) -
  //       container_padding_right (20) - total_child_width (150)
  EXPECT_EQ(PhysicalOffset(340, 30), rtl->PhysicalLocation());

  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  // 90 is the same as "vlr".
  // 134 = total_container_height (400) - container_border_bottom (40) -
  //       container_padding_bottom (30) - horizontal_scrollbar_height (16) -
  //       total_child_height (150)
  EXPECT_EQ(PhysicalOffset(90, 134), rtl_vlr->PhysicalLocation());

  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  // Horizontal is the same as "vrl".
  // Vertical is the same as "rtl_vlr".
  EXPECT_EQ(PhysicalOffset(325, 134), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest,
       LocationOfAbsoluteAutoTopLeftGrandChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>
      .container { position: relative; }
      .intermediate { width: 200%; height: 200%; }
      .child { position: absolute; }
    </style>
    <div class="container">
      <div class="intermediate">
        <div id="normal" class="child"></div>
      </div>
    </div>
    <div class="container vlr">
      <div class="intermediate">
        <div id="vlr" class="child"></div>
      </div>
    </div>
    <div class="container vrl">
      <div class="intermediate">
        <div id="vrl" class="child"></div>
      </div>
    </div>
    <div class="container rtl">
      <div class="intermediate">
        <div id="rtl" class="child"></div>
      </div>
    </div>
    <div class="container rtl vlr">
      <div class="intermediate">
        <div id="rtl-vlr" class="child"></div>
      </div>
    </div>
    <div class="container rtl vrl">
      <div class="intermediate">
        <div id="rtl-vrl" class="child"></div>
      </div>
    </div>
  )HTML");

  // All locations are the same as
  // LocationOfAbsoluteAutoTopLeftChildWithContainerScrollbars.

  const auto* normal = GetLayoutBoxByElementId("normal");
  EXPECT_EQ(PhysicalOffset(90, 30), normal->PhysicalLocation());

  const auto* vlr = GetLayoutBoxByElementId("vlr");
  EXPECT_EQ(PhysicalOffset(90, 30), vlr->PhysicalLocation());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  EXPECT_EQ(PhysicalOffset(325, 30), vrl->PhysicalLocation());

  const auto* rtl = GetLayoutBoxByElementId("rtl");
  EXPECT_EQ(PhysicalOffset(340, 30), rtl->PhysicalLocation());

  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  EXPECT_EQ(PhysicalOffset(90, 134), rtl_vlr->PhysicalLocation());

  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  EXPECT_EQ(PhysicalOffset(325, 134), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest, LocationOfInFlowChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>.offset { width: 100px; height: 70px; }</style>
    <div class="container">
      <div class="offset"></div>
      <div id="normal" class="child"></div>
    </div>
    <div class="container vlr">
      <div class="offset"></div>
      <div id="vlr" class="child"></div>
    </div>
    <div class="container vrl">
      <div class="offset"></div>
      <div id="vrl" class="child"></div>
    </div>
    <div class="container rtl">
      <div class="offset"></div>
      <div id="rtl" class="child"></div>
    </div>
    <div class="container rtl vlr">
      <div class="offset"></div>
      <div id="rtl-vlr" class="child"></div>
    </div>
    <div class="container rtl vrl">
      <div class="offset"></div>
      <div id="rtl-vrl" class="child"></div>
    </div>
  )HTML");

  const auto* normal = GetLayoutBoxByElementId("normal");
  // In non-flipped writing mode, child's Location is the location of the
  // top-left corner of its border box relative the top-left corner of its
  // containing box's border box.
  // 90 = container_border_left (50) + container_padding_left (40)
  // 100 = container_border_top (20) + container_padding_top (10) +
  //      offset_height (70)
  EXPECT_EQ(PhysicalOffset(90, 100), normal->PhysicalLocation());

  // 190 = container_border_left (50) + container_padding_left (40) +
  //       offset_width (100)
  // 30 = container_border_top (20) + container_padding_top (10)
  const auto* vlr = GetLayoutBoxByElementId("vlr");
  EXPECT_EQ(PhysicalOffset(190, 30), vlr->PhysicalLocation());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  // The physical location is still about the top-left corners.
  // 225 = total_container_width (540) - total_child_width (150) - 165
  // 30 = container_border_top (20) + container_padding_left (10)
  EXPECT_EQ(PhysicalOffset(225, 30), vrl->PhysicalLocation());

  const auto* rtl = GetLayoutBoxByElementId("rtl");
  // 340 = total_container_width (540) - total_child_width (150) -
  //       container_border_right (30) - contaienr_padding_right (20)
  // 100 is the same as "normal"
  EXPECT_EQ(PhysicalOffset(340, 100), rtl->PhysicalLocation());

  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  // 190 is the same as "normal"
  // 134 = total_container_height (400) - total_child_width (180) -
  //       horizontal_scrollber_height (16) -
  //       container_border_bottom (40) - contaienr_padding_bottom (30)
  EXPECT_EQ(PhysicalOffset(190, 134), rtl_vlr->PhysicalLocation());

  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  // Horizontal is the same as "vrl"
  // Vertical is the same as "rtl_vlr"
  EXPECT_EQ(PhysicalOffset(225, 134), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest, LocationOfRelativeChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>
      .offset { width: 100px; height: 70px; }
      .child { position: relative; top: 77px; left: 88px; }
    </style>
    <div class="container">
      <div class="offset"></div>
      <div id="normal" class="child"></div>
    </div>
    <div class="container vlr">
      <div class="offset"></div>
      <div id="vlr" class="child"></div>
    </div>
    <div class="container vrl">
      <div class="offset"></div>
      <div id="vrl" class="child"></div>
    </div>
    <div class="container rtl">
      <div class="offset"></div>
      <div id="rtl" class="child"></div>
    </div>
    <div class="container rtl vlr">
      <div class="offset"></div>
      <div id="rtl-vlr" class="child"></div>
    </div>
    <div class="container rtl vrl">
      <div class="offset"></div>
      <div id="rtl-vrl" class="child"></div>
    </div>
  )HTML");

  // All locations are the same as LocationOfInFlowChildWithContainerScrollbars
  // because relative offset doesn't contribute to box location.

  const auto* normal = GetLayoutBoxByElementId("normal");
  const auto* vlr = GetLayoutBoxByElementId("vlr");
  const auto* vrl = GetLayoutBoxByElementId("vrl");
  const auto* rtl = GetLayoutBoxByElementId("rtl");
  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");

  EXPECT_EQ(PhysicalOffset(178, 177), normal->PhysicalLocation());

  EXPECT_EQ(PhysicalOffset(278, 107), vlr->PhysicalLocation());

  EXPECT_EQ(PhysicalOffset(313, 107), vrl->PhysicalLocation());

  EXPECT_EQ(PhysicalOffset(428, 177), rtl->PhysicalLocation());

  EXPECT_EQ(PhysicalOffset(278, 211), rtl_vlr->PhysicalLocation());

  EXPECT_EQ(PhysicalOffset(313, 211), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest, LocationOfFloatLeftChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>.child { float: left; }</style>
    <div class="container">
      <div id="normal" class="child"></div>
    </div>
    <div class="container vlr">
      <div id="vlr" class="child"></div>
    </div>
    <div class="container vrl">
      <div id="vrl" class="child"></div>
    </div>
    <div class="container rtl">
      <div id="rtl" class="child"></div>
    </div>
    <div class="container rtl vlr">
      <div id="rtl-vlr" class="child"></div>
    </div>
    <div class="container rtl vrl">
      <div id="rtl-vrl" class="child"></div>
    </div>
  )HTML");

  const auto* normal = GetLayoutBoxByElementId("normal");
  // In non-flipped writing mode, child's Location is the location of the
  // top-left corner of its border box relative the top-left corner of its
  // containing box's border box.
  // 90 = container_border_left (50) + container_padding_left (40)
  // 30 = container_border_top (20) + container_padding_top (10)
  EXPECT_EQ(PhysicalOffset(90, 30), normal->PhysicalLocation());

  // Same as "normal".
  const auto* vlr = GetLayoutBoxByElementId("vlr");
  EXPECT_EQ(PhysicalOffset(90, 30), vlr->PhysicalLocation());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  // The physical location is still about the top-left corners.
  // 65 = container_border_right (30) + container_padding_right (20) +
  //      vertical_scrollbar_width (15)
  // 325 = total_container_width (540) - child_x (65) - total_child_width (150)
  EXPECT_EQ(PhysicalOffset(325, 30), vrl->PhysicalLocation());

  // In horizontal rtl mode, there is scrollbar on the left, so the child is
  // shifted to the right by the width of the scrollbar.
  const auto* rtl = GetLayoutBoxByElementId("rtl");
  EXPECT_EQ(PhysicalOffset(105, 30), rtl->PhysicalLocation());

  // Same as "vlr".
  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  EXPECT_EQ(PhysicalOffset(90, 30), rtl_vlr->PhysicalLocation());

  // Same as "vrl".
  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  EXPECT_EQ(PhysicalOffset(325, 30), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest, LocationOfFloatRightChildWithContainerScrollbars) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>.child { float: right; }</style>
    <div class="container">
      <div id="normal" class="child"></div>
    </div>
    <div class="container vlr">
      <div id="vlr" class="child"></div>
    </div>
    <div class="container vrl">
      <div id="vrl" class="child"></div>
    </div>
    <div class="container rtl">
      <div id="rtl" class="child"></div>
    </div>
    <div class="container rtl vlr">
      <div id="rtl-vlr" class="child"></div>
    </div>
    <div class="container rtl vrl">
      <div id="rtl-vrl" class="child"></div>
    </div>
  )HTML");

  const auto* normal = GetLayoutBoxByElementId("normal");
  // In non-flipped writing mode, child's Location is the location of the
  // top-left corner of its border box relative the top-left corner of its
  // containing box's border box.
  // 325 = total_container_width (540) - child_x (65) - total_child_width (150)
  // 30 = container_border_top (20) + container_padding_top (10)
  EXPECT_EQ(PhysicalOffset(325, 30), normal->PhysicalLocation());

  // Same as "normal".
  const auto* vlr = GetLayoutBoxByElementId("vlr");
  // 90 = container_border_left (50) + container_padding_left (40)
  // 134 = total_container_height (400) - total_child_width (180) -
  //       horizontal_scrollber_height (16) -
  //       container_border_bottom (40) - contaienr_padding_bottom (30)
  EXPECT_EQ(PhysicalOffset(90, 134), vlr->PhysicalLocation());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  // The physical location is still about the top-left corners.
  // 65 = container_border_right (30) + container_padding_right (20) +
  //      vertical_scrollbar_width (15)
  // 325 = total_container_width (540) - child_x (65) - total_child_width (150)
  EXPECT_EQ(PhysicalOffset(325, 134), vrl->PhysicalLocation());

  // In horizontal rtl mode, there is scrollbar on the left, so the child is
  // shifted to the right by the width of the scrollbar.
  const auto* rtl = GetLayoutBoxByElementId("rtl");
  EXPECT_EQ(PhysicalOffset(340, 30), rtl->PhysicalLocation());

  // Same as "vlr".
  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  EXPECT_EQ(PhysicalOffset(90, 134), rtl_vlr->PhysicalLocation());

  // Same as "vrl".
  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  EXPECT_EQ(PhysicalOffset(325, 134), rtl_vrl->PhysicalLocation());
}

TEST_F(LayoutBoxTest, GeometriesWithScrollbarsNonScrollable) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <div id="normal" class="container">
      <div class="child"></div>
    </div>
    <div id="vlr" class="container vlr">
      <div class="child"></div>
    </div>
    <div id="vrl" class="container vrl">
      <div class="child"></div>
    </div>
    <div id="rtl" class="container rtl">
      <div class="child"></div>
    </div>
    <div id="rtl-vlr" class="container rtl vlr">
      <div class="child"></div>
    </div>
    <div id="rtl-vrl" class="container rtl vrl">
      <div class="child"></div>
    </div>
  )HTML");

#define EXPECT_ZERO_SCROLL(box)                                            \
  do {                                                                     \
    EXPECT_EQ(PhysicalOffset(), box->ScrolledContentOffset());             \
    const auto* scrollable_area = box->GetScrollableArea();                \
    EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());        \
    EXPECT_EQ(gfx::Point(), scrollable_area->ScrollOrigin());              \
    EXPECT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());           \
    EXPECT_EQ(gfx::Vector2d(), scrollable_area->MaximumScrollOffsetInt()); \
    EXPECT_EQ(gfx::Vector2d(), scrollable_area->MinimumScrollOffsetInt()); \
  } while (false)

  const auto* normal = GetLayoutBoxByElementId("normal");
  EXPECT_ZERO_SCROLL(normal);
  EXPECT_EQ(gfx::Vector2d(), normal->OriginAdjustmentForScrollbars());
  // 540 = border_left + padding_left + width + padding_right + border_right
  // 400 = border_top + padding_top + height + padding_bottom + border_bottom
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), normal->PhysicalBorderBoxRect());
  // 50 = border_left, 20 = border_top
  // 445 = padding_left + (width - scrollbar_width) + padding_right
  // 324 = padding_top + (height - scrollbar_height) + padding_bottom
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), normal->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), normal->PhysicalPaddingBoxRect());
  // 90 = border_left + padding_left, 30 = border_top + padding_top
  // 385 = width - scrollbar_width, 284 = height - scrollbar_height
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), normal->PhysicalContentBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), normal->ScrollableOverflowRect());

  const auto* vlr = GetLayoutBoxByElementId("vlr");
  // Same as "normal"
  EXPECT_ZERO_SCROLL(vlr);
  EXPECT_EQ(gfx::Vector2d(), vlr->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), vlr->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vlr->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vlr->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), vlr->PhysicalContentBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vlr->ScrollableOverflowRect());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  // Same as "normal".
  EXPECT_ZERO_SCROLL(vrl);
  EXPECT_EQ(gfx::Vector2d(), vrl->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), vrl->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vrl->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vrl->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), vrl->PhysicalContentBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vrl->ScrollableOverflowRect());

  const auto* rtl = GetLayoutBoxByElementId("rtl");
  EXPECT_ZERO_SCROLL(rtl);
  // The scrollbar is on the left, shifting padding box and content box to the
  // right by 15px.
  EXPECT_EQ(gfx::Vector2d(15, 0), rtl->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), rtl->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(65, 20, 445, 324), rtl->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(65, 20, 445, 324), rtl->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(105, 30, 385, 284), rtl->PhysicalContentBoxRect());
  EXPECT_EQ(PhysicalRect(65, 20, 445, 324), rtl->ScrollableOverflowRect());

  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  // Same as "vlr".
  EXPECT_ZERO_SCROLL(rtl_vlr);
  EXPECT_EQ(gfx::Vector2d(), rtl_vlr->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), rtl_vlr->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vlr->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vlr->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), rtl_vlr->PhysicalContentBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vlr->ScrollableOverflowRect());

  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  // Same as "vrl".
  EXPECT_ZERO_SCROLL(rtl_vrl);
  EXPECT_EQ(gfx::Vector2d(), rtl_vrl->OriginAdjustmentForScrollbars());
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), rtl_vrl->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vrl->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vrl->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), rtl_vrl->PhysicalContentBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vrl->ScrollableOverflowRect());
}

TEST_F(LayoutBoxTest, GeometriesWithScrollbarsScrollable) {
  SetBodyInnerHTML(CommonStyleForGeometryWithScrollbarTests() + R"HTML(
    <style>
      .child { width: 2000px; height: 1000px; box-sizing: border-box;}
    </style>
    <div id="normal" class="container">
      <div class="child"></div>
    </div>
    <div id="vlr" class="container vlr">
      <div class="child"></div>
    </div>
    <div id="vrl" class="container vrl">
      <div class="child"></div>
    </div>
    <div id="rtl" class="container rtl">
      <div class="child"></div>
    </div>
    <div id="rtl-vlr" class="container rtl vlr">
      <div class="child"></div>
    </div>
    <div id="rtl-vrl" class="container rtl vrl">
      <div class="child"></div>
    </div>
  )HTML");

  const auto* normal = GetLayoutBoxByElementId("normal");
  const auto* scrollable_area = normal->GetScrollableArea();
  EXPECT_EQ(PhysicalOffset(), normal->ScrolledContentOffset());
  EXPECT_EQ(gfx::Vector2d(), normal->OriginAdjustmentForScrollbars());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());
  EXPECT_EQ(PhysicalRect(50, 20, 2060, 1040), normal->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 716),
            scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());
  // These are the same as in the NonScrollable test.
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), normal->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), normal->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), normal->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), normal->PhysicalContentBoxRect());

  const auto* vlr = GetLayoutBoxByElementId("vlr");
  scrollable_area = vlr->GetScrollableArea();
  EXPECT_EQ(PhysicalOffset(), vlr->ScrolledContentOffset());
  EXPECT_EQ(gfx::Vector2d(), vlr->OriginAdjustmentForScrollbars());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());
  EXPECT_EQ(PhysicalRect(50, 20, 2060, 1040), vlr->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 716),
            scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(), scrollable_area->ScrollPosition());
  // These are the same as in the NonScrollable test.
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), vlr->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vlr->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vlr->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), vlr->PhysicalContentBoxRect());

  const auto* vrl = GetLayoutBoxByElementId("vrl");
  scrollable_area = vrl->GetScrollableArea();
  EXPECT_EQ(PhysicalOffset(), vrl->ScrolledContentOffset());
  EXPECT_EQ(gfx::Vector2d(), vrl->OriginAdjustmentForScrollbars());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());
  // Same as "vlr" except for flipping.
  EXPECT_EQ(PhysicalRect(-1565, 20, 2060, 1040), vrl->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 0), scrollable_area->ScrollPosition());
  // These are the same as in the NonScrollable test.
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), vrl->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vrl->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), vrl->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), vrl->PhysicalContentBoxRect());

  const auto* rtl = GetLayoutBoxByElementId("rtl");
  scrollable_area = rtl->GetScrollableArea();
  EXPECT_EQ(PhysicalOffset(), rtl->ScrolledContentOffset());
  EXPECT_EQ(gfx::Vector2d(15, 0), rtl->OriginAdjustmentForScrollbars());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());
  EXPECT_EQ(PhysicalRect(-1550, 20, 2060, 1040), rtl->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(0, 716), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, 0), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 0), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 0), scrollable_area->ScrollPosition());
  // These are the same as in the NonScrollable test.
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), rtl->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(65, 20, 445, 324), rtl->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(65, 20, 445, 324), rtl->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(105, 30, 385, 284), rtl->PhysicalContentBoxRect());

  const auto* rtl_vlr = GetLayoutBoxByElementId("rtl-vlr");
  scrollable_area = rtl_vlr->GetScrollableArea();
  EXPECT_EQ(PhysicalOffset(), rtl_vlr->ScrolledContentOffset());
  EXPECT_EQ(gfx::Vector2d(), rtl_vlr->OriginAdjustmentForScrollbars());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());
  EXPECT_EQ(PhysicalRect(50, -696, 2060, 1040),
            rtl_vlr->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(1615, 0), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(0, -716), scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(0, 716), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(0, 716), scrollable_area->ScrollPosition());
  // These are the same as in the NonScrollable test.
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), rtl_vlr->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vlr->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vlr->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), rtl_vlr->PhysicalContentBoxRect());

  const auto* rtl_vrl = GetLayoutBoxByElementId("rtl-vrl");
  scrollable_area = rtl_vrl->GetScrollableArea();
  EXPECT_EQ(PhysicalOffset(), rtl_vrl->ScrolledContentOffset());
  EXPECT_EQ(gfx::Vector2d(), rtl_vrl->OriginAdjustmentForScrollbars());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->ScrollOffsetInt());
  // Same as "vlr" except for flipping.
  EXPECT_EQ(PhysicalRect(-1565, -696, 2060, 1040),
            rtl_vrl->ScrollableOverflowRect());
  EXPECT_EQ(gfx::Vector2d(), scrollable_area->MaximumScrollOffsetInt());
  EXPECT_EQ(gfx::Vector2d(-1615, -716),
            scrollable_area->MinimumScrollOffsetInt());
  EXPECT_EQ(gfx::Point(1615, 716), scrollable_area->ScrollOrigin());
  EXPECT_EQ(gfx::PointF(1615, 716), scrollable_area->ScrollPosition());
  EXPECT_EQ(gfx::Vector2d(), rtl_vrl->OriginAdjustmentForScrollbars());
  // These are the same as in the NonScrollable test.
  EXPECT_EQ(PhysicalRect(0, 0, 540, 400), rtl_vrl->PhysicalBorderBoxRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vrl->NoOverflowRect());
  EXPECT_EQ(PhysicalRect(50, 20, 445, 324), rtl_vrl->PhysicalPaddingBoxRect());
  EXPECT_EQ(PhysicalRect(90, 30, 385, 284), rtl_vrl->PhysicalContentBoxRect());
}

TEST_F(LayoutBoxTest,
       ThickScrollbarSubpixelSizeMarginNoDirtyLayoutAfterLayout) {
  // |target| creates horizontal scrollbar during layout because the contents
  // overflow horizontally, which causes vertical overflow because the
  // horizontal scrollbar reduces available height. For now we suppress
  // creation of the vertical scrollbar because otherwise we would need another
  // layout. The subpixel margin and size cause change of pixel snapped border
  // size after layout which requires repositioning of the overflow controls.
  // This test ensures there is no left-over dirty layout.
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar {
        width: 100px;
        height: 100px;
        background: blue;
      }
    </style>
    <div id="target"
         style="width: 150.3px; height: 150.3px; margin: 10.4px;
                font-size: 30px; overflow: auto">
      <div style="width: 200px; height: 80px"></div>
    </div>
  )HTML");

  DCHECK(!GetLayoutObjectByElementId("target")->NeedsLayout());
}

// crbug.com/1108270
TEST_F(LayoutBoxTest, MenuListIntrinsicBlockSize) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .hidden { content-visibility: hidden; }
    </style>
    <select id=container class=hidden>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason ::kTest);
  // The test passes if no crash.
}

TEST_F(LayoutBoxTest, HasReflection) {
  SetBodyInnerHTML(R"HTML(
    <style>* { -webkit-box-reflect: above; }</style>
    <table id="table">
      <colgroup id="colgroup">
        <col id="col">
      </colgroup>
      <tr id="tr"><td id="td">TD</td></tr>
    </table>
    <svg id="svg">
      <text id="svg-text">SVG text</text>
    </svg>
  )HTML");

  auto check_has_layer_and_reflection = [&](const char* element_id,
                                            bool expected) {
    auto* object = GetLayoutObjectByElementId(element_id);
    EXPECT_EQ(expected, object->HasLayer()) << element_id;
    EXPECT_EQ(expected, object->HasReflection()) << element_id;
  };
  check_has_layer_and_reflection("table", true);
  check_has_layer_and_reflection("tr", true);
  check_has_layer_and_reflection("colgroup", false);
  check_has_layer_and_reflection("col", false);
  check_has_layer_and_reflection("td", true);
  check_has_layer_and_reflection("svg", true);
  check_has_layer_and_reflection("svg-text", false);
}

TEST_F(LayoutBoxTest, PhysicalVisualOverflowRectIncludingFilters) {
  SetBodyInnerHTML(R"HTML(
    <div style="zoom: 2">
      <div id="target" style="filter: blur(2px); width: 100px; height: 100px">
        <!-- An overflowing self-painting child -->
        <div style="position: relative; height: 200px"></div>
      </div>
    </div>
  )HTML");

  // 12: blur(2) * blur-extent-ratio(3) * zoom(2)
  EXPECT_EQ(
      PhysicalRect(-12, -12, 224, 424),
      GetLayoutBoxByElementId("target")->VisualOverflowRectIncludingFilters());
}

TEST_F(LayoutBoxTest, SetNeedsOverflowRecalcLayoutBox) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .transform { transform: translateX(10px); }
    </style>
    <img id="img">
  )HTML");
  Element* element = GetElementById("img");
  LayoutObject* target = element->GetLayoutObject();
  EXPECT_FALSE(target->SelfNeedsScrollableOverflowRecalc());

  element->classList().Add(AtomicString("transform"));
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(target->PaintingLayer()->NeedsVisualOverflowRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->SelfNeedsScrollableOverflowRecalc());

  element->classList().Remove(AtomicString("transform"));
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(target->PaintingLayer()->NeedsVisualOverflowRecalc());
}

TEST_F(LayoutBoxTest, SetNeedsOverflowRecalcFlexBox) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .transform { transform: translateX(10px); }
    </style>
    <div id="flex" style="display: flex"></div>
  )HTML");
  Element* element = GetElementById("flex");
  LayoutObject* target = element->GetLayoutObject();
  EXPECT_FALSE(target->SelfNeedsScrollableOverflowRecalc());

  element->classList().Add(AtomicString("transform"));
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(target->PaintingLayer()->NeedsVisualOverflowRecalc());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->SelfNeedsScrollableOverflowRecalc());

  element->classList().Remove(AtomicString("transform"));
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(target->PaintingLayer()->NeedsVisualOverflowRecalc());
}

TEST_F(LayoutBoxTest, ScrollsWithViewportRelativePosition) {
  SetBodyInnerHTML("<div id='target' style='position: relative'></div>");
  EXPECT_FALSE(GetLayoutBoxByElementId("target")->IsFixedToView());
}

TEST_F(LayoutBoxTest, ScrollsWithViewportFixedPosition) {
  SetBodyInnerHTML("<div id='target' style='position: fixed'></div>");
  EXPECT_TRUE(GetLayoutBoxByElementId("target")->IsFixedToView());
}

TEST_F(LayoutBoxTest, ScrollsWithViewportFixedPositionInsideTransform) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateZ(0)'>
      <div id='target' style='position: fixed'></div>
    </div>
    <div style='width: 10px; height: 1000px'></div>
  )HTML");
  EXPECT_FALSE(GetLayoutBoxByElementId("target")->IsFixedToView());
}

TEST_F(LayoutBoxTest, HitTestResizerWithTextAreaChild) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id="target"
         style="width: 100px; height: 100px; overflow: auto; resize: both">
      <textarea id="textarea"
          style="width: 100%; height: 100%; resize: none"></textarea>
    </div>
  )HTML");

  EXPECT_EQ(GetElementById("target"), HitTest(99, 99));
  EXPECT_TRUE(HitTest(1, 1)->IsDescendantOrShadowDescendantOf(
      GetElementById("textarea")));
}

TEST_F(LayoutBoxTest, HitTestResizerStackedWithTextAreaChild) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id="target" style="position: relative; width: 100px; height: 100px;
                            overflow: auto; resize: both">
      <textarea id="textarea"
          style="width: 100%; height: 100%; resize: none"></textarea>
    </div>
  )HTML");

  EXPECT_EQ(GetElementById("target"), HitTest(99, 99));
  EXPECT_TRUE(HitTest(1, 1)->IsDescendantOrShadowDescendantOf(
      GetElementById("textarea")));
}

TEST_F(LayoutBoxTest, AnchorInFragmentedContainingBlock) {
  // Create a 3-column multicol layout with a fragmented containing block,
  // and a fragmented anchor element that starts from the second fragment.
  InsertStyleElement(R"CSS(
    #multicol {
      column-count: 3;
      column-width: 90px;
      column-gap: 10px;
      width: 300px;
      height: 100px;
    }
    #cb {
      position: relative;
      height: 300px;
    }
    #spacer {
      height: 110px;
    }
    #anchor {
      height: 120px;
      anchor-name: --a;
    }
    #target {
      position: absolute;
    }
  )CSS");
  SetBodyInnerHTML(R"HTML(
    <div id="multicol">
      <div id="cb">
        <div id="spacer"></div>
        <div id="anchor"></div>
        <div id="target" anchor="anchor"></div>
      </div>
    </div>
  )HTML");

  const LayoutBox* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  EXPECT_EQ(GetLayoutObjectByElementId("anchor"),
            target->FindTargetAnchor(*MakeGarbageCollected<ScopedCSSName>(
                AtomicString("--a"), &GetDocument())));
  EXPECT_EQ(GetLayoutObjectByElementId("anchor"),
            target->AcceptableImplicitAnchor());
}

TEST_F(LayoutBoxTest, AnchorInInlineContainingBlock) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <span id="not-implicit-anchor">not implicit anchor</span>
      <span style="position: relative">
        <span id="anchor" style="anchor-name: --a">anchor</span>
        <div id="target" anchor="not-implicit-anchor"
             style="position: absolute; top: anchor(--a top)"></div>
      </span>
      some text
    </div>
  )HTML");

  const LayoutBox* target = To<LayoutBox>(GetLayoutObjectByElementId("target"));
  EXPECT_EQ(GetLayoutObjectByElementId("anchor"),
            target->FindTargetAnchor(*MakeGarbageCollected<ScopedCSSName>(
                AtomicString("--a"), &GetDocument())));
  EXPECT_FALSE(target->AcceptableImplicitAnchor());
}

TEST_F(LayoutBoxTest, AnchorInInlineContainingBlockWithNameConflicts) {
  SetBodyInnerHTML(R"HTML(
    <div>
      <span style="position: relative">
        <span id="anchor1" style="anchor-name: --a">anchor</span>
        <div id="target1" style="position: absolute;top: anchor(--a top)"></div>
      </span>
      <span style="position: relative">
        <span id="anchor2" style="anchor-name: --a">anchor</span>
        <div id="target2" style="position: absolute;top: anchor(--a top)"></div>
      </span>
      <span style="position: relative">
        <span id="anchor3" style="anchor-name: --a">anchor</span>
        <div id="target3" style="position: absolute;top: anchor(--a top)"></div>
      </span>
    </div>
  )HTML");

  const ScopedCSSName& anchor_name =
      *MakeGarbageCollected<ScopedCSSName>(AtomicString("--a"), &GetDocument());

  const LayoutBox* target1 =
      To<LayoutBox>(GetLayoutObjectByElementId("target1"));
  EXPECT_EQ(GetLayoutObjectByElementId("anchor1"),
            target1->FindTargetAnchor(anchor_name));

  const LayoutBox* target2 =
      To<LayoutBox>(GetLayoutObjectByElementId("target2"));
  EXPECT_EQ(GetLayoutObjectByElementId("anchor2"),
            target2->FindTargetAnchor(anchor_name));

  const LayoutBox* target3 =
      To<LayoutBox>(GetLayoutObjectByElementId("target3"));
  EXPECT_EQ(GetLayoutObjectByElementId("anchor3"),
            target3->FindTargetAnchor(anchor_name));
}

TEST_F(LayoutBoxTest, IsUserScrollable) {
  SetBodyInnerHTML(R"HTML("
    <style>
      #target { width: 100px; height: 100px; overflow: auto; }
    </style>
    <div id="target">
      <div id="content" style="height: 200px"></div>
    </div>
  )HTML");

  auto* target_element = GetElementById("target");
  auto* target = target_element->GetLayoutBox();
  EXPECT_TRUE(target->ScrollsOverflow());
  EXPECT_TRUE(target->IsUserScrollable());

  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("overflow: hidden"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->ScrollsOverflow());
  EXPECT_FALSE(target->IsUserScrollable());

  target_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  GetElementById("content")->setAttribute(html_names::kStyleAttr,
                                          AtomicString("height: 0"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(target->ScrollsOverflow());
  EXPECT_FALSE(target->IsUserScrollable());
}

TEST_F(LayoutBoxTest, IsUserScrollableLayoutView) {
  SetBodyInnerHTML(R"HTML("
    <div id="content" style="height: 2000px"></div>
  )HTML");

  EXPECT_TRUE(GetLayoutView().ScrollsOverflow());
  EXPECT_TRUE(GetLayoutView().IsUserScrollable());

  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("overflow: hidden"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetLayoutView().ScrollsOverflow());
  EXPECT_FALSE(GetLayoutView().IsUserScrollable());

  GetDocument().body()->setAttribute(html_names::kStyleAttr, g_empty_atom);
  GetElementById("content")->setAttribute(html_names::kStyleAttr,
                                          AtomicString("height: 0"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetLayoutView().ScrollsOverflow());
  EXPECT_FALSE(GetLayoutView().IsUserScrollable());
}

TEST_F(LayoutBoxTest, LogicalTopLogicalLeft) {
  SetBodyInnerHTML(R"HTML("
    <style>
    .c { contain: layout; }
    .t { width: 1px; height:1px; margin: 3px 5px 7px 11px; }
    .htb { writing-mode: horizontal-tb; }
    .vlr { writing-mode: vertical-lr; }
    .vrl { writing-mode: vertical-rl; }
    </style>
    <div class="c htb"><div id="htb-htb" class="t htb"></div></div>
    <div class="c htb"><div id="htb-vrl" class="t vrl"></div></div>
    <div class="c htb"><div id="htb-vlr" class="t vlr"></div></div>
    <div class="c vlr"><div id="vlr-htb" class="t htb"></div></div>
    <div class="c vlr"><div id="vlr-vrl" class="t vrl"></div></div>
    <div class="c vlr"><div id="vlr-vlr" class="t vlr"></div></div>
    <div class="c vrl"><div id="vrl-htb" class="t htb"></div></div>
    <div class="c vrl"><div id="vrl-vrl" class="t vrl"></div></div>
    <div class="c vrl"><div id="vrl-vlr" class="t vlr"></div></div>
  )HTML");
  constexpr LayoutUnit kTopMargin(3);
  constexpr LayoutUnit kRightMargin(5);
  constexpr LayoutUnit kLeftMargin(11);

  // Target DIVs are placed at (3, 11) from its container top-left.
  LayoutBox* target = GetLayoutBoxByElementId("htb-htb");
  EXPECT_EQ(kTopMargin, target->LogicalTop());
  EXPECT_EQ(kLeftMargin, target->LogicalLeft());
  target = GetLayoutBoxByElementId("htb-vrl");
  EXPECT_EQ(kLeftMargin, target->LogicalTop());
  EXPECT_EQ(kTopMargin, target->LogicalLeft());
  target = GetLayoutBoxByElementId("htb-vlr");
  EXPECT_EQ(kLeftMargin, target->LogicalTop());
  EXPECT_EQ(kTopMargin, target->LogicalLeft());

  // Container's writing-mode doesn't matter if it is vertical-lr.
  target = GetLayoutBoxByElementId("vlr-htb");
  EXPECT_EQ(kTopMargin, target->LogicalTop());
  EXPECT_EQ(kLeftMargin, target->LogicalLeft());
  target = GetLayoutBoxByElementId("vlr-vrl");
  EXPECT_EQ(kLeftMargin, target->LogicalTop());
  EXPECT_EQ(kTopMargin, target->LogicalLeft());
  target = GetLayoutBoxByElementId("vlr-vlr");
  EXPECT_EQ(kLeftMargin, target->LogicalTop());
  EXPECT_EQ(kTopMargin, target->LogicalLeft());

  // In a vertical-rl container, LogicalTop() and LogicalLeft() return
  // flipped-block offsets.
  target = GetLayoutBoxByElementId("vrl-htb");
  EXPECT_EQ(kTopMargin, target->LogicalTop());
  EXPECT_EQ(kRightMargin, target->LogicalLeft());
  target = GetLayoutBoxByElementId("vrl-vrl");
  EXPECT_EQ(kRightMargin, target->LogicalTop());
  EXPECT_EQ(kTopMargin, target->LogicalLeft());
  target = GetLayoutBoxByElementId("vrl-vlr");
  EXPECT_EQ(kRightMargin, target->LogicalTop());
  EXPECT_EQ(kTopMargin, target->LogicalLeft());
}

class LayoutBoxBackgroundPaintLocationTest : public RenderingTest,
                                             public PaintTestConfigurations {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  BackgroundPaintLocation ScrollerBackgroundPaintLocation() {
    return GetLayoutBoxByElementId("scroller")->GetBackgroundPaintLocation();
  }

  const String kCommonStyle = R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        width: 300px;
        height: 300px;
        will-change: transform;
      }
      .spacer { height: 1000px; }
    </style>
  )HTML";
};

INSTANTIATE_PAINT_TEST_SUITE_P(LayoutBoxBackgroundPaintLocationTest);

TEST_P(LayoutBoxBackgroundPaintLocationTest, ContentBoxClipZeroPadding) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller' style='background: white content-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller cannot paint background into scrolling contents layer because it
  // has a content-box clip without local attachment.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest,
       AttachmentLocalContentBoxClipNonZeroPadding) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: white local content-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can paint background into scrolling contents layer because it
  // has local attachment.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, NonLocalImage) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg),
                           white local;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller cannot paint background into scrolling contents layer because
  // the background image is not locally attached.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, LocalImageAndColor) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg)
                           local, white local;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can paint background into scrolling contents layer because both
  // the image and color are locally attached.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest,
       LocalImageAndNonLocalClipPaddingColor) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg)
                           local, white padding-box;
               padding: 10px;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can paint background into scrolling contents layer because the
  // image is locally attached and even though the color is not, it is filled to
  // the padding box so it will be drawn the same as a locally attached
  // background.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest,
       LocalImageAndNonLocalClipContentColorNonZeroPadding) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
        style='background: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg)
                           local, white content-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller cannot paint background into scrolling contents layer because
  // the color is filled to the content box and we have padding so it is not
  // equivalent to a locally attached background.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, BorderBoxClipColorNoBorder) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller' class='scroller' style='background: white border-box;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can paint background into scrolling contents layer because its
  // border-box is equivalent to its padding box since it has no border.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, BorderBoxClipColorSolidBorder) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: white border-box; border: 10px solid black;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can paint background into scrolling contents layer because its
  // border is opaque so it completely covers the background outside of the
  // padding-box.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest,
       BorderBoxClipColorTranslucentBorder) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: white border-box;
                border: 10px solid rgba(0, 0, 0, 0.5);'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller paints the background into both layers because its border is
  // partially transparent so the background must be drawn to the
  // border-box edges.
  EXPECT_EQ(kBackgroundPaintInBothSpaces, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, BorderBoxClipColorDashedBorder) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: white; border: 5px dashed black;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can be painted in both layers because the background is a
  // solid color, it must be because the dashed border reveals the background
  // underneath it.
  EXPECT_EQ(kBackgroundPaintInBothSpaces, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, ContentClipColorZeroPadding) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller' style='background: white content-box;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller can paint background into scrolling contents layer because its
  // content-box is equivalent to its padding box since it has no padding.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, ContentClipColorNonZeroPadding) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller' style='background: white content-box; padding: 10px;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller cannot paint background into scrolling contents layer because
  // it has padding so its content-box is not equivalent to its padding-box.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, CustomScrollbar) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <style>
      #scroller::-webkit-scrollbar {
        width: 13px;
        height: 13px;
      }
    </style>
    <div id='scroller' style='background: white border-box;'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller paints the background into both layers because it has a custom
  // scrollbar which the background may need to draw under.
  EXPECT_EQ(kBackgroundPaintInBothSpaces, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest,
       TranslucentColorAndTranslucentBorder) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: rgba(255, 255, 255, 0.5) border-box;
                border: 5px solid rgba(0, 0, 0, 0.5);'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // #scroller17 can only be painted once as it is translucent, and it must
  // be painted in the border box space to be under the translucent border.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest,
       LocalImageTranslucentColorAndTransparentBorder) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
        style='background: local linear-gradient(blue, red),
                           rgba(0, 128, 0, 0.5);
               border: 10px solid transparent'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // https://crbug.com/1241801: The background with translucent background color
  // should not be painted twice.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, InsetBoxShadow) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: white; box-shadow: 10px 10px black inset'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // Background with inset box shadow can only be painted in the main graphics
  // layer because the shadow can't scroll.
  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, OutsetBoxShadow) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller' style='background: white; box-shadow: 10px 10px black'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // Outset box shadow doesn't affect background paint location.
  EXPECT_EQ(kBackgroundPaintInContentsSpace, ScrollerBackgroundPaintLocation());
}

TEST_P(LayoutBoxBackgroundPaintLocationTest, BorderImage) {
  SetBodyInnerHTML(kCommonStyle + R"HTML(
    <div id='scroller'
         style='background: white; border: 2px solid; border-image-width: 5px;
                border-image-source: linear-gradient(blue, red)'>
      <div class='spacer'></div>
    </div>
  )HTML");

  EXPECT_EQ(kBackgroundPaintInBorderBoxSpace,
            ScrollerBackgroundPaintLocation());
}

}  // namespace blink
