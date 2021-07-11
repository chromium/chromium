// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using ::testing::MatchesRegex;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

void SetUpHTML(PaintAndRasterInvalidationTest& test) {
  test.SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 0;
      }
      ::-webkit-scrollbar { display: none }
      #target {
        width: 50px;
        height: 100px;
        transform-origin: 0 0;
      }
      .solid {
        background: blue;
      }
      .translucent {
        background: rgba(0, 0, 255, 0.5);
      }
      .gradient {
        background-image: linear-gradient(blue, yellow);
      }
      .scroll {
        overflow: scroll;
      }
      .solid-composited-scroller {
        overflow: scroll;
        will-change: transform;
        background: blue;
      }
      .local-attachment {
        background-attachment: local;
      }
      .transform {
        transform: scale(2);
      }
      .border {
        border: 10px solid black;
      }
      .composited {
        will-change: transform;
      }
    </style>
    <div id='target' class='solid'></div>
  )HTML");
}

INSTANTIATE_PAINT_TEST_SUITE_P(PaintAndRasterInvalidationTest);

class ScopedEnablePaintInvalidationTracing {
 public:
  ScopedEnablePaintInvalidationTracing() {
    trace_event::EnableTracing(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"));
  }
  ~ScopedEnablePaintInvalidationTracing() { trace_event::DisableTracing(); }
};

TEST_P(PaintAndRasterInvalidationTest, TrackingForTracing) {
  SetBodyInnerHTML(R"HTML(
    <style>#target { width: 100px; height: 100px; background: blue }</style>
    <div id="target"></div>
  )HTML");
  auto* target = GetDocument().getElementById("target");
  auto& cc_layer =
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
          ? *GetDocument()
                 .View()
                 ->GetPaintArtifactCompositor()
                 ->RootLayer()
                 ->children()[1]
          : GetLayoutView().Layer()->GraphicsLayerBacking()->CcLayer();

  {
    ScopedEnablePaintInvalidationTracing tracing;

    target->setAttribute(html_names::kStyleAttr, "height: 200px");
    UpdateAllLifecyclePhasesForTest();
    ASSERT_TRUE(cc_layer.debug_info());
    EXPECT_EQ(1u, cc_layer.debug_info()->invalidations.size());

    target->setAttribute(html_names::kStyleAttr, "height: 200px; width: 200px");
    UpdateAllLifecyclePhasesForTest();
    ASSERT_TRUE(cc_layer.debug_info());
    EXPECT_EQ(2u, cc_layer.debug_info()->invalidations.size());
  }

  target->setAttribute(html_names::kStyleAttr, "height: 300px; width: 300px");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(cc_layer.debug_info());
  // No new invalidations tracked.
  EXPECT_EQ(2u, cc_layer.debug_info()->invalidations.size());
}

TEST_P(PaintAndRasterInvalidationTest, IncrementalInvalidationExpand) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 100px; height: 200px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(50, 0, 50, 200),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{
                      object, object->DebugName(), IntRect(0, 100, 100, 100),
                      PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, IncrementalInvalidationShrink) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 20px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(20, 0, 30, 100),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{
                      object, object->DebugName(), IntRect(0, 80, 50, 20),
                      PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, IncrementalInvalidationMixed) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 100px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(50, 0, 50, 80),
                                         PaintInvalidationReason::kIncremental},
                  RasterInvalidationInfo{
                      object, object->DebugName(), IntRect(0, 80, 50, 20),
                      PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeEmptyContent) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  // Make the box empty.
  target->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 100px; height: 80px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SubpixelChange) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       "width: 100.6px; height: 70.3px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 50, 100),
                                         PaintInvalidationReason::kGeometry},
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 101, 70),
                                         PaintInvalidationReason::kGeometry}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 50px; height: 100px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 50, 100),
                                         PaintInvalidationReason::kGeometry},
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 101, 70),
                                         PaintInvalidationReason::kGeometry}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SubpixelVisualRectChangeWithTransform) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kClassAttr, "solid transform");
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       "width: 100.6px; height: 70.3px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 100, 200),
                                         PaintInvalidationReason::kGeometry},
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 202, 140),
                                         PaintInvalidationReason::kGeometry}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 50px; height: 100px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 100, 200),
                                         PaintInvalidationReason::kGeometry},
                  RasterInvalidationInfo{object, object->DebugName(),
                                         IntRect(0, 0, 202, 140),
                                         PaintInvalidationReason::kGeometry}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SubpixelWithinPixelsChange) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  LayoutObject* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       "margin-top: 0.6px; width: 50px; height: 99.3px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object, object->DebugName(), IntRect(0, 0, 50, 100),
                  PaintInvalidationReason::kGeometry}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       "margin-top: 0.6px; width: 49.3px; height: 98.5px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object, object->DebugName(), IntRect(0, 1, 50, 99),
                  PaintInvalidationReason::kGeometry}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeRotated) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kStyleAttr, "transform: rotate(45deg)");
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       "transform: rotate(45deg); width: 200px");
  UpdateAllLifecyclePhasesForTest();
  auto expected_rect = EnclosingIntRect(
      TransformationMatrix().Rotate(45).MapRect(FloatRect(50, 0, 150, 100)));
  expected_rect.Intersect(IntRect(0, 0, 800, 600));
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object, object->DebugName(), expected_rect,
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeRotatedChild) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kStyleAttr,
                       "transform: rotate(45deg); width: 200px");
  target->setInnerHTML(
      "<div id=child style='width: 50px; height: 50px; background: "
      "red'></div>");
  UpdateAllLifecyclePhasesForTest();
  Element* child = GetDocument().getElementById("child");
  auto* child_object = child->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr,
                      "width: 100px; height: 50px; background: red");
  UpdateAllLifecyclePhasesForTest();
  auto expected_rect = EnclosingIntRect(
      TransformationMatrix().Rotate(45).MapRect(FloatRect(50, 0, 50, 50)));
  expected_rect.Intersect(IntRect(0, 0, 800, 600));
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  child_object, child_object->DebugName(), expected_rect,
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, CompositedLayoutViewResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kClassAttr, "");
  target->setAttribute(html_names::kStyleAttr, "height: 2000px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetLayoutView().GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "height: 3000px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          &ViewScrollingBackgroundClient(),
          ViewScrollingBackgroundClient().DebugName(),
          IntRect(0, 2000, 800, 1000), PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the viewport. No invalidation.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().View()->Resize(800, 1000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, CompositedLayoutViewGradientResize) {
  SetUpHTML(*this);
  GetDocument().body()->setAttribute(html_names::kClassAttr, "gradient");
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kClassAttr, "");
  target->setAttribute(html_names::kStyleAttr, "height: 2000px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetLayoutView().GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "height: 3000px");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          &ViewScrollingBackgroundClient(),
          ViewScrollingBackgroundClient().DebugName(), IntRect(0, 0, 800, 3000),
          PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the viewport. No invalidation.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().View()->Resize(800, 1000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

static const LayoutBoxModelObject& EnclosingCompositedContainer(
    const LayoutObject& layout_object) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  return layout_object.PaintingLayer()
      ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries()
      ->GetLayoutObject();
}

TEST_P(PaintAndRasterInvalidationTest, NonCompositedLayoutViewResize) {
  ScopedPreferNonCompositedScrollingForTest non_composited_scrolling(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      iframe { display: block; width: 100px; height: 100px; border: none; }
    </style>
    <iframe id='iframe'></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body { margin: 0; background: green; height: 0 }
    </style>
    <div id='content' style='width: 200px; height: 200px'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* iframe = GetDocument().getElementById("iframe");
  Element* content = ChildDocument().getElementById("content");
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetLayoutView(),
              EnclosingCompositedContainer(*content->GetLayoutObject()));
  }
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            content->GetLayoutObject()
                ->View()
                ->ComputeBackgroundPaintLocationIfComposited());
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            content->GetLayoutObject()->View()->GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  content->setAttribute(html_names::kStyleAttr, "height: 500px");
  UpdateAllLifecyclePhasesForTest();
  // No invalidation because the changed part of layout overflow is clipped.
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the iframe.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  iframe->setAttribute(html_names::kStyleAttr, "height: 200px");
  UpdateAllLifecyclePhasesForTest();
  // The iframe doesn't have anything visible by itself, so we only issue
  // raster invalidation for the frame contents.
  const auto* client = content->GetLayoutObject()->View();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client, client->DebugName(), IntRect(0, 100, 100, 100),
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, FullInvalidationWithHTMLTransform) {
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "transform: scale(0.5)");
  const DisplayItemClient* client =
      &GetDocument()
           .View()
           ->GetLayoutView()
           ->GetScrollableArea()
           ->GetScrollingBackgroundDisplayItemClient();
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().View()->Resize(IntSize(500, 500));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{client, client->DebugName(),
                                         IntRect(0, 0, 500, 500),
                                         PaintInvalidationReason::kBackground},
                  RasterInvalidationInfo{
                      client, client->DebugName(), IntRect(0, 0, 500, 500),
                      PaintInvalidationReason::kPaintProperty}));
}

