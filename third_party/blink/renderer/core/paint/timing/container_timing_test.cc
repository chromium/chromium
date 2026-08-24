// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_paint_attribution_tracker.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_test_utils.h"
#include "third_party/blink/renderer/core/paint/timing/text_element_timing.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerTimingTest : public PageTestBase {
 protected:
  ContainerTiming& GetContainerTiming() {
    return ContainerTiming::From(*GetDocument().domWindow());
  }

  void SimulatePaint(Element* element, const gfx::RectF& rect) {
    SimulateContainerTimingPaint(GetContainerTiming(), element, rect);
  }

  void TriggerPopulateEntries() {
    auto* performance =
        DOMWindowPerformance::performance(*GetDocument().domWindow());
    performance->PopulateContainerTimingEntries();
  }

  wtf_size_t GetContainerEntryCount() {
    auto* performance =
        DOMWindowPerformance::performance(*GetDocument().domWindow());
    return performance->getBufferedEntriesByType(AtomicString("container"))
        .size();
  }

 private:
  ScopedContainerTimingForTest scoped_feature_{true};
};

TEST_F(ContainerTimingTest, Propagation_BasicHierarchy) {
  // The content div contains text so it is tracked as a text aggregation node
  // by the pre-paint attribution tracker (populated by SetBodyContent via
  // UpdateAllLifecyclePhasesForTest).
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="middle">
        <div id="inner" containertiming="inner">
          <div id="content">x</div>
        </div>
      </div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  // Paint in the content node inside both container timing roots.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();

  // So we get two entries, one in each root.
  EXPECT_EQ(2u, GetContainerEntryCount());
}

