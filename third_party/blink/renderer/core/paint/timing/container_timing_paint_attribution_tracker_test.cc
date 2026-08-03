// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing_paint_attribution_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_test_utils.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ContainerTimingPaintAttributionTrackerTest : public PageTestBase {
 protected:
  ContainerTimingPaintAttributionTracker* Tracker() { return tracker_.Get(); }

 private:
  void SetUp() override {
    PageTestBase::SetUp();
    tracker_ = ContainerTiming::From(*GetDocument().domWindow())
                   .PaintAttributionTracker();
  }

  Persistent<ContainerTimingPaintAttributionTracker> tracker_;
  ScopedContainerTimingPrepaintTraversalForTest scoped_feature_{true};
};

TEST_F(ContainerTimingPaintAttributionTrackerTest, SingleRoot) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <img id="img">
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();

  Element* root = GetDocument().getElementById(AtomicString("root"));
  ASSERT_TRUE(root);

  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);

  // Image node should map to the container root.
  Element* found = tracker->GetContainerRootFor(img);
  EXPECT_EQ(found, root);
}

TEST_F(ContainerTimingPaintAttributionTrackerTest, NestedRoots) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner">
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();

  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(inner);

  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);

  // Image maps to innermost root.
  Element* found = tracker->GetContainerRootFor(img);
  EXPECT_EQ(found, inner);

  // Inner's parent root is outer.
  Element* parent = tracker->GetParentContainerRootFor(inner);
  EXPECT_EQ(parent, outer);

  // Outer has no parent root.
  Element* outer_parent = tracker->GetParentContainerRootFor(outer);
  EXPECT_EQ(outer_parent, nullptr);
}

TEST_F(ContainerTimingPaintAttributionTrackerTest, ContainerTimingIgnore) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="ignored" containertimingignore>
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();

  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);

  // Image is under an ignore node — should not be tracked.
  Element* found = tracker->GetContainerRootFor(img);
  EXPECT_EQ(found, nullptr);
}

TEST_F(ContainerTimingPaintAttributionTrackerTest, BothAttrsOnSameElement) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="both" containertiming="both" containertimingignore>
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* both = GetDocument().getElementById(AtomicString("both"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(both);

  // "both" acts as a root but does NOT bubble to outer (ignore wins for
  // parent propagation).
  Element* parent = tracker->GetParentContainerRootFor(both);
  EXPECT_EQ(parent, nullptr);

  // Image inside "both" maps to "both" root.
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);
  Element* found = tracker->GetContainerRootFor(img);
  EXPECT_EQ(found, both);
}