TEST_P(PaintAndRasterInvalidationTest, NonCompositedLayoutViewGradientResize) {
  ScopedPreferNonCompositedScrollingForTest non_composited_scrolling(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      iframe { display: block; width: 100px; height: 100px; border: none; }
    </style>
    <iframe id='iframe'></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      ::-webkit-scrollbar { display: none }
      body {
        margin: 0;
        height: 0;
        background-image: linear-gradient(blue, yellow);
      }
    </style>
    <div id='content' style='width: 200px; height: 200px'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* iframe = GetDocument().getElementById("iframe");
  Element* content = ChildDocument().getElementById("content");
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(GetLayoutView(),
              EnclosingCompositedContainer(*content->GetLayoutObject()));
  }

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  content->setAttribute(html_names::kStyleAttr, "height: 500px");
  UpdateAllLifecyclePhasesForTest();
  const auto* client = content->GetLayoutObject()->View();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client, client->DebugName(), IntRect(0, 0, 100, 100),
                  PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the iframe.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  iframe->setAttribute(html_names::kStyleAttr, "height: 200px");
  UpdateAllLifecyclePhasesForTest();
  // The iframe doesn't have anything visible by itself, so we only issue
  // raster invalidation for the frame contents.
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client, client->DebugName(), IntRect(0, 0, 100, 200),
                  PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       CompositedBackgroundAttachmentLocalResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kClassAttr,
                       "solid composited scroll local-attachment border");
  UpdateAllLifecyclePhasesForTest();
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById("child");
  UpdateAllLifecyclePhasesForTest();

  auto* target_obj = To<LayoutBoxModelObject>(target->GetLayoutObject());
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            target_obj->GetBackgroundPaintLocation());

  auto container_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return GetRasterInvalidationTracking(1);
    return target_obj->Layer()
        ->GraphicsLayerBacking(target_obj)
        ->GetRasterInvalidationTracking();
  };
  auto contents_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return GetRasterInvalidationTracking(2);
    return target_obj->Layer()
        ->GraphicsLayerBacking()
        ->GetRasterInvalidationTracking();
  };

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr, "width: 500px; height: 1000px");
  UpdateAllLifecyclePhasesForTest();
  // No invalidation on the container layer.
  EXPECT_FALSE(container_raster_invalidation_tracking()->HasInvalidations());
  // Incremental invalidation of background on contents layer.
  const auto& client = target_obj->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(contents_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &client, client.DebugName(), IntRect(0, 500, 500, 500),
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the container.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "height: 200px");
  UpdateAllLifecyclePhasesForTest();
  // Border invalidated in the container layer.
  EXPECT_THAT(container_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  target_obj, target_obj->DebugName(), IntRect(0, 0, 70, 220),
                  PaintInvalidationReason::kGeometry}));
  // No invalidation on scrolling contents for container resize.
  EXPECT_FALSE(contents_raster_invalidation_tracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       CompositedBackgroundAttachmentLocalGradientResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kClassAttr,
                       "gradient composited scroll local-attachment border");
  target->setInnerHTML(
      "<div id='child' style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById("child");
  UpdateAllLifecyclePhasesForTest();

  auto* target_obj = To<LayoutBoxModelObject>(target->GetLayoutObject());
  auto container_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return GetRasterInvalidationTracking(1);
    return target_obj->Layer()
        ->GraphicsLayerBacking(target_obj)
        ->GetRasterInvalidationTracking();
  };
  auto contents_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return GetRasterInvalidationTracking(2);
    return target_obj->Layer()
        ->GraphicsLayerBacking()
        ->GetRasterInvalidationTracking();
  };

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr, "width: 500px; height: 1000px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            target_obj->GetBackgroundPaintLocation());

  // No invalidation on the container layer.
  EXPECT_FALSE(container_raster_invalidation_tracking()->HasInvalidations());
  // Full invalidation of background on contents layer because the gradient
  // background is resized.
  const auto& client = target_obj->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(contents_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &client, client.DebugName(), IntRect(0, 0, 500, 1000),
                  PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the container.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "height: 200px");
  UpdateAllLifecyclePhasesForTest();
  // Border invalidated in the container layer.
  EXPECT_THAT(container_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  target_obj, target_obj->DebugName(), IntRect(0, 0, 70, 220),
                  PaintInvalidationReason::kGeometry}));
  // No invalidation on scrolling contents for container resize.
  EXPECT_FALSE(contents_raster_invalidation_tracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       NonCompositedBackgroundAttachmentLocalResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kClassAttr,
                       "translucent local-attachment scroll");
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById("child");
  UpdateAllLifecyclePhasesForTest();
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    EXPECT_EQ(&GetLayoutView(), EnclosingCompositedContainer(*object));
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            To<LayoutBoxModelObject>(object)
                ->ComputeBackgroundPaintLocationIfComposited());
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            object->GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr, "width: 500px; height: 1000px");
  UpdateAllLifecyclePhasesForTest();
  // No invalidation because the changed part is invisible.
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());

  // Resize the container.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "height: 200px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object, object->DebugName(), IntRect(0, 100, 50, 100),
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, CompositedSolidBackgroundResize) {
  // To trigger background painting on both container and contents layer.
  // Note that the test may need update when we change the background paint
  // location rules.
  SetPreferCompositingToLCDText(false);

  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(html_names::kClassAttr, "solid composited scroll");
  target->setInnerHTML("<div style='height: 500px'></div>",
                       ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  // Resize the scroller.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 100px");
  UpdateAllLifecyclePhasesForTest();

  auto* target_object = To<LayoutBoxModelObject>(target->GetLayoutObject());
  EXPECT_EQ(
      kBackgroundPaintInScrollingContents | kBackgroundPaintInGraphicsLayer,
      target_object->GetBackgroundPaintLocation());

  const auto* contents_raster_invalidation_tracking =
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
          ? GetRasterInvalidationTracking(2)
          : target_object->Layer()
                ->GraphicsLayerBacking()
                ->GetRasterInvalidationTracking();
  const auto& client = target_object->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(contents_raster_invalidation_tracking->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  &client, client.DebugName(), IntRect(50, 0, 50, 500),
                  PaintInvalidationReason::kIncremental}));
  const auto* container_raster_invalidation_tracking =
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
          ? GetRasterInvalidationTracking(1)
          : target_object->Layer()
                ->GraphicsLayerBacking(target_object)
                ->GetRasterInvalidationTracking();
  EXPECT_THAT(
      container_raster_invalidation_tracking->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          target_object, target_object->DebugName(), IntRect(50, 0, 50, 100),
          PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

// Changing style in a way that changes overflow without layout should cause
// the layout view to possibly need a paint invalidation since we may have
// revealed additional background that can be scrolled into view.
TEST_P(PaintAndRasterInvalidationTest, RecalcOverflowInvalidatesBackground) {
  GetDocument().GetPage()->GetSettings().SetViewportEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style type='text/css'>
      body, html {
        width: 100%;
        height: 100%;
        margin: 0px;
      }
      #container {
        will-change: transform;
        width: 100%;
        height: 100%;
      }
    </style>
    <div id='container'></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ScrollableArea* scrollable_area = GetDocument().View()->LayoutViewport();
  ASSERT_EQ(scrollable_area->MaximumScrollOffset().Height(), 0);
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->ShouldCheckForPaintInvalidation());

  Element* container = GetDocument().getElementById("container");
  container->setAttribute(html_names::kStyleAttr,
                          "transform: translateY(1000px);");
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_EQ(scrollable_area->MaximumScrollOffset().Height(), 1000);
  EXPECT_TRUE(GetDocument().GetLayoutView()->ShouldCheckForPaintInvalidation());
}

