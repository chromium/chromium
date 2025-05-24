// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/clip_path_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using CompositedPaintStatus = ElementAnimations::CompositedPaintStatus;

class MockSchedulingChromeClient : public EmptyChromeClient {
 public:
  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta delay,
                         bool) override {
    has_scheduled_animation_ = true;
  }

  bool HasScheduledAnimation() { return has_scheduled_animation_; }

  void ResetMocks() { has_scheduled_animation_ = false; }

 private:
  bool has_scheduled_animation_ = false;
};

class ClipPathPaintDefinitionTest : public PageTestBase {
 public:
  ClipPathPaintDefinitionTest() = default;
  ~ClipPathPaintDefinitionTest() override = default;

  MockSchedulingChromeClient* Client() { return chrome_client_; }

  Animation* GetFirstAnimation(Element* element) {
    ElementAnimations* ea = element->GetElementAnimations();
    EXPECT_TRUE(ea);
    EXPECT_EQ(ea->Animations().size(), 1u);
    return ea->Animations().begin()->key;
  }

  Animation* GetFirstAnimationForProperty(const Element* element,
                                          const CSSProperty& property) {
    for (const auto& animation :
         element->GetElementAnimations()->Animations()) {
      if (animation.key->CalculateAnimationPlayState() !=
              V8AnimationPlayState::Enum::kIdle &&
          animation.key->Affects(*element, property)) {
        return animation.key;
      }
    }
    return nullptr;
  }

  void EnsureCCClipPathInvariantsHoldStyleAndLayout(
      bool needs_repaint,
      CompositedPaintStatus status,
      Element* element) {
    GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
        DocumentUpdateReason::kTest);

    LayoutObject* lo = element->GetLayoutObject();

    // Changes to a compositable animation should set
    EXPECT_EQ(lo->NeedsPaintPropertyUpdate(), needs_repaint);
    // Changes to a compositable animation should set kNeedsRepaint
    EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
              (!needs_repaint || status == CompositedPaintStatus::kNoAnimation)
                  ? status
                  : CompositedPaintStatus::kNeedsRepaint);
  }

  void EnsureCCClipPathInvariantsHoldThroughoutPainting(
      bool needs_repaint,
      CompositedPaintStatus status,
      Element* element,
      Animation* animation,
      std::optional<bool> override_scheduled_animation = std::nullopt) {
    LayoutObject* lo = element->GetLayoutObject();
    EXPECT_EQ(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kCompositingInputsClean);

    GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kTest);

    // Composited paint status should be resolved by this point
    EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
              status);

    UpdateAllLifecyclePhasesForTest();

    switch (status) {
      case CompositedPaintStatus::kNoAnimation:
      case CompositedPaintStatus::kNotComposited:
        // GetAnimationIfCompositable should return nothing in this circumstance
        EXPECT_EQ(ClipPathClipper::GetClipPathAnimation(*lo),
                  nullptr);
        // If a clip path is non-composited or non-existent, then the clip path
        // mask should not be set. If it is, it can cause a crash.
        EXPECT_TRUE(!lo->FirstFragment().PaintProperties() ||
                    !lo->FirstFragment().PaintProperties()->ClipPathMask());
        // Non-composited animations SHOULD still be causing animation updates.
        // Additionally, style/layout code seems to trigger animation update for
        // the first frame after an animation cancel.
        EXPECT_EQ(
            status == CompositedPaintStatus::kNotComposited || needs_repaint,
            Client()->HasScheduledAnimation());
        break;
      case CompositedPaintStatus::kComposited:
        // GetAnimationIfCompositable should return the given animation, if it
        // is compositable
        EXPECT_EQ(ClipPathClipper::GetClipPathAnimation(*lo),
                  animation);
        // Composited clip-path animations depend on ClipPathMask() being set
        EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
        // Composited clip-path animations shouldn't cause further animation
        // updates after the first paint.
        EXPECT_EQ(override_scheduled_animation.has_value()
                      ? *override_scheduled_animation
                      : needs_repaint,
                  Client()->HasScheduledAnimation());
        break;
      case CompositedPaintStatus::kNeedsRepaint:
        // kNeedsRepaint is only valid before pre-paint has been run
        NOTREACHED();
    }
  }

  void EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      bool needs_repaint,
      CompositedPaintStatus status,
      Element* element,
      Animation* animation,
      std::optional<bool> override_scheduled_animation = std::nullopt) {
    Client()->ResetMocks();

    EnsureCCClipPathInvariantsHoldStyleAndLayout(needs_repaint, status,
                                                 element);
    EnsureCCClipPathInvariantsHoldThroughoutPainting(
        needs_repaint, status, element, animation,
        override_scheduled_animation);
  }

  // Some animations require the paint artifact compositor's update flag to be
  // correctly cleared. This ensures that the Paint Artifact Compositor has a
  // LayerTreeHost so it will run its normal logic.
  void InitPaintArtifactCompositor() {
    layer_tree_ = std::make_unique<LayerTreeHostEmbedder>();
    layer_tree_->layer_tree_host()->SetRootLayer(
        GetDocument().View()->GetPaintArtifactCompositor()->RootLayer());
  }

 protected:
  void SetUp() override {
    scoped_composite_clip_path_animation =
        std::make_unique<ScopedCompositeClipPathAnimationForTest>(true);
    scoped_composite_bgcolor_animation =
        std::make_unique<ScopedCompositeBGColorAnimationForTest>(false);
    chrome_client_ = MakeGarbageCollected<MockSchedulingChromeClient>();
    PageTestBase::SetupPageWithClients(chrome_client_);

    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
    GetDocument().Timeline().ResetForTesting();
  }

 private:
  std::unique_ptr<ScopedCompositeClipPathAnimationForTest>
      scoped_composite_clip_path_animation;
  std::unique_ptr<ScopedCompositeBGColorAnimationForTest>
      scoped_composite_bgcolor_animation;

  Persistent<MockSchedulingChromeClient> chrome_client_;
  std::unique_ptr<LayerTreeHostEmbedder> layer_tree_;
};

