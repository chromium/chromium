// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/recording_source.h"
#include "cc/layers/surface_layer.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/fake_remote_frame_host.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

#define EXPECT_SKCOLOR4F_NEAR(expected, actual, error) \
  do {                                                 \
    EXPECT_NEAR(expected.fR, actual.fR, error);        \
    EXPECT_NEAR(expected.fG, actual.fG, error);        \
    EXPECT_NEAR(expected.fB, actual.fB, error);        \
    EXPECT_NEAR(expected.fA, actual.fA, error);        \
  } while (false)

// Tests the integration between blink and cc where a layer list is sent to cc.
class CompositingTest : public PaintTestConfigurations, public testing::Test {
 public:
  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize();
    GetLocalFrameView()
        ->GetFrame()
        .GetSettings()
        ->SetPreferCompositingToLCDTextForTesting(true);
    web_view_helper_->Resize(gfx::Size(200, 200));
  }

  void TearDown() override { web_view_helper_.reset(); }

  // Both sets the inner html and runs the document lifecycle.
  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->setInnerHTML(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhasesForTest();
  }

  WebLocalFrame* LocalMainFrame() { return web_view_helper_->LocalMainFrame(); }

  LocalFrameView* GetLocalFrameView() {
    return web_view_helper_->LocalMainFrame()->GetFrameView();
  }

  WebViewImpl* WebView() { return web_view_helper_->GetWebView(); }

  cc::Layer* RootCcLayer() { return paint_artifact_compositor()->RootLayer(); }

  cc::Layer* CcLayerByDOMElementId(const char* id) {
    auto layers = CcLayersByDOMElementId(RootCcLayer(), id);
    return layers.empty() ? nullptr : layers[0];
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_helper_->LocalMainFrame()
        ->FrameWidgetImpl()
        ->LayerTreeHostForTesting();
  }

  Element* GetElementById(const char* id) {
    WebLocalFrameImpl* frame = web_view_helper_->LocalMainFrame();
    return frame->GetFrame()->GetDocument()->getElementById(AtomicString(id));
  }

  LayoutObject* GetLayoutObjectById(const char* id) {
    return GetElementById(id)->GetLayoutObject();
  }

  void UpdateAllLifecyclePhases() {
    WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  cc::PropertyTrees* GetPropertyTrees() {
    return LayerTreeHost()->property_trees();
  }

  cc::TransformNode* GetTransformNode(const cc::Layer* layer) {
    return GetPropertyTrees()->transform_tree_mutable().Node(
        layer->transform_tree_index());
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    return GetLocalFrameView()->GetPaintArtifactCompositor();
  }

 private:
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;

  test::TaskEnvironment task_environment_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingTest);

TEST_P(CompositingTest, DisableAndEnableAcceleratedCompositing) {
  UpdateAllLifecyclePhases();
  auto* settings = GetLocalFrameView()->GetFrame().GetSettings();
  size_t num_layers = RootCcLayer()->children().size();
  EXPECT_GT(num_layers, 1u);
  settings->SetAcceleratedCompositingEnabled(false);
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(paint_artifact_compositor());
  settings->SetAcceleratedCompositingEnabled(true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(num_layers, RootCcLayer()->children().size());
}

TEST_P(CompositingTest, DidScrollCallbackAfterScrollableAreaChanges) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(),
                     "<style>"
                     "  #scrollable {"
                     "    height: 100px;"
                     "    width: 100px;"
                     "    overflow: scroll;"
                     "    will-change: transform;"
                     "  }"
                     "  #forceScroll { height: 120px; width: 50px; }"
                     "</style>"
                     "<div id='scrollable'>"
                     "  <div id='forceScroll'></div>"
                     "</div>");

  UpdateAllLifecyclePhases();

  Document* document = WebView()->MainFrameImpl()->GetFrame()->GetDocument();
  Element* scrollable = document->getElementById(AtomicString("scrollable"));

  auto* scrollable_area = scrollable->GetLayoutBox()->GetScrollableArea();
  EXPECT_NE(nullptr, scrollable_area);

  CompositorElementId scroll_element_id = scrollable_area->GetScrollElementId();
  auto* overflow_scroll_layer =
      CcLayerByCcElementId(RootCcLayer(), scroll_element_id);
  const auto* scroll_node = RootCcLayer()
                                ->layer_tree_host()
                                ->property_trees()
                                ->scroll_tree()
                                .FindNodeFromElementId(scroll_element_id);
  EXPECT_EQ(scroll_node->container_bounds, gfx::Size(100, 100));

  // Ensure a synthetic impl-side scroll offset propagates to the scrollable
  // area using the DidScroll callback.
  EXPECT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back(
      {scroll_element_id, gfx::Vector2dF(0, 1), std::nullopt});
  overflow_scroll_layer->layer_tree_host()->ApplyCompositorChanges(
      &commit_data);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());

  // Make the scrollable area non-scrollable.
  scrollable->setAttribute(html_names::kStyleAttr,
                           AtomicString("overflow: visible"));

  // Update layout without updating compositing state.
  LocalMainFrame()->ExecuteScript(
      WebScriptSource("var forceLayoutFromScript = scrollable.offsetTop;"));
  EXPECT_EQ(document->Lifecycle().GetState(), DocumentLifecycle::kLayoutClean);

  EXPECT_EQ(nullptr, scrollable->GetLayoutBox()->GetScrollableArea());

  // The web scroll layer has not been deleted yet and we should be able to
  // apply impl-side offsets without crashing.
  ASSERT_EQ(overflow_scroll_layer,
            CcLayerByCcElementId(RootCcLayer(), scroll_element_id));
  commit_data.scrolls[0] = {scroll_element_id, gfx::Vector2dF(0, 1),
                            std::nullopt};
  overflow_scroll_layer->layer_tree_host()->ApplyCompositorChanges(
      &commit_data);

  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByCcElementId(RootCcLayer(), scroll_element_id));
}

TEST_P(CompositingTest, FrameViewScroll) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(),
                     "<style>"
                     "  #forceScroll {"
                     "    height: 2000px;"
                     "    width: 100px;"
                     "  }"
                     "</style>"
                     "<div id='forceScroll'></div>");

  UpdateAllLifecyclePhases();

  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  EXPECT_NE(nullptr, scrollable_area);

  const auto* scroll_node =
      RootCcLayer()
          ->layer_tree_host()
          ->property_trees()
          ->scroll_tree()
          .FindNodeFromElementId(scrollable_area->GetScrollElementId());
  ASSERT_TRUE(scroll_node);

  // Ensure a synthetic impl-side scroll offset propagates to the scrollable
  // area using the DidScroll callback.
  EXPECT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back({scrollable_area->GetScrollElementId(),
                                 gfx::Vector2dF(0, 1), std::nullopt});
  RootCcLayer()->layer_tree_host()->ApplyCompositorChanges(&commit_data);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
}

TEST_P(CompositingTest, WillChangeTransformHint) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      #willChange {
        width: 100px;
        height: 100px;
        will-change: transform;
        background: blue;
      }
    </style>
    <div id="willChange"></div>
  )HTML");
  UpdateAllLifecyclePhases();
  auto* layer = CcLayerByDOMElementId("willChange");
  auto* transform_node = GetTransformNode(layer);
  EXPECT_TRUE(transform_node->will_change_transform);
}

TEST_P(CompositingTest, WillChangeTransformHintInSVG) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <style>
      #willChange {
        width: 100px;
        height: 100px;
        will-change: transform;
      }
    </style>
    <svg width="200" height="200">
      <rect id="willChange" fill="blue"></rect>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  auto* layer = CcLayerByDOMElementId("willChange");
  auto* transform_node = GetTransformNode(layer);
  // For now will-change:transform triggers compositing for SVG, but we don't
  // pass the flag to cc to ensure raster quality.
  EXPECT_FALSE(transform_node->will_change_transform);
}

TEST_P(CompositingTest, Compositing3DTransformOnSVGModelObject) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <rect id="target" fill="blue" width="100" height="100"></rect>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 3D transform should trigger compositing.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("transform: translate3d(0, 0, 1px)"));
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_TRUE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_TRUE(CcLayerByDOMElementId("target"));

  // Removing a 3D transform removes the compositing trigger.
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("transform: none"));
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_FALSE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 2D transform should not trigger compositing.
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("transform: translate(1px, 0)"));
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Switching from a 2D to a 3D transform should trigger compositing.
  target_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("transform: translate3d(0, 0, 1px)"));
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(CcLayerByDOMElementId("target"));
}

TEST_P(CompositingTest, Compositing3DTransformOnSVGBlock) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <text id="target" x="50" y="50">text</text>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 3D transform should trigger compositing.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("transform: translate3d(0, 0, 1px)"));
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_TRUE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_TRUE(CcLayerByDOMElementId("target"));

  // Removing a 3D transform removes the compositing trigger.
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("transform: none"));
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_FALSE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 2D transform should not trigger compositing.
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("transform: translate(1px, 0)"));
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Switching from a 2D to a 3D transform should trigger compositing.
  target_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("transform: translate3d(0, 0, 1px)"));
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(CcLayerByDOMElementId("target"));
}

// Inlines do not support the transform property and should not be composited
// due to 3D transforms.
TEST_P(CompositingTest, NotCompositing3DTransformOnSVGInline) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <text x="50" y="50">
        text
        <tspan id="inline">tspan</tspan>
      </text>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("inline"));

  // Adding a 3D transform to an inline should not trigger compositing.
  auto* inline_element = GetElementById("inline");
  inline_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("transform: translate3d(0, 0, 1px)"));
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_FALSE(GetLayoutObjectById("inline")->HasTransformRelatedProperty());
  EXPECT_FALSE(CcLayerByDOMElementId("inline"));
}

TEST_P(CompositingTest, PaintPropertiesWhenCompositingSVG) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <style>
      #ancestor {
        opacity: 0.9;
      }
      #svg {
        opacity: 0.8;
      }
      #rect {
        width: 100px;
        height: 100px;
        will-change: transform;
        opacity: 0.7;
      }
    </style>
    <div id="ancestor">
      <svg id="svg" width="200" height="200">
        <rect width="10" height="10" fill="red"></rect>
        <rect id="rect" fill="blue" stroke-width="1" stroke="black"></rect>
      </svg>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();
  auto* ancestor = CcLayerByDOMElementId("ancestor");
  auto* ancestor_effect_node = GetPropertyTrees()->effect_tree_mutable().Node(
      ancestor->effect_tree_index());
  EXPECT_EQ(ancestor_effect_node->opacity, 0.9f);

  auto* svg_root = CcLayerByDOMElementId("svg");
  const auto* svg_root_effect_node =
      GetPropertyTrees()->effect_tree().Node(svg_root->effect_tree_index());
  EXPECT_EQ(svg_root_effect_node->opacity, 0.8f);
  EXPECT_EQ(svg_root_effect_node->parent_id, ancestor_effect_node->id);

  auto* rect = CcLayerByDOMElementId("rect");
  const auto* rect_effect_node =
      GetPropertyTrees()->effect_tree().Node(rect->effect_tree_index());

  EXPECT_EQ(rect_effect_node->opacity, 0.7f);
  EXPECT_EQ(rect_effect_node->parent_id, svg_root_effect_node->id);
}

TEST_P(CompositingTest, BackgroundColorInScrollingContentsLayer) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      html {
        background-color: rgb(10, 20, 30);
      }
      #scroller {
        will-change: transform;
        overflow: scroll;
        height: 100px;
        width: 100px;
        background-color: rgb(30, 40, 50);
      }
      .spacer {
        height: 1000px;
      }
    </style>
    <div id="scroller">
      <div class="spacer"></div>
    </div>
    <div class="spacer"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  LayoutView* layout_view = GetLocalFrameView()->GetLayoutView();
  Element* scroller = GetElementById("scroller");
  LayoutBox* scroller_box = scroller->GetLayoutBox();
  ASSERT_TRUE(layout_view->GetBackgroundPaintLocation() ==
              kBackgroundPaintInContentsSpace);
  ASSERT_TRUE(scroller_box->GetBackgroundPaintLocation() ==
              kBackgroundPaintInContentsSpace);

  // The root layer and root scrolling contents layer get background_color by
  // blending the CSS background-color of the <html> element with
  // LocalFrameView::BaseBackgroundColor(), which is white by default.
  auto* layer = CcLayersByName(RootCcLayer(), "LayoutView #document")[0];
  SkColor4f expected_color = SkColor4f::FromColor(SkColorSetRGB(10, 20, 30));
  EXPECT_EQ(layer->background_color(), SkColors::kTransparent);
  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_SKCOLOR4F_NEAR(layer->background_color(), expected_color, 0.005f);

  // Non-root layers set background_color based on the CSS background color of
  // the layer-defining element.
  expected_color = SkColor4f::FromColor(SkColorSetRGB(30, 40, 50));
  layer = CcLayerByDOMElementId("scroller");
  EXPECT_EQ(layer->background_color(), SkColors::kTransparent);
  scrollable_area = scroller_box->GetScrollableArea();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_SKCOLOR4F_NEAR(layer->background_color(), expected_color, 0.005f);
}

