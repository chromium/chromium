// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

using HitNodeCb =
    base::MockRepeatingCallback<ListBasedHitTestBehavior(const Node& node)>;
using testing::_;
using testing::Return;

class HitTestingTest : public RenderingTest {
 protected:
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

  static HitTestResult HitTestForOcclusion(const Element& target) {
    const LayoutObject* object = target.GetLayoutObject();
    return object->HitTestForOcclusion(VisualRectInDocument(*object));
  }
};

// Helper class used by |HitNodeCb| to allow callers to stop hit testing at a
// given node.
class HitNodeCallbackStopper : public GarbageCollected<HitNodeCallbackStopper> {
 public:
  explicit HitNodeCallbackStopper(Node* stop_node) : stop_node_(stop_node) {}
  HitNodeCallbackStopper(const HitNodeCallbackStopper&) = delete;
  HitNodeCallbackStopper& operator=(const HitNodeCallbackStopper&) = delete;
  ~HitNodeCallbackStopper() = default;

  ListBasedHitTestBehavior StopAtNode(const Node& node) {
    did_stop_hit_testing_ = false;
    if (node == stop_node_) {
      did_stop_hit_testing_ = true;
      return ListBasedHitTestBehavior::kStopHitTesting;
    }
    return ListBasedHitTestBehavior::kContinueHitTesting;
  }

  bool DidStopHitTesting() { return did_stop_hit_testing_; }

  void Trace(Visitor* visitor) const { visitor->Trace(stop_node_); }

 private:
  Member<Node> stop_node_;
  bool did_stop_hit_testing_ = false;
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

  Element* target = GetElementById("target");
  Element* occluder = GetElementById("occluder");
  HitTestResult result = HitTestForOcclusion(*target);
  EXPECT_EQ(result.InnerNode(), target);

  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  UpdateAllLifecyclePhasesForTest();
  result = HitTestForOcclusion(*target);
  EXPECT_EQ(result.InnerNode(), occluder);
}

TEST_F(HitTestingTest, OcclusionHitTestSVGTextWithFilterCrash) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    text {
      filter: blur(10px);
    }
    </style>

    <div id="target"></div>
    <svg overflow="visible" display="block">
      <text id="occluder" y="40" font-size="50px">M</text>
    </svg>
  )HTML");

  Element* target = GetElementById("target");
  Element* occluder = GetElementById("occluder");
  HitTestResult result = HitTestForOcclusion(*target);
  // The intersection will be flagged on the text node.
  EXPECT_EQ(result.InnerNode(), occluder->firstChild());
}

TEST_F(HitTestingTest, HitTestWithCallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
    }
    </style>

    <div id=target></div>
    <div id=occluder_1></div>
    <div id=occluder_2></div>
    <div id=occluder_3></div>
  )HTML");

  Element* target = GetElementById("target");
  HitNodeCb hit_node_cb;

  // Perform hit test without stopping, and verify that the result innernode is
  // set to the target.
  EXPECT_CALL(hit_node_cb, Run(_))
      .WillRepeatedly(Return(ListBasedHitTestBehavior::kContinueHitTesting));

  LocalFrame* frame = GetDocument().GetFrame();
  DCHECK(!frame->View()->NeedsLayout());
  const PhysicalRect& hit_rect =
      VisualRectInDocument(*target->GetLayoutObject());
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kIgnorePointerEventsNone | HitTestRequest::kReadOnly |
      HitTestRequest::kIgnoreClipping |
      HitTestRequest::kIgnoreZeroOpacityObjects |
      HitTestRequest::kHitTestVisualOverflow | HitTestRequest::kListBased |
      HitTestRequest::kPenetratingList | HitTestRequest::kAvoidCache;
  HitTestLocation location(hit_rect);
  HitTestResult result = frame->GetEventHandler().HitTestResultAtLocation(
      location, hit_type, target->GetLayoutObject(), true, hit_node_cb.Get());

  EXPECT_EQ(result.InnerNode(), target);

  Element* occluder_1 = GetElementById("occluder_1");
  Element* occluder_2 = GetElementById("occluder_2");
  Element* occluder_3 = GetElementById("occluder_3");

  // Ensure that occluders intersect with the target.
  const int div_height =
      GetLayoutObjectByElementId("target")->StyleRef().Height().IntValue();
  occluder_1->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  occluder_2->SetInlineStyleProperty(
      CSSPropertyID::kMarginTop,
      String::Format("%dpx", (-div_height * 1) - 10));
  occluder_3->SetInlineStyleProperty(
      CSSPropertyID::kMarginTop,
      String::Format("%dpx", (-div_height * 2) - 10));
  UpdateAllLifecyclePhasesForTest();

  // Set up HitNodeCb helper, and the HitNodeCb expectations.
  Node* stop_node = GetElementById("occluder_2");
  HitNodeCallbackStopper* hit_node_callback_stopper =
      MakeGarbageCollected<HitNodeCallbackStopper>(stop_node);
  EXPECT_CALL(hit_node_cb, Run(_))
      .WillRepeatedly(testing::Invoke(hit_node_callback_stopper,
                                      &HitNodeCallbackStopper::StopAtNode));
  EXPECT_FALSE(hit_node_callback_stopper->DidStopHitTesting());

  // Perform hit test and verify that hit testing stops at the given node.
  result = frame->GetEventHandler().HitTestResultAtLocation(
      location, hit_type, target->GetLayoutObject(), true, hit_node_cb.Get());
  EXPECT_TRUE(result.ListBasedTestResult().Contains(stop_node));
  EXPECT_TRUE(hit_node_callback_stopper->DidStopHitTesting());
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

  Element* target = GetElementById("target");
  Element* occluder = GetElementById("occluder");

  // target and occluder don't overlap, no occlusion.
  HitTestResult result = HitTestForOcclusion(*target);
  EXPECT_EQ(result.InnerNode(), target);

  // target and occluder layout rects overlap, but the overlapping area of the
  // occluder is clipped out, so no occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-4px");
  UpdateAllLifecyclePhasesForTest();
  result = HitTestForOcclusion(*target);
  EXPECT_EQ(result.InnerNode(), target);

  // target and clipped area of occluder overlap, so there is occlusion.
  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-6px");
  UpdateAllLifecyclePhasesForTest();
  result = HitTestForOcclusion(*target);
  EXPECT_EQ(result.InnerNode(), occluder);
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
