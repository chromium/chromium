// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
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
  PaintController& MainGraphicsLayerPaintController() {
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking(&GetLayoutView())
        ->GetPaintController();
  }

  CullRect GetCullRect(const PaintLayer& layer) {
    if (RuntimeEnabledFeatures::CullRectUpdateEnabled())
      return layer.GetLayoutObject().FirstFragment().GetCullRect();
    return layer.PreviousCullRect();
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
        ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                    IsSameId(GetDisplayItemClientFromLayoutObject(container1),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(content1),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(filler1),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(container2),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(content2),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(filler2),
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
            IsPaintChunk(
                1, 2,
                PaintChunk::Id(*container1_layer, DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 0, 200, 200)),
            IsPaintChunk(
                2, 3, PaintChunk::Id(*content1_layer, DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 0, 100, 100)),
            IsPaintChunk(
                3, 4, PaintChunk::Id(*filler1_layer, DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 200, 20, 20)),
            IsPaintChunk(
                4, 5,
                PaintChunk::Id(*container2_layer, DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 220, 200, 200)),
            IsPaintChunk(
                5, 6, PaintChunk::Id(*content2_layer, DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 220, 100, 100)),
            IsPaintChunk(
                6, 7, PaintChunk::Id(*filler2_layer, DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 420, 20, 20))));
  };

  check_results();

  To<HTMLElement>(content1->GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  PaintController::CounterForTesting counter;
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(6u, counter.num_cached_items);
  EXPECT_EQ(4u, counter.num_cached_subsequences);

  // We should still have the paint chunks forced by the cached subsequences.
  check_results();
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceAndChunksWithoutBackgrounds) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

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
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*container_layer, chunks.begin() + 1, 5);
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*content_layer, chunks.begin() + 3, 2);
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*inner_content_layer, chunks.begin() + 4, 1);
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler_layer, chunks.begin() + 5, 1);

  auto container_properties =
      container->FirstFragment().LocalBorderBoxProperties();
  auto content_properties = container->FirstFragment().ContentsProperties();
  HitTestData scroll_hit_test;
  scroll_hit_test.scroll_translation =
      container->FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 150, 150);

  EXPECT_THAT(
      chunks,
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container_layer, DisplayItem::kLayerChunk),
              container_properties, nullptr, IntRect(0, 0, 150, 150)),
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container, DisplayItem::kScrollHitTest),
              container_properties, &scroll_hit_test, IntRect(0, 0, 150, 150)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(*content_layer, DisplayItem::kLayerChunk),
                       content_properties, nullptr, IntRect(0, 0, 200, 100)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*inner_content_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 0, 100, 100)),
          IsPaintChunk(
              1, 1, PaintChunk::Id(*filler_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 100, 300, 300))));

  To<HTMLElement>(inner_content->GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "top: 100px; background-color: green");
  PaintController::CounterForTesting counter;
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1u, counter.num_cached_items);         // view background.
  EXPECT_EQ(1u, counter.num_cached_subsequences);  // filler layer.

  EXPECT_THAT(
      ContentDisplayItems(),
      ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(GetDisplayItemClientFromLayoutObject(inner_content),
                           kBackgroundType)));

  chunks = ContentPaintChunks();
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*container_layer, chunks.begin() + 1, 5);
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*content_layer, chunks.begin() + 3, 2);
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*inner_content_layer, chunks.begin() + 4, 1);
  EXPECT_SUBSEQUENCE_FROM_CHUNK(*filler_layer, chunks.begin() + 5, 1);

  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container_layer, DisplayItem::kLayerChunk),
              container_properties, nullptr, IntRect(0, 0, 150, 150)),
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container, DisplayItem::kScrollHitTest),
              container_properties, &scroll_hit_test, IntRect(0, 0, 150, 150)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(*content_layer, DisplayItem::kLayerChunk),
                       content_properties, nullptr, IntRect(0, 0, 200, 100)),
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(*inner_content_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 100, 100, 100)),
          IsPaintChunk(
              2, 2, PaintChunk::Id(*filler_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 100, 300, 300))));
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
  PaintContents(IntRect(0, 0, 400, 300));

  // Container1 is fully in the interest rect;
  // Container2 is partly (including its stacking chidren) in the interest rect;
  // Content2b is out of the interest rect and output nothing;
  // Container3 is partly in the interest rect.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2a, kBackgroundType),
                          IsSameId(&container3, kBackgroundType),
                          IsSameId(&content3, kBackgroundType)));

  UpdateAllLifecyclePhasesExceptPaint();
  PaintController::CounterForTesting counter;
  PaintContents(IntRect(0, 100, 300, 1000));
  // Container1 becomes partly in the interest rect, but uses cached subsequence
  // because it was fully painted before;
  // Container2's intersection with the interest rect changes;
  // Content2b is out of the interest rect and outputs nothing;
  // Container3 becomes out of the interest rect and outputs nothing.
  EXPECT_EQ(5u, counter.num_cached_items);
  EXPECT_EQ(2u, counter.num_cached_subsequences);

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2a, kBackgroundType),
                          IsSameId(&content2b, kBackgroundType)));
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
  PaintContents(IntRect(0, 0, 400, 300));

  // |target| will be partially painted. Should not trigger under-invalidation
  // checking DCHECKs.
  UpdateAllLifecyclePhasesExceptPaint();
  PaintContents(IntRect(0, 100, 300, 1000));
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
  PaintContents(IntRect(0, 0, 50, 300));

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
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType)));

  To<HTMLElement>(GetElementById("content1"))
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  UpdateAllLifecyclePhasesExceptPaint();
  PaintController::CounterForTesting counter;
  PaintContents(IntRect(0, 0, 50, 300));
  EXPECT_EQ(4u, counter.num_cached_items);
  EXPECT_EQ(1u, counter.num_cached_subsequences);

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType)));
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
                          IsSameId(content1, kBackgroundType)));
  EXPECT_EQ(IntRect(0, 0, 800, 4600), GetCullRect(*target_layer).Rect());
  auto chunks = ContentPaintChunks();
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // |target| still created subsequence (cached).
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 2);
    EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                    IsPaintChunk(1, 1), IsPaintChunk(1, 2)));
  } else {
    EXPECT_THAT(ContentDisplayItems(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                            IsSameId(content1, kBackgroundType)));
    // |target| still created subsequence (cached).
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 1);
    EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                    IsPaintChunk(1, 2)));
  }

  // Change something that triggers a repaint but |target| should use cached
  // subsequence.
  GetDocument().getElementById("change")->setAttribute(html_names::kStyleAttr,
                                                       "display: block");
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
                          IsSameId(content1, kBackgroundType)));
  EXPECT_EQ(IntRect(0, 0, 800, 4600), GetCullRect(*target_layer).Rect());
  chunks = ContentPaintChunks();
  EXPECT_EQ(CullRect(IntRect(0, 0, 800, 4600)), GetCullRect(*target_layer));
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_THAT(ContentDisplayItems(),
                ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                            IsSameId(content1, kBackgroundType)));
    // |target| still created subsequence (cached).
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 2);
    EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                    IsPaintChunk(1, 1), IsPaintChunk(1, 2)));
  } else {
    // |target| still created subsequence (cached).
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 1);
    EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                    IsPaintChunk(1, 2)));
  }

  // Scroll the view so that both |content1| and |content2| are in the interest
  // rect.
  GetLayoutView().GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 3000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesExceptPaint();
  if (RuntimeEnabledFeatures::CullRectUpdateEnabled()) {
    // The layer needs repaint when its contents cull rect changes.
    EXPECT_TRUE(target_layer->SelfNeedsRepaint());
  } else {
    // Scrolling doesn't set SelfNeedsRepaint flag. Change of paint dirty rect
    // of a partially painted layer will trigger repaint.
    EXPECT_FALSE(target_layer->SelfNeedsRepaint());
  }

  counter.Reset();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2u, counter.num_cached_items);
  EXPECT_EQ(0u, counter.num_cached_subsequences);

  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  // Painted result should include both |content1| and |content2|.
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(content1, kBackgroundType),
                          IsSameId(content2, kBackgroundType)));
  EXPECT_EQ(IntRect(0, 0, 800, 7600), GetCullRect(*target_layer).Rect());
  chunks = ContentPaintChunks();
  EXPECT_EQ(CullRect(IntRect(0, 0, 800, 7600)), GetCullRect(*target_layer));
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // |target| still created subsequence (repainted).
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 2);
    EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                    IsPaintChunk(1, 1), IsPaintChunk(1, 3)));
  } else {
    // |target| still created subsequence (repainted).
    EXPECT_SUBSEQUENCE_FROM_CHUNK(*target_layer, chunks.begin() + 1, 1);
    EXPECT_THAT(chunks, ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                                    IsPaintChunk(1, 3)));
  }
}

