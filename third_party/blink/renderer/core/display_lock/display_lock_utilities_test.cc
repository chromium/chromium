// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/intersection_observer_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class DisplayLockUtilitiesTest : public RenderingTest {
 public:
  DisplayLockUtilitiesTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void LockElement(Element& element, bool activatable) {
    if (activatable) {
      element.setAttribute(html_names::kHiddenAttr,
                           AtomicString("until-found"));
    } else {
      element.setAttribute(html_names::kStyleAttr,
                           AtomicString("content-visibility: hidden"));
    }
    UpdateAllLifecyclePhasesForTest();
  }

  void CommitElement(Element& element) {
    element.setAttribute(html_names::kStyleAttr, g_empty_atom);
    UpdateAllLifecyclePhasesForTest();
  }
};

TEST_F(DisplayLockUtilitiesTest, ShouldIgnoreHiddenUntilFoundChildren) {
  SetBodyInnerHTML(R"HTML(
    <div hidden=until-found>
      <div id=target></div>
    </div>
  )HTML");

  Node* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_TRUE(DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
      *target, DisplayLockActivationReason::kAccessibility));
}

TEST_F(DisplayLockUtilitiesTest, DISABLED_ActivatableLockedInclusiveAncestors) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id='outer'>
      <div id='innerA'>
        <div id='innermost'>text_node</div>
      </div>
      <div id='innerB'></div>
    </div>
  )HTML");

  Element& outer = *GetDocument().getElementById(AtomicString("outer"));
  Element& inner_a = *GetDocument().getElementById(AtomicString("innerA"));
  Element& inner_b = *GetDocument().getElementById(AtomicString("innerB"));
  Element& innermost = *GetDocument().getElementById(AtomicString("innermost"));
  ShadowRoot& shadow_root =
      inner_b.AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div id='shadowDiv'>shadow!</div>");
  Element& shadow_div = *shadow_root.getElementById(AtomicString("shadowDiv"));

  LockElement(outer, true);
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);
  // Querying from every element gives |outer|.
  HeapVector<Member<Element>> result_for_outer =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          outer, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_outer.size(), 1u);
  EXPECT_EQ(result_for_outer.at(0), outer);

  HeapVector<Member<Element>> result_for_inner_a =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          inner_a, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_inner_a.size(), 1u);
  EXPECT_EQ(result_for_inner_a.at(0), outer);

  HeapVector<Member<Element>> result_for_innermost =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          innermost, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_innermost.size(), 1u);
  EXPECT_EQ(result_for_innermost.at(0), outer);

  HeapVector<Member<Element>> result_for_inner_b =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          inner_b, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_inner_b.size(), 1u);
  EXPECT_EQ(result_for_inner_b.at(0), outer);

  HeapVector<Member<Element>> result_for_shadow_div =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          shadow_div, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_shadow_div.size(), 1u);
  EXPECT_EQ(result_for_shadow_div.at(0), outer);

  // Lock innermost with activatable flag.
  LockElement(innermost, true);
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  result_for_outer = DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
      outer, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_outer.size(), 1u);
  EXPECT_EQ(result_for_outer.at(0), outer);

  result_for_inner_a =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          inner_a, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_inner_a.size(), 1u);
  EXPECT_EQ(result_for_inner_a.at(0), outer);

  result_for_innermost =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          innermost, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_innermost.size(), 2u);
  EXPECT_EQ(result_for_innermost.at(0), innermost);
  EXPECT_EQ(result_for_innermost.at(1), outer);

  result_for_inner_b =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          inner_b, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_inner_b.size(), 1u);
  EXPECT_EQ(result_for_inner_b.at(0), outer);

  result_for_shadow_div =
      DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
          shadow_div, DisplayLockActivationReason::kAny);
  EXPECT_EQ(result_for_shadow_div.size(), 1u);
  EXPECT_EQ(result_for_shadow_div.at(0), outer);

  // Unlock everything.
  CommitElement(innermost);
  CommitElement(outer);
  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument()
                .GetDisplayLockDocumentState()
                .DisplayLockBlockingAllActivationCount(),
            0);

  EXPECT_EQ(DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
                outer, DisplayLockActivationReason::kAny)
                .size(),
            0u);
  EXPECT_EQ(DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
                inner_a, DisplayLockActivationReason::kAny)
                .size(),
            0u);
  EXPECT_EQ(DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
                innermost, DisplayLockActivationReason::kAny)
                .size(),
            0u);
  EXPECT_EQ(DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
                inner_b, DisplayLockActivationReason::kAny)
                .size(),
            0u);
  EXPECT_EQ(DisplayLockUtilities::ActivatableLockedInclusiveAncestors(
                shadow_div, DisplayLockActivationReason::kAny)
                .size(),
            0u);
}

