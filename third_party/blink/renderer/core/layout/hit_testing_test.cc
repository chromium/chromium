// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HitTestingTest : public RenderingTest {
 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }

  PositionWithAffinity HitTest(const PhysicalOffset offset) {
    const HitTestRequest hit_request(HitTestRequest::kActive);
    const HitTestLocation hit_location(offset);
    HitTestResult hit_result(hit_request, hit_location);
    if (!GetLayoutView().HitTest(hit_location, hit_result))
      return PositionWithAffinity();
    // Simulate |PositionWithAffinityOfHitTestResult()| in
    // "selection_controller.cc"
    LayoutObject* const layout_object =
        hit_result.InnerPossiblyPseudoNode()->GetLayoutObject();
    if (!layout_object)
      return PositionWithAffinity();
    return layout_object->PositionForPoint(hit_result.LocalPoint());
  }
};

TEST_F(HitTestingTest, OcclusionHitTest) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    </style>

    <div id=target></div>
    <div id=occluder></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

TEST_F(HitTestingTest, OcclusionHitTestWithClipPath) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    #occluder {
      clip-path: url(#clip);
    }
    </style>

    <svg viewBox="0 0 100 100" width=0>
      <clipPath id="clip">
        <circle cx="50" cy="50" r="45" stroke="none" />
      </clipPath>
    </svg>

    <div id=target></div>
    <div id=occluder></div>
  )HTML");

  Element* target = GetDocument().getElementById("target");
  Element* occluder = GetDocument().getElementById("occluder");

  // target and occluder don't overlap, no occlusion.
  HitTestResult result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  // target and occluder layout rects overlap, but the overlapping area of the
  // occluder is clipped out, so no occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-4px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), target);

  // target and clipped area of occluder overlap, so there is occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-6px");
  UpdateAllLifecyclePhasesForTest();
  result = target->GetLayoutObject()->HitTestForOcclusion();
  EXPECT_EQ(result.InnerNode(), occluder);
}

// crbug.com/1153037
TEST_F(HitTestingTest, LegacyInputElementInFragmentTraversal) {
  ScopedLayoutNGFragmentTraversalForTest fragment_traversal_feature(true);
  ScopedEditingNGForTest editing_feature(false);

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin:100px; }
    </style>
    <input id="target">
  )HTML");

  const HitTestRequest hit_request(HitTestRequest::kActive);
  const HitTestLocation hit_location(PhysicalOffset(110, 110));
  HitTestResult hit_result(hit_request, hit_location);
  ASSERT_TRUE(GetLayoutView().HitTest(hit_location, hit_result));
  ASSERT_TRUE(hit_result.InnerNode());
  const auto* layout_object = hit_result.InnerNode()->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  // In this test we'll use the legacy layout engine for form controls, so the
  // INPUT element will generate a LayoutTextControl with an inner editable
  // LayoutBlockFlow child. We'll hit-test by traversing the fragment tree
  // (rather than the LayoutObject tree). We should hit the inner
  // LayoutBlockFlow. Since it is a legacy object and it is also laid out by a
  // legacy parent, it will not generate any NG fragments. Check that we hit the
  // right node, and that the hit-testing code hasn't incorrectly set an NG
  // fragment from an ancestor.

  ASSERT_EQ(layout_object->Parent()->GetNode(),
            GetDocument().getElementById("target"));
}

TEST_F(HitTestingTest, ScrolledInline) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body {
      margin: 0;
      font-size: 50px;
      line-height: 1;
    }
    #scroller {
      width: 400px;
      height: 5em;
      overflow: scroll;
      white-space: pre;
    }
    </style>
    <div id="scroller">line1
line2
line3
line4
line5
line6
line7
line8
line9</div>
  )HTML");

  // Scroll #scroller by 2 lines. "line3" should be at the top.
  Element* scroller = GetElementById("scroller");
  scroller->setScrollTop(100);

  const auto& text = *To<Text>(GetElementById("scroller")->firstChild());

  // Expect to hit test position 12 (beginning of line3).
  EXPECT_EQ(PositionWithAffinity(Position(text, 12)),
            HitTest(PhysicalOffset(5, 5)));
}

}  // namespace blink
