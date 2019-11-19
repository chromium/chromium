// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

using testing::ElementsAre;

namespace blink {

INSTANTIATE_PAINT_TEST_SUITE_P(PaintControllerPaintTest);

using PaintControllerPaintTestForCAP = PaintControllerPaintTest;
INSTANTIATE_CAP_TEST_SUITE_P(PaintControllerPaintTestForCAP);

TEST_P(PaintControllerPaintTest, FullDocumentPaintingWithCaret) {
  SetBodyInnerHTML(
      "<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto& div = *To<Element>(GetDocument().body()->firstChild());
  InlineTextBox& text_inline_box =
      *ToLayoutText(div.firstChild()->GetLayoutObject())->FirstTextBox();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&text_inline_box, kForegroundType)));

  div.focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&text_inline_box, kForegroundType),
          // New!
          IsSameId(&CaretDisplayItemClientForTesting(), DisplayItem::kCaret)));
}

TEST_P(PaintControllerPaintTest, InlineRelayout) {
  SetBodyInnerHTML(
      "<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA "
      "BBBBBBBBBB</div>");
  auto& div = *To<Element>(GetDocument().body()->firstChild());
  auto& div_block =
      *To<LayoutBlock>(GetDocument().body()->firstChild()->GetLayoutObject());
  LayoutText& text = *ToLayoutText(div_block.FirstChild());
  DisplayItemClient& first_text_box =
      text.FirstInlineFragment()
          ? (DisplayItemClient&)*text.FirstInlineFragment()
          : (DisplayItemClient&)*text.FirstTextBox();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&first_text_box, kForegroundType)));

  div.setAttribute(html_names::kStyleAttr, "width: 10px; height: 200px");
  UpdateAllLifecyclePhasesForTest();

  LayoutText& new_text = *ToLayoutText(div_block.FirstChild());
  DisplayItemClient& new_first_text_box =
      new_text.FirstInlineFragment()
          ? (DisplayItemClient&)*new_text.FirstInlineFragment()
          : (DisplayItemClient&)*text.FirstTextBox();
  DisplayItemClient& second_text_box =
      new_text.FirstInlineFragment()
          ? (DisplayItemClient&)*NGPaintFragment::
                TraverseNextForSameLayoutObject::Next(
                    new_text.FirstInlineFragment())
          : (DisplayItemClient&)*new_text.FirstTextBox()
                ->NextForSameLayoutObject();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&new_first_text_box, kForegroundType),
                          IsSameId(&second_text_box, kForegroundType)));
}

TEST_P(PaintControllerPaintTest, ChunkIdClientCacheFlag) {
  SetBodyInnerHTML(R"HTML(
    <div id='div' style='width: 200px; height: 200px; opacity: 0.5'>
      <div style='width: 100px; height: 100px; background-color:
    blue'></div>
      <div style='width: 100px; height: 100px; background-color:
    blue'></div>
    </div>
  )HTML");
  auto& div = *To<LayoutBlock>(GetLayoutObjectByElementId("div"));
  LayoutObject& sub_div = *div.FirstChild();
  LayoutObject& sub_div2 = *sub_div.NextSibling();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&sub_div, kBackgroundType),
                          IsSameId(&sub_div2, kBackgroundType)));

  EXPECT_FALSE(div.Layer()->IsJustCreated());
  // Client used by only paint chunks and non-cachaeable display items but not
  // by any cacheable display items won't be marked as validly cached.
  EXPECT_TRUE(ClientCacheIsValid(*div.Layer()));
  EXPECT_FALSE(ClientCacheIsValid(div));
  EXPECT_TRUE(ClientCacheIsValid(sub_div));
}

TEST_P(PaintControllerPaintTest, CompositingNoFold) {
  SetBodyInnerHTML(R"HTML(
    <div id='div' style='width: 200px; height: 200px; opacity: 0.5'>
      <div style='width: 100px; height: 100px; background-color:
    blue'></div>
    </div>
  )HTML");
  auto& div = *To<LayoutBlock>(GetLayoutObjectByElementId("div"));
  LayoutObject& sub_div = *div.FirstChild();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&sub_div, kBackgroundType)));
}

