// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
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

TEST_P(PaintControllerPaintTest, InlineRelayout) {
  SetBodyInnerHTML(
      "<div id='div' style='width:100px; height: 200px'>AAAAAAAAAA "
      "BBBBBBBBBB</div>");
  auto& div = *To<Element>(GetDocument().body()->firstChild());
  auto& div_block =
      *To<LayoutBlock>(GetDocument().body()->firstChild()->GetLayoutObject());
  LayoutText& text = *ToLayoutText(div_block.FirstChild());
  const DisplayItemClient* first_text_box = text.FirstTextBox();
  wtf_size_t first_text_box_fragment_id = 0;
  if (text.IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveTo(text);
    first_text_box = cursor.Current().GetDisplayItemClient();
    first_text_box_fragment_id = cursor.Current().FragmentId();
  }

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(first_text_box, kForegroundType,
                                   first_text_box_fragment_id)));

  div.setAttribute(html_names::kStyleAttr, "width: 10px; height: 200px");
  UpdateAllLifecyclePhasesForTest();

  LayoutText& new_text = *ToLayoutText(div_block.FirstChild());
  const DisplayItemClient* new_first_text_box = text.FirstTextBox();
  const DisplayItemClient* second_text_box = nullptr;
  wtf_size_t second_text_box_fragment_id = 0;
  if (!text.IsInLayoutNGInlineFormattingContext()) {
    second_text_box = new_text.FirstTextBox()->NextForSameLayoutObject();
  } else {
    NGInlineCursor cursor;
    cursor.MoveTo(text);
    new_first_text_box = cursor.Current().GetDisplayItemClient();
    cursor.MoveToNextForSameLayoutObject();
    second_text_box = cursor.Current().GetDisplayItemClient();
    second_text_box_fragment_id = cursor.Current().FragmentId();
  }

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(new_first_text_box, kForegroundType,
                                   first_text_box_fragment_id),
                          IsSameId(second_text_box, kForegroundType,
                                   second_text_box_fragment_id)));
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

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
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

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
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

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&div1, kBackgroundType),
                          IsSameId(&div2, kBackgroundType)));
  HitTestData view_scroll_hit_test;
  view_scroll_hit_test.scroll_translation =
      GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
  view_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
  EXPECT_THAT(
      GetPaintController().PaintChunks()[0],
      IsPaintChunk(0, 0,
                   PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
                   GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                   &view_scroll_hit_test, IntRect(0, 0, 800, 600)));
  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK(3, nullptr)));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(5000, 5000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&div2, kBackgroundType),
                          IsSameId(&div3, kBackgroundType),
                          IsSameId(&div4, kBackgroundType)));
  EXPECT_THAT(
      GetPaintController().PaintChunks()[0],
      IsPaintChunk(0, 0,
                   PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
                   GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                   &view_scroll_hit_test, IntRect(0, 0, 800, 600)));
  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK(4, nullptr)));
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
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&div1, kBackgroundType),
                          IsSameId(&div2, kBackgroundType)));
  HitTestData container_scroll_hit_test;
  container_scroll_hit_test.scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  container_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*container.Layer(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 200, 200)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(container, DisplayItem::kScrollHitTest),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       &container_scroll_hit_test, IntRect(0, 0, 200, 200)),
          IsPaintChunk(
              1, 3,
              PaintChunk::Id(container, kClippedContentsBackgroundChunkType),
              container.FirstFragment().ContentsProperties())));

  container.GetScrollableArea()->SetScrollOffset(
      ScrollOffset(5000, 5000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  // Cull rect after scroll: (1000,1000 8100x8100)
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&div2, kBackgroundType),
                          IsSameId(&div3, kBackgroundType),
                          IsSameId(&div4, kBackgroundType)));
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*container.Layer(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 200, 200)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(container, DisplayItem::kScrollHitTest),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       &container_scroll_hit_test, IntRect(0, 0, 200, 200)),
          IsPaintChunk(
              1, 4,
              PaintChunk::Id(container, kClippedContentsBackgroundChunkType),
              container.FirstFragment().ContentsProperties())));
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
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(&container, kBackgroundType),
                  IsSameId(&container.GetScrollableArea()
                                ->GetScrollingBackgroundDisplayItemClient(),
                           kBackgroundType),
                  IsSameId(&child, kBackgroundType)));
  HitTestData view_scroll_hit_test;
  view_scroll_hit_test.scroll_translation =
      GetLayoutView().FirstFragment().PaintProperties()->ScrollTranslation();
  view_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
  HitTestData container_scroll_hit_test;
  container_scroll_hit_test.scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  container_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(*container.Layer(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 200, 200)),
          IsPaintChunk(2, 2,
                       PaintChunk::Id(container, DisplayItem::kScrollHitTest),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       &container_scroll_hit_test, IntRect(0, 0, 200, 200)),
          IsPaintChunk(2, 4,
                       PaintChunk::Id(container, kScrollingBackgroundChunkType),
                       container.FirstFragment().ContentsProperties())));
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

  auto& html = *GetDocument().documentElement()->GetLayoutBox();
  auto& container = *ToLayoutBox(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto& neg_z_child = *ToLayoutBox(GetLayoutObjectByElementId("negZChild"));
  auto& pos_z_child = *ToLayoutBox(GetLayoutObjectByElementId("posZChild"));

  // Container is not a stacking context because no z-index is auto.
  // Negative z-index descendants are painted before the background and
  // positive z-index descendants are painted after the background. Scroll hit
  // testing should hit positive descendants, the container, and then negative
  // descendants so the scroll hit test should be immediately after the
  // background.
  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(&neg_z_child, kBackgroundType),
                  IsSameId(&container, kBackgroundType),
                  IsSameId(&container.GetScrollableArea()
                                ->GetScrollingBackgroundDisplayItemClient(),
                           kBackgroundType),
                  IsSameId(&child, kBackgroundType),
                  IsSameId(&pos_z_child, kBackgroundType)));
  HitTestData container_scroll_hit_test;
  container_scroll_hit_test.scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  container_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(*neg_z_child.Layer(), DisplayItem::kLayerChunk),
              neg_z_child.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              2, 2,
              PaintChunk::Id(*html.Layer(), DisplayItem::kLayerChunkForeground),
              html.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 800, 200)),
          IsPaintChunk(
              2, 3,
              PaintChunk::Id(*container.Layer(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 200, 200)),
          IsPaintChunk(3, 3,
                       PaintChunk::Id(container, DisplayItem::kScrollHitTest),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       &container_scroll_hit_test, IntRect(0, 0, 200, 200)),
          IsPaintChunk(3, 5,
                       PaintChunk::Id(container, kScrollingBackgroundChunkType),
                       container.FirstFragment().ContentsProperties()),
          IsPaintChunk(
              5, 6,
              PaintChunk::Id(*pos_z_child.Layer(), DisplayItem::kLayerChunk),
              pos_z_child.FirstFragment().LocalBorderBoxProperties())));
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

  auto& container = *ToLayoutBox(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto& neg_z_child = *ToLayoutBox(GetLayoutObjectByElementId("negZChild"));
  auto& pos_z_child = *ToLayoutBox(GetLayoutObjectByElementId("posZChild"));

  // Container is a stacking context because z-index is non-auto.
  // Both positive and negative z-index descendants are painted after the
  // background. The scroll hit test should be after the background but before
  // the z-index descendants to ensure hit test order is correct.
  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(&container, kBackgroundType),
                  IsSameId(&container.GetScrollableArea()
                                ->GetScrollingBackgroundDisplayItemClient(),
                           kBackgroundType),
                  IsSameId(&neg_z_child, kBackgroundType),
                  IsSameId(&child, kBackgroundType),
                  IsSameId(&pos_z_child, kBackgroundType)));
  HitTestData container_scroll_hit_test;
  container_scroll_hit_test.scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  container_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(*container.Layer(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 200, 200)),
          IsPaintChunk(2, 2,
                       PaintChunk::Id(container, DisplayItem::kScrollHitTest),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       &container_scroll_hit_test, IntRect(0, 0, 200, 200)),
          IsPaintChunk(2, 3,
                       PaintChunk::Id(container, kScrollingBackgroundChunkType),
                       container.FirstFragment().ContentsProperties()),
          IsPaintChunk(
              3, 4,
              PaintChunk::Id(*neg_z_child.Layer(), DisplayItem::kLayerChunk),
              neg_z_child.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              4, 5,
              PaintChunk::Id(container, kClippedContentsBackgroundChunkType),
              container.FirstFragment().ContentsProperties()),
          IsPaintChunk(
              5, 6,
              PaintChunk::Id(*pos_z_child.Layer(), DisplayItem::kLayerChunk),
              pos_z_child.FirstFragment().LocalBorderBoxProperties())));
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

  auto& html = *GetDocument().documentElement()->GetLayoutBox();
  auto& container = *ToLayoutBox(GetLayoutObjectByElementId("container"));
  auto& child = *GetLayoutObjectByElementId("child");
  auto& neg_z_child = *ToLayoutBox(GetLayoutObjectByElementId("negZChild"));
  auto& pos_z_child = *ToLayoutBox(GetLayoutObjectByElementId("posZChild"));

  // Even though container does not paint a background, the scroll hit test
  // should still be between the negative z-index child and the regular child.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&neg_z_child, kBackgroundType),
                          IsSameId(&child, kBackgroundType),
                          IsSameId(&pos_z_child, kBackgroundType)));
  HitTestData container_scroll_hit_test;
  container_scroll_hit_test.scroll_translation =
      container.FirstFragment().PaintProperties()->ScrollTranslation();
  container_scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 200, 200);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(*neg_z_child.Layer(), DisplayItem::kLayerChunk),
              neg_z_child.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              2, 2,
              PaintChunk::Id(*html.Layer(), DisplayItem::kLayerChunkForeground),
              html.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 800, 200)),
          IsPaintChunk(
              2, 2,
              PaintChunk::Id(*container.Layer(), DisplayItem::kLayerChunk),
              container.FirstFragment().LocalBorderBoxProperties(), nullptr,
              IntRect(0, 0, 200, 200)),
          IsPaintChunk(2, 2,
                       PaintChunk::Id(container, DisplayItem::kScrollHitTest),
                       container.FirstFragment().LocalBorderBoxProperties(),
                       &container_scroll_hit_test, IntRect(0, 0, 200, 200)),
          IsPaintChunk(
              2, 3,
              PaintChunk::Id(container, kClippedContentsBackgroundChunkType),
              container.FirstFragment().ContentsProperties()),
          IsPaintChunk(
              3, 4,
              PaintChunk::Id(*pos_z_child.Layer(), DisplayItem::kLayerChunk),
              pos_z_child.FirstFragment().LocalBorderBoxProperties())));
}

}  // namespace blink
