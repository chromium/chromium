// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class AnchorPositionScrollDataTest : public RenderingTest {
 public:
  AnchorPositionScrollDataTest() = default;

  void SimulateFrame() {
    // Advance time by 100 ms.
    auto new_time = GetAnimationClock().CurrentTime() + base::Milliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }

  bool AnimationScheduled() const {
    return GetChromeClient().AnimationScheduled();
  }
  void UnsetAnimationScheduled() {
    GetChromeClient().UnsetAnimationScheduled();
  }

  void ScrollTo(const char* id, int offset) {
    auto* scroller = To<LayoutBoxModelObject>(GetLayoutObjectByElementId(id));
    DCHECK(scroller) << id;
    DCHECK(scroller->GetScrollableArea()) << id;
    scroller->GetScrollableArea()->SetScrollOffset(
        ScrollOffset(0, offset), mojom::blink::ScrollType::kProgrammatic);
  }
};

TEST_F(AnchorPositionScrollDataTest, HasDataAndScrollAdjustment) {
  SetBodyInnerHTML(R"HTML(
    <div style="position: relative">
      <div style="overflow: scroll; height: 20px;">
        Lorem ipsum
        <span id="anchor" style="anchor-name: --a1">anchor</span>
        dolor sit amet
        <div style="height: 100px"></div>
      </div>
      <div id="anchored"
           style="position: absolute; position-anchor: --a1; top: anchor(top)">
        anchored
      </div>
      <div id="no-anchor-usage"
           style="position: absolute; position-anchor: --a1;">
        anchor not used in any anchor function
      </div>
      <div id="no-anchor"
           style="position: absolute; position-anchor: --b1; top: anchor(top)">
        anchor not found
      </div>
      <div id="not-anchor-positioned"
           style="position-anchor: --a1; top: anchor(top)">
        not anchor positioned
      </div>
    </div>
  )HTML");

  const Element* anchored = GetElementById("anchored");
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData()->NeedsScrollAdjustment());

  const Element* no_anchor_usage = GetElementById("no-anchor-usage");
  EXPECT_TRUE(no_anchor_usage->GetAnchorPositionScrollData());
  EXPECT_FALSE(
      no_anchor_usage->GetAnchorPositionScrollData()->NeedsScrollAdjustment());

  const Element* no_anchor = GetElementById("no-anchor");
  EXPECT_TRUE(no_anchor->GetAnchorPositionScrollData());
  EXPECT_FALSE(
      no_anchor->GetAnchorPositionScrollData()->NeedsScrollAdjustment());

  const Element* not_anchor_positioned =
      GetElementById("not-anchor-positioned");
  EXPECT_FALSE(not_anchor_positioned->GetAnchorPositionScrollData());
}

// Tests that AnchorPositionScrollData should be properly detached if an element
// changes from anchor-positioned to no longer anchor-positioned
TEST_F(AnchorPositionScrollDataTest, Detach) {
  SetBodyInnerHTML(R"HTML(
    <style>.anchored { position: absolute; position-anchor: --a1; }</style>
    <div style="position: relative>
      <div style="overflow: scroll; height: 20px;">
        Lorem ipsum
        <span id="anchor" style="anchor-name: --a1">anchor</span>
        dolor sit amet
        <div style="height: 100px"></div>
      </div>

      <div class="anchored" id="remove">Will be removed</div>
      <div class="anchored" id="display-none">Will be display: none</div>
      <div class="anchored" id="in-flow">Will be in flow</div>
    </div>
  )HTML");

  Element* remove = GetElementById("remove");
  Element* display_none = GetElementById("display-none");
  Element* in_flow = GetElementById("in-flow");

  EXPECT_TRUE(remove->GetAnchorPositionScrollData());
  EXPECT_TRUE(display_none->GetAnchorPositionScrollData());
  EXPECT_TRUE(in_flow->GetAnchorPositionScrollData());

  remove->remove();
  display_none->setAttribute(html_names::kStyleAttr,
                             AtomicString("display: none"));
  in_flow->setAttribute(html_names::kStyleAttr,
                        AtomicString("position: static"));

  UnsetAnimationScheduled();

  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  // AnchorPositionScrollData should be detached when element no longer needs
  // it.
  EXPECT_FALSE(remove->GetAnchorPositionScrollData());
  EXPECT_FALSE(display_none->GetAnchorPositionScrollData());
  EXPECT_FALSE(in_flow->GetAnchorPositionScrollData());

  // We shouldn't need a new frame for detached AnchorPositionScrollData.
  EXPECT_FALSE(AnimationScheduled());
}

