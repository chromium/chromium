// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using ::testing::MatchesRegex;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

const RasterInvalidationTracking* GetRasterInvalidationTracking(
    const LocalFrameView& root_frame_view,
    wtf_size_t index,
    const String& name_regex) {
  if (auto* client = root_frame_view.GetPaintArtifactCompositor()
                         ->ContentLayerClientForTesting(index)) {
    DCHECK(client->Layer().draws_content())
        << index << ": " << client->Layer().DebugName();
    DCHECK(::testing::Matcher<std::string>(
               ::testing::ContainsRegex(name_regex.Utf8()))
               .Matches(client->Layer().DebugName()))
        << index << ": " << client->Layer().DebugName()
        << " regex=" << name_regex;
    return client->GetRasterInvalidator().GetTracking();
  }
  return nullptr;
}

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
  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto& cc_layer = *GetDocument()
                        .View()
                        ->GetPaintArtifactCompositor()
                        ->RootLayer()
                        ->children()[1];

  {
    ScopedEnablePaintInvalidationTracing tracing;

    target->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
    UpdateAllLifecyclePhasesForTest();
    ASSERT_TRUE(cc_layer.debug_info());
    EXPECT_EQ(1u, cc_layer.debug_info()->invalidations.size());

    target->setAttribute(html_names::kStyleAttr,
                         AtomicString("height: 200px; width: 200px"));
    UpdateAllLifecyclePhasesForTest();
    ASSERT_TRUE(cc_layer.debug_info());
    EXPECT_EQ(2u, cc_layer.debug_info()->invalidations.size());
  }

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("height: 300px; width: 300px"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(cc_layer.debug_info());
  // No new invalidations tracked.
  EXPECT_EQ(2u, cc_layer.debug_info()->invalidations.size());
}

