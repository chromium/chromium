// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_scroll_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class AnchorScrollDataTest : public RenderingTest,
                             private ScopedCSSAnchorPositioningForTest {
 public:
  AnchorScrollDataTest() : ScopedCSSAnchorPositioningForTest(true) {}

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

TEST_F(AnchorScrollDataTest, HasDataAndTranslation) {
  SetBodyInnerHTML(R"HTML(
    <div style="position: relative">
      <div style="overflow: scroll; height: 20px;">
        Lorem ipsum
        <span id="anchor" style="anchor-name: --a1">anchor</span>
        dolor sit amet
        <div style="height: 100px"></div>
      </div>
      <div id="anchored" style="position: absolute; anchor-scroll: --a1">
        anchored
      </div>
      <div id="no-anchor" style="position: absolute; anchor-scroll: --b1">
        anchor not found
      </div>
      <div id="not-anchor-positioned" style="anchor-scroll: --a1">
        not anchor positioned
      </div>
    </div>
  )HTML");

  const Element* anchored = GetElementById("anchored");
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_TRUE(anchored->GetAnchorScrollData()->HasTranslation());

  const Element* no_anchor = GetElementById("no-anchor");
  EXPECT_TRUE(no_anchor->GetAnchorScrollData());
  EXPECT_FALSE(no_anchor->GetAnchorScrollData()->HasTranslation());

  const Element* not_anchor_positioned =
      GetElementById("not-anchor-positioned");
  EXPECT_FALSE(not_anchor_positioned->GetAnchorScrollData());
}

// Tests that AnchorScrollData should be properly detached if an element changes
// from using anchor-scroll to no longer using anchor-scroll
TEST_F(AnchorScrollDataTest, Detach) {
  SetBodyInnerHTML(R"HTML(
    <style>.anchored { position: absolute; anchor-scroll: --a1; }</style>
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

  EXPECT_TRUE(remove->GetAnchorScrollData());
  EXPECT_TRUE(display_none->GetAnchorScrollData());
  EXPECT_TRUE(in_flow->GetAnchorScrollData());

  remove->remove();
  display_none->setAttribute(html_names::kStyleAttr, "display: none");
  in_flow->setAttribute(html_names::kStyleAttr, "position: static");

  UnsetAnimationScheduled();

  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  // AnchorScrollData should be detached when element no longer needs it.
  EXPECT_FALSE(remove->GetAnchorScrollData());
  EXPECT_FALSE(display_none->GetAnchorScrollData());
  EXPECT_FALSE(in_flow->GetAnchorScrollData());

  // We shouldn't need a new frame for detached AnchorScrollData.
  EXPECT_FALSE(AnimationScheduled());
}

// Verifies that a new frame is scheduled if a style update changes the size of
// a scroller, which in turn changes the scroll offset.
TEST_F(AnchorScrollDataTest, ScrollerSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; height: 100px; }
      #scroller.changed { height: 200px; }
      #spacer { height: 400px; }
      #anchor { anchor-name: --a; }
      #anchored {
        position: absolute;
        top: anchor(--a top);
        anchor-scroll: --a;
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
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_EQ(ScrollOffset(0, 300),
            anchored->GetAnchorScrollData()->AccumulatedScrollOffset());

  GetElementById("scroller")->classList().Add("changed");

  // This shouldn't update AnchorScrollData, because style isn't updated yet.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_EQ(ScrollOffset(0, 300),
            anchored->GetAnchorScrollData()->AccumulatedScrollOffset());

  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();

  // Now scroller is resized to 300px height, and the scroll offset is reduced
  // to 200 as the result. This makes the AnchorScrollData stale and triggers a
  // frame update.
  EXPECT_TRUE(AnimationScheduled());

  // Snapshot is updated in the next frame.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_EQ(ScrollOffset(0, 200),
            anchored->GetAnchorScrollData()->AccumulatedScrollOffset());

  // Should not schedule another frame after all updates are done.
  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(AnimationScheduled());
}

// Verifies that a new frame is scheduled if a style update changes the size of
// the content of a scroller, which in turn changes the scroll offset.
TEST_F(AnchorScrollDataTest, ScrollContentSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; height: 100px; }
      #spacer { height: 400px; }
      #spacer.changed { height: 300px; }
      #anchor { anchor-name: --a; }
      #anchored {
        position: absolute;
        top: anchor(--a top);
        anchor-scroll: --a;
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
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_EQ(ScrollOffset(0, 300),
            anchored->GetAnchorScrollData()->AccumulatedScrollOffset());

  GetElementById("spacer")->classList().Add("changed");

  // This shouldn't update AnchorScrollData, because style isn't updated yet.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_EQ(ScrollOffset(0, 300),
            anchored->GetAnchorScrollData()->AccumulatedScrollOffset());

  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();

  // Now scroll content is resized to 300px height, and the scroll offset is
  // reduced to 200 as the result. This makes the AnchorScrollData stale and
  // triggers a frame update.
  EXPECT_TRUE(AnimationScheduled());

  // Snapshot is updated in the next frame.
  SimulateFrame();
  EXPECT_TRUE(anchored->GetAnchorScrollData());
  EXPECT_EQ(ScrollOffset(0, 200),
            anchored->GetAnchorScrollData()->AccumulatedScrollOffset());

  // Should not schedule another frame after all updates are done.
  UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(AnimationScheduled());
}

}  // namespace blink
