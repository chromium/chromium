// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::Contains;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace blink {

class PaintLayerPainterTest : public PaintControllerPaintTest {
  USING_FAST_MALLOC(PaintLayerPainterTest);

 public:
  CullRect GetCullRect(const PaintLayer& layer) {
    return layer.GetLayoutObject().FirstFragment().GetCullRect();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintLayerPainterTest);

TEST_P(PaintLayerPainterTest, CachedSubsequenceAndChunksWithBackgrounds) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id='container1' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: red'></div>
    </div>
    <div id='filler1' style='position: relative; z-index: 2;
        width: 20px; height: 20px; background-color: gray'></div>
    <div id='container2' style='position: relative; z-index: 3;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2' style='position: absolute; width: 100px;
          height: 100px; background-color: green;'></div>
    </div>
    <div id='filler2' style='position: relative; z-index: 4;
        width: 20px; height: 20px; background-color: gray'></div>
  )HTML");

  auto* container1 = GetLayoutObjectByElementId("container1");
  auto* content1 = GetLayoutObjectByElementId("content1");
  auto* filler1 = GetLayoutObjectByElementId("filler1");
  auto* container2 = GetLayoutObjectByElementId("container2");
  auto* content2 = GetLayoutObjectByElementId("content2");
  auto* filler2 = GetLayoutObjectByElementId("filler2");

  auto* container1_layer = To<LayoutBoxModelObject>(container1)->Layer();
  auto* content1_layer = To<LayoutBoxModelObject>(content1)->Layer();
  auto* filler1_layer = To<LayoutBoxModelObject>(filler1)->Layer();
  auto* container2_layer = To<LayoutBoxModelObject>(container2)->Layer();
  auto* content2_layer = To<LayoutBoxModelObject>(content2)->Layer();
  auto* filler2_layer = To<LayoutBoxModelObject>(filler2)->Layer();
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  auto check_results = [&]() {
    EXPECT_THAT(
        ContentDisplayItems(),
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
            IsSameId(GetDisplayItemClientFromLayoutObject(container1)->Id(),
                     kBackgroundType),
            IsSameId(GetDisplayItemClientFromLayoutObject(content1)->Id(),
                     kBackgroundType),
            IsSameId(GetDisplayItemClientFromLayoutObject(filler1)->Id(),
                     kBackgroundType),
            IsSameId(GetDisplayItemClientFromLayoutObject(container2)->Id(),
                     kBackgroundType),
            IsSameId(GetDisplayItemClientFromLayoutObject(content2)->Id(),
                     kBackgroundType),
            IsSameId(GetDisplayItemClientFromLayoutObject(filler2)->Id(),
                     kBackgroundType)));

    // Check that new paint chunks were forced for the layers.
    auto chunks = ContentPaintChunks();
    auto chunk_it = chunks.begin();
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*container1_layer, chunk_it + 1, 2);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*content1_layer, chunk_it + 2, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler1_layer, chunk_it + 3, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*container2_layer, chunk_it + 4, 2);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*content2_layer, chunk_it + 5, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler2_layer, chunk_it + 6, 1);

    EXPECT_THAT(
        chunks,
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(1, 2,
                         PaintChunk::Id(container1_layer->Id(),
                                        DisplayItem::kLayerChunk),
                         chunk_state, nullptr, gfx::Rect(0, 0, 200, 200)),
            IsPaintChunk(
                2, 3,
                PaintChunk::Id(content1_layer->Id(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, gfx::Rect(0, 0, 100, 100)),
            IsPaintChunk(
                3, 4,
                PaintChunk::Id(filler1_layer->Id(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, gfx::Rect(0, 200, 20, 20)),
            IsPaintChunk(4, 5,
                         PaintChunk::Id(container2_layer->Id(),
                                        DisplayItem::kLayerChunk),
                         chunk_state, nullptr, gfx::Rect(0, 220, 200, 200)),
            IsPaintChunk(
                5, 6,
                PaintChunk::Id(content2_layer->Id(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, gfx::Rect(0, 220, 100, 100)),
            IsPaintChunk(
                6, 7,
                PaintChunk::Id(filler2_layer->Id(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, gfx::Rect(0, 420, 20, 20))));
  };

  check_results();

  To<HTMLElement>(content1->GetNode())
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: absolute; width: 100px; height: 100px; "
                       "background-color: green"));
  PaintController::CounterForTesting counter;
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(6u, counter.num_cached_items);
  EXPECT_EQ(4u, counter.num_cached_subsequences);

  // We should still have the paint chunks forced by the cached subsequences.
  check_results();
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceAndChunksWithoutBackgrounds) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      ::-webkit-scrollbar { display: none }
    </style>
    <div id='container' style='position: relative; z-index: 0;
        width: 150px; height: 150px; overflow: scroll'>
      <div id='content' style='position: relative; z-index: 1;
          width: 200px; height: 100px'>
        <div id='inner-content'
             style='position: absolute; width: 100px; height: 100px'></div>
      </div>
      <div id='filler' style='position: relative; z-index: 2;
          width: 300px; height: 300px'></div>
    </div>
  )HTML");

  auto* container = GetLayoutObjectByElementId("container");
  auto* content = GetLayoutObjectByElementId("content");
  auto* inner_content = GetLayoutObjectByElementId("inner-content");
  auto* filler = GetLayoutObjectByElementId("filler");

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM));

  auto* container_layer = To<LayoutBoxModelObject>(container)->Layer();
  auto* content_layer = To<LayoutBoxModelObject>(content)->Layer();
  auto* inner_content_layer = To<LayoutBoxModelObject>(inner_content)->Layer();
  auto* filler_layer = To<LayoutBoxModelObject>(filler)->Layer();

  auto chunks = ContentPaintChunks();
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*container_layer, chunks.begin() + 1, 6);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*content_layer, chunks.begin() + 4, 2);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*inner_content_layer, chunks.begin() + 5, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler_layer, chunks.begin() + 6, 1);
  } else {
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*container_layer, chunks.begin() + 1, 5);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*content_layer, chunks.begin() + 3, 2);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*inner_content_layer, chunks.begin() + 4, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler_layer, chunks.begin() + 5, 1);
  }

  auto container_properties =
      container->FirstFragment().LocalBorderBoxProperties();
  auto content_properties = container->FirstFragment().ContentsProperties();
  auto* scroll_hit_test = MakeGarbageCollected<HitTestData>();
  scroll_hit_test->scroll_translation =
      container->FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test->scroll_hit_test_rect = gfx::Rect(0, 0, 150, 150);

  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_THAT(
        chunks,
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container_layer->Id(), DisplayItem::kLayerChunk),
                container_properties, nullptr, gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container->Id(), DisplayItem::kScrollHitTest),
                container_properties, scroll_hit_test,
                gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container->Id(), kScrollingBackgroundChunkType),
                content_properties, nullptr, gfx::Rect(0, 0, 300, 400)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(content_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 0, 200, 100)),
            IsPaintChunk(1, 1,
                         PaintChunk::Id(inner_content_layer->Id(),
                                        DisplayItem::kLayerChunk),
                         content_properties, nullptr,
                         gfx::Rect(0, 0, 100, 100)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(filler_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 100, 300, 300))));
  } else {
    EXPECT_THAT(
        chunks,
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container_layer->Id(), DisplayItem::kLayerChunk),
                container_properties, nullptr, gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container->Id(), DisplayItem::kScrollHitTest),
                container_properties, scroll_hit_test,
                gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(content_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 0, 200, 100)),
            IsPaintChunk(1, 1,
                         PaintChunk::Id(inner_content_layer->Id(),
                                        DisplayItem::kLayerChunk),
                         content_properties, nullptr,
                         gfx::Rect(0, 0, 100, 100)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(filler_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 100, 300, 300))));
  }

  To<HTMLElement>(inner_content->GetNode())
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: absolute; width: 100px; height: 100px; "
                       "top: 100px; background-color: green"));
  PaintController::CounterForTesting counter;
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1u, counter.num_cached_items);         // view background.
  EXPECT_EQ(1u, counter.num_cached_subsequences);  // filler layer.

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
          IsSameId(GetDisplayItemClientFromLayoutObject(inner_content)->Id(),
                   kBackgroundType)));

  chunks = ContentPaintChunks();
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*container_layer, chunks.begin() + 1, 6);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*content_layer, chunks.begin() + 4, 2);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*inner_content_layer, chunks.begin() + 5, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler_layer, chunks.begin() + 6, 1);

    EXPECT_THAT(
        ContentPaintChunks(),
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container_layer->Id(), DisplayItem::kLayerChunk),
                container_properties, nullptr, gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container->Id(), DisplayItem::kScrollHitTest),
                container_properties, scroll_hit_test,
                gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container->Id(), kScrollingBackgroundChunkType),
                content_properties, nullptr, gfx::Rect(0, 0, 300, 400)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(content_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 0, 200, 100)),
            IsPaintChunk(1, 2,
                         PaintChunk::Id(inner_content_layer->Id(),
                                        DisplayItem::kLayerChunk),
                         content_properties, nullptr,
                         gfx::Rect(0, 100, 100, 100)),
            IsPaintChunk(
                2, 2,
                PaintChunk::Id(filler_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 100, 300, 300))));
  } else {
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*container_layer, chunks.begin() + 1, 5);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*content_layer, chunks.begin() + 3, 2);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*inner_content_layer, chunks.begin() + 4, 1);
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler_layer, chunks.begin() + 5, 1);

    EXPECT_THAT(
        ContentPaintChunks(),
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container_layer->Id(), DisplayItem::kLayerChunk),
                container_properties, nullptr, gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(container->Id(), DisplayItem::kScrollHitTest),
                container_properties, scroll_hit_test,
                gfx::Rect(0, 0, 150, 150)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(content_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 0, 200, 100)),
            IsPaintChunk(1, 2,
                         PaintChunk::Id(inner_content_layer->Id(),
                                        DisplayItem::kLayerChunk),
                         content_properties, nullptr,
                         gfx::Rect(0, 100, 100, 100)),
            IsPaintChunk(
                2, 2,
                PaintChunk::Id(filler_layer->Id(), DisplayItem::kLayerChunk),
                content_properties, nullptr, gfx::Rect(0, 100, 300, 300))));
  }
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnCullRectChange) {
  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
       width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2a' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
      <div id='content2b' style='position: absolute; top: 200px;
          width: 100px; height: 100px; background-color: green'></div>
    </div>
    <div id='container3' style='position: absolute; z-index: 2;
        left: 300px; top: 0; width: 200px; height: 200px;
        background-color: blue'>
      <div id='content3' style='position: absolute; width: 200px;
          height: 200px; background-color: green'></div>
    </div>
  )HTML");
  InvalidateAll();

  const DisplayItemClient& container1 =
      *GetDisplayItemClientFromElementId("container1");
  const DisplayItemClient& content1 =
      *GetDisplayItemClientFromElementId("content1");
  const DisplayItemClient& container2 =
      *GetDisplayItemClientFromElementId("container2");
  const DisplayItemClient& content2a =
      *GetDisplayItemClientFromElementId("content2a");
  const DisplayItemClient& content2b =
      *GetDisplayItemClientFromElementId("content2b");
  const DisplayItemClient& container3 =
      *GetDisplayItemClientFromElementId("container3");
  const DisplayItemClient& content3 =
      *GetDisplayItemClientFromElementId("content3");

  UpdateAllLifecyclePhasesExceptPaint();
  PaintContents(gfx::Rect(0, 0, 400, 300));

  // Container1 is fully in the interest rect;
  // Container2 is partly (including its stacking chidren) in the interest rect;
  // Content2b is out of the interest rect and output nothing;
  // Container3 is partly in the interest rect.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2a.Id(), kBackgroundType),
                          IsSameId(container3.Id(), kBackgroundType),
                          IsSameId(content3.Id(), kBackgroundType)));

  UpdateAllLifecyclePhasesExceptPaint();
  PaintController::CounterForTesting counter;
  PaintContents(gfx::Rect(0, 100, 300, 1000));
  // Container1 becomes partly in the interest rect, but uses cached subsequence
  // because it was fully painted before;
  // Container2's intersection with the interest rect changes;
  // Content2b is out of the interest rect and outputs nothing;
  // Container3 becomes out of the interest rect and outputs nothing.
  EXPECT_EQ(5u, counter.num_cached_items);
  EXPECT_EQ(2u, counter.num_cached_subsequences);

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2a.Id(), kBackgroundType),
                          IsSameId(content2b.Id(), kBackgroundType)));
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnCullRectChangeUnderInvalidationChecking) {
  ScopedPaintUnderInvalidationCheckingForTest under_invalidation_checking(true);

  SetBodyInnerHTML(R"HTML(
    <style>p { width: 200px; height: 50px; background: green }</style>
    <div id='target' style='position: relative; z-index: 1'>
      <p></p><p></p><p></p><p></p>
    </div>
  )HTML");
  InvalidateAll();

  // |target| will be fully painted.
  UpdateAllLifecyclePhasesExceptPaint();
  PaintContents(gfx::Rect(0, 0, 400, 300));

  // |target| will be partially painted. Should not trigger under-invalidation
  // checking DCHECKs.
  UpdateAllLifecyclePhasesExceptPaint();
  PaintContents(gfx::Rect(0, 100, 300, 1000));
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnStyleChangeWithCullRectClipping) {
  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='overflow: hidden; width: 100px;
          height: 100px; background-color: red'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2' style='overflow: hidden; width: 100px;
          height: 100px; background-color: green'></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesExceptPaint();
  // PaintResult of all subsequences will be MayBeClippedByCullRect.
  PaintContents(gfx::Rect(0, 0, 50, 300));

  const DisplayItemClient& container1 =
      *GetDisplayItemClientFromElementId("container1");
  const DisplayItemClient& content1 =
      *GetDisplayItemClientFromElementId("content1");
  const DisplayItemClient& container2 =
      *GetDisplayItemClientFromElementId("container2");
  const DisplayItemClient& content2 =
      *GetDisplayItemClientFromElementId("content2");

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType)));

  To<HTMLElement>(GetElementById("content1"))
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: absolute; width: 100px; height: 100px; "
                       "background-color: green"));
  UpdateAllLifecyclePhasesExceptPaint();
  PaintController::CounterForTesting counter;
  PaintContents(gfx::Rect(0, 0, 50, 300));
  EXPECT_EQ(4u, counter.num_cached_items);

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container1.Id(), kBackgroundType),
                          IsSameId(content1.Id(), kBackgroundType),
                          IsSameId(container2.Id(), kBackgroundType),
                          IsSameId(content2.Id(), kBackgroundType)));
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceRetainsPreviousPaintResult) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html, body { height: 100%; margin: 0 }
      ::-webkit-scrollbar { display:none }
    </style>
    <div id="target" style="height: 8000px; contain: paint">
      <div id="content1" style="height: 100px; background: blue"></div>
      <div style="height: 6000px"></div>
      <div id="content2" style="height: 100px; background: blue"></div>
    </div>
    <div id="change" style="display: none"></div>
  )HTML");

  const auto* target = GetLayoutBoxByElementId("target");
  const auto* target_layer = target->Layer();
  const auto* content1 = GetLayoutObjectByElementId("content1");
  const auto* content2 = GetLayoutObjectByElementId("content2");
  // |target| is partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  // |content2| is out of the cull rect.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(content1->Id(), kBackgroundType)));
  EXPECT_EQ(gfx::Rect(0, 0, 800, 4600), GetCullRect(*target_layer).Rect());
  auto chunks = ContentPaintChunks();
  // |target| still created subsequence (cached).
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 2);
  EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                  IsPaintChunk(1, 1), IsPaintChunk(1, 2)));

  // Change something that triggers a repaint but |target| should use cached
  // subsequence.
  GetDocument()
      .getElementById(AtomicString("change"))
      ->setAttribute(html_names::kStyleAttr, AtomicString("display: block"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());
  PaintController::CounterForTesting counter;
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, counter.num_cached_items);
  EXPECT_EQ(1u, counter.num_cached_subsequences);

  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(content1->Id(), kBackgroundType)));
  EXPECT_EQ(gfx::Rect(0, 0, 800, 4600), GetCullRect(*target_layer).Rect());
  chunks = ContentPaintChunks();
  EXPECT_EQ(CullRect(gfx::Rect(0, 0, 800, 4600)), GetCullRect(*target_layer));
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(content1->Id(), kBackgroundType)));
  // |target| still created subsequence (cached).
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 2);
  EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                  IsPaintChunk(1, 1), IsPaintChunk(1, 2)));

  // Scroll the view so that both |content1| and |content2| are in the interest
  // rect.
  GetLayoutView().GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 3000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  // The layer needs repaint when its contents cull rect changes.
  EXPECT_TRUE(target_layer->SelfNeedsRepaint());

  counter.Reset();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, counter.num_cached_items);
  EXPECT_EQ(0u, counter.num_cached_subsequences);

  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  // Painted result should include both |content1| and |content2|.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(content1->Id(), kBackgroundType),
                          IsSameId(content2->Id(), kBackgroundType)));
  EXPECT_EQ(gfx::Rect(0, 0, 800, 7600), GetCullRect(*target_layer).Rect());
  chunks = ContentPaintChunks();
  EXPECT_EQ(CullRect(gfx::Rect(0, 0, 800, 7600)), GetCullRect(*target_layer));
  // |target| still created subsequence (repainted).
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 2);
  EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                  IsPaintChunk(1, 1), IsPaintChunk(1, 3)));
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline) {
  AtomicString style_without_outline(
      "width: 50px; height: 50px; background-color: green");
  AtomicString style_with_outline("outline: 1px solid blue; " +
                                  style_without_outline);
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='outline'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& outline_div =
      *GetDocument().getElementById(AtomicString("outline"))->GetLayoutObject();
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();

  auto& self_painting_layer_object = *To<LayoutBoxModelObject>(
      GetDocument()
          .getElementById(AtomicString("self-painting-layer"))
          ->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  auto& non_self_painting_layer =
      *GetPaintLayerByElementId("non-self-painting-layer");
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == outline_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());

  // Outline on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantOutlines.
  To<HTMLElement>(self_painting_layer_object.GetNode())
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("position: absolute; outline: 1px solid green"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_THAT(ContentDisplayItems(),
              Contains(IsSameId(self_painting_layer_object.Id(),
                                DisplayItem::PaintPhaseToDrawingType(
                                    PaintPhase::kSelfOutlineOnly))));

  // needsPaintPhaseDescendantOutlines should be set when any descendant on the
  // same layer has outline.
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_outline);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      ContentDisplayItems(),
      Contains(IsSameId(outline_div.Id(), DisplayItem::PaintPhaseToDrawingType(
                                              PaintPhase::kSelfOutlineOnly))));

  // needsPaintPhaseDescendantOutlines should be reset when no outline is
  // actually painted.
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat) {
  AtomicString style_without_float(
      "width: 50px; height: 50px; background-color: green");
  AtomicString style_with_float("float: left; " + style_without_float);
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='float' style='width: 10px; height: 10px;
              background-color: blue'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& float_div =
      *GetDocument().getElementById(AtomicString("float"))->GetLayoutObject();
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();

  auto& self_painting_layer_object = *To<LayoutBoxModelObject>(
      GetDocument()
          .getElementById(AtomicString("self-painting-layer"))
          ->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  auto& non_self_painting_layer =
      *GetPaintLayerByElementId("non-self-painting-layer");
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == float_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());

  // needsPaintPhaseFloat should be set when any descendant on the same layer
  // has float.
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_float);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(ContentDisplayItems(),
              Contains(IsSameId(float_div.Id(),
                                DisplayItem::kBoxDecorationBackground)));

  // needsPaintPhaseFloat should be reset when there is no float actually
  // painted.
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <span id='span' style='position: relative'>
          <div id='float' style='width: 10px; height: 10px;
              background-color: blue; float: left'></div>
        </span>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  LayoutObject& float_div =
      *GetDocument().getElementById(AtomicString("float"))->GetLayoutObject();
  PaintLayer& span_layer = *GetPaintLayerByElementId("span");
  ASSERT_TRUE(&span_layer == float_div.EnclosingLayer());
  ASSERT_TRUE(span_layer.NeedsPaintPhaseFloat());
  auto& self_painting_layer = *GetPaintLayerByElementId("self-painting-layer");
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  auto& non_self_painting_layer =
      *GetPaintLayerByElementId("non-self-painting-layer");
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(span_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_THAT(ContentDisplayItems(),
              Contains(IsSameId(float_div.Id(),
                                DisplayItem::kBoxDecorationBackground)));
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerAddition) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-layer'>
      <div style='height: 100px'>
        <div style='height: 20px; outline: 1px solid red;
            background-color: green'>outline and background</div>
        <div style='float: left'>float</div>
      </div>
    </div>
  )HTML");

  auto& layer_div = *To<LayoutBoxModelObject>(
      GetDocument()
          .getElementById(AtomicString("will-be-layer"))
          ->GetLayoutObject());
  EXPECT_FALSE(layer_div.HasLayer());

  PaintLayer& html_layer =
      *To<LayoutBoxModelObject>(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseFloat());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("position: relative"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-self-painting' style='width: 100px; height: 100px;
    overflow: hidden'>
      <div>
        <div style='outline: 1px solid red; background-color: green'>
          outline and background
        </div>
      </div>
    </div>
  )HTML");

  auto& layer_div = *To<LayoutBoxModelObject>(
      GetLayoutObjectByElementId("will-be-self-painting"));
  ASSERT_TRUE(layer_div.HasLayer());
  EXPECT_FALSE(layer_div.Layer()->IsSelfPaintingLayer());

  PaintLayer& html_layer =
      *To<LayoutBoxModelObject>(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("width: 100px; height: 100px; overflow: "
                                  "hidden; position: relative"));
  UpdateAllLifecyclePhasesForTest();
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingNonSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-non-self-painting' style='width: 100px; height: 100px;
    overflow: hidden; position: relative'>
      <div>
        <div style='outline: 1px solid red; background-color: green'>
          outline and background
        </div>
      </div>
    </div>
  )HTML");

  auto& layer_div = *To<LayoutBoxModelObject>(
      GetLayoutObjectByElementId("will-be-non-self-painting"));
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());

  PaintLayer& html_layer =
      *To<LayoutBoxModelObject>(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantOutlines());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(
          html_names::kStyleAttr,
          AtomicString("width: 100px; height: 100px; overflow: hidden"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintWithOverriddenCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id="stacking" style="opacity: 0.5; height: 200px;">
      <div id="absolute" style="position: absolute; height: 200px"></div>
    </div>
  )HTML");

  auto& stacking = *GetPaintLayerByElementId("stacking");
  auto& absolute = *GetPaintLayerByElementId("absolute");
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), GetCullRect(stacking).Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), GetCullRect(absolute).Rect());
  EXPECT_EQ(kFullyPainted, stacking.PreviousPaintResult());
  EXPECT_EQ(kFullyPainted, absolute.PreviousPaintResult());
  {
    OverriddenCullRectScope scope(stacking, CullRect(gfx::Rect(0, 0, 100, 100)),
                                  /*disable_expansion*/ false);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100), GetCullRect(stacking).Rect());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100), GetCullRect(absolute).Rect());
    PaintController controller;
    GraphicsContext context(controller);
    PaintLayerPainter(stacking).Paint(context);
  }
  // Should restore the original status after OverridingCullRectScope.
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), GetCullRect(stacking).Rect());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), GetCullRect(absolute).Rect());
  EXPECT_EQ(kFullyPainted, stacking.PreviousPaintResult());
  EXPECT_EQ(kFullyPainted, absolute.PreviousPaintResult());
  EXPECT_FALSE(stacking.SelfOrDescendantNeedsRepaint());
  EXPECT_FALSE(absolute.SelfOrDescendantNeedsRepaint());
}