TEST_P(PaintAndRasterInvalidationTest, IncrementalInvalidationExpand) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 100px; height: 200px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{object->Id(), object->DebugName(),
                                 gfx::Rect(50, 0, 50, 200),
                                 PaintInvalidationReason::kIncremental},
          RasterInvalidationInfo{object->Id(), object->DebugName(),
                                 gfx::Rect(0, 100, 100, 100),
                                 PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, IncrementalInvalidationShrink) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 20px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{object->Id(), object->DebugName(),
                                 gfx::Rect(20, 0, 30, 100),
                                 PaintInvalidationReason::kIncremental},
          RasterInvalidationInfo{object->Id(), object->DebugName(),
                                 gfx::Rect(0, 80, 50, 20),
                                 PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, IncrementalInvalidationMixed) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 100px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{object->Id(), object->DebugName(),
                                 gfx::Rect(50, 0, 50, 80),
                                 PaintInvalidationReason::kIncremental},
          RasterInvalidationInfo{object->Id(), object->DebugName(),
                                 gfx::Rect(0, 80, 50, 20),
                                 PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeEmptyContent) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  // Make the view not solid color so that we can track raster invalidations.
  GetDocument().body()->setAttribute(
      html_names::kStyleAttr,
      AtomicString("height: 400px; background: linear-gradient(red, blue)"));
  // Make the box empty.
  target->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 100px; height: 80px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SubpixelChange) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 100.6px; height: 70.3px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 50, 100),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 101, 70),
                                         PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 50px; height: 100px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 50, 100),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 101, 70),
                                         PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SubpixelVisualRectChangeWithTransform) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kClassAttr, AtomicString("solid transform"));
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 100.6px; height: 70.3px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 100, 200),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 202, 140),
                                         PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 50px; height: 100px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 100, 200),
                                         PaintInvalidationReason::kLayout},
                  RasterInvalidationInfo{object->Id(), object->DebugName(),
                                         gfx::Rect(0, 0, 202, 140),
                                         PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SubpixelWithinPixelsChange) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LayoutObject* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(
      html_names::kStyleAttr,
      AtomicString("margin-top: 0.6px; width: 50px; height: 99.3px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object->Id(), object->DebugName(), gfx::Rect(0, 0, 50, 100),
                  PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(
      html_names::kStyleAttr,
      AtomicString("margin-top: 0.6px; width: 49.3px; height: 98.5px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object->Id(), object->DebugName(), gfx::Rect(0, 1, 50, 99),
                  PaintInvalidationReason::kLayout}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeRotated) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("transform: rotate(45deg)"));
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("transform: rotate(45deg); width: 200px"));
  UpdateAllLifecyclePhasesForTest();
  auto expected_rect =
      MakeRotationMatrix(45).MapRect(gfx::Rect(50, 0, 150, 100));
  expected_rect.Intersect(gfx::Rect(0, 0, 800, 600));
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object->Id(), object->DebugName(), expected_rect,
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, ResizeRotatedChild) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("transform: rotate(45deg); width: 200px"));
  target->setInnerHTML(
      "<div id=child style='width: 50px; height: 50px; background: "
      "red'></div>");
  UpdateAllLifecyclePhasesForTest();
  Element* child = GetDocument().getElementById(AtomicString("child"));
  auto* child_object = child->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(
      html_names::kStyleAttr,
      AtomicString("width: 100px; height: 50px; background: red"));
  UpdateAllLifecyclePhasesForTest();
  auto expected_rect = MakeRotationMatrix(45).MapRect(gfx::Rect(50, 0, 50, 50));
  expected_rect.Intersect(gfx::Rect(0, 0, 800, 600));
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  child_object->Id(), child_object->DebugName(), expected_rect,
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, CompositedLayoutViewResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kClassAttr, g_empty_atom);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 2000px"));
  // Make the scrolling contents layer not solid color so that we can track
  // raster invalidations.
  target->setInnerHTML("<div style='height: 20px'>Text</div>");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            GetLayoutView().GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 3000px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  ViewScrollingBackgroundClient().Id(),
                  ViewScrollingBackgroundClient().DebugName(),
                  gfx::Rect(0, 2000, 800, 1000),
                  PaintInvalidationReason::kIncremental}));
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
  GetDocument().body()->setAttribute(html_names::kClassAttr,
                                     AtomicString("gradient"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kClassAttr, g_empty_atom);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 2000px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            GetLayoutView().GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 3000px"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          ViewScrollingBackgroundClient().Id(),
          ViewScrollingBackgroundClient().DebugName(),
          gfx::Rect(0, 0, 800, 3000), PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the viewport. No invalidation.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().View()->Resize(800, 1000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
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
  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  LayoutView* iframe_layout_view = ChildDocument().View()->GetLayoutView();
  Element* content = ChildDocument().getElementById(AtomicString("content"));
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            iframe_layout_view->GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  content->setAttribute(html_names::kStyleAttr, AtomicString("height: 500px"));
  UpdateAllLifecyclePhasesForTest();
  // No invalidation because the changed part of scrollable overflow is clipped.
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the iframe.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  iframe->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  UpdateAllLifecyclePhasesForTest();
  // The iframe doesn't have anything visible by itself, so we only issue
  // raster invalidation for the frame contents.
  const auto& client = iframe_layout_view->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client.Id(), client.DebugName(), gfx::Rect(0, 100, 100, 100),
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, FullInvalidationWithHTMLTransform) {
  GetDocument().documentElement()->setAttribute(
      html_names::kStyleAttr, AtomicString("transform: scale(0.5)"));
  const DisplayItemClient& client = ViewScrollingBackgroundClient();
  UpdateAllLifecyclePhasesForTest();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().View()->Resize(gfx::Size(500, 500));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{client.Id(), client.DebugName(),
                                 gfx::Rect(0, 0, 500, 500),
                                 PaintInvalidationReason::kBackground},
          RasterInvalidationInfo{client.Id(), client.DebugName(),
                                 gfx::Rect(0, 0, 500, 500),
                                 PaintInvalidationReason::kBackground}));
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
  Element* iframe = GetDocument().getElementById(AtomicString("iframe"));
  const auto* iframe_layout_view = ChildDocument().View()->GetLayoutView();
  Element* content = ChildDocument().getElementById(AtomicString("content"));

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  content->setAttribute(html_names::kStyleAttr, AtomicString("height: 500px"));
  UpdateAllLifecyclePhasesForTest();
  const auto* client = &iframe_layout_view->GetScrollableArea()
                            ->GetScrollingBackgroundDisplayItemClient();
  // The two invalidations are for the old background and the new background.
  // The rects are the same because they are clipped by the layer bounds.
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{client->Id(), client->DebugName(),
                                 gfx::Rect(0, 0, 100, 100),
                                 PaintInvalidationReason::kBackground},
          RasterInvalidationInfo{client->Id(), client->DebugName(),
                                 gfx::Rect(0, 0, 100, 100),
                                 PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the iframe.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  iframe->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  UpdateAllLifecyclePhasesForTest();
  // The iframe doesn't have anything visible by itself, so we only issue
  // raster invalidation for the frame contents.
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{client->Id(), client->DebugName(),
                                 gfx::Rect(0, 100, 100, 100),
                                 PaintInvalidationReason::kIncremental},
          RasterInvalidationInfo{client->Id(), client->DebugName(),
                                 gfx::Rect(0, 0, 100, 200),
                                 PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       CompositedBackgroundAttachmentLocalResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(
      html_names::kClassAttr,
      AtomicString("solid composited scroll local-attachment border"));
  UpdateAllLifecyclePhasesForTest();
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById(AtomicString("child"));
  UpdateAllLifecyclePhasesForTest();

  auto* target_obj = target->GetLayoutBox();
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            target_obj->GetBackgroundPaintLocation());

  auto container_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    return GetRasterInvalidationTracking(0, "target");
  };
  auto contents_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    return GetRasterInvalidationTracking(1, "target");
  };

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("width: 500px; height: 1000px"));
  UpdateAllLifecyclePhasesForTest();
  // No invalidation on the container layer.
  EXPECT_FALSE(container_raster_invalidation_tracking()->HasInvalidations());
  // Incremental invalidation of background on contents layer.
  const auto& client = target_obj->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(contents_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client.Id(), client.DebugName(), gfx::Rect(0, 500, 500, 500),
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the container.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  UpdateAllLifecyclePhasesForTest();
  // Border invalidated in the container layer.
  EXPECT_THAT(container_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  target_obj->Id(), target_obj->DebugName(),
                  gfx::Rect(0, 0, 70, 220), PaintInvalidationReason::kLayout}));
  // No invalidation on scrolling contents for container resize.
  EXPECT_FALSE(contents_raster_invalidation_tracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       CompositedBackgroundAttachmentLocalGradientResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(
      html_names::kClassAttr,
      AtomicString("gradient composited scroll local-attachment border"));
  target->setInnerHTML(
      "<div id='child' style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById(AtomicString("child"));
  UpdateAllLifecyclePhasesForTest();

  auto* target_obj = target->GetLayoutBox();
  auto container_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    return GetRasterInvalidationTracking(0, "target");
  };
  auto contents_raster_invalidation_tracking =
      [&]() -> const RasterInvalidationTracking* {
    return GetRasterInvalidationTracking(1, "target");
  };

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("width: 500px; height: 1000px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            target_obj->GetBackgroundPaintLocation());

  // No invalidation on the container layer.
  EXPECT_FALSE(container_raster_invalidation_tracking()->HasInvalidations());
  // Full invalidation of background on contents layer because the gradient
  // background is resized.
  const auto& client = target_obj->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(contents_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client.Id(), client.DebugName(), gfx::Rect(0, 0, 500, 1000),
                  PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Resize the container.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  UpdateAllLifecyclePhasesForTest();
  // Border invalidated in the container layer.
  EXPECT_THAT(container_raster_invalidation_tracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  target_obj->Id(), target_obj->DebugName(),
                  gfx::Rect(0, 0, 70, 220), PaintInvalidationReason::kLayout}));
  // No invalidation on scrolling contents for container resize.
  EXPECT_FALSE(contents_raster_invalidation_tracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       NonCompositedBackgroundAttachmentLocalResize) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutBox();
  target->setAttribute(html_names::kClassAttr,
                       AtomicString("translucent local-attachment scroll"));
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById(AtomicString("child"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(kBackgroundPaintInContentsSpace,
            object->GetBackgroundPaintLocation());

  // Resize the content.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  child->setAttribute(html_names::kStyleAttr,
                      AtomicString("width: 500px; height: 1000px"));
  UpdateAllLifecyclePhasesForTest();
  // No invalidation because the changed part is invisible.
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());

  // Resize the container.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  object->Id(), object->DebugName(), gfx::Rect(0, 100, 50, 100),
                  PaintInvalidationReason::kIncremental}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, CompositedSolidBackgroundResize) {
  // To trigger background painting on both container and contents layer.
  // Note that the test may need update when we change the background paint
  // location rules.
  SetPreferCompositingToLCDText(false);

  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  target->setAttribute(html_names::kClassAttr,
                       AtomicString("solid composited scroll"));
  target->setInnerHTML(
      "<div style='width: 50px; height: 500px; background: yellow'></div>");
  UpdateAllLifecyclePhasesForTest();

  // Resize the scroller.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr, AtomicString("width: 100px"));
  UpdateAllLifecyclePhasesForTest();

  auto* target_object = target->GetLayoutBox();
  EXPECT_EQ(kBackgroundPaintInBothSpaces,
            target_object->GetBackgroundPaintLocation());

  const auto* contents_raster_invalidation_tracking =
      GetRasterInvalidationTracking(0, "target");
  // Only the contents layer is eligible for blink-side raster invalidation.
  EXPECT_FALSE(GetRasterInvalidationTracking(1, ""));
  const auto& client = target_object->GetScrollableArea()
                           ->GetScrollingBackgroundDisplayItemClient();
  EXPECT_THAT(contents_raster_invalidation_tracking->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client.Id(), client.DebugName(), gfx::Rect(50, 0, 50, 500),
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
  ASSERT_EQ(scrollable_area->MaximumScrollOffset().y(), 0);
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->ShouldCheckForPaintInvalidation());

  Element* container = GetDocument().getElementById(AtomicString("container"));
  container->setAttribute(html_names::kStyleAttr,
                          AtomicString("transform: translateY(1000px);"));
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_EQ(scrollable_area->MaximumScrollOffset().y(), 1000);
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
  target->SetShouldDoFullPaintInvalidationWithoutLayoutChange(
      PaintInvalidationReason::kStyle);
  target->SetShouldDelayFullPaintInvalidation();
  EXPECT_FALSE(target->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(target->ShouldDelayFullPaintInvalidation());
  EXPECT_EQ(PaintInvalidationReason::kStyle,
            target->PaintInvalidationReasonForPrePaint());
  EXPECT_FALSE(target->ShouldCheckLayoutForPaintInvalidation());
  EXPECT_TRUE(target->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(target->Parent()->ShouldCheckForPaintInvalidation());

  GetDocument().View()->SetTracksRasterInvalidations(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  EXPECT_FALSE(target->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(target->ShouldDelayFullPaintInvalidation());
  EXPECT_EQ(PaintInvalidationReason::kStyle,
            target->PaintInvalidationReasonForPrePaint());
  EXPECT_FALSE(target->ShouldCheckLayoutForPaintInvalidation());
  EXPECT_TRUE(target->ShouldCheckForPaintInvalidation());
  EXPECT_TRUE(target->Parent()->ShouldCheckForPaintInvalidation());
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  // Scroll target into view.
  GetDocument().domWindow()->scrollTo(0, 4000);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          target->Id(), target->DebugName(), gfx::Rect(0, 4000, 100, 100),
          PaintInvalidationReason::kStyle}));
  EXPECT_EQ(PaintInvalidationReason::kNone,
            target->PaintInvalidationReasonForPrePaint());
  EXPECT_FALSE(target->ShouldDelayFullPaintInvalidation());
  EXPECT_FALSE(target->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(target->Parent()->ShouldCheckForPaintInvalidation());
  EXPECT_FALSE(target->ShouldCheckLayoutForPaintInvalidation());
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
  To<Element>(mask_rect->GetNode())
      ->setAttribute(svg_names::kXAttr, AtomicString("20"));
  UpdateAllLifecyclePhasesForTest();

  // Should invalidate raster for real_rect only.
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{real_rect->Id(), real_rect->DebugName(),
                                 gfx::Rect(155, 166, 7, 8),
                                 PaintInvalidationReason::kImage},
          RasterInvalidationInfo{real_rect->Id(), real_rect->DebugName(),
                                 gfx::Rect(154, 165, 9, 10),
                                 PaintInvalidationReason::kImage}));

  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, SVGWithFilterNoOpStyleUpdate) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <filter id="f">
        <feGaussianBlur stdDeviation="5"/>
      </filter>
      <rect width="100" height="100" style="filter: url(#f)"/>
    </svg>
  )HTML");

  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("--x: 42"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetRasterInvalidationTracking()->HasInvalidations());
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, PaintPropertyChange) {
  SetUpHTML(*this);
  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();
  target->setAttribute(html_names::kClassAttr, AtomicString("solid transform"));
  UpdateAllLifecyclePhasesForTest();

  auto* layer = To<LayoutBoxModelObject>(object)->Layer();
  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("transform: scale(3)"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(layer->SelfNeedsRepaint());
  const auto* transform =
      object->FirstFragment().PaintProperties()->Transform();
  EXPECT_TRUE(transform->Changed(
      PaintPropertyChangeType::kChangedOnlySimpleValues, *transform->Parent()));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(
          RasterInvalidationInfo{layer->Id(), layer->DebugName(),
                                 gfx::Rect(0, 0, 100, 200),
                                 PaintInvalidationReason::kPaintProperty},
          RasterInvalidationInfo{layer->Id(), layer->DebugName(),
                                 gfx::Rect(0, 0, 150, 300),
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

  Element* target = GetDocument().getElementById(AtomicString("target"));
  LayoutObject* rect = GetLayoutObjectByElementId("rect");
  EXPECT_TRUE(static_cast<const DisplayItemClient*>(rect)->IsValid());

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("width: 200px; height: 200px"));
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

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->setScrollTop(100);

  const auto* sticky = GetLayoutObjectByElementId("sticky");
  EXPECT_TRUE(sticky->NeedsPaintPropertyUpdate());
  EXPECT_EQ(PhysicalOffset(), sticky->FirstFragment().PaintOffset());
  EXPECT_EQ(gfx::Vector2dF(0, 50), sticky->FirstFragment()
                                       .PaintProperties()
                                       ->StickyTranslation()
                                       ->Get2dTranslation());
  const auto* inner = GetLayoutObjectByElementId("inner");
  EXPECT_EQ(PhysicalOffset(), inner->FirstFragment().PaintOffset());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(sticky->NeedsPaintPropertyUpdate());
  EXPECT_EQ(PhysicalOffset(), sticky->FirstFragment().PaintOffset());
  EXPECT_EQ(gfx::Vector2dF(0, 150), sticky->FirstFragment()
                                        .PaintProperties()
                                        ->StickyTranslation()
                                        ->Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(), inner->FirstFragment().PaintOffset());
}

TEST_P(PaintAndRasterInvalidationTest, NoDamageDueToFloatingPointError) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #canvas {
          position: absolute;
          top: 0;
          left: 0;
          width: 0;
          height: 0;
          will-change: transform;
          transform-origin: top left;
          transform: scale(1.8);
        }
        #tile {
          position: absolute;
          will-change: transform;
          transform-origin: top left;
          transform: translateX(49px) translateY(100px) scale(0.555555555556);
        }
        #tileInner {
          transform-origin: top left;
          transform: scale(1.8);
          width: 200px;
          height: 200px;
          background: lightblue;
        }
      </style>
      <div id="canvas" class="initial">
        <div id="tile">
          <div id="tileInner"></div>
        </div>
      </div>
  )HTML");

  auto* canvas = GetDocument().getElementById(AtomicString("canvas"));
  for (double x = 0; x < 200; x += 1) {
    GetDocument().View()->SetTracksRasterInvalidations(true);
    canvas->setAttribute(
        html_names::kStyleAttr,
        AtomicString(String::Format("transform: translateX(%lfpx) scale(1.8)",
                                    x / 1.8)));
    UpdateAllLifecyclePhasesForTest();
    EXPECT_FALSE(GetRasterInvalidationTracking(0, "tile")->HasInvalidations());
    GetDocument().View()->SetTracksRasterInvalidations(false);
  }
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

  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* object = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);

  target->setAttribute(html_names::kStyleAttr, AtomicString("width: 200px"));
  UpdateAllLifecyclePhasesForTest();

  Vector<RasterInvalidationInfo> invalidations;
  // This is for DisplayItem::kResizerScrollHitTest.
  invalidations.push_back(RasterInvalidationInfo{
      object->Id(), object->DebugName(), gfx::Rect(100, 0, 100, 100),
      PaintInvalidationReason::kIncremental});
  const auto& scroll_corner = To<LayoutBoxModelObject>(object)
                                  ->GetScrollableArea()
                                  ->GetScrollCornerDisplayItemClient();
  invalidations.push_back(RasterInvalidationInfo{
      scroll_corner.Id(), scroll_corner.DebugName(), gfx::Rect(93, 93, 7, 7),
      PaintInvalidationReason::kLayout});
  invalidations.push_back(RasterInvalidationInfo{
      scroll_corner.Id(), scroll_corner.DebugName(), gfx::Rect(193, 93, 7, 7),
      PaintInvalidationReason::kLayout});
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAreArray(invalidations));

  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, VisibilityChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      /* Make the view not solid color so that we can track raster
         invalidations. */
      body { background: linear-gradient(red, blue); }
      #target { width: 100px; height: 100px; background: blue; }
    </style>
    <div id="target"></div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  const DisplayItemClient* client = target->GetLayoutObject();

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("visibility: hidden"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client->Id(), client->DebugName(), gfx::Rect(8, 8, 100, 100),
                  PaintInvalidationReason::kDisappeared}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  GetDocument().View()->SetTracksRasterInvalidations(true);
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("visibility: visible"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  client->Id(), client->DebugName(), gfx::Rect(8, 8, 100, 100),
                  PaintInvalidationReason::kAppeared}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest,
       DelayedInvalidationImageChangedFromAnimatedToStatic) {
  const String kStaticImage =
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUA/"
      "wA0XsCoAAAACklEQVQIHWNgAAAAAgABz8g15QAAAABJRU5ErkJggg==";
  SetBodyInnerHTML(R"HTML(
    <div id="spacer" style="background-image:
      url()HTML" + kStaticImage +
                   R"HTML()">
    </div>
    <div style="height: 2250px"></div>
    <div id="target" style="
      background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      width: 100px;
      height: 100px">
    </div>
  )HTML");

  auto* target_element = GetElementById("target");
  auto* spacer_element = GetElementById("spacer");
  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_FALSE(target->ShouldDelayFullPaintInvalidation());

  // Simulate an image change notification on #target.
  auto* anim_background_image =
      target->StyleRef().BackgroundLayers().GetImage();
  ASSERT_TRUE(anim_background_image);
  auto* anim_image_resource_content = anim_background_image->CachedImage();
  ASSERT_TRUE(anim_image_resource_content);
  ASSERT_TRUE(anim_image_resource_content->GetImage()->MaybeAnimated());
  static_cast<ImageObserver*>(anim_image_resource_content)
      ->Changed(anim_image_resource_content->GetImage());
  EXPECT_TRUE(target->MayNeedPaintInvalidationAnimatedBackgroundImage());

  // Change the paint offset of #target to get a layout/geometry paint
  // invalidation reason.
  spacer_element->SetInlineStyleProperty(CSSPropertyID::kHeight, 100,
                                         CSSPrimitiveValue::UnitType::kPixels);

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(target->ShouldDelayFullPaintInvalidation());

  GetDocument().View()->SetTracksRasterInvalidations(true);

  // Update #target's style to point to a non-animated image.
  target_element->SetInlineStyleProperty(
      CSSPropertyID::kBackgroundImage,
      AtomicString("url(" + kStaticImage + ")"));

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          target->Id(), target->DebugName(), gfx::Rect(8, 2358, 100, 100),
          PaintInvalidationReason::kBackground}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(PaintAndRasterInvalidationTest, RepaintScrollbarThumbOnHover) {
  // In RasterInducingScroll the scrollbar is composited, not using blink
  // raster invalidation.
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    return;
  }

  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0}</style>
    <div id="target" style="width: 100px; height: 100px; overflow-y: auto">
      <div style="height: 200px"></div>
    </div>
  )HTML");

  GetDocument().View()->SetTracksRasterInvalidations(true);
  Scrollbar* scrollbar = GetLayoutBoxByElementId("target")
                             ->GetScrollableArea()
                             ->VerticalScrollbar();
  scrollbar->SetHoveredPart(kThumbPart);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_THAT(
      GetRasterInvalidationTracking()->Invalidations(),
      UnorderedElementsAre(RasterInvalidationInfo{
          scrollbar->Id(), scrollbar->DebugName(), scrollbar->FrameRect(),
          PaintInvalidationReason::kScrollControl}));
}

