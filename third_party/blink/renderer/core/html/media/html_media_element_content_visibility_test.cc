// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLMediaElementContentVisibilityTest : public RenderingTest {
 public:
  HTMLMediaElementContentVisibilityTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void LockElement(Element& element) {
    element.setAttribute(html_names::kStyleAttr,
                         AtomicString("content-visibility: hidden"));
    UpdateAllLifecyclePhasesForTest();
  }

  void CommitElement(Element& element) {
    element.setAttribute(html_names::kStyleAttr, g_empty_atom);
    UpdateAllLifecyclePhasesForTest();
  }

 private:
  ScopedContentVisibilityMediaPlaybackForTest scoped_feature_{true};
};

TEST_F(HTMLMediaElementContentVisibilityTest,
       MutedWhileAncestorLockedCrossingFrames) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id="grandparent">
      <iframe id="frame" src="http://test.com"></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id="parent">
      <video id="media"></video>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* grandparent =
      GetDocument().getElementById(AtomicString("grandparent"));
  Element* parent = ChildDocument().getElementById(AtomicString("parent"));
  auto* media = DynamicTo<HTMLMediaElement>(
      ChildDocument().getElementById(AtomicString("media")));

  ASSERT_TRUE(grandparent);
  ASSERT_TRUE(parent);
  ASSERT_TRUE(media);
  EXPECT_EQ(1, media->ComputeVolumeMultiplier());

  LockElement(*parent);
  EXPECT_EQ(0, media->ComputeVolumeMultiplier());

  LockElement(*grandparent);
  EXPECT_EQ(0, media->ComputeVolumeMultiplier());

  // Still locked by the cross-frame grandparent.
  CommitElement(*parent);
  EXPECT_EQ(0, media->ComputeVolumeMultiplier());

  CommitElement(*grandparent);
  // The iframe document was still throttled during the lifecycle update run
  // by CommitElement() and becomes unthrottled at the end of it, so one more
  // update is needed to resolve style in the iframe.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1, media->ComputeVolumeMultiplier());
}

TEST_F(HTMLMediaElementContentVisibilityTest,
       MutesMediaInDisplayLockedChildFrame) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <div id="container">
      <iframe id="frame" src="http://test.com"></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <video id="media"></video>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  auto* media = DynamicTo<HTMLMediaElement>(
      ChildDocument().getElementById(AtomicString("media")));
  ASSERT_TRUE(container);
  ASSERT_TRUE(media);
  EXPECT_EQ(1, media->ComputeVolumeMultiplier());

  // The child frame view is display locked as a whole, which also throttles
  // its lifecycle updates.
  LockElement(*container);
  EXPECT_TRUE(ChildDocument().View()->IsDisplayLockThrottled());
  EXPECT_EQ(0, media->ComputeVolumeMultiplier());

  CommitElement(*container);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(ChildDocument().View()->IsDisplayLockThrottled());
  EXPECT_EQ(1, media->ComputeVolumeMultiplier());
}

// Per css-contain-2, contents skipped due to `content-visibility: auto` may
// remain relevant to the user, so their media is not muted.
TEST_F(HTMLMediaElementContentVisibilityTest,
       ContentVisibilityAutoDoesNotMuteMediaWhenSkipped) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .spacer { height: 10000px; }
      #target {
        content-visibility: auto;
        contain-intrinsic-size: 100px;
      }
    </style>
    <div class="spacer"></div>
    <div id="target"><video id="media"></video></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  UpdateAllLifecyclePhasesForTest();
  auto* target = GetDocument().getElementById(AtomicString("target"));
  auto* media = DynamicTo<HTMLMediaElement>(
      GetDocument().getElementById(AtomicString("media")));
  ASSERT_TRUE(target);
  ASSERT_TRUE(media);
  ASSERT_TRUE(target->GetDisplayLockContext());
  EXPECT_TRUE(target->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(1, media->ComputeVolumeMultiplier());

  GetDocument().scrollingElement()->setScrollTop(10000);
  UpdateAllLifecyclePhasesForTest();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->GetDisplayLockContext()->IsLocked());
  EXPECT_EQ(1, media->ComputeVolumeMultiplier());
}

}  // namespace blink