// Test the case where there is a clip-path animation with two simple
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, SimpleClipPathAnimationNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 30s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  // Run lifecycle once more to ensure invariants hold post initial paint.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kComposited, element,
      animation);
}

// Test the case where there is a clip-path animation with two simple
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, ReverseClipPathAnimationNoUpdates) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 30s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(15000));
  animation->updatePlaybackRate(-1);

  // Run lifecycle once more: animation should still be composited. Because it's
  // the same animation, it shouldn't schedule an animation update
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation, false);

  // Run lifecycle once more: repaints should be avoided even with negative
  // playback rate
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kComposited, element,
      animation);
}

// Test the case where there is a clip-path animation with two simple
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, SimpleClipPathAnimationFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: initial;
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 30s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kNotComposited,
      element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kNotComposited, element,
      animation);

  // Run lifecycle once more to ensure invariants hold post initial paint.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kNotComposited,
      element, animation);
}

// Cannot animate a <br>
TEST_F(ClipPathPaintDefinitionTest, SimpleClipPathAnimationFallbackOnBR) {
  SetBodyInnerHTML(R"HTML(
      <style>
          @keyframes clippath {
              0% {
                  clip-path: initial;
              }
              100% {
                  clip-path: circle(30% at 30% 30%);
              }
          }
          .animation br {
              animation: clippath 30s;
          }
      </style>
      <div id="container">
        <br id="target">
      </div>
    )HTML");

  Element* container = GetElementById("container");
  Element* element = GetElementById("target");
  container->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kNotComposited,
      element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kNotComposited, element,
      animation);

  // Run lifecycle once more to ensure invariants hold post initial paint.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kNotComposited,
      element, animation);
}