TEST_F(DisplayLockUtilitiesTest, LockedSubtreeCrossingFrames) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="grandparent">
      <iframe id="frame" src="http://test.com"></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      div {
        contain: style layout;
      }
    </style>
    <div id="parent">
      <div id="child"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* grandparent =
      GetDocument().getElementById(AtomicString("grandparent"));
  Element* parent = ChildDocument().getElementById(AtomicString("parent"));
  Element* child = ChildDocument().getElementById(AtomicString("child"));

  ASSERT_TRUE(grandparent);
  ASSERT_TRUE(parent);
  ASSERT_TRUE(child);

  // Lock parent.
  LockElement(*parent, false);

  EXPECT_EQ(
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(), 0);
  EXPECT_EQ(
      ChildDocument().GetDisplayLockDocumentState().LockedDisplayLockCount(),
      1);

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_FALSE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));

  // Lock grandparent.
  LockElement(*grandparent, false);

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));

  // Unlock parent.
  CommitElement(*parent);

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_TRUE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));

  // Unlock grandparent.
  CommitElement(*grandparent);

  // CommitElement(*grandparent) ran a lifecycle update, but during that update
  // the iframe document was still throttled, so did not update style. The
  // iframe document should have become unthrottled at the end of that update,
  // so it takes an additional lifecycle update to resolve style in the iframe.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(
      DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*grandparent));
  EXPECT_FALSE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*parent));
  EXPECT_FALSE(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*child));
}

TEST_F(DisplayLockUtilitiesTest, InteractionWithIntersectionObserver) {
  SetHtmlInnerHTML(R"HTML(
    <div id="container"><iframe id="frame"></iframe></div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="target"></target>
  )HTML");

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* target = ChildDocument().getElementById(AtomicString("target"));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  LockElement(*container, false);
  EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRenderingForTest());

  target->setInnerHTML("Hello, world!");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_TRUE(ChildDocument().Lifecycle().GetState() ==
              DocumentLifecycle::kVisualUpdatePending);

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(ChildDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kDisplayLockIntersectionObserver);
  observer->observe(target);
  UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  EXPECT_TRUE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_EQ(ChildDocument().Lifecycle().GetState(),
            DocumentLifecycle::kVisualUpdatePending);
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->GetGeometry().IsIntersecting());
  EXPECT_EQ(observer_delegate->LastEntry()->GetGeometry().TargetRect(),
            gfx::RectF());
  EXPECT_EQ(observer_delegate->LastEntry()->GetGeometry().RootRect(),
            gfx::RectF());

  CommitElement(*container);
  test::RunPendingTasks();
  EXPECT_FALSE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_EQ(ChildDocument().Lifecycle().GetState(),
            DocumentLifecycle::kVisualUpdatePending);
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  EXPECT_FALSE(ChildDocument().View()->ShouldThrottleRenderingForTest());
  EXPECT_FALSE(ChildDocument().View()->NeedsLayout());
  EXPECT_EQ(ChildDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPaintClean);
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->GetGeometry().IsIntersecting());
  EXPECT_NE(observer_delegate->LastEntry()->GetGeometry().TargetRect(),
            gfx::RectF());
  EXPECT_EQ(observer_delegate->LastEntry()->GetGeometry().IntersectionRect(),
            observer_delegate->LastEntry()->GetGeometry().TargetRect());
  EXPECT_NE(observer_delegate->LastEntry()->GetGeometry().RootRect(),
            gfx::RectF());
}

TEST_F(DisplayLockUtilitiesTest, ContainerQueryCrash) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #container {
        content-visibility: hidden;
        container-type: size;
      }
    </style>
    <div id="container"><div id="child"></div></div>
  )HTML");

  auto* child = DynamicTo<HTMLElement>(
      GetDocument().getElementById(AtomicString("child")));
  ASSERT_TRUE(child);

  // Should not fail DCHECKs or crash.
  child->offsetTopForBinding();
}

}  // namespace blink