TEST_P(CompositingTest, BackgroundColorInGraphicsLayer) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      html {
        background-image: linear-gradient(rgb(10, 20, 30), rgb(60, 70, 80));
        background-attachment: fixed;
      }
      #scroller {
        will-change: transform;
        overflow: scroll;
        height: 100px;
        width: 100px;
        background-color: rgba(30, 40, 50, .6);
        background-clip: content-box;
        background-attachment: scroll;
        padding: 1px;
      }
      .spacer {
        height: 1000px;
      }
    </style>
    <div id="scroller">
      <div class="spacer"></div>
    </div>
    <div class="spacer"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  LayoutView* layout_view = GetLocalFrameView()->GetLayoutView();
  Element* scroller = GetElementById("scroller");
  LayoutBox* scroller_box = scroller->GetLayoutBox();
  ASSERT_TRUE(layout_view->GetBackgroundPaintLocation() ==
              kBackgroundPaintInBorderBoxSpace);
  ASSERT_TRUE(scroller_box->GetBackgroundPaintLocation() ==
              kBackgroundPaintInBorderBoxSpace);

  // The root layer gets background_color by blending the CSS background-color
  // of the <html> element with LocalFrameView::BaseBackgroundColor(), which is
  // white by default. In this case, because the background is a gradient, it
  // will blend transparent with white, resulting in white. Because the
  // background is painted into the root graphics layer, the root scrolling
  // contents layer should not checkerboard, so its background color should be
  // transparent.
  auto* layer = CcLayersByName(RootCcLayer(), "LayoutView #document")[0];
  EXPECT_EQ(layer->background_color(), SkColors::kWhite);
  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_EQ(layer->background_color(), SkColors::kTransparent);
  EXPECT_EQ(layer->SafeOpaqueBackgroundColor(), SkColors::kTransparent);

  // Non-root layers set background_color based on the CSS background color of
  // the layer-defining element.
  SkColor4f expected_color =
      SkColor4f::FromColor(SkColorSetARGB(roundf(255. * 0.6), 30, 40, 50));
  layer = CcLayerByDOMElementId("scroller");
  EXPECT_SKCOLOR4F_NEAR(layer->background_color(), expected_color, 0.005f);
  scrollable_area = scroller_box->GetScrollableArea();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_EQ(layer->background_color(), SkColors::kTransparent);
  EXPECT_EQ(layer->SafeOpaqueBackgroundColor(), SkColors::kTransparent);
}

TEST_P(CompositingTest, ContainPaintLayerBounds) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="target" style="will-change: transform; contain: paint;
                            width: 200px; height: 100px">
      <div style="width: 300px; height: 400px"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();
  auto* layer = CcLayersByDOMElementId(RootCcLayer(), "target")[0];
  ASSERT_TRUE(layer);
  EXPECT_EQ(gfx::Size(200, 100), layer->bounds());
}

// https://crbug.com/1422877:
TEST_P(CompositingTest, CompositedOverlayScrollbarUnderNonFastBorderRadius) {
  ScopedMockOverlayScrollbars mock_overlay_scrollbars;

  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="rounded" style="width: 150px; height: 150px;
                             border-radius: 10px / 20px; overflow: hidden;
                             will-change: opacity">
      Content1
      <div id="scroll1" style="width: 100px; height: 100px; overflow: scroll">
        <div style="height: 2000px">Content2</div>
      </div>
      Content3
      <div id="scroll2" style="width: 100px; height: 100px; overflow: scroll">
        <div style="height: 2000px">Content4</div>
      </div>
      Content5
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  ASSERT_TRUE(GetLayoutObjectById("scroll1")
                  ->FirstFragment()
                  .PaintProperties()
                  ->VerticalScrollbarEffect());
  EXPECT_EQ(1u, CcLayersByName(RootCcLayer(), "Synthesized Clip").size());
}

// https://crbug.com/1459318
TEST_P(CompositingTest,
       FullPACUpdateOnScrollWithSyntheticClipAcrossScrollerSimpleRadius) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="scroll" style="width: 200px; height: 200px;
                            border-radius: 2px;
                            overflow: scroll; background: white">
      <div id="masked" style="width: 100px; height: 100px;
                              backdrop-filter: blur(1px)"></div>
      <div style="height: 200px"></div>
    </div>
  )HTML");

  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  GetElementById("scroll")->scrollTo(0, 2);
  GetLocalFrameView()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(paint_artifact_compositor()->NeedsUpdate());
  UpdateAllLifecyclePhases();
}

// https://crbug.com/1459318
TEST_P(CompositingTest,
       FullPACUpdateOnScrollWithSyntheticClipAcrossScrollerComplexRadius) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="scroll" style="width: 200px; height: 200px;
                            border-radius: 2px / 4px;
                            overflow: scroll; background: white">
      <div id="masked" style="width: 100px; height: 100px;
                              backdrop-filter: blur(1px)"></div>
      <div style="height: 200px"></div>
    </div>
  )HTML");

  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  GetElementById("scroll")->scrollTo(0, 2);
  GetLocalFrameView()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(paint_artifact_compositor()->NeedsUpdate());
  UpdateAllLifecyclePhases();
}

TEST_P(CompositingTest, HitTestOpaqueness) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="transparent1" style="pointer-events: none; will-change: transform;
                                  width: 100px; height: 50px">
    </div>
    <div id="transparent2" style="pointer-events: none; will-change: transform;
                                  width: 100px; height: 50px; background: red">
    </div>

    <!-- Transparent parent with a small opaque child. -->
    <div id="mixed1" style="pointer-events: none; will-change: transform;
                            width: 200px; height: 50px">
      Transparent parent
      <div style="pointer-events: auto">Opaque child</div>
    </div>
    <!-- Layer with mixed opaque areas and transparent gaps. -->
    <div id="mixed2" style="will-change: transform; width: 0">
      <div style="margin: 10px; width: 200px; height: 50px">Opaque child1</div>
      <div style="margin: 10px; width: 200px; height: 50px">Opaque child2</div>
    </div>
    <div id="mixed3" style="will-change: transform; border-radius: 10px;
                            width: 50px; height: 50px">
    </div>

    <div id="opaque1" style="will-change: transform; width: 50px; height: 50px">
       Opaque
    </div>
    <!-- Two adjacent opaque children fills the layer, making the layer
         opaque. -->
    <div id="opaque2" style="will-change: transform; width: 0">
      <div style="width: 100px; height: 50px">Opaque child1</div>
      <div style="width: 100px; height: 50px">Opaque child2</div>
    </div>
    <!-- Child pointer-events:none doesn't affect opaqueness of parent. -->
    <div id="opaque3"
         style="will-change: transform; width: 100px; height: 100px">
      <div style="width: 50px; height: 50px; pointer-events: none"></div>
    </div>
    <!-- An opaque child fills the transparent parent, making the layer
         opaque. -->
    <div id="opaque4" style="will-change: transform; pointer-events: none">
      <div style="height: 50px; pointer-events: auto"></div>
    </div>
    <!-- An opaque child fills the mixed layer, making the layer opaque. -->
    <div id="opaque5" style="will-change: transform; border-radius: 10px;
                             width: 50px; height; 50px">
      <div style="height: 50px"></div>
    </div>
    <!-- This is opaque because the svg element (opaque to hit test) fully
         contains the circle (mixed opaqueness to hit test). -->
    <svg id="opaque6" style="will-change: transform">
      <circle cx="20" cy="20" r="20"/>
    </svg>
  )HTML");

  const auto hit_test_transparent =
      RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
          ? cc::HitTestOpaqueness::kTransparent
          : cc::HitTestOpaqueness::kMixed;
  EXPECT_EQ(hit_test_transparent,
            CcLayersByDOMElementId(RootCcLayer(), "transparent1")[0]
                ->hit_test_opaqueness());
  EXPECT_EQ(hit_test_transparent,
            CcLayersByDOMElementId(RootCcLayer(), "transparent2")[0]
                ->hit_test_opaqueness());
  EXPECT_EQ(cc::HitTestOpaqueness::kMixed,
            CcLayersByDOMElementId(RootCcLayer(), "mixed1")[0]
                ->hit_test_opaqueness());
  EXPECT_EQ(cc::HitTestOpaqueness::kMixed,
            CcLayersByDOMElementId(RootCcLayer(), "mixed2")[0]
                ->hit_test_opaqueness());
  EXPECT_EQ(cc::HitTestOpaqueness::kMixed,
            CcLayersByDOMElementId(RootCcLayer(), "mixed3")[0]
                ->hit_test_opaqueness());
  const auto hit_test_opaque =
      RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
          ? cc::HitTestOpaqueness::kOpaque
          : cc::HitTestOpaqueness::kMixed;
  EXPECT_EQ(hit_test_opaque, CcLayersByDOMElementId(RootCcLayer(), "opaque1")[0]
                                 ->hit_test_opaqueness());
  EXPECT_EQ(hit_test_opaque, CcLayersByDOMElementId(RootCcLayer(), "opaque2")[0]
                                 ->hit_test_opaqueness());
  EXPECT_EQ(hit_test_opaque, CcLayersByDOMElementId(RootCcLayer(), "opaque3")[0]
                                 ->hit_test_opaqueness());
  EXPECT_EQ(hit_test_opaque, CcLayersByDOMElementId(RootCcLayer(), "opaque4")[0]
                                 ->hit_test_opaqueness());
  EXPECT_EQ(hit_test_opaque, CcLayersByDOMElementId(RootCcLayer(), "opaque5")[0]
                                 ->hit_test_opaqueness());
  EXPECT_EQ(hit_test_opaque, CcLayersByDOMElementId(RootCcLayer(), "opaque6")[0]
                                 ->hit_test_opaqueness());
}

TEST_P(CompositingTest, HitTestOpaquenessOfSolidColorLayer) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="target" style="will-change: transform; width: 100px; height: 100px;
                            background: green">
    </div>
  )HTML");

  auto* layer = CcLayersByDOMElementId(RootCcLayer(), "target")[0];
  EXPECT_TRUE(layer->IsSolidColorLayerForTesting());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_EQ(cc::HitTestOpaqueness::kOpaque, layer->hit_test_opaqueness());
  } else {
    EXPECT_EQ(cc::HitTestOpaqueness::kMixed, layer->hit_test_opaqueness());
  }
}

TEST_P(CompositingTest, HitTestOpaquenessOfEmptyInline) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      html, body { margin: 0; }
      #inline {
        pointer-events: none;
      }
      #scrollable {
        width: 150px;
        height: 150px;
        overflow-y: scroll;
      }
      #scrollable::-webkit-scrollbar {
        display: none;
      }
      #content {
        height: 1000px;
        width: 150px;
        background: linear-gradient(blue, yellow);
        pointer-events: auto;
      }
    </style>
    <span id="inline"><div id="scrollable"><div id="content"></div></div></span>
  )HTML");

  // We should have a layer for the scrolling contents.
  auto* scrolling_contents =
      CcLayersByDOMElementId(RootCcLayer(), "scrollable").back();
  EXPECT_EQ(gfx::Size(150, 1000), scrolling_contents->bounds());

  // If there is a following layer for inline contents, it should be non-opaque.
  auto html_layers = CcLayersByName(RootCcLayer(), "LayoutBlockFlow HTML");
  auto* html = html_layers.empty() ? nullptr : html_layers.back();
  if (html) {
    EXPECT_GT(html->id(), scrolling_contents->id());
    EXPECT_EQ(gfx::Size(200, 150), html->bounds());
    EXPECT_NE(cc::HitTestOpaqueness::kOpaque, html->hit_test_opaqueness());
  }
}