TEST_P(PaintControllerPaintTestForCAP, FrameScrollingContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0; width: 10000px; height: 1000px }
      div { position: absolute; width: 100px; height: 100px;
            background: blue; }
    </style>
    <div id='div1' style='top: 0; left: 0'></div>
    <div id='div2' style='top: 3000px; left: 3000px'></div>
    <div id='div3' style='top: 6000px; left: 6000px'></div>
    <div id='div4' style='top: 9000px; left: 9000px'></div>
  )HTML");

  const auto& div1 = *GetLayoutObjectByElementId("div1");
  const auto& div2 = *GetLayoutObjectByElementId("div2");
  const auto& div3 = *GetLayoutObjectByElementId("div3");
  const auto& div4 = *GetLayoutObjectByElementId("div4");

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&GetLayoutView(), kScrollHitTestType),
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&div1, kBackgroundType), IsSameId(&div2, kBackgroundType)));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(5000, 5000), kProgrammaticScroll);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&GetLayoutView(), kScrollHitTestType),
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&div2, kBackgroundType), IsSameId(&div3, kBackgroundType),
          IsSameId(&div4, kBackgroundType)));
}

TEST_P(PaintControllerPaintTestForCAP, BlockScrollingNonLayeredContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0 }
      div { width: 100px; height: 100px; background: blue; }
      container { display: block; width: 200px; height: 200px;
                  overflow: scroll; will-change: transform; }
    </style>
    <container id='container'>
      <div id='div1'></div>
      <div id='div2' style='margin-top: 2900px; margin-left: 3000px'></div>
      <div id='div3' style='margin-top: 2900px; margin-left: 6000px'></div>
      <div id='div4' style='margin-top: 2900px; margin-left: 9000px'></div>
    </container>
  )HTML");

  auto& container = *To<LayoutBlock>(GetLayoutObjectByElementId("container"));
  auto& div1 = *GetLayoutObjectByElementId("div1");
  auto& div2 = *GetLayoutObjectByElementId("div2");
  auto& div3 = *GetLayoutObjectByElementId("div3");
  auto& div4 = *GetLayoutObjectByElementId("div4");

  // Initial cull rect: (0,0 4200x4200)
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&container, kScrollHitTestType),
          IsSameId(&div1, kBackgroundType), IsSameId(&div2, kBackgroundType)));

  container.GetScrollableArea()->SetScrollOffset(ScrollOffset(5000, 5000),
                                                 kProgrammaticScroll);
  UpdateAllLifecyclePhasesForTest();

  // Cull rect after scroll: (1000,1000 8100x8100)
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&container, kScrollHitTestType),
          IsSameId(&div2, kBackgroundType), IsSameId(&div3, kBackgroundType),
          IsSameId(&div4, kBackgroundType)));
}

TEST_P(PaintControllerPaintTestForCAP, ScrollHitTestOrder) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0 }
      #container { width: 200px; height: 200px;
                  overflow: scroll; background: red; }
      #child { width: 100px; height: 300px; background: green; }
      #forceDocumentScroll { height: 1000px; }
    </style>
    <div id='container'>
      <div id='child'></div>
    </div>
    <div id='forceDocumentScroll'/>
  )HTML");

  auto& container = *To<LayoutBlock>(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");

  // The container's items should all be after the document's scroll hit test
  // to ensure the container is hit before the document. Similarly, the child's
  // items should all be after the container's scroll hit test.
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&GetLayoutView(), kScrollHitTestType),
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&container, kBackgroundType),
          IsSameId(&container, kScrollHitTestType),
          IsSameId(&container.GetScrollableArea()
                        ->GetScrollingBackgroundDisplayItemClient(),
                   kBackgroundType),
          IsSameId(&child, kBackgroundType)));
}

