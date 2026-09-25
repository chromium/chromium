// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/inherited_animations/inherited_animation_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class InheritedAnimationTrackerTest : public RenderingTest {
 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }
  Animation* SourceAnimation() {
    return GetElementById("source")->getAnimations()[0];
  }
  bool SourceIsComposited() {
    return SourceAnimation()->HasActiveAnimationsOnCompositor();
  }
  bool SourceHasUnsupportedInheritance() {
    return SourceAnimation()->GetCompositingDecisionState().disposition &
           CompositorAnimations::kUnsupportedInheritance;
  }

 private:
  ScopedTrackAnimatedSourcesForTest track_animated_sources_{true};
};

TEST_F(InheritedAnimationTrackerTest, InheritedOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        will-change: opacity;
      }
      @keyframes fade {
        from { opacity: 1; }
        to { opacity: 0.5; }
      }
      #source {
        animation: fade 10s linear;
      }
    </style>
    <div id=source><div id=target></div></div>
  )HTML");
  Animation* animation = SourceAnimation();
  EXPECT_TRUE(SourceIsComposited());

  // A descendant inheriting the animated value keeps it off the compositor.
  Element* target = GetElementById("target");
  target->SetInlineStyleProperty(CSSPropertyID::kOpacity, "inherit");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(SourceIsComposited());
  EXPECT_TRUE(SourceHasUnsupportedInheritance());

  // The flag is never cleared, so it stays off once the descendant no longer
  // inherits.
  target->RemoveInlineStyleProperty(CSSPropertyID::kOpacity);
  animation->cancel();
  UpdateAllLifecyclePhasesForTest();
  animation->play(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(animation->HasActiveAnimationsOnCompositor());
}

TEST_F(InheritedAnimationTrackerTest, InheritedOpacityOnPseudoElement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        will-change: opacity;
      }
      @keyframes fade {
        from { opacity: 1; }
        to { opacity: 0.5; }
      }
      #source {
        animation: fade 10s linear;
      }
      #source::before {
        content: '';
        display: block;
        opacity: inherit;
      }
    </style>
    <div id=source></div>
  )HTML");
  EXPECT_FALSE(SourceIsComposited());
  EXPECT_TRUE(SourceHasUnsupportedInheritance());
}

TEST_F(InheritedAnimationTrackerTest, InheritedTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        will-change: transform;
      }
      @keyframes slide {
        from { transform: none; }
        to { transform: translateX(100px); }
      }
      #source {
        animation: slide 10s linear;
      }
      #target {
        transform: inherit;
      }
    </style>
    <div id=source><div id=target></div></div>
  )HTML");
  EXPECT_FALSE(SourceIsComposited());
  EXPECT_TRUE(SourceHasUnsupportedInheritance());
}

}  // namespace blink