TEST_P(CompositingTest, HitTestOpaquenessOnChangeOfUsedPointerEvents) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="parent">
      <div id="target" style="will-change: transform; width: 50px; height: 50px;
                              background: blue">
      </div>
    </div>
  )HTML");

  const auto hit_test_transparent =
      RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
          ? cc::HitTestOpaqueness::kTransparent
          : cc::HitTestOpaqueness::kMixed;
  const auto hit_test_opaque =
      RuntimeEnabledFeatures::HitTestOpaquenessEnabled()
          ? cc::HitTestOpaqueness::kOpaque
          : cc::HitTestOpaqueness::kMixed;

  Element* parent = GetElementById("parent");
  Element* target = GetElementById("target");
  const LayoutBox* target_box = target->GetLayoutBox();
  EXPECT_EQ(EPointerEvents::kAuto, target_box->StyleRef().UsedPointerEvents());
  ASSERT_FALSE(target_box->Layer()->SelfNeedsRepaint());
  auto* display_item_client = static_cast<const DisplayItemClient*>(target_box);
  ASSERT_TRUE(display_item_client->IsValid());
  const cc::Layer* target_layer =
      CcLayersByDOMElementId(RootCcLayer(), "target")[0];
  EXPECT_EQ(hit_test_opaque, target_layer->hit_test_opaqueness());

  target->SetInlineStyleProperty(CSSPropertyID::kPointerEvents, "none");
  GetLocalFrameView()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  // Change of PointerEvents should not invalidate the painting layer, but not
  // the display item client.
  EXPECT_EQ(EPointerEvents::kNone, target_box->StyleRef().UsedPointerEvents());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(target_box->Layer()->SelfNeedsRepaint());
  }
  EXPECT_TRUE(display_item_client->IsValid());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(hit_test_transparent, target_layer->hit_test_opaqueness());

  target->RemoveInlineStyleProperty(CSSPropertyID::kPointerEvents);
  GetLocalFrameView()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(EPointerEvents::kAuto, target_box->StyleRef().UsedPointerEvents());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(target_box->Layer()->SelfNeedsRepaint());
  }
  EXPECT_TRUE(display_item_client->IsValid());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(hit_test_opaque, target_layer->hit_test_opaqueness());

  parent->setAttribute(html_names::kInertAttr, AtomicString(""));
  GetLocalFrameView()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(EPointerEvents::kNone, target_box->StyleRef().UsedPointerEvents());
  // Change of parent inert attribute (affecting target's used pointer events)
  // should invalidate the painting layer but not the display item client.
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(target_box->Layer()->SelfNeedsRepaint());
  }
  EXPECT_TRUE(display_item_client->IsValid());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(hit_test_transparent, target_layer->hit_test_opaqueness());

  parent->removeAttribute(html_names::kInertAttr);
  GetLocalFrameView()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(EPointerEvents::kAuto, target_box->StyleRef().UsedPointerEvents());
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_TRUE(target_box->Layer()->SelfNeedsRepaint());
  }
  EXPECT_TRUE(display_item_client->IsValid());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(hit_test_opaque, target_layer->hit_test_opaqueness());
}

// Based on the minimized test case of https://crbug.com/343198769.
TEST_P(CompositingTest,
       NonStackedScrollerWithRelativeChildAboveFixedAndAbsolute) {
  GetLocalFrameView()
      ->GetFrame()
      .GetSettings()
      ->SetPreferCompositingToLCDTextForTesting(false);

  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <style>
      div { width: 100px; height: 100px; }
      ::-webkit-scrollbar { display: none; }
    </style>
    <div id="fixed" style="position: fixed"></div>
    <div id="absolute" style="position: absolute"></div>
    <div style="overflow: scroll">
      <div id="relative" style="position: relative; height: 2000px">
        Contents
      </div>
    </div>
  )HTML");

  EXPECT_TRUE(CcLayerByDOMElementId("fixed"));     // Directly composited.
  EXPECT_TRUE(CcLayerByDOMElementId("absolute"));  // Overlaps with #fixed.
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    // Not merged because that would miss #relative's scroll state without a
    // MainThreadScrollHitTestRegion.
    EXPECT_TRUE(CcLayerByDOMElementId("relative"));
  } else {
    // Merged into #absolute.
    EXPECT_FALSE(CcLayerByDOMElementId("relative"));
  }

  GetElementById("fixed")->SetInlineStyleProperty(CSSPropertyID::kPosition,
                                                  "absolute");
  UpdateAllLifecyclePhases();
  // All layers are merged together.
  EXPECT_FALSE(CcLayerByDOMElementId("fixed"));
  EXPECT_FALSE(CcLayerByDOMElementId("absolute"));
  EXPECT_FALSE(CcLayerByDOMElementId("relative"));
}

TEST_P(CompositingTest, AnchorPositionAdjustmentTransformIdReference) {
  GetLocalFrameView()
      ->GetFrame()
      .GetSettings()
      ->SetPreferCompositingToLCDTextForTesting(false);

  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="anchored1"
         style="position: absolute; position-anchor: --a; top: anchor(bottom)">
      anchored
    </div>
    <div id="scroller" style="overflow: scroll; width: 200px; height: 200px">
      <div id="anchor" style="anchor-name: --a">anchor</div>
      <div style="height: 1000px"></div>
    </div>
    <div id="anchored2"
         style="position: absolute; position-anchor: --a; top: anchor(bottom)">
      anchored
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  int scroll_translation_id =
      GetElementById("scroller")
          ->GetLayoutObject()
          ->FirstFragment()
          .PaintProperties()
          ->ScrollTranslation()
          ->CcNodeId(LayerTreeHost()->property_trees()->sequence_number());
  EXPECT_LT(scroll_translation_id,
            CcLayersByDOMElementId(RootCcLayer(), "anchored1")[0]
                ->transform_tree_index());
  EXPECT_LT(scroll_translation_id,
            CcLayersByDOMElementId(RootCcLayer(), "anchored2")[0]
                ->transform_tree_index());
}

TEST_P(CompositingTest, ScrollingContentsCullRect) {
  GetLocalFrameView()
      ->GetFrame()
      .GetSettings()
      ->SetPreferCompositingToLCDTextForTesting(false);

  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <style>
      .scroller { width: 200px; height: 200px; overflow: scroll; }
    </style>
    <div id="short-composited-scroller" class="scroller">
      <div style="height: 2000px; background: yellow">Content</div>
    </div>
    <div id="long-composited-scroller" class="scroller">
      <div style="height: 10000px; background: yellow">Content</div>
    </div>
    <div id="narrow-non-composited-scroller" class="scroller">
      <div style="width: 200px; height: 2000px">Content</div>
    </div>
    <div id="wide-non-composited-scroller" class="scroller">
      <div style="width: 10000px; height: 200px">Content</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();

  EXPECT_TRUE(CcLayerByDOMElementId("short-composited-scroller"));
  EXPECT_TRUE(CcLayerByDOMElementId("long-composited-scroller"));
  EXPECT_FALSE(CcLayerByDOMElementId("narrow-non-composited-scroller"));
  EXPECT_FALSE(CcLayerByDOMElementId("wide-non-composited-scroller"));

  auto check_cull_rect = [&](const char* id,
                             const std::optional<gfx::Rect>& expected) {
    const gfx::Rect* actual =
        GetPropertyTrees()->scroll_tree().ScrollingContentsCullRect(
            GetLayoutObjectById(id)
                ->FirstFragment()
                .PaintProperties()
                ->Scroll()
                ->GetCompositorElementId());
    if (expected) {
      ASSERT_TRUE(actual);
      EXPECT_EQ(*expected, *actual);
    } else {
      EXPECT_FALSE(actual);
    }
  };

  check_cull_rect("short-composited-scroller", std::nullopt);
  check_cull_rect("long-composited-scroller", gfx::Rect(0, 0, 200, 4200));
  check_cull_rect("narrow-non-composited-scroller", std::nullopt);
  check_cull_rect("wide-non-composited-scroller", gfx::Rect(0, 0, 4200, 200));

  GetElementById("short-composited-scroller")->scrollTo(5000, 5000);
  GetElementById("long-composited-scroller")->scrollTo(5000, 5000);
  GetElementById("narrow-non-composited-scroller")->scrollTo(5000, 5000);
  GetElementById("wide-non-composited-scroller")->scrollTo(5000, 5000);
  UpdateAllLifecyclePhases();

  EXPECT_TRUE(CcLayerByDOMElementId("short-composited-scroller"));
  EXPECT_TRUE(CcLayerByDOMElementId("long-composited-scroller"));
  EXPECT_FALSE(CcLayerByDOMElementId("narrow-non-composited-scroller"));
  EXPECT_FALSE(CcLayerByDOMElementId("wide-non-composited-scroller"));

  check_cull_rect("short-composited-scroller", std::nullopt);
  check_cull_rect("long-composited-scroller", gfx::Rect(0, 1000, 200, 8200));
  check_cull_rect("narrow-non-composited-scroller", std::nullopt);
  check_cull_rect("wide-non-composited-scroller",
                  gfx::Rect(1000, 0, 8200, 200));
}

class CompositingSimTest : public PaintTestConfigurations, public SimTest {
 public:
  void InitializeWithHTML(const String& html) {
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(html);
    UpdateAllLifecyclePhases();
    DCHECK(paint_artifact_compositor());
  }

  const cc::Layer* RootCcLayer() {
    return paint_artifact_compositor()->RootLayer();
  }

  const cc::Layer* CcLayerByDOMElementId(const char* id) {
    auto layers = CcLayersByDOMElementId(RootCcLayer(), id);
    return layers.empty() ? nullptr : layers[0];
  }

  const cc::Layer* CcLayerByOwnerNode(Node* node) {
    return CcLayerByOwnerNodeId(RootCcLayer(), node->GetDomNodeId());
  }

  const cc::Layer* CcLayerForIFrameContent(Document* iframe_doc) {
    if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
      return CcLayerByOwnerNode(iframe_doc);
    }
    return CcLayerByOwnerNode(iframe_doc->documentElement());
  }

  Element* GetElementById(const char* id) {
    return MainFrame().GetFrame()->GetDocument()->getElementById(
        AtomicString(id));
  }

  void UpdateAllLifecyclePhases() {
    WebView().MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void UpdateAllLifecyclePhasesExceptPaint() {
    WebView().MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kPrePaint,
                                                 DocumentUpdateReason::kTest);
  }

  cc::PropertyTrees* GetPropertyTrees() {
    return Compositor().LayerTreeHost()->property_trees();
  }

  cc::TransformNode* GetTransformNode(const cc::Layer* layer) {
    return GetPropertyTrees()->transform_tree_mutable().Node(
        layer->transform_tree_index());
  }

  cc::EffectNode* GetEffectNode(const cc::Layer* layer) {
    return GetPropertyTrees()->effect_tree_mutable().Node(
        layer->effect_tree_index());
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    return MainFrame().GetFrameView()->GetPaintArtifactCompositor();
  }

 private:
  void SetUp() override {
    SimTest::SetUp();
    // Ensure a non-empty size so painting does not early-out.
    WebView().Resize(gfx::Size(800, 600));
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingSimTest);

TEST_P(CompositingSimTest, LayerUpdatesDoNotInvalidateEarlierLayers) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='a'></div>
      <div id='b'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* a_layer = CcLayerByDOMElementId("a");
  auto* b_element = GetElementById("b");
  auto* b_layer = CcLayerByDOMElementId("b");

  // Initially, neither a nor b should have a layer that should push properties.
  const cc::LayerTreeHost& host = *Compositor().LayerTreeHost();
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          a_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          b_layer));

  // Modifying b should only cause the b layer to need to push properties.
  b_element->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.2"));
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          a_layer));
  EXPECT_TRUE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          b_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          a_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          b_layer));
}

TEST_P(CompositingSimTest, LayerUpdatesDoNotInvalidateLaterLayers) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='a'></div>
      <div id='b' style='opacity: 0.2;'></div>
      <div id='c'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* a_element = GetElementById("a");
  auto* a_layer = CcLayerByDOMElementId("a");
  auto* b_element = GetElementById("b");
  auto* b_layer = CcLayerByDOMElementId("b");
  auto* c_layer = CcLayerByDOMElementId("c");

  // Initially, no layer should need to push properties.
  const cc::LayerTreeHost& host = *Compositor().LayerTreeHost();
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          a_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          b_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          c_layer));

  // Modifying a and b (adding opacity to a and removing opacity from b) should
  // not cause the c layer to push properties.
  a_element->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.3"));
  b_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          a_layer));
  EXPECT_TRUE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          b_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          c_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          a_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          b_layer));
  EXPECT_FALSE(
      host.pending_commit_state()->layers_that_should_push_properties.count(
          c_layer));
}