TEST_P(PaintLayerPainterTest, HintedPaintChunksWithBackgrounds) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      div { background: blue }
    </style>
    <div id='container1' style='position: relative; height: 150px; z-index: 1'>
      <div id='content1a' style='overflow: hidden; height: 100px'></div>
      <div id='content1b' style='overflow: hidden; height: 100px'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1'>
      <div id='content2a' style='overflow: hidden; height: 100px'></div>
      <div id='content2b'
           style='position: relative; z-index: -1; height: 100px'></div>
    </div>
  )HTML");

  auto* container1 = GetLayoutBoxByElementId("container1");
  auto* content1a = GetLayoutBoxByElementId("content1a");
  auto* content1b = GetLayoutBoxByElementId("content1b");
  auto* container2 = GetLayoutBoxByElementId("container2");
  auto* content2a = GetLayoutBoxByElementId("content2a");
  auto* content2b = GetLayoutBoxByElementId("content2b");
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(container1, kBackgroundType),
                          IsSameId(content1a, kBackgroundType),
                          IsSameId(content1b, kBackgroundType),
                          IsSameId(container2, kBackgroundType),
                          IsSameId(content2b, kBackgroundType),
                          IsSameId(content2a, kBackgroundType)));

  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          // Includes |container1| and |content1a|.
          IsPaintChunk(
              1, 4,
              PaintChunk::Id(*container1->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 0, 800, 200)),
          IsPaintChunk(
              4, 5,
              PaintChunk::Id(*container2->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 150, 800, 200)),
          IsPaintChunk(
              5, 6,
              PaintChunk::Id(*content2b->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 250, 800, 100)),
          IsPaintChunk(6, 7,
                       PaintChunk::Id(*container2->Layer(),
                                      DisplayItem::kLayerChunkForeground),
                       chunk_state, nullptr, IntRect(0, 150, 800, 100))));
}