// This tests an optimization for motionmark suits, where changing styles of
// elements with a clip path should not, by itself, require a full paint
// invalidation. For example, transform changes can skip paint invalidation.
TEST_P(PaintAndRasterInvalidationTest,
       StyleChangesWithClipPathDoNotInvalidate) {
  SetBodyInnerHTML(R"HTML(
    <svg width='300' height='300'>
      <defs>
        <clipPath id='c' clipPathUnits='objectBoundingBox'>
          <rect />
        </clipPath>
      </defs>
      <rect id='rect'
          width='100'
          height='100'
          transform='translate(100,100)'
          clip-path='url(#c)' />
    </svg>
  )HTML");

  auto* rect = GetDocument().getElementById(AtomicString("rect"));
  EXPECT_FALSE(rect->GetLayoutObject()->ShouldDoFullPaintInvalidation());
  rect->setAttribute(svg_names::kTransformAttr,
                     AtomicString("translate(200,100)"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(rect->GetLayoutObject()->ShouldDoFullPaintInvalidation());
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

  auto* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  ResetInvalidationRecorded();

  target->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.98"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(InvalidationRecorded());
}

TEST_F(PaintInvalidatorCustomClientTest,
       NoInvalidationRepeatedUpdateLifecyleExceptPaint) {
  SetBodyInnerHTML("<div id=target style='opacity: 0.99'></div>");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  ResetInvalidationRecorded();

  target->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.98"));
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  // Only paint property change doesn't need repaint.
  EXPECT_FALSE(
      GetDocument().View()->GetLayoutView()->Layer()->DescendantNeedsRepaint());
  // Just needs to invalidate the chrome client.
  EXPECT_TRUE(InvalidationRecorded());

  ResetInvalidationRecorded();
  // Let PrePaintTreeWalk do something instead of no-op, without any real
  // change.
  GetDocument().View()->SetNeedsPaintPropertyUpdate();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(
      GetDocument().View()->GetLayoutView()->Layer()->DescendantNeedsRepaint());
  EXPECT_FALSE(InvalidationRecorded());
}

}  // namespace blink