TEST_P(CompositingSimTest,
       NoopChangeDoesNotCauseFullTreeSyncOrPropertyTreeUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <div></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially the host should not need to sync.
  cc::LayerTreeHost& layer_tree_host = *Compositor().LayerTreeHost();
  EXPECT_FALSE(layer_tree_host.needs_full_tree_sync());
  int sequence_number = GetPropertyTrees()->sequence_number();
  EXPECT_GT(sequence_number, 0);

  // A no-op update should not cause the host to need a full tree sync.
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer_tree_host.needs_full_tree_sync());
  // It should also not cause a property tree update - the sequence number
  // should not change.
  EXPECT_EQ(sequence_number, GetPropertyTrees()->sequence_number());
}

// When a property tree change occurs that affects layer transform in the
// general case, all layers associated with the changed property tree node, and
// all layers associated with a descendant of the changed property tree node
// need to have |subtree_property_changed| set for damage tracking. In
// non-layer-list mode, this occurs in BuildPropertyTreesInternal (see:
// SetLayerPropertyChangedForChild).
TEST_P(CompositingSimTest, LayerSubtreeTransformPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          transform: translate(10px, 10px);
          background: lightgreen;
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(outer_element_layer)->transform_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(inner_element_layer)->transform_changed);

  // Modifying the transform style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("transform: rotate(10deg)"));
  UpdateAllLifecyclePhases();
  // This is still set by the traditional GraphicsLayer::SetTransform().
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  // Set by blink::PropertyTreeManager.
  EXPECT_TRUE(GetTransformNode(outer_element_layer)->transform_changed);
  // TODO(wangxianzhu): Probably avoid setting this flag on transform change.
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(inner_element_layer)->transform_changed);

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(outer_element_layer)->transform_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(inner_element_layer)->transform_changed);
}

// When a property tree change occurs that affects layer transform in a simple
// case (ie before and after transforms both preserve axis alignment), the
// transforms can be directly updated without explicitly marking layers as
// damaged. The ensure damage occurs, the transform node should have
// |transform_changed| set.
TEST_P(CompositingSimTest, DirectTransformPropertyUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        @keyframes animateTransformA {
          0% { transform: translateX(0px); }
          100% { transform: translateX(100px); }
        }
        @keyframes animateTransformB {
          0% { transform: translateX(200px); }
          100% { transform: translateX(300px); }
        }
        #outer {
          width: 100px;
          height: 100px;
          background: lightgreen;
          animation-name: animateTransformA;
          animation-duration: 999s;
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto transform_tree_index = outer_element_layer->transform_tree_index();
  const auto* transform_node =
      GetPropertyTrees()->transform_tree().Node(transform_tree_index);

  // Initially, transform should be unchanged.
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the transform in a simple way allowed for a direct update.
  outer_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("animation-name: animateTransformB"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

// Test that, for simple transform updates with an existing cc transform node,
// we can go from style change to updated cc transform node without running
// the blink property tree builder and without running paint artifact
// compositor.
// This is similar to |DirectTransformPropertyUpdate|, but the update is done
// from style rather than the property tree builder.
TEST_P(CompositingSimTest, FastPathTransformUpdateFromStyle) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes animation {
          0% { transform: translateX(200px); }
          100% { transform: translateX(300px); }
        }
        #div {
          transform: translateX(100px);
          width: 100px;
          height: 100px;
          /*
            This causes the transform to have an active animation, but because
            the delay is so large, it will not have an effect for the duration
            of this unit test.
          */
          animation-name: animation;
          animation-duration: 999s;
          animation-delay: 999s;
        }
      </style>
      <div id='div'></div>
  )HTML");

  Compositor().BeginFrame();

  // Check the initial state of the blink transform node.
  auto* div = GetElementById("div");
  auto* div_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_TRUE(div_properties);
  EXPECT_EQ(gfx::Transform::MakeTranslation(100, 0),
            div_properties->Transform()->Matrix());
  EXPECT_TRUE(div_properties->Transform()->HasActiveTransformAnimation());
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());

  // Check the initial state of the cc transform node.
  auto* div_cc_layer = CcLayerByDOMElementId("div");
  auto transform_tree_index = div_cc_layer->transform_tree_index();
  const auto* transform_node =
      GetPropertyTrees()->transform_tree().Node(transform_tree_index);
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_EQ(100.0f, transform_node->local.To2dTranslation().x());

  // Change the transform style and ensure the blink and cc transform nodes are
  // not marked for a full update.
  div->setAttribute(html_names::kStyleAttr,
                    AtomicString("transform: translateX(400px)"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Continue to run the lifecycle to paint and ensure that updates are
  // performed.
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_EQ(gfx::Transform::MakeTranslation(400, 0),
            div_properties->Transform()->Matrix());
  EXPECT_EQ(400.0f, transform_node->local.To2dTranslation().x());
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_TRUE(transform_node->transform_changed);

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

// Same as the test above but for opacity changes
TEST_P(CompositingSimTest, FastPathOpacityUpdateFromStyle) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes animation {
          0% { opacity: 0.2; }
          100% { opacity: 0.8; }
        }
        #div {
          opacity: 0.1;
          width: 100px;
          height: 100px;
          /*
            This causes the opacity to have an active animation, but because
            the delay is so large, it will not have an effect for the duration
            of this unit test.
          */
          animation-name: animation;
          animation-duration: 999s;
          animation-delay: 999s;
        }
      </style>
      <div id='div'></div>
  )HTML");

  Compositor().BeginFrame();

  // Check the initial state of the blink effect node.
  auto* div = GetElementById("div");
  auto* div_properties =
      div->GetLayoutObject()->FirstFragment().PaintProperties();
  ASSERT_TRUE(div_properties);
  EXPECT_NEAR(0.1, div_properties->Effect()->Opacity(), 0.001);
  EXPECT_TRUE(div_properties->Effect()->HasActiveOpacityAnimation());
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());

  // Check the initial state of the cc effect node.
  auto* div_cc_layer = CcLayerByDOMElementId("div");
  auto effect_tree_index = div_cc_layer->effect_tree_index();
  const auto* effect_node =
      GetPropertyTrees()->effect_tree().Node(effect_tree_index);
  EXPECT_FALSE(effect_node->effect_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_NEAR(0.1, effect_node->opacity, 0.001);

  // Change the effect style and ensure the blink and cc effect nodes are
  // not marked for a full update.
  div->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.15"));
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Continue to run the lifecycle to paint and ensure that updates are
  // performed.
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_NEAR(0.15, div_properties->Effect()->Opacity(), 0.001);
  EXPECT_NEAR(0.15, effect_node->opacity, 0.001);
  EXPECT_TRUE(effect_node->effect_changed);
  EXPECT_FALSE(div->GetLayoutObject()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_TRUE(effect_node->effect_changed);

  // After a frame the |opacity_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(effect_node->effect_changed);
}

TEST_P(CompositingSimTest, DirectSVGTransformPropertyUpdate) {
  InitializeWithHTML(R"HTML(
    <!doctype html>
    <style>
      @keyframes animateTransformA {
        0% { transform: translateX(0px); }
        100% { transform: translateX(100px); }
      }
      @keyframes animateTransformB {
        0% { transform: translateX(200px); }
        100% { transform: translateX(300px); }
      }
      #willChangeWithAnimation {
        width: 100px;
        height: 100px;
        animation-name: animateTransformA;
        animation-duration: 999s;
      }
    </style>
    <svg width="200" height="200">
      <rect id="willChangeWithAnimation" fill="blue"></rect>
    </svg>
  )HTML");

  Compositor().BeginFrame();

  auto* will_change_layer = CcLayerByDOMElementId("willChangeWithAnimation");
  auto transform_tree_index = will_change_layer->transform_tree_index();
  const auto* transform_node =
      GetPropertyTrees()->transform_tree().Node(transform_tree_index);

  // Initially, transform should be unchanged.
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the transform in a simple way allowed for a direct update.
  auto* will_change_element = GetElementById("willChangeWithAnimation");
  will_change_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("animation-name: animateTransformB"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

// This test is similar to |DirectTransformPropertyUpdate| but tests that
// the changed value of a directly updated transform is still set if some other
// change causes PaintArtifactCompositor to run and do non-direct updates.
TEST_P(CompositingSimTest, DirectTransformPropertyUpdateCausesChange) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        @keyframes animateTransformA {
          0% { transform: translateX(0px); }
          100% { transform: translateX(100px); }
        }
        @keyframes animateTransformB {
          0% { transform: translateX(200px); }
          100% { transform: translateX(300px); }
        }
        #outer {
          width: 100px;
          height: 100px;
          animation-name: animateTransformA;
          animation-duration: 999s;
          background: lightgreen;
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
          transform: translate(3px, 4px);
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto outer_transform_tree_index = outer_element_layer->transform_tree_index();
  const auto* outer_transform_node =
      GetPropertyTrees()->transform_tree().Node(outer_transform_tree_index);

  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  auto inner_transform_tree_index = inner_element_layer->transform_tree_index();
  const auto* inner_transform_node =
      GetPropertyTrees()->transform_tree().Node(inner_transform_tree_index);

  // Initially, the transforms should be unchanged.
  EXPECT_FALSE(outer_transform_node->transform_changed);
  EXPECT_FALSE(inner_transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the outer transform in a simple way should allow for a direct
  // update of the outer transform. Modifying the inner transform in a
  // non-simple way should not allow for a direct update of the inner transform.
  outer_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("animation-name: animateTransformB"));
  inner_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("transform: rotate(30deg)"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(outer_transform_node->transform_changed);
  EXPECT_FALSE(inner_transform_node->transform_changed);
  EXPECT_TRUE(paint_artifact_compositor()->NeedsUpdate());

  // After a PaintArtifactCompositor update, which was needed due to the inner
  // element's transform change, both the inner and outer transform nodes
  // should be marked as changed to ensure they result in damage.
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_transform_node->transform_changed);
  EXPECT_TRUE(inner_transform_node->transform_changed);

  // After a frame the |transform_changed| values should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_transform_node->transform_changed);
  EXPECT_FALSE(inner_transform_node->transform_changed);
}

// This test ensures that the correct transform nodes are created and bits set
// so that the browser controls movement adjustments needed by bottom-fixed
// elements will work.
TEST_P(CompositingSimTest, AffectedByOuterViewportBoundsDelta) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        body { height: 2000px; }
        #fixed {
          width: 100px;
          height: 100px;
          position: fixed;
          left: 0;
          background-color: red;
        }
      </style>
      <div id='fixed'></div>
  )HTML");

  auto* fixed_element = GetElementById("fixed");
  auto* fixed_element_layer = CcLayerByDOMElementId("fixed");

  // Fix the DIV to the bottom of the viewport. Since the viewport height will
  // expand/contract, the fixed element will need to be moved as the bounds
  // delta changes.
  {
    fixed_element->setAttribute(html_names::kStyleAttr,
                                AtomicString("bottom: 0"));
    Compositor().BeginFrame();

    auto transform_tree_index = fixed_element_layer->transform_tree_index();
    const auto* transform_node =
        GetPropertyTrees()->transform_tree().Node(transform_tree_index);

    DCHECK(transform_node);
    EXPECT_TRUE(transform_node->moved_by_outer_viewport_bounds_delta_y);
  }

  // Fix it to the top now. Since the top edge doesn't change (relative to the
  // renderer origin), we no longer need to move it as the bounds delta
  // changes.
  {
    fixed_element->setAttribute(html_names::kStyleAttr, AtomicString("top: 0"));
    Compositor().BeginFrame();

    auto transform_tree_index = fixed_element_layer->transform_tree_index();
    const auto* transform_node =
        GetPropertyTrees()->transform_tree().Node(transform_tree_index);

    DCHECK(transform_node);
    EXPECT_FALSE(transform_node->moved_by_outer_viewport_bounds_delta_y);
  }
}

// When a property tree change occurs that affects layer transform-origin, the
// transform can be directly updated without explicitly marking the layer as
// damaged. The ensure damage occurs, the transform node should have
// |transform_changed| set.
TEST_P(CompositingSimTest, DirectTransformOriginPropertyUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        @keyframes animateTransformA {
          0% { transform: translateX(0px); }
          100% { transform: translateX(100px); }
        }
        @keyframes animateTransformB {
          0% { transform: translateX(200px); }
          100% { transform: translateX(300px); }
        }
        #box {
          width: 100px;
          height: 100px;
          animation-name: animateTransformA;
          animation-duration: 999s;
          transform-origin: 10px 10px 100px;
          background: lightblue;
        }
      </style>
      <div id='box'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* box_element = GetElementById("box");
  auto* box_element_layer = CcLayerByDOMElementId("box");
  auto transform_tree_index = box_element_layer->transform_tree_index();
  const auto* transform_node =
      GetPropertyTrees()->transform_tree().Node(transform_tree_index);

  // Initially, transform should be unchanged.
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the transform-origin in a simple way allowed for a direct update.
  box_element->setAttribute(html_names::kStyleAttr,
                            AtomicString("animation-name: animateTransformB"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

// This test is similar to |LayerSubtreeTransformPropertyChanged| but for
// effect property node changes.
TEST_P(CompositingSimTest, LayerSubtreeEffectPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          filter: blur(10px);
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform, filter;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(outer_element_layer)->effect_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(inner_element_layer)->effect_changed);

  // Modifying the filter style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("filter: blur(20px)"));
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  // Set by blink::PropertyTreeManager.
  EXPECT_TRUE(GetEffectNode(outer_element_layer)->effect_changed);
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(inner_element_layer)->effect_changed);

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(outer_element_layer)->effect_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(inner_element_layer)->effect_changed);
}

