// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/client_layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class TrackedElementRectTest : public SimTest {
 public:
  TrackedElementRectTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    SetPreferCompositingToLCDText(true);
    ResizeView(gfx::Size(800, 600));
  }

  cc::LayerTreeHostImpl* GetLayerTreeHostImpl() {
    return static_cast<cc::SingleThreadProxy*>(
               GetWebFrameWidget().LayerTreeHostForTesting()->proxy())
        ->LayerTreeHostImplForTesting();
  }

  gfx::Rect TrackElementAndGetVisibleBounds(Element* element) {
    const base::Token element_id = base::Token(0x1, 0x2);
    const viz::TrackedElementFeature feature =
        static_cast<viz::TrackedElementFeature>(0);

    element->SetTrackedElementSubRect(
        feature, TrackedElementSubRect(
                     TrackedElementId(element_id),
                     /*should_add_to_compositor_frame_metadata=*/false,
                     /*should_exclude_fixed_and_sticky_occlusions=*/true));

    Compositor().BeginFrame();

    cc::LayerTreeHostImpl* host_impl = GetLayerTreeHostImpl();
    EXPECT_TRUE(host_impl);
    if (!host_impl) {
      return gfx::Rect();
    }

    viz::TrackedElementRects rects = host_impl->CollectTrackedElementRects(
        /*is_for_compositor_frame_metadata=*/false,
        /*need_occlusion=*/true);

    EXPECT_EQ(1u, rects.size());
    EXPECT_TRUE(rects.contains(feature));
    if (!rects.contains(feature)) {
      return gfx::Rect();
    }
    const auto& feature_rects = rects.at(feature);
    EXPECT_EQ(1u, feature_rects.size());
    if (feature_rects.empty()) {
      return gfx::Rect();
    }
    EXPECT_EQ(element_id, feature_rects[0].id);

    return feature_rects[0].visible_bounds;
  }
};

TEST_F(TrackedElementRectTest, ChildOfStickyContainerDoesNotOccludeSibling) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; height: 2000px; }
      #sticky {
        position: sticky;
        top: 0;
        width: 400px;
        height: 200px;
        background: blue;
      }
      #child1 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: red;
      }
      #child2 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: green;
      }
    </style>
    <div id="sticky">
      <div id="child1"></div>
      <div id="child2"></div>
    </div>
  )HTML");

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  ASSERT_TRUE(child1);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(child1);

  // Since child1 and child2 share the sticky container's transform node,
  // child2 (even though in front of child1 and covering the exact same rect
  // (10,10,100,100), should not occlude child1. Thus visible_bounds remains
  // unoccluded.
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), visible_bounds);
}

TEST_F(TrackedElementRectTest, ChildWithOwnStickyTransformDoesOccludeSibling) {
  SimRequest main_resource("https://example.com/test2.html", "text/html");
  LoadURL("https://example.com/test2.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; height: 2000px; }
      #sticky_parent {
        position: sticky;
        top: 0;
        width: 400px;
        height: 300px;
        background: blue;
      }
      #child1 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: red;
      }
      #child2 {
        position: sticky;
        top: 10px; left: 10px;
        width: 100px; height: 50px;
        background: green;
      }
    </style>
    <div id="sticky_parent">
      <div id="child1"></div>
      <div id="child2"></div>
    </div>
  )HTML");

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  ASSERT_TRUE(child1);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(child1);

  // Since child2 has its own sticky position constraint, child1 will scroll
  // independently of it. We should consider child2 to be an occluder of child1.
  EXPECT_EQ(gfx::Rect(10, 60, 100, 50), visible_bounds);
}

TEST_F(TrackedElementRectTest, GrandchildrenOcclusionScenario) {
  SimRequest main_resource("https://example.com/test3.html", "text/html");
  LoadURL("https://example.com/test3.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; height: 2000px; }
      #sticky_parent {
        position: sticky;
        top: 0;
        width: 400px;
        height: 400px;
        background: blue;
      }
      #child1 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: red;
      }
      #grandchild1 {
        position: absolute;
        top: 0; left: 0;
        width: 30px; height: 100px;
        background: yellow;
      }
      #grandchild2 {
        position: sticky;
        top: 0; margin-left: 30px;
        width: 70px; height: 100px;
        background: green;
      }
    </style>
    <div id="sticky_parent">
      <div id="child1">
        <div id="grandchild1"></div>
        <div id="grandchild2"></div>
      </div>
    </div>
  )HTML");

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  ASSERT_TRUE(child1);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(child1);

  // grandchild1 shares the sticky parent's transform node with child1, so it is
  // NOT included as an occluder. grandchild2 has position: sticky so it creates
  // its own transform node, and IS included as an occluder.
  EXPECT_EQ(gfx::Rect(10, 10, 30, 100), visible_bounds);
}