TEST_F(ContainerTimingTest, Propagation_InsertContainerRootInMiddle) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="middle">
        <div id="inner" containertiming="inner">
          <div id="content">x</div>
        </div>
      </div>
    </div>
  )HTML");

  auto* middle = GetDocument().getElementById(AtomicString("middle"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(middle);
  ASSERT_TRUE(content);

  // Initial paint in the element inside both container timing roots.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_after_first_paint = GetContainerEntryCount();
  // So we get a performance entry for each root.
  EXPECT_EQ(2u, count_after_first_paint);

  // Converted an element in the middle to be a new container timing root. It
  // should not break traversal.
  middle->setAttribute(html_names::kContainertimingAttr,
                       AtomicString("middle"));
  // Run the lifecycle to update the pre-paint tracker with the new root.
  UpdateAllLifecyclePhasesForTest();

  // Second paint in a different area - should now propagate through middle
  SimulatePaint(content, gfx::RectF(100, 100, 100, 100));
  TriggerPopulateEntries();

  // As the entries are still not removed, we accumulate the ones that were
  // already emitted. Now we should get three new performance entries, two for
  // the original roots and one for the newly added.
  auto count_after_second_paint = GetContainerEntryCount();
  EXPECT_EQ(count_after_first_paint + 3u, count_after_second_paint);
}

TEST_F(ContainerTimingTest, Propagation_AddIgnoreStopsPropagation) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner">
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* inner = GetDocument().getElementById(AtomicString("inner"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(inner);
  ASSERT_TRUE(content);

  // As usual, a paint is propagated and performance entries happen for the two
  // container timing roots.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_after_first_paint = GetContainerEntryCount();
  EXPECT_EQ(2u, count_after_first_paint);

  // An element with containertimingignore blocks propagation upwards.
  inner->setAttribute(html_names::kContainertimingignoreAttr, g_empty_atom);
  // Run the lifecycle so the pre-paint tracker picks up the ignore flag.
  UpdateAllLifecyclePhasesForTest();

  // With the second paint, the inner container timing root gets its new
  // performance entry. But the outer one does not because ignore blocks
  // propagation.
  SimulatePaint(content, gfx::RectF(100, 100, 100, 100));
  TriggerPopulateEntries();
  auto count_after_second_paint = GetContainerEntryCount();
  EXPECT_EQ(count_after_first_paint + 1u, count_after_second_paint);
}

TEST_F(ContainerTimingTest, Propagation_RemoveIgnoreRestoresPropagation) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner" containertimingignore>
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* inner = GetDocument().getElementById(AtomicString("inner"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(inner);
  ASSERT_TRUE(content);

  // First paint only propagates to the inner root. Outer root does not get a
  // performance entry. So only one entry.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_after_first_paint = GetContainerEntryCount();
  EXPECT_EQ(1u, count_after_first_paint);

  inner->removeAttribute(html_names::kContainertimingignoreAttr);
  // Run the lifecycle so the pre-paint tracker restores propagation.
  UpdateAllLifecyclePhasesForTest();

  // On the second paint, without ignore in the middle, we get new performance
  // entries from the inner and outer roots.
  SimulatePaint(content, gfx::RectF(100, 100, 100, 100));
  TriggerPopulateEntries();
  auto count_after_second_paint = GetContainerEntryCount();
  EXPECT_EQ(count_after_first_paint + 2u, count_after_second_paint);
}

TEST_F(ContainerTimingTest, Propagation_DeeplyNestedHierarchy) {
  SetBodyContent(R"HTML(
    <div id="a" containertiming="a">
      <div id="b">
        <div id="c" containertiming="c">
          <div id="d">
            <div id="e" containertiming="e">
              <div id="content">x</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  // Test a deeper hierarchy. We should get performance entries in all three
  // container timing roots.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(3u, GetContainerEntryCount());
}

// Regression test: after an attribute change on a middle root, re-painting
// under the innermost container must reach all ancestor roots.
TEST_F(ContainerTimingTest, Propagation_ParentCacheReEstablishment) {
  SetBodyContent(R"HTML(
    <div id="a" containertiming="a">
      <div id="b" containertiming="b">
        <div id="c" containertiming="c">
          <div id="content">x</div>
        </div>
      </div>
    </div>
  )HTML");

  auto* b = GetDocument().getElementById(AtomicString("b"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(b);
  ASSERT_TRUE(content);

  // First paint: all three records created, links C→B→A established.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_after_first_paint = GetContainerEntryCount();
  EXPECT_EQ(3u, count_after_first_paint);

  // Attribute change on B; the next lifecycle update re-attributes the changed
  // subtree, rebuilding the pre-paint tracker's parent links.
  b->setAttribute(html_names::kContainertimingAttr, AtomicString("b2"));
  // Run the lifecycle to re-attribute the changed subtree.
  UpdateAllLifecyclePhasesForTest();

  // Second paint in a new area. Re-establishment must reach A, not just B.
  SimulatePaint(content, gfx::RectF(200, 200, 100, 100));
  TriggerPopulateEntries();

  // All three roots (a, b/b2, c) must receive the second paint.
  auto count_after_second_paint = GetContainerEntryCount();
  EXPECT_EQ(count_after_first_paint + 3u, count_after_second_paint);
}

TEST_F(ContainerTimingTest, Propagation_IgnoreBlocksOnMultiplePaints) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner" containertimingignore>
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  // Each paint should produce exactly one entry (inner only). Outer must
  // never receive an entry across all three paints.
  SimulatePaint(content, gfx::RectF(0, 0, 50, 50));
  TriggerPopulateEntries();
  auto count_after_paint1 = GetContainerEntryCount();
  EXPECT_EQ(1u, count_after_paint1);

  SimulatePaint(content, gfx::RectF(100, 0, 50, 50));
  TriggerPopulateEntries();
  auto count_after_paint2 = GetContainerEntryCount();
  EXPECT_EQ(count_after_paint1 + 1u, count_after_paint2);

  SimulatePaint(content, gfx::RectF(200, 0, 50, 50));
  TriggerPopulateEntries();
  auto count_after_paint3 = GetContainerEntryCount();
  EXPECT_EQ(count_after_paint2 + 1u, count_after_paint3);
}

// Regression test for the FastHasAttribute guard on the initial tracker entry.
// Removing containertiming from the innermost root (without running pre-paint)
// must not produce a record for the now-non-root element.
TEST_F(ContainerTimingTest, StaleTrackerGuard_InitialRootRemoved) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner">
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* inner = GetDocument().getElementById(AtomicString("inner"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(inner);
  ASSERT_TRUE(content);

  // Initial paint - tracker populated, two entries emitted (inner + outer).
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_initial = GetContainerEntryCount();
  EXPECT_EQ(2u, count_initial);

  // Remove containertiming from inner without triggering a pre-paint walk.
  // The tracker's marked_nodes_ still says content → inner, but inner no
  // longer has the attribute.
  inner->removeAttribute(html_names::kContainertimingAttr);

  // Paint in a new area — the stale tracker entry must be rejected by the
  // FastHasAttribute guard and no new record created.
  SimulatePaint(content, gfx::RectF(200, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(count_initial, GetContainerEntryCount());
}

// Regression test for the FastHasAttribute guard on the parent-root chain.
// Removing containertiming from an ancestor root (without running pre-paint)
// must not produce a record for that ancestor.
TEST_F(ContainerTimingTest, StaleTrackerGuard_ParentRootRemoved) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner">
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* outer = GetDocument().getElementById(AtomicString("outer"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(content);

  // Initial paint — both roots receive entries.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_initial = GetContainerEntryCount();
  EXPECT_EQ(2u, count_initial);

  // Remove containertiming from outer without triggering a pre-paint walk.
  // container_root_parents_[inner] still says → outer, but outer no longer
  // has the attribute.
  outer->removeAttribute(html_names::kContainertimingAttr);

  // Paint in a new area — inner must receive an entry; outer must not.
  SimulatePaint(content, gfx::RectF(200, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(count_initial + 1u, GetContainerEntryCount());
}

// Regression test for the graceful early return in OnElementPainted (previously
// a CHECK). The painted element is inside a container timing root, but it is
// neither a text-aggregation nor an image-generating node, so the pre-paint
// walk never marks it in the tracker and GetContainerRootFor() returns null.
// This must not crash and must not produce an entry. It mirrors the
// presentation-time case where a painted element has been detached since paint.
TEST_F(ContainerTimingTest, NullContainerRoot_UntrackedElementReturns) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="plain"></div>
    </div>
  )HTML");

  auto* plain = GetDocument().getElementById(AtomicString("plain"));
  ASSERT_TRUE(plain);

  // Inside #root, but the pre-paint walk never marks it, so the tracker has no
  // mapping and no entry may be produced.
  SimulatePaint(plain, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(0u, GetContainerEntryCount());
}

// Regression test for removing the ContainerTimingChanged() cleanliness CHECKs
// from OnElementPainted. That method runs at presentation time, so the painted
// element's ContainerTimingChanged() bit can be dirty when an attribute was
// toggled since the last pre-paint. Toggling containertimingignore on a
// self-root keeps its containertiming attribute (so it stays a valid root)
// while MarkContainerTimingChanged() dirties its layout object. Painting must
// proceed without crashing and still record.
TEST_F(ContainerTimingTest, PaintWithDirtyContainerTimingBit_DoesNotCrash) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">x</div>
  )HTML");

  auto* root = GetDocument().getElementById(AtomicString("root"));
  ASSERT_TRUE(root);

  // First paint: bit is clean, one entry emitted.
  SimulatePaint(root, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_initial = GetContainerEntryCount();
  EXPECT_EQ(1u, count_initial);

  // Dirty the layout object's ContainerTimingChanged() bit without a pre-paint
  // to clear it. #root keeps its containertiming attribute.
  root->setAttribute(html_names::kContainertimingignoreAttr, g_empty_atom);
  ASSERT_TRUE(root->GetLayoutObject());
  ASSERT_TRUE(ContainerTimingChanged(*root->GetLayoutObject()));

  // Painting with the dirty bit must not crash and must still record.
  SimulatePaint(root, gfx::RectF(200, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(count_initial + 1u, GetContainerEntryCount());
}

// Exercises the cached root branch in pre_paint_tree_walk's
// UpdateContainerTimingContext: after the initial walk sets
// ShouldInheritContainerTimingRoot=false on the CT root, a subsequent walk
// triggered by a non-CT change must reuse the cached root rather than
// re-running UpdateOnPrePaint.
TEST_F(ContainerTimingTest, Propagation_CachedRootPath) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="content" style="width: 100px;">x</div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  // Initial paint — attribution via the initial walk.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_initial = GetContainerEntryCount();
  EXPECT_EQ(1u, count_initial);

  // Non-CT-affecting layout change. The pre-paint walk re-visits the subtree
  // but without the kContainerTimingContext walk reason, so the cached root
  // path runs in UpdateContainerTimingContext.
  content->setAttribute(html_names::kStyleAttr, AtomicString("width: 200px;"));
  UpdateAllLifecyclePhasesForTest();

  // Second paint must still attribute correctly via the cached root.
  SimulatePaint(content, gfx::RectF(200, 200, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(count_initial + 1u, GetContainerEntryCount());
}

// Exercises the cached stop-node branch in pre_paint_tree_walk's
// UpdateContainerTimingContext: after the initial walk caches the
// containertimingignore element as a stop node, a subsequent walk triggered
// by a non-CT change must reuse the cached state and continue to block
// propagation.
TEST_F(ContainerTimingTest, Propagation_CachedStopNodePath) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="stop" containertimingignore>
        <div id="content" style="width: 100px;">x</div>
      </div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  // Initial paint — content is under a stop node, no entry produced.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(0u, GetContainerEntryCount());

  // Non-CT-affecting layout change re-walks the subtree; the cached stop-node
  // branch must run on the ignore element.
  content->setAttribute(html_names::kStyleAttr, AtomicString("width: 200px;"));
  UpdateAllLifecyclePhasesForTest();

  // Second paint must remain blocked by the cached stop node.
  SimulatePaint(content, gfx::RectF(200, 200, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(0u, GetContainerEntryCount());
}

// A container root with containertimingignore that contains text must
// receive its own paints (via the text-aggregation descent in pre-paint), but
// must not propagate to an ancestor container root.
TEST_F(ContainerTimingTest, TextRootWithIgnore_SelfReported) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="text-root" containertiming="text-root" containertimingignore>
        Hello
      </div>
    </div>
  )HTML");

  auto* text_root = GetDocument().getElementById(AtomicString("text-root"));
  ASSERT_TRUE(text_root);

  // The text aggregator is the text-root div; the pre-paint walk marks
  // text-root→text-root via the text-descent branch. SimulatePaint with
  // element=text-root then looks up text-root in the tracker.
  SimulatePaint(text_root, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();

  // Only the text-root entry is produced; ignore blocks propagation to outer.
  EXPECT_EQ(1u, GetContainerEntryCount());
}

// An ancestor with only `containertimingignore` (no `containertiming`) must
// stop the upward walk and produce no entries.
// The deprecated dashed spelling must keep working until it is removed.
TEST_F(ContainerTimingTest, Propagation_DeprecatedDashedIgnoreStopsWalk) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="ignored" containertiming-ignore>
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(0u, GetContainerEntryCount());
}

TEST_F(ContainerTimingTest, Propagation_IgnoreOnlyStopsWalk) {
  SetBodyContent(R"HTML(
    <div containertiming="outer">
      <div containertimingignore>
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(0u, GetContainerEntryCount());
}

// Painting an element with no `containertiming` ancestor at all must produce
// no entries — it is not attributed to any container root.
TEST_F(ContainerTimingTest, Propagation_NoRootInAncestors) {
  SetBodyContent(R"HTML(
    <div><div id="content">x</div></div>
  )HTML");

  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(content);

  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(0u, GetContainerEntryCount());
}

// Toggling `containertimingignore` via setAttribute after the initial layout
// exercises the branch in OnContainerTimingIgnoreAttrChanged that marks the
// layout object so the pre-paint walk reattributes the subtree.
TEST_F(ContainerTimingTest, Propagation_AddIgnoreMarksLayout) {
  SetBodyContent(R"HTML(
    <div containertiming="outer">
      <div id="inner" containertiming="inner">
        <div id="content">x</div>
      </div>
    </div>
  )HTML");

  auto* inner = GetDocument().getElementById(AtomicString("inner"));
  auto* content = GetDocument().getElementById(AtomicString("content"));
  ASSERT_TRUE(inner);
  ASSERT_TRUE(content);

  // First paint reaches both roots.
  SimulatePaint(content, gfx::RectF(0, 0, 100, 100));
  TriggerPopulateEntries();
  auto count_after_first_paint = GetContainerEntryCount();
  EXPECT_EQ(2u, count_after_first_paint);

  // setAttribute on the live element triggers
  // OnContainerTimingIgnoreAttrChanged, which calls
  // MarkContainerTimingChanged() on inner's LayoutObject so the next pre-paint
  // walk re-attributes the subtree.
  inner->setAttribute(html_names::kContainertimingignoreAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  // Second paint must now stop at the inner root.
  SimulatePaint(content, gfx::RectF(200, 200, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(count_after_first_paint + 1u, GetContainerEntryCount());
}

// Two sibling `containertiming` mutations between lifecycle updates exercise
// the break in LayoutObject::MarkContainerTimingChanged(): walking from the
// second sibling reaches a shared ancestor that already has the descendant
// bit set from the first sibling's walk, and stops.
TEST_F(ContainerTimingTest, Propagation_TwoSiblingsBreakOnSharedAncestor) {
  SetBodyContent(R"HTML(
    <div id="parent">
      <div id="a"><div id="ca">x</div></div>
      <div id="b"><div id="cb">y</div></div>
    </div>
  )HTML");

  auto* a = GetDocument().getElementById(AtomicString("a"));
  auto* b = GetDocument().getElementById(AtomicString("b"));
  auto* ca = GetDocument().getElementById(AtomicString("ca"));
  auto* cb = GetDocument().getElementById(AtomicString("cb"));
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(ca);
  ASSERT_TRUE(cb);

  // Two attribute mutations without an intervening lifecycle update. The walk
  // for `b` reaches `parent`, finds the descendant bit already set from `a`'s
  // walk, and breaks.
  a->setAttribute(html_names::kContainertimingAttr, AtomicString("a"));
  b->setAttribute(html_names::kContainertimingAttr, AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();

  SimulatePaint(ca, gfx::RectF(0, 0, 100, 100));
  SimulatePaint(cb, gfx::RectF(200, 0, 100, 100));
  TriggerPopulateEntries();
  // One entry per new root.
  EXPECT_EQ(2u, GetContainerEntryCount());
}

// The tracker attributes content to the innermost container root. The tracker
// keys the innermost box that directly contains text, i.e. `grandchild`.
TEST_F(ContainerTimingTest, AttributesContentToRoot) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="child">
        <div id="grandchild">x</div>
      </div>
    </div>
  )HTML");

  auto* root = GetDocument().getElementById(AtomicString("root"));
  auto* grandchild = GetDocument().getElementById(AtomicString("grandchild"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(grandchild);

  EXPECT_EQ(root,
            GetContainerTiming().PaintAttributionTracker()->GetContainerRootFor(
                grandchild));
}

// A painted element that is not under any container root must be ignored, even
// though OnElementPainted() is invoked for it (text records also exist for
// LCP-only reasons). This is the load-bearing filter that replaces the node
// flag gate inside OnElementPainted().
TEST_F(ContainerTimingTest, UnrelatedElementNotAttributed) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="inside">x</div>
    </div>
    <div id="outside">y</div>
  )HTML");

  auto* root = GetDocument().getElementById(AtomicString("root"));
  auto* inside = GetDocument().getElementById(AtomicString("inside"));
  auto* outside = GetDocument().getElementById(AtomicString("outside"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(inside);
  ASSERT_TRUE(outside);

  auto* tracker = GetContainerTiming().PaintAttributionTracker();
  EXPECT_EQ(root, tracker->GetContainerRootFor(inside));
  EXPECT_EQ(nullptr, tracker->GetContainerRootFor(outside));

  // Paint under the root: attributed. Paint the unrelated element: ignored.
  SimulatePaint(inside, gfx::RectF(0, 0, 100, 100));
  SimulatePaint(outside, gfx::RectF(200, 200, 100, 100));
  TriggerPopulateEntries();
  EXPECT_EQ(1u, GetContainerEntryCount());
}

// A subtree inserted under an existing root after initial layout must be
// attributed by the next pre-paint walk, with no node-flag maintenance running.
TEST_F(ContainerTimingTest, InsertionUnderRootAttributed) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="existing">x</div>
    </div>
  )HTML");

  auto* root = GetDocument().getElementById(AtomicString("root"));
  ASSERT_TRUE(root);

  auto* inserted = GetDocument().CreateRawElement(html_names::kDivTag);
  inserted->setTextContent("z");
  root->AppendChild(inserted);
  UpdateAllLifecyclePhasesForTest();

  // The tracker attributes the newly inserted subtree to the root.
  EXPECT_EQ(root,
            GetContainerTiming().PaintAttributionTracker()->GetContainerRootFor(
                inserted));

  SimulatePaint(inserted, gfx::RectF(0, 0, 50, 50));
  TriggerPopulateEntries();
  EXPECT_EQ(1u, GetContainerEntryCount());
}

// Removing the containertiming attribute must clear the tracker attribution on
// the next pre-paint walk (driven by MarkContainerTimingChanged(), not the node
// flag).
TEST_F(ContainerTimingTest, RemovingRootClearsAttribution) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="inside">x</div>
    </div>
  )HTML");

  auto* root = GetDocument().getElementById(AtomicString("root"));
  auto* inside = GetDocument().getElementById(AtomicString("inside"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(inside);

  auto* tracker = GetContainerTiming().PaintAttributionTracker();
  EXPECT_EQ(root, tracker->GetContainerRootFor(inside));

  root->removeAttribute(html_names::kContainertimingAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(nullptr, tracker->GetContainerRootFor(inside));
}

// The text paint-timing gate consults the tracker: true under a root, false
// when unrelated, and always true for elements explicitly registered for
// element timing.
TEST_F(ContainerTimingTest, TextGateUsesTracker) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="inside">x</div>
    </div>
    <div id="outside">y</div>
    <div id="et" elementtiming="et">z</div>
  )HTML");

  auto* inside = GetDocument().getElementById(AtomicString("inside"));
  auto* outside = GetDocument().getElementById(AtomicString("outside"));
  auto* et = GetDocument().getElementById(AtomicString("et"));
  ASSERT_TRUE(inside);
  ASSERT_TRUE(outside);
  ASSERT_TRUE(et);

  EXPECT_TRUE(TextElementTiming::NeededForTiming(*inside));
  EXPECT_FALSE(TextElementTiming::NeededForTiming(*outside));
  // elementtiming registration is independent of container timing.
  EXPECT_TRUE(TextElementTiming::NeededForTiming(*et));
}

class ContainerTimingIframeIsolationTest : public PageTestBase {
 protected:
  void SetUp() override {
    SetupPageWithClients(nullptr,
                         MakeGarbageCollected<SingleChildLocalFrameClient>());
  }

 private:
  ScopedContainerTimingForTest scoped_feature_{true};
};

TEST_F(ContainerTimingIframeIsolationTest, IframeIsolation) {
  SetBodyContent(R"HTML(<iframe id="child" srcdoc=""></iframe>)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* child_frame = DynamicTo<LocalFrame>(GetFrame().Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  LocalDOMWindow* child_window = child_frame->DomWindow();
  ASSERT_TRUE(child_window);
  Document* child_document = child_frame->GetDocument();
  ASSERT_TRUE(child_document);

  // srcdoc content does not reliably parse in this fixture; inject the child
  // body programmatically instead.
  child_document->body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='child_root' containertiming='child_ct'>"
      "<div id='child_content'>child text</div>"
      "</div>");
  UpdateAllLifecyclePhasesForTest();

  ContainerTiming& parent_ct =
      ContainerTiming::From(*GetDocument().domWindow());
  ContainerTiming& child_ct = ContainerTiming::From(*child_window);

  EXPECT_NE(&parent_ct, &child_ct);
  // Each frame owns an independent pre-paint attribution tracker.
  EXPECT_NE(parent_ct.PaintAttributionTracker(),
            child_ct.PaintAttributionTracker());

  Element* child_content =
      child_document->getElementById(AtomicString("child_content"));
  ASSERT_TRUE(child_content);
  SimulateContainerTimingPaint(child_ct, child_content,
                               gfx::RectF(0, 0, 100, 100));

  auto* parent_performance =
      DOMWindowPerformance::performance(*GetDocument().domWindow());
  auto* child_performance = DOMWindowPerformance::performance(*child_window);
  parent_performance->PopulateContainerTimingEntries();
  child_performance->PopulateContainerTimingEntries();

  EXPECT_EQ(0u, parent_performance
                    ->getBufferedEntriesByType(AtomicString("container"))
                    .size());
  EXPECT_EQ(
      1u, child_performance->getBufferedEntriesByType(AtomicString("container"))
              .size());
}

}  // namespace blink