// This test is similar to |LayerSubtreeTransformPropertyChanged| but for
// clip property node changes.
TEST_P(CompositingSimTest, LayerSubtreeClipPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          position: absolute;
          clip: rect(10px, 80px, 70px, 40px);
          background: lightgreen;
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Modifying the clip style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("clip: rect(1px, 8px, 7px, 4px);"));
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

TEST_P(CompositingSimTest, LayerSubtreeOverflowClipPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          position: absolute;
          overflow: hidden;
        }
        #inner {
          width: 200px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Modifying the clip width should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              AtomicString("width: 200px;"));
  UpdateAllLifecyclePhases();
  // The overflow clip does not affect |outer_element_layer|, so
  // subtree_property_changed should be false for it. It does affect
  // |inner_element_layer| though.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

// This test is similar to |LayerSubtreeClipPropertyChanged| but for cases when
// the clip node itself does not change but the clip node associated with a
// layer changes.
TEST_P(CompositingSimTest, LayerClipPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #outer {
          width: 100px;
          height: 100px;
        }
        #inner {
          width: 50px;
          height: 200px;
          backface-visibility: hidden;
          background: lightblue;
        }
      </style>
      <div id='outer' style='overflow: hidden;'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  EXPECT_TRUE(inner_element_layer->should_check_backface_visibility());

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Removing overflow: hidden on the outer div should set
  // |subtree_property_changed| on the inner div's cc::Layer.
  auto* outer_element = GetElementById("outer");
  outer_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhases();

  inner_element_layer = CcLayerByDOMElementId("inner");
  EXPECT_TRUE(inner_element_layer->should_check_backface_visibility());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

TEST_P(CompositingSimTest, SafeOpaqueBackgroundColor) {
  InitializeWithHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      body { background: yellow; }
      div {
        position: absolute;
        z-index: 1;
        width: 20px;
        height: 20px;
        will-change: transform; /* Composited */
      }
      #opaque-color {
        background: blue;
      }
      #opaque-image, #opaque-image-translucent-color {
        background: linear-gradient(blue, green);
      }
      #partly-opaque div {
        width: 15px;
        height: 15px;
        background: blue;
        will-change: initial;
      }
      #translucent, #opaque-image-translucent-color div {
        background: rgba(0, 255, 255, 0.5);
        will-change: initial;
      }
    </style>
    <div id="opaque-color"></div>
    <div id="opaque-image"></div>
    <div id="opaque-image-translucent-color">
      <div></div>
    </div>
    <div id="partly-opaque">
      <div></div>
    </div>
    <div id="translucent"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* opaque_color = CcLayerByDOMElementId("opaque-color");
  EXPECT_TRUE(opaque_color->contents_opaque());
  EXPECT_EQ(opaque_color->background_color(), SkColors::kBlue);
  EXPECT_EQ(opaque_color->SafeOpaqueBackgroundColor(), SkColors::kBlue);

  auto* opaque_image = CcLayerByDOMElementId("opaque-image");
  EXPECT_FALSE(opaque_image->contents_opaque());
  EXPECT_EQ(opaque_image->background_color(), SkColors::kTransparent);
  EXPECT_EQ(opaque_image->SafeOpaqueBackgroundColor(), SkColors::kTransparent);

  // TODO(crbug.com/1399566): Alpha here should be 0.5.
  const SkColor4f kTranslucentCyan{0.0f, 1.0f, 1.0f, 128.0f / 255.0f};
  auto* opaque_image_translucent_color =
      CcLayerByDOMElementId("opaque-image-translucent-color");
  EXPECT_TRUE(opaque_image_translucent_color->contents_opaque());
  EXPECT_EQ(opaque_image_translucent_color->background_color(),
            kTranslucentCyan);
  // Use background_color() with the alpha channel forced to be opaque.
  EXPECT_EQ(opaque_image_translucent_color->SafeOpaqueBackgroundColor(),
            SkColors::kCyan);

  auto* partly_opaque = CcLayerByDOMElementId("partly-opaque");
  EXPECT_FALSE(partly_opaque->contents_opaque());
  EXPECT_EQ(partly_opaque->background_color(), SkColors::kBlue);
  // SafeOpaqueBackgroundColor() returns SK_ColorTRANSPARENT when
  // background_color() is opaque and contents_opaque() is false.
  EXPECT_EQ(partly_opaque->SafeOpaqueBackgroundColor(), SkColors::kTransparent);

  auto* translucent = CcLayerByDOMElementId("translucent");
  EXPECT_FALSE(translucent->contents_opaque());
  EXPECT_EQ(translucent->background_color(), kTranslucentCyan);
  // SafeOpaqueBackgroundColor() returns background_color() if it's not opaque
  // and contents_opaque() is false.
  EXPECT_EQ(translucent->SafeOpaqueBackgroundColor(), kTranslucentCyan);
}

TEST_P(CompositingSimTest, SquashingLayerSafeOpaqueBackgroundColor) {
  InitializeWithHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div {
        position: absolute;
        z-index: 1;
        width: 20px;
        height: 20px;
      }
      #behind {
        top: 12px;
        left: 12px;
        background: blue;
        will-change: transform; /* Composited */
      }
      #topleft {
        top: 0px;
        left: 0px;
        background: lime;
      }
      #bottomright {
        top: 24px;
        left: 24px;
        width: 100px;
        height: 100px;
        background: cyan;
      }
    </style>
    <div id="behind"></div>
    <div id="topleft"></div>
    <div id="bottomright"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* squashing_layer = CcLayerByDOMElementId("topleft");
  ASSERT_TRUE(squashing_layer);
  EXPECT_EQ(gfx::Size(124, 124), squashing_layer->bounds());

  // Top left and bottom right are squashed.
  // This squashed layer should not be opaque, as it is squashing two squares
  // with some gaps between them.
  EXPECT_FALSE(squashing_layer->contents_opaque());
  // The background color of #bottomright is used as the background color
  // because it covers the most significant area of the squashing layer.
  EXPECT_EQ(squashing_layer->background_color(), SkColors::kCyan);
  // SafeOpaqueBackgroundColor() returns SK_ColorTRANSPARENT when
  // background_color() is opaque and contents_opaque() is false.
  EXPECT_EQ(squashing_layer->SafeOpaqueBackgroundColor(),
            SkColors::kTransparent);
}

// Test that a pleasant checkerboard color is used in the presence of blending.
TEST_P(CompositingSimTest, RootScrollingContentsSafeOpaqueBackgroundColor) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <div style="mix-blend-mode: multiply;"></div>
      <div id="forcescroll" style="height: 10000px;"></div>
  )HTML");
  Compositor().BeginFrame();

  auto* scrolling_contents = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(),
      MainFrame().GetFrameView()->LayoutViewport()->GetScrollElementId());
  EXPECT_EQ(scrolling_contents->background_color(), SkColors::kWhite);
  EXPECT_EQ(scrolling_contents->SafeOpaqueBackgroundColor(), SkColors::kWhite);
}

TEST_P(CompositingSimTest, NonDrawableLayersIgnoredForRenderSurfaces) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #outer {
          width: 100px;
          height: 100px;
          opacity: 0.5;
          background: blue;
        }
        #inner {
          width: 10px;
          height: 10px;
          will-change: transform;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  EXPECT_FALSE(inner_element_layer->draws_content());
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  EXPECT_TRUE(outer_element_layer->draws_content());

  // The inner element layer is only needed for hit testing and does not draw
  // content, so it should not cause a render surface.
  auto effect_tree_index = outer_element_layer->effect_tree_index();
  const auto* effect_node =
      GetPropertyTrees()->effect_tree().Node(effect_tree_index);
  EXPECT_EQ(effect_node->opacity, 0.5f);
  EXPECT_FALSE(effect_node->HasRenderSurface());
}

TEST_P(CompositingSimTest, NoRenderSurfaceWithAxisAlignedTransformAnimation) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes translation {
          0% { transform: translate(10px, 11px); }
          100% { transform: translate(20px, 21px); }
        }
        .animate {
          animation-name: translation;
          animation-duration: 1s;
          width: 100px;
          height: 100px;
          overflow: hidden;
        }
        .compchild {
          height: 200px;
          width: 10px;
          background: lightblue;
          will-change: transform;
        }
      </style>
      <div class="animate"><div class="compchild"></div></div>
  )HTML");
  Compositor().BeginFrame();
  // No effect node with kClipAxisAlignment should be created because the
  // animation is axis-aligned.
  for (const auto& effect_node : GetPropertyTrees()->effect_tree().nodes()) {
    EXPECT_NE(cc::RenderSurfaceReason::kClipAxisAlignment,
              effect_node.render_surface_reason);
  }
}

TEST_P(CompositingSimTest, PromoteCrossOriginIframe) {
  InitializeWithHTML("<!DOCTYPE html><iframe id=iframe sandbox></iframe>");
  Compositor().BeginFrame();
  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  auto* layer = CcLayerForIFrameContent(iframe_doc);
  EXPECT_TRUE(layer);
  EXPECT_EQ(layer->bounds(), gfx::Size(300, 150));
}

// On initial layout, the iframe is not yet loaded and is not considered
// cross origin. This test ensures the iframe is promoted due to being cross
// origin after the iframe loads.
TEST_P(CompositingSimTest, PromoteCrossOriginIframeAfterLoading) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest frame_resource("https://origin-b.com/b.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" src="https://origin-b.com/b.html"></iframe>
  )HTML");
  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  EXPECT_TRUE(CcLayerForIFrameContent(iframe_doc));
}

// An iframe that is cross-origin to the parent should be composited. This test
// sets up nested frames with domains A -> B -> A. Both the child and grandchild
// frames should be composited because they are cross-origin to their parent.
TEST_P(CompositingSimTest, PromoteCrossOriginToParent) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest child_resource("https://origin-b.com/b.html", "text/html");
  SimRequest grandchild_resource("https://origin-a.com/c.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="main_iframe" src="https://origin-b.com/b.html"></iframe>
  )HTML");
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="child_iframe" src="https://origin-a.com/c.html"></iframe>
  )HTML");
  grandchild_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("main_iframe"))
          ->contentDocument();
  EXPECT_TRUE(CcLayerByOwnerNode(iframe_doc));

  iframe_doc = To<HTMLFrameOwnerElement>(
                   iframe_doc->getElementById(AtomicString("child_iframe")))
                   ->contentDocument();
  EXPECT_TRUE(CcLayerForIFrameContent(iframe_doc));
}

// Initially the iframe is cross-origin and should be composited. After changing
// to same-origin, the frame should no longer be composited.
TEST_P(CompositingSimTest, PromoteCrossOriginIframeAfterDomainChange) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest frame_resource("https://sub.origin-a.com/b.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" src="https://sub.origin-a.com/b.html"></iframe>
  )HTML");
  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  auto* iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("iframe")));

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  EXPECT_TRUE(CcLayerForIFrameContent(iframe_doc));

  NonThrowableExceptionState exception_state;
  GetDocument().setDomain(String("origin-a.com"), exception_state);
  iframe_element->contentDocument()->setDomain(String("origin-a.com"),
                                               exception_state);
  // We may not have scheduled a visual update so force an update instead of
  // using BeginFrame.
  UpdateAllLifecyclePhases();

  iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  EXPECT_FALSE(CcLayerForIFrameContent(iframe_doc));
}