class PaintLayerPainterPaintedOutputInvisibleTest
    : public PaintLayerPainterTest {
 protected:
  void RunTest() {
    SetBodyInnerHTML(R"HTML(
      <div id="parent">
        <div id="target">
          <div id="child"></div>
        </div>
      </div>
      <style>
        #parent {
          width: 10px;
          height: 10px;
          will-change: transform;
        }
        #target {
          width: 100px;
          height: 100px;
          opacity: 0.0001;
        }
        #child {
          width: 200px;
          height: 50px;
          opacity: 0.9;
        }
    )HTML" + additional_style_ +
                     "</style>");

    auto* parent = GetLayoutObjectByElementId("parent");
    auto* parent_layer = To<LayoutBox>(parent)->Layer();
    auto* target = GetLayoutObjectByElementId("target");
    auto* target_layer = To<LayoutBox>(target)->Layer();
    auto* child = GetLayoutObjectByElementId("child");
    auto* child_layer = To<LayoutBox>(child)->Layer();

    EXPECT_EQ(expected_invisible_,
              PaintLayerPainter::PaintedOutputInvisible(
                  target_layer->GetLayoutObject().StyleRef()));

    auto* cc_layer =
        CcLayersByDOMElementId(GetDocument().View()->RootCcLayer(),
                               expected_composited_ ? "target" : "parent")[0];
    ASSERT_TRUE(cc_layer);
    EXPECT_EQ(gfx::Size(200, 100), cc_layer->bounds());

    auto chunks = ContentPaintChunks();
    EXPECT_THAT(
        chunks,
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(parent_layer->Id(), DisplayItem::kLayerChunk),
                parent->FirstFragment().LocalBorderBoxProperties(), nullptr,
                gfx::Rect(0, 0, 10, 10)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(target_layer->Id(), DisplayItem::kLayerChunk),
                target->FirstFragment().LocalBorderBoxProperties(), nullptr,
                gfx::Rect(0, 0, 100, 100)),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(child_layer->Id(), DisplayItem::kLayerChunk),
                child->FirstFragment().LocalBorderBoxProperties(), nullptr,
                gfx::Rect(0, 0, 200, 50))));
    EXPECT_FALSE(chunks[1].effectively_invisible);
    EXPECT_EQ(expected_invisible_, chunks[2].effectively_invisible);
    EXPECT_EQ(expected_invisible_, chunks[3].effectively_invisible);
  }

  String additional_style_;
  bool expected_composited_ = false;
  bool expected_invisible_ = true;
  bool expected_paints_with_transparency_ = true;
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintLayerPainterPaintedOutputInvisibleTest);