TEST_F(TrackedElementRectTest, ChildWithWillChangeTransformScenario) {
  SimRequest main_resource("https://example.com/test4.html", "text/html");
  LoadURL("https://example.com/test4.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; height: 2000px; }
      #sticky_parent {
        position: sticky;
        top: 0;
        width: 400px;
        height: 300px;
        background: blue;
      }
      #child1 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: red;
      }
      #child2 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: green;
        will-change: transform;
      }
    </style>
    <div id="sticky_parent">
      <div id="child1"></div>
      <div id="child2"></div>
    </div>
  )HTML");

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  ASSERT_TRUE(child1);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(child1);

  // child2 has will-change: transform and will get its own transform node, but
  // is not sticky/fixed itself. It shares the same sticky parent as child1. It
  // It should NOT be considered an occluder for child1.
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), visible_bounds);
}

TEST_F(TrackedElementRectTest, CommonTransformAncestorScenario) {
  SimRequest main_resource("https://example.com/test6.html", "text/html");
  LoadURL("https://example.com/test6.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; height: 2000px; }
      #sticky_parent {
        position: sticky;
        top: 0;
        width: 400px;
        height: 400px;
        background: blue;
      }
      #subtree_a {
        position: absolute;
        top: 0; left: 0;
        width: 200px; height: 200px;
        will-change: transform;
      }
      #subtree_b {
        position: absolute;
        top: 0; left: 0;
        width: 200px; height: 200px;
        will-change: transform;
        z-index: 2;
      }
      #child1 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: red;
      }
      #child2 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: green;
      }
    </style>
    <div id="sticky_parent">
      <div id="subtree_a">
        <div id="child1"></div>
      </div>
      <div id="subtree_b">
        <div id="child2"></div>
      </div>
    </div>
  )HTML");

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  ASSERT_TRUE(child1);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(child1);

  // child1 (in subtree_a) and child2 (in subtree_b) have independent transform
  // nodes, but share sticky_parent as a common ancestor. As a result child2
  // should not be considered an occluder of child1.
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), visible_bounds);
}

TEST_F(TrackedElementRectTest, FixedOccluderDoesOcclude) {
  SimRequest main_resource("https://example.com/test7.html", "text/html");
  LoadURL("https://example.com/test7.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; }
      #fixed_occluder {
        position: fixed;
        top: 10px; left: 10px;
        width: 100px; height: 50px;
        background: green;
        z-index: 2;
      }
      #container {
        position: relative;
        width: 200px; height: 200px;
        margin-top: 10px; margin-left: 10px;
        background: blue;
      }
      #tracked_child {
        position: absolute;
        top: 0; left: 0;
        width: 100px; height: 100px;
        background: red;
      }
    </style>
    <div id="fixed_occluder"></div>
    <div id="container">
      <div id="tracked_child"></div>
    </div>
  )HTML");

  Element* tracked_child =
      GetDocument().getElementById(AtomicString("tracked_child"));
  ASSERT_TRUE(tracked_child);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(tracked_child);

  EXPECT_EQ(gfx::Rect(10, 60, 100, 50), visible_bounds);
}

TEST_F(TrackedElementRectTest, CommonFixedAncestorScenario) {
  SimRequest main_resource("https://example.com/test8.html", "text/html");
  LoadURL("https://example.com/test8.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; height: 2000px; }
      #fixed_parent {
        position: fixed;
        top: 0; left: 0;
        width: 400px;
        height: 400px;
        background: blue;
      }
      #subtree_a {
        position: absolute;
        top: 0; left: 0;
        width: 200px; height: 200px;
        will-change: transform;
      }
      #subtree_b {
        position: absolute;
        top: 0; left: 0;
        width: 200px; height: 200px;
        will-change: transform;
        z-index: 2;
      }
      #child1 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 100px;
        background: red;
      }
      #child2 {
        position: absolute;
        top: 10px; left: 10px;
        width: 100px; height: 50px;
        background: green;
      }
    </style>
    <div id="fixed_parent">
      <div id="subtree_a">
        <div id="child1"></div>
      </div>
      <div id="subtree_b">
        <div id="child2"></div>
      </div>
    </div>
  )HTML");

  Element* child1 = GetDocument().getElementById(AtomicString("child1"));
  ASSERT_TRUE(child1);

  gfx::Rect visible_bounds = TrackElementAndGetVisibleBounds(child1);

  // child1 (in subtree_a) and child2 (in subtree_b) have independent transform
  // nodes, but share fixed_parent as a common ancestor. As a result child2
  // should not be considered an occluder of child1.
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), visible_bounds);
}

}  // namespace blink