// This test sets up nested frames with domains A -> B -> A. Initially, the
// child frame and grandchild frame should be composited. After changing the
// child frame to A (same-origin), both child and grandchild frames should no
// longer be composited.
TEST_P(CompositingSimTest, PromoteCrossOriginToParentIframeAfterDomainChange) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest child_resource("https://sub.origin-a.com/b.html", "text/html");
  SimRequest grandchild_resource("https://origin-a.com/c.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="main_iframe" src="https://sub.origin-a.com/b.html"></iframe>
  )HTML");
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="child_iframe" src="https://origin-a.com/c.html"></iframe>
  )HTML");
  grandchild_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("main_iframe"))
          ->contentDocument();
  EXPECT_TRUE(CcLayerByOwnerNode(iframe_doc));

  iframe_doc = To<HTMLFrameOwnerElement>(
                   iframe_doc->getElementById(AtomicString("child_iframe")))
                   ->contentDocument();
  EXPECT_TRUE(CcLayerForIFrameContent(iframe_doc));

  auto* main_iframe_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("main_iframe")));
  NonThrowableExceptionState exception_state;

  GetDocument().setDomain(String("origin-a.com"), exception_state);
  auto* child_iframe_element = To<HTMLIFrameElement>(
      main_iframe_element->contentDocument()->getElementById(
          AtomicString("child_iframe")));
  child_iframe_element->contentDocument()->setDomain(String("origin-a.com"),
                                                     exception_state);
  main_iframe_element->contentDocument()->setDomain(String("origin-a.com"),
                                                    exception_state);

  // We may not have scheduled a visual update so force an update instead of
  // using BeginFrame.
  UpdateAllLifecyclePhases();
  iframe_doc = To<HTMLFrameOwnerElement>(GetElementById("main_iframe"))
                   ->contentDocument();
  EXPECT_FALSE(CcLayerByOwnerNode(iframe_doc));

  iframe_doc = To<HTMLFrameOwnerElement>(
                   iframe_doc->getElementById(AtomicString("child_iframe")))
                   ->contentDocument();
  EXPECT_FALSE(CcLayerForIFrameContent(iframe_doc));
}

// Regression test for https://crbug.com/1095167. Render surfaces require that
// EffectNode::stable_id is set.
TEST_P(CompositingTest, EffectNodesShouldHaveElementIds) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div style="overflow: hidden; border-radius: 2px; height: 10px;">
      <div style="backdrop-filter: grayscale(3%);">
        a
        <span style="backdrop-filter: grayscale(3%);">b</span>
      </div>
    </div>
  )HTML");
  auto* property_trees = RootCcLayer()->layer_tree_host()->property_trees();
  for (const auto& effect_node : property_trees->effect_tree().nodes()) {
    if (effect_node.parent_id != cc::kInvalidPropertyNodeId) {
      EXPECT_TRUE(!!effect_node.element_id);
    }
  }
}

TEST_P(CompositingSimTest, ImplSideScrollSkipsCommit) {
  InitializeWithHTML(R"HTML(
    <div id='scroller' style='will-change: transform; overflow: scroll;
        width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* scroller = GetDocument().getElementById(AtomicString("scroller"));
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  auto element_id = scrollable_area->GetScrollElementId();

  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());

  // Simulate the scroll update with scroll delta from impl-side.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.emplace_back(element_id, gfx::Vector2dF(0, 10),
                                   std::nullopt);
  Compositor().LayerTreeHost()->ApplyCompositorChanges(&commit_data);
  EXPECT_EQ(gfx::PointF(0, 10), scrollable_area->ScrollPosition());
  EXPECT_EQ(
      gfx::PointF(0, 10),
      GetPropertyTrees()->scroll_tree().current_scroll_offset(element_id));

  UpdateAllLifecyclePhasesExceptPaint();
  // The scroll offset change should be directly updated, and the direct update
  // should not schedule commit because the scroll offset is the same as the
  // current cc scroll offset.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());

  // Update just the blink lifecycle because a full frame would clear the bit
  // for whether a commit was requested.
  UpdateAllLifecyclePhases();

  // A main frame is needed to call UpdateLayers which updates property trees,
  // re-calculating cached to/from-screen transforms.
  EXPECT_TRUE(Compositor().LayerTreeHost()->RequestedMainFramePending());

  // A full commit is not needed.
  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
}

TEST_P(CompositingSimTest, RasterInducingScrollSkipsCommit) {
  InitializeWithHTML(R"HTML(
    <div id='scroller' style='overflow: scroll; width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* scroller = GetDocument().getElementById(AtomicString("scroller"));
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  auto element_id = scrollable_area->GetScrollElementId();

  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());

  // Simulate the scroll update with scroll delta from impl-side.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.emplace_back(element_id, gfx::Vector2dF(0, 10),
                                   std::nullopt);
  Compositor().LayerTreeHost()->ApplyCompositorChanges(&commit_data);
  EXPECT_EQ(gfx::PointF(0, 10), scrollable_area->ScrollPosition());
  EXPECT_EQ(
      gfx::PointF(0, 10),
      GetPropertyTrees()->scroll_tree().current_scroll_offset(element_id));

  UpdateAllLifecyclePhasesExceptPaint();
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    // The scroll offset change should be directly updated, and the direct
    // update should not schedule commit because the scroll offset is the same
    // as the current cc scroll offset.
    EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
    EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
  } else {
    EXPECT_TRUE(paint_artifact_compositor()->NeedsUpdate());
    EXPECT_TRUE(Compositor().LayerTreeHost()->CommitRequested());
  }

  // Update just the blink lifecycle because a full frame would clear the bit
  // for whether a commit was requested.
  UpdateAllLifecyclePhases();

  // A main frame is needed to call UpdateLayers which updates property trees,
  // re-calculating cached to/from-screen transforms.
  EXPECT_TRUE(Compositor().LayerTreeHost()->RequestedMainFramePending());

  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    // A full commit is not needed.
    EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
  } else {
    EXPECT_TRUE(Compositor().LayerTreeHost()->CommitRequested());
  }
}

TEST_P(CompositingSimTest, ImplSideScrollUnpaintedSkipsCommit) {
  InitializeWithHTML(R"HTML(
    <div style='height: 10000px'></div>
    <div id='scroller' style='overflow: scroll; width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* scroller = GetDocument().getElementById(AtomicString("scroller"));
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  auto element_id = scrollable_area->GetScrollElementId();

  // The scroller is far away from the viewport so is not painted.
  // The scroll node always exists.
  auto* scroll_node =
      GetPropertyTrees()->scroll_tree().FindNodeFromElementId(element_id);
  ASSERT_TRUE(scroll_node);
  EXPECT_EQ(cc::kInvalidPropertyNodeId, scroll_node->transform_id);

  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());

  // Simulate the scroll update with scroll delta from impl-side.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.emplace_back(element_id, gfx::Vector2dF(0, 10),
                                   std::nullopt);
  Compositor().LayerTreeHost()->ApplyCompositorChanges(&commit_data);
  EXPECT_EQ(gfx::PointF(0, 10), scrollable_area->ScrollPosition());
  EXPECT_EQ(
      gfx::PointF(0, 10),
      GetPropertyTrees()->scroll_tree().current_scroll_offset(element_id));

  // Update just the blink lifecycle because a full frame would clear the bit
  // for whether a commit was requested.
  UpdateAllLifecyclePhases();

  // A main frame is needed to call UpdateLayers which updates property trees,
  // re-calculating cached to/from-screen transforms.
  EXPECT_TRUE(Compositor().LayerTreeHost()->RequestedMainFramePending());

  // A full commit is not needed.
  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
}