TEST_P(PaintLayerPainterPaintedOutputInvisibleTest, TinyOpacity) {
  expected_composited_ = false;
  expected_invisible_ = true;
  expected_paints_with_transparency_ = true;
  RunTest();
}

TEST_P(PaintLayerPainterPaintedOutputInvisibleTest,
       TinyOpacityAndWillChangeOpacity) {
  additional_style_ = "#target { will-change: opacity; }";
  expected_composited_ = true;
  expected_invisible_ = false;
  expected_paints_with_transparency_ = false;
  RunTest();
}

TEST_P(PaintLayerPainterPaintedOutputInvisibleTest,
       TinyOpacityAndBackdropFilter) {
  additional_style_ = "#target { backdrop-filter: blur(2px); }";
  expected_composited_ = true;
  expected_invisible_ = false;
  expected_paints_with_transparency_ = false;
  RunTest();
}

TEST_P(PaintLayerPainterPaintedOutputInvisibleTest,
       TinyOpacityAndWillChangeTransform) {
  additional_style_ = "#target { will-change: transform; }";
  expected_composited_ = true;
  expected_invisible_ = true;
  expected_paints_with_transparency_ = false;
  RunTest();
}

TEST_P(PaintLayerPainterPaintedOutputInvisibleTest, NonTinyOpacity) {
  additional_style_ = "#target { opacity: 0.5; }";
  expected_composited_ = false;
  expected_invisible_ = false;
  expected_paints_with_transparency_ = true;
  RunTest();
}

TEST_P(PaintLayerPainterPaintedOutputInvisibleTest,
       NonTinyOpacityAndWillChangeOpacity) {
  additional_style_ = "#target { opacity: 1; will-change: opacity; }";
  expected_composited_ = true;
  expected_invisible_ = false;
  expected_paints_with_transparency_ = false;
  RunTest();
}

}  // namespace blink