TEST_P(PaintLayerPainterTest, HintedPaintChunksWithoutBackgrounds) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id='container1' style='position: relative; height: 150px; z-index: 1'>
      <div id='content1a' style='overflow: hidden; height: 100px'></div>
      <div id='content1b' style='overflow: hidden; height: 100px'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1'>
      <div id='content2a' style='overflow: hidden; height: 100px'></div>
      <div id='content2b'
           style='position: relative; z-index: -1; height: 100px'></div>
    </div>
  )HTML");

  auto* container1 = GetLayoutBoxByElementId("container1");
  auto* container2 = GetLayoutBoxByElementId("container2");
  auto* content2b = GetLayoutBoxByElementId("content2b");
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM));

  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*container1->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 0, 800, 200)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*container2->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 150, 800, 200)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*content2b->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 250, 800, 100)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(*container2->Layer(),
                                      DisplayItem::kLayerChunkForeground),
                       chunk_state, nullptr, IntRect(0, 150, 800, 100))));
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline) {
  AtomicString style_without_outline =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_outline =
      "outline: 1px solid blue; " + style_without_outline;
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
      *GetDocument().getElementById("outline")->GetLayoutObject();
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();

  auto& self_painting_layer_object = *To<LayoutBoxModelObject>(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
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
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; outline: 1px solid green");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_THAT(ContentDisplayItems(),
              Contains(IsSameId(&self_painting_layer_object,
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
      Contains(IsSameId(&outline_div, DisplayItem::PaintPhaseToDrawingType(
                                          PaintPhase::kSelfOutlineOnly))));

  // needsPaintPhaseDescendantOutlines should be reset when no outline is
  // actually painted.
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat) {
  AtomicString style_without_float =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_float = "float: left; " + style_without_float;
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
      *GetDocument().getElementById("float")->GetLayoutObject();
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();

  auto& self_painting_layer_object = *To<LayoutBoxModelObject>(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
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
  EXPECT_THAT(
      ContentDisplayItems(),
      Contains(IsSameId(&float_div, DisplayItem::kBoxDecorationBackground)));

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
      *GetDocument().getElementById("float")->GetLayoutObject();
  PaintLayer& span_layer = *GetPaintLayerByElementId("span");
  ASSERT_TRUE(&span_layer == float_div.EnclosingLayer());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    ASSERT_TRUE(span_layer.NeedsPaintPhaseFloat());
  } else {
    ASSERT_FALSE(span_layer.NeedsPaintPhaseFloat());
  }
  auto& self_painting_layer = *GetPaintLayerByElementId("self-painting-layer");
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  auto& non_self_painting_layer =
      *GetPaintLayerByElementId("non-self-painting-layer");
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
    EXPECT_TRUE(span_layer.NeedsPaintPhaseFloat());
  } else {
    EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
    EXPECT_FALSE(span_layer.NeedsPaintPhaseFloat());
  }
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_THAT(
      ContentDisplayItems(),
      Contains(IsSameId(&float_div, DisplayItem::kBoxDecorationBackground)));
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
      GetDocument().getElementById("will-be-layer")->GetLayoutObject());
  EXPECT_FALSE(layer_div.HasLayer());

  PaintLayer& html_layer =
      *To<LayoutBoxModelObject>(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseFloat());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, "position: relative");
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
      ->setAttribute(
          html_names::kStyleAttr,
          "width: 100px; height: 100px; overflow: hidden; position: relative");
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
      ->setAttribute(html_names::kStyleAttr,
                     "width: 100px; height: 100px; overflow: hidden");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
}