TEST_P(PaintAndRasterInvalidationTest, DelayedFullPaintInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div style='height: 4000px'></div>
    <div id='target' style='width: 100px; height: 100px; background: blue'>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  target->SetShouldDoFullPaintInvalidationWithoutGeometryChange(
      PaintInvalidationReason::kForTesting);
  target->SetShouldDelayFullPaintInvalidation();
  EXPECT_FALSE(target->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(target->ShouldDelayFullPaintInvalidation());
  EXPECT_EQ(PaintInvalidationReason::kForTesting,
            target->FullPaintInvalidationReason());
  EXPECT_FALSE(target->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(target->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(target->Parent()->ShouldCheckForPaintInvalidation());

  GetDocument().View()->SetTracksRasterInvalidations(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  EXPECT_FALSE(target->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(target->ShouldDelayFullPaintInvalidation());
  EXPECT_EQ(PaintInvalidationReason::kForTesting,
            target->FullPaintInvalidationReason());
  EXPECT_FALSE(target->ShouldCheckGeometryForPaintInvalidation());
  EXPECT_TRUE(target->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(target->Parent()->ShouldCheckForPaintInvalidation());
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  // Scroll target into view.
  GetDocument().domWindow()->scrollTo(0, 4000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  target, target->DebugName(), IntRect(0, 4000, 100, 100),
                  PaintInvalidationReason::kForTesting}));
  EXPECT_EQ(PaintInvalidationReason::kNone,
            target->FullPaintInvalidationReason());
  EXPECT_FALSE(target->ShouldDelayFullPaintInvalidation());
  EXPECT_FALSE(target->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(target->Parent()->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(target->ShouldCheckGeometryForPaintInvalidation());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SVGHiddenContainer) {
  SetBodyInnerHTML(R"HTML(
    <svg style='position: absolute; top: 100px; left: 100px'>
      <mask id='mask'>
        <g transform='scale(2)'>
          <rect id='mask-rect' x='11' y='22' width='33' height='44'/>
        </g>
      </mask>
      <rect id='real-rect' x='55' y='66' width='7' height='8'
          mask='url(#mask)'/>
    </svg>
  )HTML");

  auto* mask_rect = GetLayoutObjectByElementId("mask-rect");
  auto* real_rect = GetLayoutObjectByElementId("real-rect");

  GetDocument().View()->SetTracksRasterInvalidations(true);
  To<Element>(mask_rect->GetNode())->setAttribute("x", "20");
  UpdateAllLifecyclePhasesForTest();

  // Should invalidate raster for real_rect only.
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{real_rect, real_rect->DebugName(),
                                         IntRect(155, 166, 7, 8),
                                         PaintInvalidationReason::kSubtree},
                  RasterInvalidationInfo{real_rect, real_rect->DebugName(),
                                         IntRect(154, 165, 9, 10),
                                         PaintInvalidationReason::kSubtree}));

  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, PaintPropertyChange) {
  // TODO(wangxianzhu): See the TODO in CullRectUpdater::SetFragmentCullRects().
  if (RuntimeEnabledFeatures::CullRectUpdateEnabled())
    return;

  SetUpHTML(*this);
  Element* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kClassAttr, "solid transform");
  UpdateAllLifecyclePhasesForTest();

  auto* layer = To<LayoutBoxModelObject>(object)->Layer();
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "transform: scale(3)");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(layer->SelfNeedsRepaint());
  const auto* transform =
      object->FirstFragment().PaintProperties()->Transform();
  EXPECT_TRUE(transform->Changed(
      PaintPropertyChangeType::kChangedOnlySimpleValues, *transform->Parent()));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{
                      layer, layer->DebugName(), IntRect(0, 0, 100, 200),
                      PaintInvalidationReason::kPaintProperty},
                  RasterInvalidationInfo{
                      layer, layer->DebugName(), IntRect(0, 0, 150, 300),
                      PaintInvalidationReason::kPaintProperty}));
  EXPECT_FALSE(transform->Changed(PaintPropertyChangeType::kChangedOnlyValues,
                                  *transform->Parent()));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeContainerOfFixedSizeSVG) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 100px">
      <svg viewBox="0 0 200 200" width="100" height="100">
        <rect id="rect" width="100%" height="100%"/>
      </svg>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  LayoutObject* rect = GetLayoutObjectByElementId("rect");
  EXPECT_TRUE(static_cast<const DisplayItemClient*>(rect)->IsValid());

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, "width: 200px; height: 200px");
  UpdateAllLifecyclePhasesExceptPaint();

  // We don't invalidate paint of the SVG rect.
  EXPECT_TRUE(static_cast<const DisplayItemClient*>(rect)->IsValid());

  UpdateAllLifecyclePhasesForTest();
  // No raster invalidations because the resized-div doesn't paint anything by
  // itself, and the svg is fixed sized.
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ScrollingInvalidatesStickyOffset) {
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="width:300px; height:200px; overflow:scroll">
      <div id="sticky" style="position:sticky; top:50px;
          width:50px; height:100px; background:red;">
        <div id="inner" style="width:100px; height:50px; background:red;">
        </div>
      </div>
      <div style="height:1000px;"></div>
    </div>
  )HTML");

  Element* scroller = GetDocument().getElementById("scroller");
  scroller->setScrollTop(100);

  const auto* sticky = GetLayoutObjectByElementId("sticky");
  EXPECT_TRUE(sticky->NeedsPaintPropertyUpdate());
  EXPECT_EQ(PhysicalOffset(), sticky->FirstFragment().PaintOffset());
  EXPECT_EQ(FloatSize(0, 50), sticky->FirstFragment()
                                  .PaintProperties()
                                  ->StickyTranslation()
                                  ->Translation2D());
  const auto* inner = GetLayoutObjectByElementId("inner");
  EXPECT_EQ(PhysicalOffset(), inner->FirstFragment().PaintOffset());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(sticky->NeedsPaintPropertyUpdate());
  EXPECT_EQ(PhysicalOffset(), sticky->FirstFragment().PaintOffset());
  EXPECT_EQ(FloatSize(0, 150), sticky->FirstFragment()
                                   .PaintProperties()
                                   ->StickyTranslation()
                                   ->Translation2D());
  EXPECT_EQ(PhysicalOffset(), inner->FirstFragment().PaintOffset());
}

