// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class DeferredShapingTest : public RenderingTest {
 protected:
  bool IsDefer(const char* id_value) const {
    const auto* layout_object = GetLayoutObjectByElementId(id_value);
    return layout_object && layout_object->IsShapingDeferred();
  }

  bool IsLocked(const char* id_value) const {
    const auto* context = GetElementById(id_value)->GetDisplayLockContext();
    return context && context->IsLocked();
  }

  void ScrollAndWaitForIntersectionCheck(double new_scroll_top) {
    GetDocument().scrollingElement()->setScrollTop(new_scroll_top);
    UpdateAllLifecyclePhasesForTest();
    UpdateAllLifecyclePhasesForTest();
  }

 private:
  ScopedLayoutNGForTest enablee_layout_ng_{true};
  ScopedDeferredShapingForTest enable_deferred_shapign_{true};
};

TEST_F(DeferredShapingTest, Basic) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  ScrollAndWaitForIntersectionCheck(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_FALSE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, ViewportMargin) {
  // The box starting around y=1200 (viewport height * 2) is not deferred due to
  // a viewport margin setting for IntersectionObserver.
  SetBodyInnerHTML(R"HTML(
<div style="height:1200px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_FALSE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, AlreadyAuto) {
  // If the element has content-visibility:auto, it never be deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target" style="content-visibility:auto">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  ScrollAndWaitForIntersectionCheck(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_FALSE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, AlreadyHidden) {
  // If the element has content-visibility:hidden, it never be deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target" style="content-visibility:hidden">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  ScrollAndWaitForIntersectionCheck(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, DynamicAuto) {
  // If a deferred element gets content-visibility:auto, it stops deferring.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  GetElementById("target")->setAttribute("style", "content-visibility:auto");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  ScrollAndWaitForIntersectionCheck(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_FALSE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, DynamicHidden) {
  // If a deferred element gets content-visibility:hidden, it stops deferring.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  GetElementById("target")->setAttribute("style", "content-visibility:hidden");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  GetElementById("target")->setAttribute("style", "content-visibility:visible");
  // A change of content-visibility property triggers a full layout, and the
  // target box is determined as "deferred" again.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  ScrollAndWaitForIntersectionCheck(1800);
  EXPECT_FALSE(IsDefer("target"));
  EXPECT_FALSE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, DynamicPropertyChange) {
  // If a property of a deferred element is changed, it keeps deferred.
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<div id="target">IFC</div>
)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  GetElementById("target")->setAttribute("style", "width: 10em;");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, ListMarkerCrash) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<ul>
<li id="target">IFC</li>
</ul>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  // Re-layout the target while deferred.
  GetElementById("target")->setTextContent("foobar");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crash.
}

TEST_F(DeferredShapingTest, UpdateTextInDeferred) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<p id="target">IFC</p>
</ul>)HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));

  // Force to mark FMP. It disables deferred shaping.
  PaintTiming::From(GetDocument()).MarkFirstPaint();
  PaintTiming::From(GetDocument()).MarkFirstContentfulPaint();
  auto& fmp_detector = FirstMeaningfulPaintDetector::From(GetDocument());
  fmp_detector.MarkNextPaintAsMeaningfulForTesting();
  fmp_detector.NotifyPaint();
  fmp_detector.OnNetwork2Quiet();
  EXPECT_TRUE(fmp_detector.SeenFirstMeaningfulPaint());

  // Re-layout the target while it was deferred but deferred shaping is
  // disabled. We had an inconsistent state issue that the target was
  // locked but not deferred.
  GetElementById("target")->setTextContent("foobar");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));
}

TEST_F(DeferredShapingTest, NonLayoutNGBlockFlow) {
  SetBodyInnerHTML(R"HTML(
<div style="height:1800px"></div>
<table><caption id="target">IFC</caption></table>)HTML");
  UpdateAllLifecyclePhasesForTest();
  // LayoutNGTableCaption, which is not a subclass of LayoutNGBlockFlow,
  // should support IsShapingDeferred().
  EXPECT_TRUE(IsDefer("target"));
  EXPECT_TRUE(IsLocked("target"));
}

}  // namespace blink