TEST_P(CompositingSimTest, ImplSideScaleSkipsCommit) {
  InitializeWithHTML(R"HTML(
    <div>Empty Page</div>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
  ASSERT_EQ(1.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  // Simulate a page scale delta (i.e. user pinch-zoomed) on the compositor.
  cc::CompositorCommitData commit_data;
  commit_data.page_scale_delta = 2.f;

  {
    auto sync = Compositor().LayerTreeHost()->SimulateSyncingDeltasForTesting();
    Compositor().LayerTreeHost()->ApplyCompositorChanges(&commit_data);
  }

  // The transform tree's page scale factor isn't computed until we perform a
  // lifecycle update.
  ASSERT_EQ(1.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  // Update just the blink lifecycle because a full frame would clear the bit
  // for whether a commit was requested.
  UpdateAllLifecyclePhases();

  EXPECT_EQ(2.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  // A main frame is needed to call UpdateLayers which updates property trees,
  // re-calculating cached to/from-screen transforms.
  EXPECT_TRUE(Compositor().LayerTreeHost()->RequestedMainFramePending());

  // A full commit is not needed.
  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
}

// Ensure that updates to page scale coming from the main thread update the
// page scale factor on the transform tree.
TEST_P(CompositingSimTest, MainThreadScaleUpdatesTransformTree) {
  InitializeWithHTML(R"HTML(
    <div>Empty Page</div>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(1.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  VisualViewport& viewport = WebView().GetPage()->GetVisualViewport();

  // This test checks that the transform tree's page scale factor is correctly
  // updated when scale is set with an existing property tree.
  ASSERT_TRUE(viewport.GetPageScaleNode());
  viewport.SetScale(2.f);

  // The scale factor on the layer tree should be updated immediately.
  ASSERT_EQ(2.f, Compositor().LayerTreeHost()->page_scale_factor());

  // The transform tree's page scale factor isn't computed until we perform a
  // lifecycle update.
  ASSERT_EQ(1.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  Compositor().BeginFrame();

  EXPECT_EQ(2.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  // Ensure the transform node is also correctly updated.
  const cc::TransformNode* scale_node =
      GetPropertyTrees()->transform_tree().FindNodeFromElementId(
          viewport.GetPageScaleNode()->GetCompositorElementId());
  ASSERT_TRUE(scale_node);
  EXPECT_TRUE(scale_node->local.IsScale2d());
  EXPECT_EQ(gfx::Vector2dF(2, 2), scale_node->local.To2dScale());
}

// Similar to above but ensure the transform tree is correctly setup when scale
// already exists when building the tree.
TEST_P(CompositingSimTest, BuildTreeSetsScaleOnTransformTree) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <div>Empty Page</div>
  )HTML");

  VisualViewport& viewport = WebView().GetPage()->GetVisualViewport();

  // This test checks that the transform tree's page scale factor is correctly
  // set when scale is set before property trees have been built.
  ASSERT_FALSE(viewport.GetPageScaleNode());
  viewport.SetScale(2.f);

  // The scale factor on the layer tree should be updated immediately.
  ASSERT_EQ(2.f, Compositor().LayerTreeHost()->page_scale_factor());

  // The transform tree's page scale factor isn't computed until we perform a
  // lifecycle update.
  ASSERT_EQ(1.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  Compositor().BeginFrame();

  EXPECT_EQ(2.f, GetPropertyTrees()->transform_tree().page_scale_factor());

  // Ensure the transform node is also correctly updated.
  const cc::TransformNode* scale_node =
      GetPropertyTrees()->transform_tree().FindNodeFromElementId(
          viewport.GetPageScaleNode()->GetCompositorElementId());
  ASSERT_TRUE(scale_node);
  EXPECT_TRUE(scale_node->local.IsScale2d());
  EXPECT_EQ(gfx::Vector2dF(2, 2), scale_node->local.To2dScale());
}

TEST_P(CompositingSimTest, ScrollWithMainThreadReasonsNeedsCommit) {
  InitializeWithHTML(R"HTML(
    <style>
      body { height: 2500px; }
      #h { background: url(data:image/png;base64,invalid) fixed; }
    </style>
    <div id="h">ABCDE</div>
  )HTML");
  Compositor().BeginFrame();
  auto* layer_tree_host = Compositor().LayerTreeHost();
  EXPECT_FALSE(layer_tree_host->CommitRequested());

  // Simulate 100px scroll from compositor thread.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.emplace_back(
      MainFrame().GetFrameView()->LayoutViewport()->GetScrollElementId(),
      gfx::Vector2dF(0, 100.f), std::nullopt);
  layer_tree_host->ApplyCompositorChanges(&commit_data);

  // Due to main thread scrolling reasons (fixed-background element), we need a
  // commit to push the update to the transform tree.
  EXPECT_TRUE(layer_tree_host->CommitRequested());
}

TEST_P(CompositingSimTest, FrameAttribution) {
  InitializeWithHTML(R"HTML(
    <div id='child' style='will-change: transform;'>test</div>
    <iframe id='iframe' sandbox></iframe>
  )HTML");

  Compositor().BeginFrame();

  // Ensure that we correctly attribute child layers in the main frame to their
  // containing document.
  auto* child_layer = CcLayerByDOMElementId("child");
  ASSERT_TRUE(child_layer);

  auto* child_transform_node = GetTransformNode(child_layer);
  ASSERT_TRUE(child_transform_node);

  // Iterate the transform tree to gather the parent frame element ID.
  cc::ElementId visible_frame_element_id;
  const auto* current_transform_node = child_transform_node;
  while (current_transform_node) {
    visible_frame_element_id = current_transform_node->visible_frame_element_id;
    if (visible_frame_element_id)
      break;
    current_transform_node =
        GetPropertyTrees()->transform_tree().parent(current_transform_node);
  }

  EXPECT_EQ(visible_frame_element_id,
            CompositorElementIdFromUniqueObjectId(
                GetDocument().GetDomNodeId(),
                CompositorElementIdNamespace::kDOMNodeId));

  // Test that a layerized subframe's frame element ID is that of its
  // containing document.
  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  auto* iframe_layer = CcLayerForIFrameContent(iframe_doc);
  ASSERT_TRUE(iframe_layer);
  auto* iframe_transform_node = GetTransformNode(iframe_layer);
  EXPECT_TRUE(iframe_transform_node);

  EXPECT_EQ(iframe_transform_node->visible_frame_element_id,
            CompositorElementIdFromUniqueObjectId(
                iframe_doc->GetDomNodeId(),
                CompositorElementIdNamespace::kDOMNodeId));
}

TEST_P(CompositingSimTest, VisibleFrameRootLayers) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest frame_resource("https://origin-b.com/b.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" src="https://origin-b.com/b.html"></iframe>
  )HTML");
  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  // Ensure that the toplevel is marked as a visible root.
  auto* toplevel_layer = CcLayerByOwnerNode(&GetDocument());
  ASSERT_TRUE(toplevel_layer);
  auto* toplevel_transform_node = GetTransformNode(toplevel_layer);
  ASSERT_TRUE(toplevel_transform_node);

  EXPECT_TRUE(toplevel_transform_node->visible_frame_element_id);

  // Ensure that the iframe is marked as a visible root.
  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  auto* iframe_layer = CcLayerForIFrameContent(iframe_doc);
  ASSERT_TRUE(iframe_layer);
  auto* iframe_transform_node = GetTransformNode(iframe_layer);
  ASSERT_TRUE(iframe_transform_node);

  EXPECT_TRUE(iframe_transform_node->visible_frame_element_id);

  // Verify that after adding `pointer-events: none`, the subframe is no longer
  // considered a visible root.
  GetElementById("iframe")->SetInlineStyleProperty(
      CSSPropertyID::kPointerEvents, "none");

  UpdateAllLifecyclePhases();

  iframe_layer = CcLayerForIFrameContent(iframe_doc);
  ASSERT_TRUE(iframe_layer);
  iframe_transform_node = GetTransformNode(iframe_layer);
  ASSERT_TRUE(iframe_transform_node);

  EXPECT_FALSE(iframe_transform_node->visible_frame_element_id);
}

TEST_P(CompositingSimTest, DecompositedTransformWithChange) {
  InitializeWithHTML(R"HTML(
    <style>
      svg { overflow: hidden; }
      .initial { transform: rotate3d(0,0,1,10deg); }
      .changed { transform: rotate3d(0,0,1,0deg); }
    </style>
    <div style='will-change: transform;'>
      <svg id='svg' xmlns='http://www.w3.org/2000/svg' class='initial'>
        <line x1='50%' x2='50%' y1='0' y2='100%' stroke='blue'/>
        <line y1='50%' y2='50%' x1='0' x2='100%' stroke='blue'/>
      </svg>
    </div>
  )HTML");

  Compositor().BeginFrame();

  auto* svg_element_layer = CcLayerByDOMElementId("svg");
  EXPECT_FALSE(svg_element_layer->subtree_property_changed());

  auto* svg_element = GetElementById("svg");
  svg_element->setAttribute(html_names::kClassAttr, AtomicString("changed"));
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(svg_element_layer->subtree_property_changed());
}

// A simple repaint update should use a fast-path in PaintArtifactCompositor.
TEST_P(CompositingSimTest, BackgroundColorChangeUsesRepaintUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: white;
        }
      </style>
      <div id='target'></div>
  )HTML");

  Compositor().BeginFrame();

  EXPECT_EQ(CcLayerByDOMElementId("target")->background_color(),
            SkColors::kWhite);

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("background: black"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  // Though a repaint-only update was done, the background color should still
  // be updated.
  EXPECT_EQ(CcLayerByDOMElementId("target")->background_color(),
            SkColors::kBlack);
}

// Similar to |BackgroundColorChangeUsesRepaintUpdate| but with multiple paint
// chunks being squashed into a single PendingLayer, and the background coming
// from the last paint chunk.
TEST_P(CompositingSimTest, MultipleChunkBackgroundColorChangeRepaintUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          position: absolute;
          width: 20px;
          height: 20px;
          top: 0px;
          left: 0px;
        }
        #a {
          background: lime;
        }
        #b {
          background: red;
          transform: translate(-100px, -100px);
        }
        #c {
          width: 800px;
          height: 600px;
          background: black;
        }
      </style>
      <div id="a"></div>
      <div id="b"></div>
      <!-- background color source -->
      <div id="c"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* scrolling_contents = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(),
      MainFrame().GetFrameView()->LayoutViewport()->GetScrollElementId());

  EXPECT_EQ(scrolling_contents->background_color(), SkColors::kBlack);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* background_element = GetElementById("c");
  background_element->setAttribute(html_names::kStyleAttr,
                                   AtomicString("background: white"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  // Though a repaint-only update was done, the background color should still
  // be updated.
  EXPECT_EQ(scrolling_contents->background_color(), SkColors::kWhite);
}

// Similar to |BackgroundColorChangeUsesRepaintUpdate| but with post-paint
// composited SVG. This test changes paint for a composited SVG element, as well
// as a regular HTML element in the presence of composited SVG.
TEST_P(CompositingSimTest, SVGColorChangeUsesRepaintUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        rect, div {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <svg>
        <rect fill="blue" />
        <rect id="rect" fill="blue" />
        <rect fill="blue" />
      </svg>
      <div id="div" style="background: blue;" />
      <svg>
        <rect fill="blue" />
      </svg>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* rect_element = GetElementById("rect");
  rect_element->setAttribute(svg_names::kFillAttr, AtomicString("black"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* div_element = GetElementById("div");
  div_element->setAttribute(html_names::kStyleAttr,
                            AtomicString("background: black"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);
}

TEST_P(CompositingSimTest, ChangingOpaquenessRequiresFullUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightgreen;
        }
      </style>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_TRUE(CcLayerByDOMElementId("target")->contents_opaque());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A change in opaqueness still requires a full update because opaqueness is
  // used during compositing to set the cc::Layer's contents opaque property
  // (see: PaintArtifactCompositor::CompositedLayerForPendingLayer).
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("background: rgba(1, 0, 0, 0.1)"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque());
}

TEST_P(CompositingSimTest, ChangingContentsOpaqueForTextRequiresFullUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
        #textContainer {
          width: 50px;
          height: 50px;
          padding: 5px;
          background: lightblue;
        }
      </style>
      <div id="target">
        <div id="textContainer">
          mars
        </div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque());
  EXPECT_TRUE(CcLayerByDOMElementId("target")->contents_opaque_for_text());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A change in opaqueness for text still requires a full update because
  // opaqueness is used during compositing to set the cc::Layer's contents
  // opaque for text property (see:
  // PaintArtifactCompositor::CompositedLayerForPendingLayer).
  auto* text_container_element = GetElementById("textContainer");
  text_container_element->setAttribute(
      html_names::kStyleAttr, AtomicString("background: rgba(1, 0, 0, 0.1)"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque());
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque_for_text());
}

TEST_P(CompositingSimTest, ChangingDrawsContentRequiresFullUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_FALSE(CcLayerByDOMElementId("target")->draws_content());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A simple repaint change that causes Layer::DrawsContent to change still
  // needs to cause a full update because it can affect whether mask layers are
  // created.
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("background: rgba(0,0,0,0.5)"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
  EXPECT_TRUE(CcLayerByDOMElementId("target")->draws_content());
}

TEST_P(CompositingSimTest, ContentsOpaqueForTextWithSubpixelSizeSimpleBg) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <div id="target" style="will-change: transform; background: white;
                              width: 100.6px; height: 10.3px">
        TEXT
      </div>
  )HTML");
  Compositor().BeginFrame();
  auto* cc_layer = CcLayerByDOMElementId("target");
  // We adjust visual rect of the DrawingDisplayItem with simple painting to the
  // bounds of the painting.
  EXPECT_EQ(gfx::Size(101, 10), cc_layer->bounds());
  EXPECT_TRUE(cc_layer->contents_opaque());
  EXPECT_TRUE(cc_layer->contents_opaque_for_text());
}

TEST_P(CompositingSimTest, ContentsOpaqueForTextWithSubpixelSizeComplexBg) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <div id="target" style="will-change: transform; background: white;
                              border: 2px inset blue;
                              width: 100.6px; height: 10.3px">
        TEXT
      </div>
  )HTML");
  Compositor().BeginFrame();
  auto* cc_layer = CcLayerByDOMElementId("target");
  EXPECT_EQ(gfx::Size(105, 15), cc_layer->bounds());
  EXPECT_FALSE(cc_layer->contents_opaque());
  EXPECT_TRUE(cc_layer->contents_opaque_for_text());
}

TEST_P(CompositingSimTest, ContentsOpaqueForTextWithPartialBackground) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <div id="target" style="will-change: transform; padding: 10px">
        <div style="background: white">TEXT</div>
      </div>
  )HTML");
  Compositor().BeginFrame();
  auto* cc_layer = CcLayerByDOMElementId("target");
  EXPECT_FALSE(cc_layer->contents_opaque());
  EXPECT_TRUE(cc_layer->contents_opaque_for_text());
}

TEST_P(CompositingSimTest, ContentsOpaqueForTextWithBorderRadiusAndPadding) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <div id="target" style="will-change: transform; border-radius: 5px;
                              padding: 10px; background: blue">
        TEXT
      </div>
  )HTML");
  Compositor().BeginFrame();
  auto* cc_layer = CcLayerByDOMElementId("target");
  EXPECT_FALSE(cc_layer->contents_opaque());
  EXPECT_TRUE(cc_layer->contents_opaque_for_text());
}