TEST_P(PaintAndRasterInvalidationTest, ResizeElementWhichHasNonCustomResizer) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * { margin: 0;}
      div {
        width: 100px;
        height: 100px;
        background-color: red;
        overflow: hidden;
        resize: both;
      }
    </style>
    <div id='target'></div>
  )HTML");

  auto* target = GetDocument().getElementById("target");
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);

  target->setAttribute(html_names::kStyleAttr, "width: 200px");
  UpdateAllLifecyclePhasesForTest();

  Vector<RasterInvalidationInfo> invalidations;
  // This is for DisplayItem::kResizerScrollHitTest.
  invalidations.push_back(RasterInvalidationInfo{
      object, object->DebugName(), IntRect(100, 0, 100, 100),
      PaintInvalidationReason::kIncremental});
  const auto& scroll_corner = To<LayoutBoxModelObject>(object)
                                  ->GetScrollableArea()
                                  ->GetScrollCornerDisplayItemClient();
  invalidations.push_back(RasterInvalidationInfo{
      &scroll_corner, scroll_corner.DebugName(), IntRect(93, 93, 7, 7),
      PaintInvalidationReason::kGeometry});
  invalidations.push_back(RasterInvalidationInfo{
      &scroll_corner, scroll_corner.DebugName(), IntRect(193, 93, 7, 7),
      PaintInvalidationReason::kGeometry});
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAreArray(invalidations));

  GetDocument().View()->SetTracksRasterInvalidations(false);
}