TEST_F(ClipPathPaintDefinitionTest, ClipPathAnimationCancel) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 30s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  animation->cancel();

  // Cancelling the animation should reset status and the clippath properties
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ true, CompositedPaintStatus::kNoAnimation, element,
      animation);
}

// Test the case where a 2nd composited clip path animation causes a fallback to
// the main thread. In this case, the paint properties should update to avoid
// any crashes or paint worklets existing beyond their validity.
TEST_F(ClipPathPaintDefinitionTest, FallbackOnNonCompositableSecondAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 30s;
        }
        .animation2 {
            animation: clippath 30s, clippath 15s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  // Adding a 2nd clip path animation is non-compositable.

  element->setAttribute(html_names::kClassAttr, AtomicString("animation2"));

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kNotComposited, element,
      animation);

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kNotComposited,
      element, animation);
}

TEST_F(ClipPathPaintDefinitionTest,
       NoInvalidationsOnPseudoWithTransformAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        @keyframes transform {
            0% {
                transform: rotate(10deg);
            }
            100% {
                transform: rotate(360deg);
            }
        }
      .animation {
        animation: transform 30s;
      }
      .animation:after {
        animation: clippath 30s;
      }
      #target:after{
        content:"";
      }
    </style>
    <span id="target"></span>
  )HTML");

  Element* element = GetElementById("target");
  Element* element_pseudo = To<Element>(element->PseudoAwareFirstChild());
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited,
      element_pseudo);

  Animation* animation = GetFirstAnimation(element_pseudo);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited,
      element_pseudo, animation);

  // Re-run to ensure composited clip path status is not being reset. Note that
  // we allow for an animation update to be scheduled here, due to the transform
  // animation.

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kComposited,
      element_pseudo, animation, /* override_scheduled_animation= */ true);
}

// Test the case where there is a clip-path animation with two shape()
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, ShapeClipPathAnimationNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: shape(from 10px 10px, vline by 20px, hline by 20px);
            }
            100% {
                clip-path: shape(from 10px 10px, vline by 30px, hline by 30px);
            }
        }
        .animation {
            animation: clippath 30s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  // Run lifecycle once more to ensure invariants hold post initial paint.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kComposited, element,
      animation);
}

// Test the case where there is a clip-path animation with two simple
// keyframes that will not fall back to main.
TEST_F(ClipPathPaintDefinitionTest, WillChangeContents) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 30s;
        }

        .willchangecontents {
            will-change: contents;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation = GetFirstAnimation(element);

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  // Set will-change: contents. In this case, the paint status should switch to
  // kNotComposited during pre-paint.

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation willchangecontents"));

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);
  // Will-change: repaint updates paint properties
  EXPECT_TRUE(element->GetLayoutObject()->NeedsPaintPropertyUpdate());

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ false, CompositedPaintStatus::kNotComposited,
      element, animation);
}

// Test that the special animation restart for percent translate animations does
// not trigger lifecycle issues for cc clippaths.
TEST_F(ClipPathPaintDefinitionTest, ChangeDimensionPecentTranslateAnim) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
                transform: translate(10%, 10%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
                transform: translate(20%, 20%);
            }
        }
        .animation {
            animation: clippath 30s;
        }
        .oldsize {
            width: 100px;
            height: 100px;
        }
        .newsize {
            width: 125px;
            height: 125px;
        }
        #target {
            transform: translate(1%, 1%);
        }
    </style>
    <div id="target" class="oldsize">
    </div>
  )HTML");
  InitPaintArtifactCompositor();
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("target");
  // Init animation with clip-path and a translate.

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation oldsize"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element);

  Animation* animation =
      GetFirstAnimationForProperty(element, GetCSSPropertyClipPath());

  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(0));
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      /* needs_repaint= */ true, CompositedPaintStatus::kComposited, element,
      animation);

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation newsize"));

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      /* needs_repaint= */ false, CompositedPaintStatus::kComposited, element,
      animation, /* override_scheduled_animation= */ true);
}

}  // namespace blink
