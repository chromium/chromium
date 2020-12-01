// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class DisplayLockUtilitiesTest
    : public RenderingTest,
      private ScopedCSSContentVisibilityHiddenMatchableForTest {
 public:
  DisplayLockUtilitiesTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedCSSContentVisibilityHiddenMatchableForTest(true) {}

  void LockElement(Element& element, bool activatable) {
    StringBuilder value;
    value.Append("content-visibility: hidden");
    if (activatable)
      value.Append("-matchable");
    element.setAttribute(html_names::kStyleAttr, value.ToAtomicString());
    UpdateAllLifecyclePhasesForTest();
  }

  void CommitElement(Element& element) {
    element.setAttribute(html_names::kStyleAttr, "");
    UpdateAllLifecyclePhasesForTest();
  }
};

TEST_F(DisplayLockUtilitiesTest, ShouldIgnoreHiddenMatchableChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .hidden { content-visibility: hidden-matchable }
    </style>
    <div class=hidden>
      <div id=target></div>
    </div>
  )HTML");

  Node* target = GetDocument().getElementById("target");
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

  Element& outer = *GetDocument().getElementById("outer");
  Element& inner_a = *GetDocument().getElementById("innerA");
  Element& inner_b = *GetDocument().getElementById("innerB");
  Element& innermost = *GetDocument().getElementById("innermost");
  ShadowRoot& shadow_root =
      inner_b.AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML("<div id='shadowDiv'>shadow!</div>");
  Element& shadow_div = *shadow_root.getElementById("shadowDiv");

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

  Element* grandparent = GetDocument().getElementById("grandparent");
  Element* parent = ChildDocument().getElementById("parent");
  Element* child = ChildDocument().getElementById("child");

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
}  // namespace blink