// Verifies that a new frame is scheduled if a style update changes the size of
// a scroller, which in turn changes the scroll offset.
TEST_F(AnchorPositionScrollDataTest, ScrollerSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; height: 100px; }
      #scroller.changed { height: 200px; }
      #spacer { height: 400px; }
      #anchor { anchor-name: --a; }
      #anchored {
        position: absolute;
        top: anchor(--a top);
        position-anchor: --a;
      }
    </style>
    <div style="position: relative">
      <div id="scroller">
        <div id="anchor"></div>
        <div id="spacer"></div>
      </div>
      <div id="anchored"></div>
    </div>
  )HTML");

  ScrollTo("scroller", 300);

  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  Element* anchored = GetElementById("anchored");
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_EQ(gfx::Vector2dF(0, 300),
            anchored->GetAnchorPositionScrollData()->AccumulatedAdjustment());

  GetElementById("scroller")->classList().Add(AtomicString("changed"));

  // This shouldn't update AnchorPositionScrollData, because style isn't updated
  // yet.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_EQ(gfx::Vector2dF(0, 300),
            anchored->GetAnchorPositionScrollData()->AccumulatedAdjustment());

  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();

  // Now scroller is resized to 300px height, and the scroll offset is reduced
  // to 200 as the result. This makes the AnchorPositionScrollData stale and
  // triggers a frame update.
  EXPECT_TRUE(AnimationScheduled());

  // Snapshot is updated in the next frame.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_EQ(gfx::Vector2dF(0, 200),
            anchored->GetAnchorPositionScrollData()->AccumulatedAdjustment());

  // Should not schedule another frame after all updates are done.
  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(AnimationScheduled());
}

// Verifies that a new frame is scheduled if a style update changes the size of
// the content of a scroller, which in turn changes the scroll offset.
TEST_F(AnchorPositionScrollDataTest, ScrollContentSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; height: 100px; }
      #spacer { height: 400px; }
      #spacer.changed { height: 300px; }
      #anchor { anchor-name: --a; }
      #anchored {
        position: absolute;
        top: anchor(--a top);
        position-anchor: --a;
      }
    </style>
    <div style="position: relative">
      <div id="scroller">
        <div id="anchor"></div>
        <div id="spacer"></div>
      </div>
      <div id="anchored"></div>
    </div>
  )HTML");

  ScrollTo("scroller", 300);

  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  Element* anchored = GetElementById("anchored");
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_EQ(gfx::Vector2dF(0, 300),
            anchored->GetAnchorPositionScrollData()->AccumulatedAdjustment());

  GetElementById("spacer")->classList().Add(AtomicString("changed"));

  // This shouldn't update AnchorPositionScrollData, because style isn't updated
  // yet.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_EQ(gfx::Vector2dF(0, 300),
            anchored->GetAnchorPositionScrollData()->AccumulatedAdjustment());

  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();

  // Now scroll content is resized to 300px height, and the scroll offset is
  // reduced to 200 as the result. This makes the AnchorPositionScrollData stale
  // and triggers a frame update.
  EXPECT_TRUE(AnimationScheduled());

  // Snapshot is updated in the next frame.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorPositionScrollData());
  EXPECT_EQ(gfx::Vector2dF(0, 200),
            anchored->GetAnchorPositionScrollData()->AccumulatedAdjustment());

  // Should not schedule another frame after all updates are done.
  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(AnimationScheduled());
}

}  // namespace blink