using PaintLayerPainterTestCAP = PaintLayerPainterTest;

INSTANTIATE_CAP_TEST_SUITE_P(PaintLayerPainterTestCAP);

TEST_P(PaintLayerPainterTestCAP, SimpleCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 200px; height: 200px; position: relative'>
    </div>
  )HTML");

  EXPECT_EQ(IntRect(0, 0, 800, 600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, TallLayerCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 200px; height: 10000px; position: relative'>
    </div>
  )HTML");

  // Viewport rect (0, 0, 800, 600) expanded by 4000 for scrolling then clipped
  // by the contents rect.
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, WideLayerCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 10000px; height: 200px; position: relative'>
    </div>
  )HTML");

  // Same as TallLayerCullRect.
  EXPECT_EQ(IntRect(0, 0, 4800, 600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, TallScrolledLayerCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 200px; height: 12000px; position: relative'>
    </div>
  )HTML");

  // Viewport rect (0, 0, 800, 600) expanded by 4000 for scrolling then clipped
  // by the contents rect.
  EXPECT_EQ(IntRect(0, 0, 800, 4600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 4000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 0, 800, 8600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 4500), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Used the previous cull rect because the scroll amount is small.
  EXPECT_EQ(IntRect(0, 0, 800, 8600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 4600), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Used new cull rect.
  EXPECT_EQ(IntRect(0, 600, 800, 8600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, WholeDocumentCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  GetDocument().GetSettings()->SetMainFrameClipsContent(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      div { background: blue; }
      ::-webkit-scrollbar { display: none; }
    </style>
    <div id='relative'
         style='width: 200px; height: 10000px; position: relative'>
    </div>
    <div id='fixed' style='width: 200px; height: 200px; position: fixed'>
    </div>
    <div id='scroll' style='width: 200px; height: 200px; overflow: scroll'>
      <div id='below-scroll' style='height: 5000px; position: relative'></div>
      <div style='height: 200px'>Should not paint</div>
    </div>
    <div id='normal' style='width: 200px; height: 200px'></div>
  )HTML");

  // Viewport clipping is disabled.
  EXPECT_TRUE(GetCullRect(*GetLayoutView().Layer()).IsInfinite());
  EXPECT_TRUE(GetCullRect(*GetPaintLayerByElementId("relative")).IsInfinite());
  EXPECT_TRUE(GetCullRect(*GetPaintLayerByElementId("fixed")).IsInfinite());
  EXPECT_TRUE(GetCullRect(*GetPaintLayerByElementId("scroll")).IsInfinite());

  // Cull rect is normal for contents below scroll other than the viewport.
  EXPECT_EQ(IntRect(0, 0, 200, 4200),
            GetCullRect(*GetPaintLayerByElementId("below-scroll")).Rect());

  EXPECT_THAT(ContentDisplayItems(),
              UnorderedElementsAre(
                  VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                  IsSameId(GetDisplayItemClientFromElementId("relative"),
                           kBackgroundType),
                  IsSameId(GetDisplayItemClientFromElementId("normal"),
                           kBackgroundType),
                  IsSameId(GetDisplayItemClientFromElementId("scroll"),
                           kBackgroundType),
                  IsSameId(&GetLayoutBoxByElementId("scroll")
                                ->GetScrollableArea()
                                ->GetScrollingBackgroundDisplayItemClient(),
                           kBackgroundType),
                  IsSameId(GetDisplayItemClientFromElementId("below-scroll"),
                           kBackgroundType),
                  IsSameId(GetDisplayItemClientFromElementId("fixed"),
                           kBackgroundType)));
}

TEST_P(PaintLayerPainterTestCAP, VerticalRightLeftWritingModeDocument) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { writing-mode: vertical-rl; }
      body { margin: 0; }
    </style>
    <div id='target' style='width: 10000px; height: 200px; position: relative'>
    </div>
  )HTML");

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(-5000, 0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  // A scroll by -5000px is equivalent to a scroll by (10000 - 5000 - 800)px =
  // 4200px in non-RTL mode. Expanding the resulting rect by 4000px in each
  // direction and clipping by the contents rect yields this result.
  EXPECT_EQ(IntRect(200, 0, 8800, 600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

// TODO(wangxianzhu): These tests should correspond to the tests in
// CompositedLayerMapping testing interest rects. However, for now because in
// CompositeAfterPaint we expand cull rect for composited scrollers only, so
// the tests are modified to use composited scrolling. Will change these back to
// their original version when we support expansion for all composited layers.
// Will be done in CullRectUpdate.
TEST_P(PaintLayerPainterTestCAP, ScaledCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: scaleX(3) scaleY(0.5)'>
      <div id='target' style='height: 400px; position: relative'></div>
      <div style='width: 10000px; height: 10000px'></div>
    </div>
  )HTML");

  // The expansion is 4000 / max(scaleX, scaleY).
  EXPECT_EQ(IntRect(0, 0, 8200, 8300),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, ScaledAndRotatedCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: scaleX(3) scaleY(0.5) rotateZ(45deg)'>
      <div id='target' style='height: 400px; position: relative;
               will-change: transform'></div>
      <div style='width: 10000px; height: 10000px'></div>
    </div>
  )HTML");

  // The expansion 6599 is 4000 * max_dimension(1x1 rect projected from screen
  // to local).
  EXPECT_EQ(IntRect(0, 0, 6799, 6899),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, 3DRotated90DegreesCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: rotateY(90deg)'>
      <div id='target' style='height: 400px; position: relative'></div>
      <div style='width: 10000px; height: 10000px'></div>
    </div>
  )HTML");

  // It's rotated 90 degrees about the X axis, which means its visual content
  // rect is empty, we fall back to the 4000px cull rect padding amount.
  EXPECT_EQ(IntRect(0, 0, 4200, 4300),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, 3DRotatedNear90DegreesCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: rotateY(89.9999deg)'>
      <div id='target' style='height: 400px; position: relative'></div>
      <div style='width: 10000px; height: 10000px'></div>
    </div>
  )HTML");

  // Because the layer is rotated to almost 90 degrees, floating-point error
  // leads to a reverse-projected rect that is much much larger than the
  // original layer size in certain dimensions. In such cases, we often fall
  // back to the 4000px cull rect padding amount.
  EXPECT_EQ(IntRect(0, 0, 4200, 4300),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, PerspectiveCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id=target style='transform: perspective(1000px) rotateX(-100deg);'>
      <div style='width: 2000px; height: 3000px></div>
    </div>
  )HTML");

  EXPECT_TRUE(GetCullRect(*GetPaintLayerByElementId("target"))
                  .Rect()
                  .Contains(IntRect(0, 0, 2000, 3000)));
}

TEST_P(PaintLayerPainterTestCAP, 3D45DegRotatedTallCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 200px; height: 10000px; transform: rotateY(45deg)'>
    </div>
  )HTML");

  // See CompositedLayerMappingTest.3D45DegRotatedTallInterestRect (which with
  // be combined with this one) for why the cull rect covers the whole layer.
  EXPECT_TRUE(GetCullRect(*GetPaintLayerByElementId("target"))
                  .Rect()
                  .Contains(IntRect(0, 0, 200, 10000)));
}

TEST_P(PaintLayerPainterTestCAP, FixedPositionInNonScrollableViewCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 1000px; height: 2000px;
                            position: fixed; top: 100px; left: 200px;'>
    </div>
  )HTML");

  // The cull rect is in the coordinate space of the containing transform
  // (LayoutView's contents space).
  EXPECT_EQ(IntRect(0, 0, 800, 600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, FixedPositionInScrollableViewCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 1000px; height: 2000px;
                            position: fixed; top: 100px; left: 200px;'>
    </div>
    <div style='height: 3000px'></div>
  )HTML");

  EXPECT_EQ(IntRect(-200, -100, 800, 600),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, LayerOffscreenNearCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                position: absolute; top: 3000px; left: 0px;'>
      <div id='target' style='height: 500px; position: relative'></div>
      <div style='width: 10000px; height: 10000px'></div>
    </div>
  )HTML");

  EXPECT_EQ(IntRect(0, 0, 4200, 4300),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, LayerOffscreenFarCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                position: absolute; top: 9000px'>
      <div id='target' style='height: 500px; position: relative'></div>
      <div style='width: 10000px; height: 10000px'></div>
    </div>
  )HTML");

  // The layer is too far away from the viewport.
  EXPECT_EQ(IntRect(), GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, ScrollingLayerCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      div::-webkit-scrollbar { width: 5px; }
    </style>
    <div style='width: 200px; height: 200px; overflow: scroll'>
      <div id='target'
           style='width: 100px; height: 10000px; position: relative'>
      </div>
    </div>
  )HTML");

  // In screen space, the scroller is (8, 8, 195, 193) (because of overflow clip
  // of 'target', scrollbar and root margin).
  // Applying the viewport clip of the root has no effect because
  // the clip is already small. Mapping it down into the graphics layer
  // space yields (0, 0, 195, 193). This is then expanded by 4000px and clipped
  // by the contents rect.
  EXPECT_EQ(IntRect(0, 0, 195, 4193),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, NonCompositedScrollingLayerCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      div::-webkit-scrollbar { width: 5px; }
    </style>
    <div style='width: 200px; height: 200px; overflow: scroll'>
      <div id='target'
           style='width: 100px; height: 10000px; position: relative'>
      </div>
    </div>
  )HTML");

  // See ScrollingLayerCullRect for the calculation.
  EXPECT_EQ(IntRect(0, 0, 195, 193),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
}

TEST_P(PaintLayerPainterTestCAP, ClippedBigLayer) {
  SetBodyInnerHTML(R"HTML(
    <div style='width: 1px; height: 1px; overflow: hidden'>
      <div id='target'
           style='width: 10000px; height: 10000px; position: relative'>
      </div>
    </div>
  )HTML");

  EXPECT_EQ(IntRect(8, 8, 1, 1),
            GetCullRect(*GetPaintLayerByElementId("target")).Rect());
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
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      EXPECT_THAT(
          chunks,
          ElementsAre(
              VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
              IsPaintChunk(
                  1, 1, PaintChunk::Id(*parent_layer, DisplayItem::kLayerChunk),
                  parent->FirstFragment().LocalBorderBoxProperties(), nullptr,
                  IntRect(0, 0, 10, 10)),
              IsPaintChunk(
                  1, 1, PaintChunk::Id(*target_layer, DisplayItem::kLayerChunk),
                  target->FirstFragment().LocalBorderBoxProperties(), nullptr,
                  IntRect(0, 0, 100, 100)),
              IsPaintChunk(
                  1, 1, PaintChunk::Id(*child_layer, DisplayItem::kLayerChunk),
                  child->FirstFragment().LocalBorderBoxProperties(), nullptr,
                  IntRect(0, 0, 200, 50))));
      EXPECT_FALSE((chunks.begin() + 1)->effectively_invisible);
      EXPECT_EQ(expected_invisible_,
                (chunks.begin() + 2)->effectively_invisible);
      EXPECT_EQ(expected_invisible_,
                (chunks.begin() + 3)->effectively_invisible);
    } else {
      EXPECT_EQ(expected_paints_with_transparency_,
                target_layer->PaintsWithTransparency(kGlobalPaintNormalPhase));
    }
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