TEST_F(ContainerTimingPaintAttributionTrackerTest, TextAggregationNodeTracked) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="box"><span id="span">Hello</span></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* box = GetDocument().getElementById(AtomicString("box"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(box);

  // Text paint timing is aggregated up to the nearest box ancestor (|box|) of
  // the LayoutText. The pre-paint walk pushes |box| down as the text
  // aggregation node while visiting the text, so the tracker marks |box| under
  // the container root.
  EXPECT_EQ(tracker->GetContainerRootFor(box), root);
}

TEST_F(ContainerTimingPaintAttributionTrackerTest, ShadowTreeElementsExcluded) {
  SetBodyContent(R"HTML(
    <div id="host" containertiming="root"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  ASSERT_TRUE(host);
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      R"HTML(<img id="shadow_img">)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();

  // Shadow img should NOT be tracked.
  Element* shadow_img = shadow_root.getElementById(AtomicString("shadow_img"));
  ASSERT_TRUE(shadow_img);
  EXPECT_EQ(tracker->GetContainerRootFor(shadow_img), nullptr);
}

// An <img> that is itself a container timing root must be marked as a tracked
// node mapping to itself, so a paint on the img reports under that root.
TEST_F(ContainerTimingPaintAttributionTrackerTest, ImageIsContainerRoot) {
  SetBodyContent(R"HTML(
    <img id="img" containertiming="img-root">
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);

  // The img maps to itself as the container root.
  EXPECT_EQ(tracker->GetContainerRootFor(img), img);
  // No parent root for the outermost root.
  EXPECT_EQ(tracker->GetParentContainerRootFor(img), nullptr);
}

// An <img> with both containertiming and containertimingignore is itself a
// container root and reports its own paint under that root. The ignore
// attribute only blocks upward propagation to ancestor roots — it does not
// suppress self-marking.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       ImageRootWithIgnoreSelfMarked) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <img id="img" containertiming="img-root" containertimingignore>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(img);

  // The img maps to itself as the container root (self-marked even with
  // ignore set).
  EXPECT_EQ(tracker->GetContainerRootFor(img), img);
  // But the parent chain is broken by the ignore attribute, so a paint on
  // the img does not propagate to outer.
  EXPECT_EQ(tracker->GetParentContainerRootFor(img), nullptr);
}

// A div with a background-image and containertiming is treated as an image
// container root by IsImageType().
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       BackgroundImageContainerRoot) {
  SetBodyContent(R"HTML(
    <style>#bg { background-image: url('data:image/png;base64,iVBORw0KGgo='); }</style>
    <div id="bg" containertiming="bg-root"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* bg = GetDocument().getElementById(AtomicString("bg"));
  ASSERT_TRUE(bg);

  // The bg element is both the container root and the image node, so it maps
  // to itself.
  EXPECT_EQ(tracker->GetContainerRootFor(bg), bg);
}

// A ::before pseudo-element with `content: url(...)` produces an anonymous
// LayoutImage (created by LayoutImage::CreateAnonymous in content_data.cc).
// UpdateOnPrePaint takes the no-node branch and uses GeneratingNode() to find
// the originating element, marking it under the container root.
TEST_F(ContainerTimingPaintAttributionTrackerTest, AnonymousImageLayoutObject) {
  SetBodyContent(R"HTML(
    <style>
      #bg::before {
        content: url('data:image/png;base64,iVBORw0KGgo=');
        display: inline-block;
      }
    </style>
    <div id="root" containertiming="root">
      <div id="bg"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* bg = GetDocument().getElementById(AtomicString("bg"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(bg);

  PseudoElement* before = bg->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(before);
  LayoutObject* before_lo = before->GetLayoutObject();
  ASSERT_TRUE(before_lo);
  LayoutObject* image_lo = before_lo->SlowFirstChild();
  ASSERT_TRUE(image_lo);
  // The image LO must be anonymous (no DOM node) and image-typed for this
  // test to actually exercise the no-node branch.
  ASSERT_FALSE(image_lo->GetNode());
  ASSERT_TRUE(paint_timing::IsImageType(*image_lo));

  // The pre-paint walk visits the anonymous LayoutImage and takes the no-node
  // branch; GeneratingNode() walks up to the pseudo's ultimate originating
  // element (#bg), which the tracker marks under root.
  auto* tracker = Tracker();
  EXPECT_EQ(tracker->GetContainerRootFor(bg), root);
}

// A ::before pseudo-element with `content: "x"` produces an anonymous
// LayoutTextFragment (LayoutTextFragment::CreateAnonymous in content_data.cc).
// UpdateOnPrePaint takes the no-node text branch and marks the supplied
// text_aggregator under the container root.
TEST_F(ContainerTimingPaintAttributionTrackerTest, AnonymousTextLayoutObject) {
  SetBodyContent(R"HTML(
    <style>#bg::before { content: "x"; }</style>
    <div id="root" containertiming="root">
      <div id="bg"></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* bg = GetDocument().getElementById(AtomicString("bg"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(bg);

  PseudoElement* before = bg->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(before);
  LayoutObject* before_lo = before->GetLayoutObject();
  ASSERT_TRUE(before_lo);
  LayoutObject* text_lo = before_lo->SlowFirstChild();
  ASSERT_TRUE(text_lo);
  ASSERT_FALSE(text_lo->GetNode());
  ASSERT_TRUE(text_lo->IsText());

  // The pre-paint walk visits the anonymous LayoutTextFragment and takes the
  // no-node text branch, marking the text aggregator (#bg) under the root.
  auto* tracker = Tracker();
  EXPECT_EQ(tracker->GetContainerRootFor(bg), root);
}

// Removing `containertiming` from a self-marked image root must erase the
// self-mark on the next pre-paint walk, so paint-time attribution no longer
// finds the now-stale root. removeAttribute() marks the LayoutObject dirty
// (MarkContainerTimingChanged), so the next lifecycle update re-visits the
// image with a null context and clears it.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       ErasesSelfMarkOnAttributeRemoval) {
  SetBodyContent(R"HTML(
    <img id="img" containertiming="img-root">
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);

  ASSERT_EQ(tracker->GetContainerRootFor(img), img);

  // Drop the attribute and let the pre-paint walk re-attribute. The self-mark
  // and the container_root_parents_ entry must be gone.
  img->removeAttribute(html_names::kContainertimingAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(img), nullptr);
  EXPECT_EQ(tracker->GetParentContainerRootFor(img), nullptr);
}

// Removing `containertiming` from a root must clear descendant entries that
// mapped to it. The walk descends through the affected subtree and clears
// each previously-marked leaf as it visits it (null context_container_root).
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       PrunesDescendantsOnRootAttributeRemoval) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <img id="img">
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(img);

  ASSERT_EQ(tracker->GetContainerRootFor(img), root);

  root->removeAttribute(html_names::kContainertimingAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(img), nullptr);
  EXPECT_EQ(tracker->GetParentContainerRootFor(root), nullptr);
}

// Once an inner root is inserted, the pre-paint walk re-attributes its subtree
// to that inner root. Removing an outer root must not clobber entries already
// re-attributed to the inner root.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       ReattributesNodeWhenInnerRootInserted) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner">Hello</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(inner);

  // Initially the inner box's text aggregates up to outer.
  ASSERT_EQ(tracker->GetContainerRootFor(inner), outer);

  // Make inner its own root: its text now aggregates to inner.
  inner->setAttribute(html_names::kContainertimingAttr, AtomicString("inner"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(tracker->GetContainerRootFor(inner), inner);

  // Removing outer's root must not wipe the still-valid attribution under
  // inner: the walk re-points inner's subtree to inner, so dropping outer
  // leaves those entries untouched.
  outer->removeAttribute(html_names::kContainertimingAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(inner), inner);
}

TEST_F(ContainerTimingPaintAttributionTrackerTest,
       PrunesDescendantsOnTransitionToIgnoreOnly) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <img id="img">
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(img);

  ASSERT_EQ(tracker->GetContainerRootFor(img), root);

  root->removeAttribute(html_names::kContainertimingAttr);
  root->setAttribute(html_names::kContainertimingignoreAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(img), nullptr);
  EXPECT_EQ(tracker->GetParentContainerRootFor(root), nullptr);
}

// none -> CTI on a non-root element creates a stop in the middle. The walk
// descends and clears leaves below the new stop.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       StopInsertedBetweenRootAndLeaf) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="mid">
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* mid = GetDocument().getElementById(AtomicString("mid"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(mid);
  ASSERT_TRUE(img);
  ASSERT_EQ(tracker->GetContainerRootFor(img), root);

  mid->setAttribute(html_names::kContainertimingignoreAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  // Stop in the middle: img is no longer attributed to root.
  EXPECT_EQ(tracker->GetContainerRootFor(img), nullptr);
}

// CTI -> none on a non-root re-opens propagation; the walk re-attributes the
// previously-cleared leaves to the surviving ancestor root.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       StopRemovedReattributesLeaf) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <div id="mid" containertimingignore>
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* mid = GetDocument().getElementById(AtomicString("mid"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(root);
  ASSERT_TRUE(mid);
  ASSERT_TRUE(img);
  ASSERT_EQ(tracker->GetContainerRootFor(img), nullptr);

  mid->removeAttribute(html_names::kContainertimingignoreAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(img), root);
}

// CT -> both (add CTI to an existing root). The root's parent mapping flips
// from the ancestor root to nullptr; descendants keep mapping to this root.
TEST_F(ContainerTimingPaintAttributionTrackerTest, IgnoreAddedToExistingRoot) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner">
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(inner);
  ASSERT_TRUE(img);
  ASSERT_EQ(tracker->GetContainerRootFor(img), inner);
  ASSERT_EQ(tracker->GetParentContainerRootFor(inner), outer);

  inner->setAttribute(html_names::kContainertimingignoreAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  // Descendants still map to inner; inner just no longer propagates upward.
  EXPECT_EQ(tracker->GetContainerRootFor(img), inner);
  EXPECT_EQ(tracker->GetParentContainerRootFor(inner), nullptr);
}

// both -> CT (remove CTI from a both-attr root). The root's parent mapping
// flips from nullptr to the ancestor root; descendants stay mapped to this
// root.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       IgnoreRemovedFromBothAttrRoot) {
  SetBodyContent(R"HTML(
    <div id="outer" containertiming="outer">
      <div id="inner" containertiming="inner" containertimingignore>
        <img id="img">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(outer);
  ASSERT_TRUE(inner);
  ASSERT_TRUE(img);
  ASSERT_EQ(tracker->GetContainerRootFor(img), inner);
  ASSERT_EQ(tracker->GetParentContainerRootFor(inner), nullptr);

  inner->removeAttribute(html_names::kContainertimingignoreAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(img), inner);
  EXPECT_EQ(tracker->GetParentContainerRootFor(inner), outer);
}

// Removing CT from one subtree must not affect a sibling subtree's mappings.
// Verifies the walk only descends through the changed subtree.
TEST_F(ContainerTimingPaintAttributionTrackerTest, SiblingSubtreeUnaffected) {
  SetBodyContent(R"HTML(
    <div id="parent">
      <div id="a" containertiming="a">
        <img id="img_a">
      </div>
      <div id="b" containertiming="b">
        <img id="img_b">
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* img_a = GetDocument().getElementById(AtomicString("img_a"));
  Element* img_b = GetDocument().getElementById(AtomicString("img_b"));
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(img_a);
  ASSERT_TRUE(img_b);
  ASSERT_EQ(tracker->GetContainerRootFor(img_a), a);
  ASSERT_EQ(tracker->GetContainerRootFor(img_b), b);

  a->removeAttribute(html_names::kContainertimingAttr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker->GetContainerRootFor(img_a), nullptr);
  // b and its subtree must remain unchanged.
  EXPECT_EQ(tracker->GetContainerRootFor(img_b), b);
  EXPECT_EQ(tracker->GetParentContainerRootFor(b), nullptr);
}

// An image-type leaf with containertimingignore (and no containertiming)
// under a CT ancestor must not be stored in the tracker. The leaf-marking
// step would otherwise map the image to the ancestor root, which would be
// semantically wrong even though ContributesToContainerTiming masks it at
// paint time.
TEST_F(ContainerTimingPaintAttributionTrackerTest, IgnoreOnlyOnImageLeaf) {
  SetBodyContent(R"HTML(
    <div id="root" containertiming="root">
      <img id="img" containertimingignore>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(img);

  // CTI on the image means it must not be attributed to the ancestor root.
  EXPECT_EQ(tracker->GetContainerRootFor(img), nullptr);
}

// Removing CT from two nested roots in one frame: the walk descends once and
// re-attributes leaves to the outermost surviving root.
TEST_F(ContainerTimingPaintAttributionTrackerTest,
       MultiLevelRootsRemovedInOneFrame) {
  SetBodyContent(R"HTML(
    <div id="a" containertiming="a">
      <div id="b" containertiming="b">
        <div id="c" containertiming="c">
          <img id="img">
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* tracker = Tracker();
  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));
  Element* img = GetDocument().getElementById(AtomicString("img"));
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  ASSERT_TRUE(img);
  ASSERT_EQ(tracker->GetContainerRootFor(img), c);

  b->removeAttribute(html_names::kContainertimingAttr);
  c->removeAttribute(html_names::kContainertimingAttr);
  UpdateAllLifecyclePhasesForTest();

  // img falls through to the outermost surviving root.
  EXPECT_EQ(tracker->GetContainerRootFor(img), a);
  EXPECT_EQ(tracker->GetParentContainerRootFor(b), nullptr);
  EXPECT_EQ(tracker->GetParentContainerRootFor(c), nullptr);
}

}  // namespace blink