class PaintInvalidatorTestClient : public RenderingTestChromeClient {
 public:
  void InvalidateContainer() override { invalidation_recorded_ = true; }

  bool InvalidationRecorded() { return invalidation_recorded_; }

  void ResetInvalidationRecorded() { invalidation_recorded_ = false; }

 private:
  bool invalidation_recorded_ = false;
};

class PaintInvalidatorCustomClientTest : public RenderingTest {
 public:
  PaintInvalidatorCustomClientTest()
      : RenderingTest(MakeGarbageCollected<EmptyLocalFrameClient>()),
        chrome_client_(MakeGarbageCollected<PaintInvalidatorTestClient>()) {}

  PaintInvalidatorTestClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  bool InvalidationRecorded() { return chrome_client_->InvalidationRecorded(); }

  void ResetInvalidationRecorded() {
    chrome_client_->ResetInvalidationRecorded();
  }

 private:
  Persistent<PaintInvalidatorTestClient> chrome_client_;
};

TEST_F(PaintInvalidatorCustomClientTest,
       NonCompositedInvalidationChangeOpacity) {
  // This test runs in a non-composited mode, so invalidations should
  // be issued via InvalidateChromeClient.
  SetBodyInnerHTML("<div id=target style='opacity: 0.99'></div>");

  auto* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  ResetInvalidationRecorded();

  target->setAttribute(html_names::kStyleAttr, "opacity: 0.98");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(InvalidationRecorded());
}

TEST_F(PaintInvalidatorCustomClientTest,
       NoInvalidationRepeatedUpdateLifecyleExceptPaint) {
  SetBodyInnerHTML("<div id=target style='opacity: 0.99'></div>");

  auto* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  ResetInvalidationRecorded();

  target->setAttribute(html_names::kStyleAttr, "opacity: 0.98");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(
      GetDocument().View()->GetLayoutView()->Layer()->DescendantNeedsRepaint());
  EXPECT_TRUE(InvalidationRecorded());

  ResetInvalidationRecorded();
  // Let PrePaintTreeWalk do something instead of no-op.
  GetDocument().View()->SetNeedsPaintPropertyUpdate();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  // The layer DescendantNeedsRepaint flag is only cleared after paint.
  EXPECT_TRUE(
      GetDocument().View()->GetLayoutView()->Layer()->DescendantNeedsRepaint());
  EXPECT_FALSE(InvalidationRecorded());
}

}  // namespace blink