TEST_P(CompositingSimTest, FullCompositingUpdateReasons) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
          position: absolute;
        }
        #a {
          background: lightgreen;
          z-index: 10;
        }
        #b {
          background: lightblue;
          z-index: 20;
        }
      </style>
      <div id="a"></div>
      <div id="b"></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Reordering paint chunks requires a full update. Overlap testing and the
  // order of synthetic effect layers are two examples of paint changes that
  // affect compositing decisions.
  auto* b_element = GetElementById("b");
  b_element->setAttribute(html_names::kStyleAttr, AtomicString("z-index: 5"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Removing a paint chunk requires a full update.
  b_element->setAttribute(html_names::kStyleAttr,
                          AtomicString("display: none"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Adding a paint chunk requires a full update.
  b_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Changing the size of a chunk affects overlap and requires a full update.
  b_element->setAttribute(html_names::kStyleAttr, AtomicString("width: 101px"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

// Similar to |FullCompositingUpdateReasons| but for changes in post-paint
// composited SVG.
TEST_P(CompositingSimTest, FullCompositingUpdateReasonWithCompositedSVG) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #rect {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <svg>
        <rect id="rect" fill="blue" />
      </svg>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Changing the size of a chunk affects overlap and requires a full update.
  auto* rect = GetElementById("rect");
  rect->setAttribute(html_names::kStyleAttr, AtomicString("width: 101px"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

TEST_P(CompositingSimTest, FullCompositingUpdateForJustCreatedChunks) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        .firstLetterStyle:first-letter {
          background: red;
        }
        rect {
          width: 100px;
          height: 100px;
          fill: blue;
        }
      </style>
      <svg>
        <rect style="will-change: transform;"></rect>
        <rect id="target"></rect>
      </svg>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A new LayoutObject is "just created" and will not match existing chunks and
  // needs a full update. A first letter style adds a pseudo element which
  // results in rebuilding the #target LayoutObject.
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kClassAttr,
                       AtomicString("firstLetterStyle"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

TEST_P(CompositingSimTest, FullCompositingUpdateForUncachableChunks) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        rect {
          width: 100px;
          height: 100px;
          fill: blue;
          will-change: transform;
        }
        div {
          width: 100px;
          height: 100px;
          background: lightblue;
        }
      </style>
      <svg>
        <rect id="rect"></rect>
      </svg>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  // Make the rect display item client uncachable. To avoid depending on when
  // this occurs in practice (see: |DisplayItemCacheSkipper|), this is done
  // directly.
  auto* rect = GetElementById("rect");
  auto* rect_client = static_cast<DisplayItemClient*>(rect->GetLayoutObject());
  rect_client->Invalidate(PaintInvalidationReason::kUncacheable);
  rect->setAttribute(html_names::kStyleAttr, AtomicString("fill: green"));
  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A full update should be required due to the presence of uncacheable
  // paint chunks.
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("background: lightgreen"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

TEST_P(CompositingSimTest, DecompositeScrollerInHiddenIframe) {
  SimRequest top_resource("https://example.com/top.html", "text/html");
  SimRequest middle_resource("https://example.com/middle.html", "text/html");
  SimRequest bottom_resource("https://example.com/bottom.html", "text/html");

  LoadURL("https://example.com/top.html");
  top_resource.Complete(R"HTML(
    <iframe id='middle' src='https://example.com/middle.html'></iframe>
  )HTML");
  middle_resource.Complete(R"HTML(
    <iframe id='bottom' src='bottom.html'></iframe>
  )HTML");
  bottom_resource.Complete(R"HTML(
    <div id='scroller' style='overflow:scroll;max-height:100px;background-color:#888'>
      <div style='height:1000px;'>Hello, world!</div>
    </div>
  )HTML");

  LocalFrame& middle_frame =
      *To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  LocalFrame& bottom_frame = *To<LocalFrame>(middle_frame.Tree().FirstChild());
  middle_frame.View()->BeginLifecycleUpdates();
  bottom_frame.View()->BeginLifecycleUpdates();
  Compositor().BeginFrame();
  EXPECT_TRUE(CcLayerByDOMElementId("scroller"));

  // Hide the iframes. Scroller should be decomposited.
  GetDocument()
      .getElementById(AtomicString("middle"))
      ->SetInlineStyleProperty(CSSPropertyID::kVisibility, CSSValueID::kHidden);
  Compositor().BeginFrame();
  EXPECT_FALSE(CcLayerByDOMElementId("scroller"));
}

TEST_P(CompositingSimTest, ForeignLayersInMovedSubsequence) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <style> iframe { isolation: isolate; } </style>
      <iframe sandbox src="https://origin-b.com/b.html"></iframe>
      <div id="target" style="background: blue;">a</div>
  )HTML");

  FakeRemoteFrameHost remote_frame_host;
  WebRemoteFrameImpl* remote_frame = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(
      MainFrame().FirstChild(), remote_frame,
      remote_frame_host.BindNewAssociatedRemote());

  Compositor().BeginFrame();

  auto remote_surface_layer = cc::SurfaceLayer::Create();
  remote_frame->GetFrame()->SetCcLayerForTesting(remote_surface_layer, true);
  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("background: green;"));
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  remote_frame->Detach();
}

// While not required for correctness, it is important for performance that
// snapped backgrounds use solid color layers which avoid tiling.
TEST_P(CompositingSimTest, SolidColorLayersWithSnapping) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #snapDown {
          width: 60.1px;
          height: 100px;
          will-change: opacity;
          background: blue;
        }
        #snapUp {
          width: 60.9px;
          height: 100px;
          will-change: opacity;
          background: blue;
        }
      </style>
      <div id="snapDown"></div>
      <div id="snapUp"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* snap_down = CcLayerByDOMElementId("snapDown");
  auto* snap_up = CcLayerByDOMElementId("snapUp");
  EXPECT_TRUE(snap_down->IsSolidColorLayerForTesting());
  EXPECT_TRUE(snap_up->IsSolidColorLayerForTesting());
}

TEST_P(CompositingSimTest, SolidColorLayerWithSubpixelTransform) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #forceCompositing {
          position: absolute;
          width: 100px;
          height: 100px;
          will-change: transform;
        }
        #target {
          position: absolute;
          top: 0;
          left: 0;
          width: 60.9px;
          height: 60.1px;
          transform: translate(0.4px, 0.6px);
          background: blue;
        }
      </style>
      <div id="forceCompositing"></div>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* target = CcLayerByDOMElementId("target");
  EXPECT_TRUE(target->IsSolidColorLayerForTesting());
  EXPECT_NEAR(0.4, target->offset_to_transform_parent().x(), 0.001);
  EXPECT_NEAR(0.6, target->offset_to_transform_parent().y(), 0.001);
}

// While not required for correctness, it is important for performance (e.g.,
// the MotionMark Focus benchmark) that we do not decomposite effect nodes (see:
// |PaintArtifactCompositor::DecompositeEffect|) when the author has specified
// 3D transforms which are frequently used as a generic compositing trigger.
TEST_P(CompositingSimTest, EffectCompositedWith3DTransform) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          width: 100px;
          height: 100px;
          background: rebeccapurple;
          transform: translate3d(1px, 1px, 0);
        }
      </style>
      <div id="opacity" style="opacity: 0.5;"></div>
      <div id="filter" style="filter: blur(1px);"></div>
  )HTML");
  Compositor().BeginFrame();

  auto* opacity_effect = GetEffectNode(CcLayerByDOMElementId("opacity"));
  EXPECT_TRUE(opacity_effect);
  EXPECT_EQ(opacity_effect->opacity, 0.5f);
  EXPECT_TRUE(opacity_effect->filters.IsEmpty());

  auto* filter_effect = GetEffectNode(CcLayerByDOMElementId("filter"));
  EXPECT_TRUE(filter_effect);
  EXPECT_EQ(filter_effect->opacity, 1.f);
  EXPECT_FALSE(filter_effect->filters.IsEmpty());
}

// The main thread will not have a chance to update the painted content of an
// animation running on the compositor, so ensure the cc::Layer with animating
// opacity has content when starting the animation, even if the opacity is
// initially 0.
TEST_P(CompositingSimTest, CompositorAnimationOfOpacityHasPaintedContent) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes opacity {
          0% { opacity: 0; }
          99% { opacity: 0; }
          100% { opacity: 0.5; }
        }
        #animation {
          animation-name: opacity;
          animation-duration: 999s;
          width: 100px;
          height: 100px;
          background: lightblue;
        }
      </style>
      <div id="animation"></div>
  )HTML");
  Compositor().BeginFrame();
  EXPECT_TRUE(CcLayerByDOMElementId("animation")->draws_content());
}

TEST_P(CompositingSimTest, CompositorAnimationOfNonInvertibleTransform) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes anim {
          0% { transform: scale(0); }
          99% { transform: scale(0); }
          100% { transform: scale(1); }
        }
        #animation {
          animation-name: anim;
          animation-duration: 999s;
          width: 100px;
          height: 100px;
          background: lightblue;
        }
      </style>
      <div id="animation"></div>
  )HTML");
  Compositor().BeginFrame();
  EXPECT_TRUE(CcLayerByDOMElementId("animation"));
  EXPECT_TRUE(CcLayerByDOMElementId("animation")->draws_content());
}

TEST_P(CompositingSimTest, CompositorAnimationRevealsChild) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes anim {
          0% { transform: rotateX(90deg); }
          99% { transform: rotateX(90deg); }
          100% { transform: rotateX(360deg); }
        }
        #animation {
          animation-name: anim;
          animation-duration: 999s;
          transform-style: preserve-3d;
          background: green;
          width: 100px;
          height: 100px;
        }
        #child {
          position: absolute;
          top: 0;
          left: 0;
          width: 50px;
          height: 100px;
          background: green;
          will-change: transform;
          transform: translateZ(16px);
        }
      </style>
      <div id="animation">
        <div id="child"></div>
      </div>
  )HTML");
  Compositor().BeginFrame();
  EXPECT_TRUE(CcLayerByDOMElementId("animation"));
  EXPECT_TRUE(CcLayerByDOMElementId("animation")->draws_content());
  // Though #child is not initially visible, it should be painted because it can
  // animate into view.
  EXPECT_TRUE(CcLayerByDOMElementId("child"));
  EXPECT_TRUE(CcLayerByDOMElementId("child")->draws_content());
}

static String ImageFileAsDataURL(const String& filename) {
  return "data:image/jpeg;base64," +
         Base64Encode(base::as_byte_span(
             *test::ReadFromFile(test::CoreTestDataPath(filename))));
}

TEST_P(CompositingSimTest, CompositedImageWithSubpixelOffset) {
  // The image is 100x50 with normal orientation.
  InitializeWithHTML("<!DOCTYPE html><img id='image' src='" +
                     ImageFileAsDataURL("exif-orientation-1-ul.jpg") +
                     "' style='position: absolute; width: 400px; height: 800px;"
                     "         top: 10.6px; will-change: top'>");
  Compositor().BeginFrame();
  auto* image_layer =
      static_cast<const cc::PictureLayer*>(CcLayerByDOMElementId("image"));
  ASSERT_TRUE(image_layer);
  EXPECT_EQ(gfx::Vector2dF(0.25f, 0.0625f),
            image_layer->GetRecordingSourceForTesting()
                .directly_composited_image_info()
                ->default_raster_scale);
}

TEST_P(CompositingSimTest, CompositedImageWithSubpixelOffsetAndOrientation) {
  // The image is 50x100 after transposed.
  InitializeWithHTML("<!DOCTYPE html><img id='image' src='" +
                     ImageFileAsDataURL("exif-orientation-5-lu.jpg") +
                     "' style='position: absolute; width: 800px; height: 400px;"
                     "         top: 10.6px; will-change: top'>");
  Compositor().BeginFrame();
  auto* image_layer =
      static_cast<const cc::PictureLayer*>(CcLayerByDOMElementId("image"));
  ASSERT_TRUE(image_layer);
  EXPECT_EQ(gfx::Vector2dF(0.0625f, 0.25f),
            image_layer->GetRecordingSourceForTesting()
                .directly_composited_image_info()
                ->default_raster_scale);
}

TEST_P(CompositingSimTest, ScrollingContentsLayerRecordedBounds) {
  InitializeWithHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div div {
        width: 2000px; height: 2000px; margin-top: 2000px; background: white;
      }
    </style>
    <div id="scroller" style="overflow: scroll; will-change: scroll-position;
                              width: 200px; height: 200px">
      <div>1</div>
      <div>2</div>
      <div>3</div>
      <div>4</div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* layer = static_cast<const cc::PictureLayer*>(
      ScrollingContentsCcLayerByScrollElementId(RootCcLayer(),
                                                GetElementById("scroller")
                                                    ->GetLayoutBox()
                                                    ->GetScrollableArea()
                                                    ->GetScrollElementId()));
  ASSERT_TRUE(layer);
  if (RuntimeEnabledFeatures::HitTestOpaquenessEnabled()) {
    EXPECT_EQ(gfx::Size(2000, 16000), layer->bounds());
    EXPECT_EQ(gfx::Rect(0, 0, 2000, 16000),
              layer->GetRecordingSourceForTesting().recorded_bounds());
  } else {
    EXPECT_EQ(gfx::Size(2000, 2000), layer->bounds());
    EXPECT_EQ(gfx::Rect(0, 0, 2000, 2000),
              layer->GetRecordingSourceForTesting().recorded_bounds());
  }
}

TEST_P(CompositingSimTest, NestedBoxReflectCrash) {
  InitializeWithHTML(R"HTML(
    <!DOCTYPE html>
    <div style="-webkit-box-reflect: right">
      <div style="-webkit-box-reflect: right">
        <div style="position: absolute">X</div>
      </div>
    </div>
  )HTML");
  Compositor().BeginFrame();
  // Passes if no crash.
}

TEST_P(CompositingSimTest, ScrollbarLayerWithDecompositedTransform) {
  if (!RuntimeEnabledFeatures::RasterInducingScrollEnabled()) {
    GTEST_SKIP();
  }
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();
  MainFrame()
      .GetFrame()
      ->GetSettings()
      ->SetPreferCompositingToLCDTextForTesting(false);
  InitializeWithHTML(R"HTML(
    <!DOCTYPE html>
    <div style="position: absolute; top: 100px; left: 200px;
                width: 100px; height: 100px; overflow: auto">
      <div style="height: 2000px"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* scrollbar_layer = CcLayersByName(RootCcLayer(), "VerticalScrollbar")[0];
  EXPECT_EQ(gfx::Vector2dF(285, 100),
            scrollbar_layer->offset_to_transform_parent());
  EXPECT_FALSE(scrollbar_layer->subtree_property_changed());

  paint_artifact_compositor()->SetNeedsUpdate();
  UpdateAllLifecyclePhases();
  EXPECT_EQ(gfx::Vector2dF(285, 100),
            scrollbar_layer->offset_to_transform_parent());
  EXPECT_FALSE(scrollbar_layer->subtree_property_changed());
}

}  // namespace blink