TEST_P(PaintControllerPaintTestForCAP, NonStackingScrollHitTestOrder) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0 }
      #container { width: 200px; height: 200px;
                  overflow: scroll; background: blue;
                  position: relative; z-index: auto; }
      #child { width: 80px; height: 20px; background: white; }
      #negZChild { width: 60px; height: 300px; background: purple;
                   position: absolute; z-index: -1; top: 0; }
      #posZChild { width: 40px; height: 300px; background: yellow;
                   position: absolute; z-index: 1; top: 0; }
    </style>
    <div id='container'>
      <div id='child'></div>
      <div id='negZChild'></div>
      <div id='posZChild'></div>
    </div>
  )HTML");

  auto& container = *To<LayoutBlock>(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto& neg_z_child = *GetLayoutObjectByElementId("negZChild");
  auto& pos_z_child = *GetLayoutObjectByElementId("posZChild");

  // Container is not a stacking context because no z-index is auto.
  // Negative z-index descendants are painted before the background and
  // positive z-index descendants are painted after the background. Scroll hit
  // testing should hit positive descendants, the container, and then negative
  // descendants so the ScrollHitTest item should be immediately after the
  // background.
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&neg_z_child, kBackgroundType),
          IsSameId(&container, kBackgroundType),
          IsSameId(&container, kScrollHitTestType),
          IsSameId(&container.GetScrollableArea()
                        ->GetScrollingBackgroundDisplayItemClient(),
                   kBackgroundType),
          IsSameId(&child, kBackgroundType),
          IsSameId(&pos_z_child, kBackgroundType)));
}

TEST_P(PaintControllerPaintTestForCAP, StackingScrollHitTestOrder) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0 }
      #container { width: 200px; height: 200px;
                  overflow: scroll; background: blue;
                  position: relative; z-index: 0; }
      #child { width: 80px; height: 20px; background: white; }
      #negZChild { width: 60px; height: 300px; background: purple;
                   position: absolute; z-index: -1; top: 0; }
      #posZChild { width: 40px; height: 300px; background: yellow;
                   position: absolute; z-index: 1; top: 0; }
    </style>
    <div id='container'>
      <div id='child'></div>
      <div id='negZChild'></div>
      <div id='posZChild'></div>
    </div>
  )HTML");

  auto& container = *To<LayoutBlock>(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto& neg_z_child = *GetLayoutObjectByElementId("negZChild");
  auto& pos_z_child = *GetLayoutObjectByElementId("posZChild");

  // Container is a stacking context because z-index is non-auto.
  // Both positive and negative z-index descendants are painted after the
  // background. The scroll hit test should be after the background but before
  // the z-index descendants to ensure hit test order is correct.
  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(&container, kBackgroundType),
          IsSameId(&container, kScrollHitTestType),
          IsSameId(&container.GetScrollableArea()
                        ->GetScrollingBackgroundDisplayItemClient(),
                   kBackgroundType),
          IsSameId(&neg_z_child, kBackgroundType),
          IsSameId(&child, kBackgroundType),
          IsSameId(&pos_z_child, kBackgroundType)));
}

TEST_P(PaintControllerPaintTestForCAP,
       NonStackingScrollHitTestOrderWithoutBackground) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0 }
      #container { width: 200px; height: 200px;
                  overflow: scroll; background: transparent;
                  position: relative; z-index: auto; }
      #child { width: 80px; height: 20px; background: white; }
      #negZChild { width: 60px; height: 300px; background: purple;
                   position: absolute; z-index: -1; top: 0; }
      #posZChild { width: 40px; height: 300px; background: yellow;
                   position: absolute; z-index: 1; top: 0; }
    </style>
    <div id='container'>
      <div id='child'></div>
      <div id='negZChild'></div>
      <div id='posZChild'></div>
    </div>
  )HTML");

  auto& container = *To<LayoutBlock>(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto& neg_z_child = *GetLayoutObjectByElementId("negZChild");
  auto& pos_z_child = *GetLayoutObjectByElementId("posZChild");

  // Even though container does not paint a background, the scroll hit test item
  // should still be between the negative z-index child and the regular child.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(&neg_z_child, kBackgroundType),
                          IsSameId(&container, kScrollHitTestType),
                          IsSameId(&child, kBackgroundType),
                          IsSameId(&pos_z_child, kBackgroundType)));
}

}  // namespace blink
