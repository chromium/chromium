// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/clip_path_paint_definition.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
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
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace cc {
class AnimationEvents : public MutatorEvents {};
}  // namespace cc

namespace blink {

enum UpdatesNeededForNextFrame {
  kNoMainFrameUpdates = 0,
  kPaintStatusReset = 1 << 0,
  kNeedsPaintPropertyUpdate = 1 << 2,
  kScheduledAnimationUpdate = 1 << 3,
  kAllUpdates =
      kPaintStatusReset | kNeedsPaintPropertyUpdate | kScheduledAnimationUpdate
};

using CompositedPaintStatus = ElementAnimations::CompositedPaintStatus;

class MockChromeClientWithAnimationHost : public EmptyChromeClient {
 public:
  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta delay,
                         bool) override {
    has_scheduled_animation_ = true;
  }

  bool HasScheduledAnimation() { return has_scheduled_animation_; }

  void ResetMocks() { has_scheduled_animation_ = false; }

  cc::AnimationHost* GetCompositorAnimationHost(LocalFrame&) const override {
    return this->animation_host_.get();
  }

  void SetCompositorAnimationHost(cc::AnimationHost* host) {
    this->animation_host_ = host;
  }

  void TearDown() { animation_host_ = nullptr; }

 private:
  bool has_scheduled_animation_ = false;
  raw_ptr<cc::AnimationHost> animation_host_;
};

class ClipPathPaintDefinitionTest : public PageTestBase {
 public:
  ClipPathPaintDefinitionTest() = default;
  ~ClipPathPaintDefinitionTest() override = default;

  MockChromeClientWithAnimationHost* Client() { return chrome_client_; }

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
      CompositedPaintStatus status,
      Element* element,
      UpdatesNeededForNextFrame updates) {
    GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
        DocumentUpdateReason::kTest);

    LayoutObject* lo = element->GetLayoutObject();

    // Changes to a compositable animation should set NeedsPaintPropertyUpdate.
    EXPECT_EQ(lo->NeedsPaintPropertyUpdate(),
              !!(updates & kNeedsPaintPropertyUpdate));
    // Changes to a compositable animation should set kNeedsRepaint.
    EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
              (!(updates & kPaintStatusReset) ||
               status == CompositedPaintStatus::kNoAnimation)
                  ? status
                  : CompositedPaintStatus::kNeedsRepaint);
  }

  void EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus status,
      Element* element,
      Animation* animation,
      UpdatesNeededForNextFrame updates) {
    LayoutObject* lo = element->GetLayoutObject();
    EXPECT_EQ(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kCompositingInputsClean);

    GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kTest);

    // Composited paint status should be resolved by this point.
    EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
              status);

    UpdateAllLifecyclePhasesForTest();

    switch (status) {
      case CompositedPaintStatus::kNoAnimation:
      case CompositedPaintStatus::kNotComposited:
        // GetAnimationIfCompositable should return nothing in this
        // circumstance.
        EXPECT_EQ(ClipPathClipper::GetClipPathAnimation(*lo), nullptr);
        // If a clip path is non-composited or non-existent, then the clip path
        // mask should not be set. If it is, it can cause a crash.
        EXPECT_TRUE(!lo->FirstFragment().PaintProperties() ||
                    !lo->FirstFragment().PaintProperties()->ClipPathMask());
        // Non-composited animations SHOULD still be causing animation updates.
        // Additionally, style/layout code seems to trigger animation update for
        // the first frame after an animation cancel.
        EXPECT_EQ(!!(updates & kScheduledAnimationUpdate),
                  Client()->HasScheduledAnimation());
        break;
      case CompositedPaintStatus::kComposited:
        // GetAnimationIfCompositable should return the given animation, if it
        // is compositable.
        EXPECT_EQ(ClipPathClipper::GetClipPathAnimation(*lo), animation);
        // Composited clip-path animations depend on ClipPathMask() being set.
        EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
        // Composited clip-path animations shouldn't cause further animation
        // updates after the first paint.
        EXPECT_EQ(!!(updates & kScheduledAnimationUpdate),
                  Client()->HasScheduledAnimation());
        break;
      case CompositedPaintStatus::kNeedsRepaint:
        // kNeedsRepaint is only valid before pre-paint has been run.
        NOTREACHED();
    }
  }

  int ExpectNoFallbackForAnimatedElement(Element* element, int init_time_ms) {
    InitPaintArtifactCompositor();
    UpdateAndAdvanceTimeTo(init_time_ms);

    EnsureCCClipPathInvariantsHoldStyleAndLayout(
        CompositedPaintStatus::kComposited, element,
        UpdatesNeededForNextFrame::kAllUpdates);

    Animation* animation = GetFirstAnimation(element);

    EnsureCCClipPathInvariantsHoldThroughoutPainting(
        CompositedPaintStatus::kComposited, element, animation,
        UpdatesNeededForNextFrame::kAllUpdates);

    StartAllWaitingAnimationsOnCompositor(element, 0);

    // Tick the animation in order to ensure that the animation has an
    // opportunity to create a style change.
    UpdateAndAdvanceTimeTo(init_time_ms + 1000);

    // Run lifecycle once more to ensure invariants hold post initial paint.
    EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
        CompositedPaintStatus::kComposited, element, animation,
        UpdatesNeededForNextFrame::kNoMainFrameUpdates);

    return init_time_ms + 1000;
  }

  void EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus status,
      Element* element,
      Animation* animation,
      UpdatesNeededForNextFrame updates) {
    Client()->ResetMocks();

    EnsureCCClipPathInvariantsHoldStyleAndLayout(status, element, updates);
    EnsureCCClipPathInvariantsHoldThroughoutPainting(status, element, animation,
                                                     updates);
  }

  void UpdateAndAdvanceTimeTo(int ms) {
    GetDocument().GetPage()->Animator().ServiceScriptedAnimations(
        base::TimeTicks() + base::Milliseconds(ms));
  }

  // This creates and applies mutator events as would happen if there were a
  // real cc thread, ensuring NotifyCompositorAnimationStarted is called.
  void StartAllWaitingAnimationsOnCompositor(Element* element, int ms) {
    std::unique_ptr<cc::MutatorEvents> events =
        layer_tree_->animation_host()->CreateEvents();

    for (const auto& animation :
         element->GetElementAnimations()->Animations()) {
      if (animation.key->HasActiveAnimationsOnCompositor() &&
          !animation.key->StartTimeInternal()) {
        cc::Animation* cc_anim =
            animation.key->GetCompositorAnimation()->CcAnimation();
        cc_anim->Tick(base::TimeTicks() + base::Milliseconds(ms));
        cc_anim->UpdateState(true,
                             static_cast<cc::AnimationEvents*>(events.get()));
      }
    }

    layer_tree_->layer_tree_host()->ApplyMutatorEvents(std::move(events));
  }

  // Some animations require the paint artifact compositor's update flag to be
  // correctly cleared. This ensures that the Paint Artifact Compositor has a
  // LayerTreeHost so it will run its normal logic.
  void InitPaintArtifactCompositor() {
    layer_tree_->layer_tree_host()->SetRootLayer(
        GetDocument().View()->GetPaintArtifactCompositor()->RootLayer());
  }

 protected:
  void SetUp() override {
    scoped_composite_clip_path_animation =
        std::make_unique<ScopedCompositeClipPathAnimationForTest>(true);
    scoped_composite_bgcolor_animation =
        std::make_unique<ScopedCompositeBGColorAnimationForTest>(false);

    layer_tree_ = std::make_unique<LayerTreeHostEmbedder>();
    chrome_client_ = MakeGarbageCollected<MockChromeClientWithAnimationHost>();
    chrome_client_->SetCompositorAnimationHost(layer_tree_->animation_host());

    PageTestBase::SetupPageWithClients(chrome_client_);

    GetDocument().GetSettings()->SetAcceleratedCompositingEnabled(true);
    GetDocument().Timeline().ResetForTesting();
  }

  void TearDown() override { chrome_client_->TearDown(); }

 private:
  std::unique_ptr<ScopedCompositeClipPathAnimationForTest>
      scoped_composite_clip_path_animation;
  std::unique_ptr<ScopedCompositeBGColorAnimationForTest>
      scoped_composite_bgcolor_animation;

  Persistent<MockChromeClientWithAnimationHost> chrome_client_;
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
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  ExpectNoFallbackForAnimatedElement(element, 0);
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
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  ExpectNoFallbackForAnimatedElement(element, 0);
}

TEST_F(ClipPathPaintDefinitionTest, ClipPathNoneNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: none;
            }
            100% {
                clip-path: circle(50% at 50% 50%);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  ExpectNoFallbackForAnimatedElement(element, 0);
}

TEST_F(ClipPathPaintDefinitionTest, ClipCalcNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
            }
            100% {
                clip-path: circle(calc(2em + 2%) at 50% 50%);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  ExpectNoFallbackForAnimatedElement(element, 0);
}

TEST_F(ClipPathPaintDefinitionTest, ClipNoneNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: none;
            }
            100% {
                clip-path: circle(50% at 50% 50%);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  ExpectNoFallbackForAnimatedElement(element, 0);
}

TEST_F(ClipPathPaintDefinitionTest, ClipDelayNotFallback) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: none;
            }
            100% {
                clip-path: circle(50% at 50% 50%);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end) 0.5s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  ExpectNoFallbackForAnimatedElement(element, 0);
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
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  StartAllWaitingAnimationsOnCompositor(element, 0);

  // Advance animation to the 3rd frame.
  UpdateAndAdvanceTimeTo(2000 + 1);

  // As usual, one expects no updates from composited animations.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kNoMainFrameUpdates);

  // Reverse the animation.
  animation->updatePlaybackRate(-1);

  // Run lifecycle once more: animation should still be composited. Because it's
  // the same animation, it shouldn't schedule an animation update.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kPaintStatusReset |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate));

  // Advance animation back (since it is reversed) to the 2nd frame.
  UpdateAndAdvanceTimeTo(2000 + 1 + 1000 + 2);

  // Run lifecycle once more: repaints should be avoided even with negative
  // playback rate.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kNoMainFrameUpdates);
}

// Clip-path: initial is not composited and must fall back to main thread.
TEST_F(ClipPathPaintDefinitionTest, FallbackForClipPathInital) {
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
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kNotComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Advance the animation time.
  UpdateAndAdvanceTimeTo(500);

  // Animation should not be updating the composited paint status, but we do
  // expect scheduled animation updates since the main thread is responsible for
  // the animation.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  // Advance the animation time to the next meaningful frame.
  UpdateAndAdvanceTimeTo(2000 + 1);

  // Main frame should still be producing frames.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kScheduledAnimationUpdate |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate));
}

// Clip-path: none requires the cull rect, but perspective makes the cull rect
// infinite, as a result, we must fall back in this case.
TEST_F(ClipPathPaintDefinitionTest, FallbackForClipPathNoneWithPerspective) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(10% at 30% 30%);
            }
            100% {
                clip-path: none;
            }
        }
        .animation {
            animation: clippath 4s infinite;
        }
    </style>
    <div style="transform: perspective(200px);">
        <div id ="target" style="width: 100px; height: 100px;">
        </div>
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kNotComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Advance the animation time.
  UpdateAndAdvanceTimeTo(500);

  // Animation should not be updating the composited paint status, but we do
  // expect scheduled animation updates since the main thread is responsible for
  // the animation.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  // Advance the animation time to the next meaningful frame.
  UpdateAndAdvanceTimeTo(2000 + 1);

  // Main frame should still be producing frames.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kScheduledAnimationUpdate |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate));
}

// <br> cannot be composited due to it not supporting paint properties, so we
// fall back in this case. See crbug.com/401076540.
TEST_F(ClipPathPaintDefinitionTest, SimpleClipPathAnimationFallbackOnBR) {
  SetBodyInnerHTML(R"HTML(
      <style>
          @keyframes clippath {
              0% {
                  clip-path: circle(30% at 30% 30%);
              }
              100% {
                  clip-path: circle(50% at 50% 50%);
              }
          }
          .animation br {
              animation: clippath 4s steps(4, jump-end);
          }
      </style>
      <div id="container">
        <br id="target">
      </div>
    )HTML");

  Element* container = GetElementById("container");
  Element* element = GetElementById("target");
  container->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kNotComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  UpdateAndAdvanceTimeTo(500);

  // Animation should still run, but the composited paint status should not
  // change.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);
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
            animation: clippath 4s steps(4, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  StartAllWaitingAnimationsOnCompositor(element, 0);

  // New frames should not produce updates.
  UpdateAndAdvanceTimeTo(1001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kNoMainFrameUpdates);

  animation->cancel();

  // Cancelling the animation should reset status and the clippath properties.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNoAnimation, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);
}

// Clip-path animations with descendant transform animations must fall back to
// main thread due to difficulty determining animation bounds.
TEST_F(ClipPathPaintDefinitionTest,
       FallbackWithNoneKeyframeAndChildTransformAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(30% at 30% 30%);
            }
            100% {
                clip-path: none;
            }
        }
        @keyframes transform {
            0% {
                transform: translateX(0px);
            }
            100% {
                transform: translateX(100px);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
        .child-animation {
            animation: transform 4s;
        }
    </style>
    <div id="target" style="width: 100px; height: 100px;">
      <div id="child" style="width: 50px; height: 50px;">
      </div>
    </div>
  )HTML");
  InitPaintArtifactCompositor();

  Element* element = GetElementById("target");
  Element* child = GetElementById("child");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));
  child->setAttribute(html_names::kClassAttr, AtomicString("child-animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kNotComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  UpdateAndAdvanceTimeTo(500);

  // Animation should still run, but the composited paint status should not
  // change due to the child transform animation.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);
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
            animation: clippath 4s steps(4, jump-end);
        }
        .animation2 {
            animation: clippath 4s steps(4, jump-end), clippath 8s steps(8, jump-end);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  StartAllWaitingAnimationsOnCompositor(element, 0);
  UpdateAndAdvanceTimeTo(300);

  // Adding a 2nd clip path animation is non-compositable.

  element->setAttribute(html_names::kClassAttr, AtomicString("animation2"));

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  UpdateAndAdvanceTimeTo(700);

  // Animation(s) should still run and produce frames, but the composited paint
  // status should not change.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  UpdateAndAdvanceTimeTo(700 + 1001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kScheduledAnimationUpdate |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate));
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
        animation: transform 4s;
      }
      .animation:after {
        animation: clippath 4s steps(4, jump-end);
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

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element_pseudo,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element_pseudo);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element_pseudo, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  StartAllWaitingAnimationsOnCompositor(element, 0);
  UpdateAndAdvanceTimeTo(500);

  // Re-run to ensure composited clip path status is not being reset. Note that
  // we allow for an animation update to be scheduled here, due to the transform
  // animation.

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element_pseudo, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);
}

// Setting will-change: contents should force a fallback, even if an animation
// is already running.
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
            animation: clippath 4s steps(4, jump-end);
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

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation = GetFirstAnimation(element);

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  StartAllWaitingAnimationsOnCompositor(element, 0);
  UpdateAndAdvanceTimeTo(500);

  // Set will-change: contents. In this case, the paint status should switch to
  // kNotComposited during pre-paint.

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation willchangecontents"));

  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);

  // Will-change: repaint updates paint properties.
  EXPECT_TRUE(element->GetLayoutObject()->NeedsPaintPropertyUpdate());

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  // Expect animation to continue to work as expected.
  UpdateAndAdvanceTimeTo(1001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kScheduledAnimationUpdate |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate));
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
            animation: clippath 4s steps(4, jump-end);
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

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  Element* element = GetElementById("target");
  // Init animation with clip-path and a translate.

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation oldsize"));

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);

  Animation* animation =
      GetFirstAnimationForProperty(element, GetCSSPropertyClipPath());

  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  StartAllWaitingAnimationsOnCompositor(element, 0);

  // These animations should play fine together, only animation updates from the
  // transform are expected.
  UpdateAndAdvanceTimeTo(1001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  // A new size should trigger an animation update.
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation newsize"));

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);
}

// Test the case where a transition retarget may result in the paint status not
// being properly reset.
TEST_F(ClipPathPaintDefinitionTest, TransitionRetarget) {
  SetBodyInnerHTML(R"HTML(
    <style>
        #target {
            transition: 1s clip-path ease-in-out;
            clip-path: circle(25% at 50% 50%);
        }

        #target.transition {
             clip-path: circle(50% at 50% 50%);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("transition"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  // Ensure transition starts as normal.
  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);
  Animation* animation = GetFirstAnimation(element);
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Simulate the animation being started on compositor, so that the animation
  // receives a start time.
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0));

  // Advance some time in the transition so that cancelling it will require
  // reversing it.
  UpdateAndAdvanceTimeTo(500);
  UpdateAllLifecyclePhasesForTest();

  // Cancel the transition by resetting the style.
  element->setAttribute(html_names::kClassAttr, AtomicString(""));

  // Run all lifecycle phases except paint. This should trigger a transition
  // retarget, and resolve the clip path status of this brand new ransition as
  // kComposited, as clip-path status is resolved early in pre-paint.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  // Transition retargeting will set a start time, but will still allow
  // compositing.
  animation = GetFirstAnimationForProperty(element, GetCSSPropertyClipPath());
  EXPECT_TRUE(animation->StartTimeInternal().has_value());
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kComposited);

  // Simulate another animation update that finishes the transition before
  // PreCommit has been run the first time.
  UpdateAndAdvanceTimeTo(1000);

  // Even though the newly-finished transition was never started on compositor,
  // the completion of it should trigger a status reset.
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kNeedsRepaint);

  // Update the lifecycle, at this point, pre-commit should run, but there's
  // nothing to start on the compositor because the transition is already
  // finished. By the end of this call, all strong references to the transition
  // will be gone.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
            CompositedPaintStatus::kNotComposited);

  // Ensure the transition is garbage collected.
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Force paint invalidation and run lifecycle to ensure no CHECK failures or
  // other crashes occur during painting, even though the transition has been
  // removed from memory.
  element->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(ClipPathPaintDefinitionTest, BoundingRectCorrectForSimpleKeyframeUnion) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(20% at 20% 20%);
            }
            100% {
                clip-path: circle(20% at 70% 70%);
            }
        }
        .animation {
            animation: clippath 4s;
        }
    </style>
    <div id ="target" style="width: 200px; height: 200px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init
  UpdateAndAdvanceTimeTo(0);
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);

  const LayoutObject* layout_object = element->GetLayoutObject();

  gfx::RectF reference_box = ClipPathClipper::CalcLocalReferenceBox(
      *layout_object, ClipPathOperation::OperationType::kShape,
      GeometryBox::kBorderBox);

  // Keyframe 0: circle(20% at 20% 20%)
  BasicShapeCircle* circle1 = MakeGarbageCollected<BasicShapeCircle>();
  circle1->SetCenterX(BasicShapeCenterCoordinate(
      BasicShapeCenterCoordinate::kTopLeft, Length::Percent(20.0f)));
  circle1->SetCenterY(BasicShapeCenterCoordinate(
      BasicShapeCenterCoordinate::kTopLeft, Length::Percent(20.0f)));
  circle1->SetRadius(BasicShapeRadius(Length::Percent(20.0f)));

  // Keyframe 100: circle(20% at 70% 70%)
  BasicShapeCircle* circle2 = MakeGarbageCollected<BasicShapeCircle>();
  circle2->SetCenterX(BasicShapeCenterCoordinate(
      BasicShapeCenterCoordinate::kTopLeft, Length::Percent(70.0f)));
  circle2->SetCenterY(BasicShapeCenterCoordinate(
      BasicShapeCenterCoordinate::kTopLeft, Length::Percent(70.0f)));
  circle2->SetRadius(BasicShapeRadius(Length::Percent(20.0f)));

  // Get bounding rects from the generated paths
  gfx::RectF bounds1 =
      circle1->GetPath(reference_box, 1.f /* zoom */, 1.f /* scale */)
          .BoundingRect();
  gfx::RectF bounds2 =
      circle2->GetPath(reference_box, 1.f /* zoom */, 1.f /* scale */)
          .BoundingRect();

  // Compute union of both bounding rects to get animation bounds
  gfx::RectF animation_bounds = bounds1;
  animation_bounds.Union(bounds2);

  // Test that we can access the animation generator for comparison
  ClipPathPaintImageGenerator* generator =
      layout_object->GetFrame()->GetClipPathPaintImageGenerator();

  std::optional<gfx::RectF> generator_bounds =
      generator->GetAnimationBoundingRect(*layout_object);

  EXPECT_TRUE(generator_bounds.has_value());
  EXPECT_TRUE(generator_bounds->Contains(animation_bounds));
}

TEST_F(ClipPathPaintDefinitionTest, BoundingRectCorrectForExtrapolation) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: inset(10% 10%);
            }
            100% {
                clip-path: inset(0% 0%);
            }
        }
        .animation {
            animation: clippath 4s cubic-bezier(0,10,1,10);
        }
    </style>
    <div id ="target" style="width: 200px; height: 200px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init
  UpdateAndAdvanceTimeTo(0);
  GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kTest);

  const LayoutObject* layout_object = element->GetLayoutObject();

  // The animation bounds should encompass the entire extrapolated range
  // From original rect (0,0,200,200) to maximally extrapolated position
  // cubic-bezier(0,10,1,10) has a maximum value just higher than 7.629
  // Extrapolation factor: 7.629 - 1 = 6.629
  // Keyframe difference: inset(10% 10%) - inset(0% 0%) = 10% on each side
  // With 200px dimensions: 10% = 20px per side
  // Rect dimensions will be 200px + 20px * 6.629 for both sides, with positions
  // being (width - 200)/2, (height - 200)/2

  gfx::RectF expected_bounds(-10.f * 6.629f, -10.f * 6.629f,
                             200.f + (20.f * 6.629f), 200.f + (20.f * 6.629f));

  // Test that we can access the animation generator for comparison
  ClipPathPaintImageGenerator* generator =
      layout_object->GetFrame()->GetClipPathPaintImageGenerator();

  std::optional<gfx::RectF> generator_bounds =
      generator->GetAnimationBoundingRect(*layout_object);

  EXPECT_TRUE(generator_bounds.has_value());
  EXPECT_TRUE(generator_bounds->Contains(expected_bounds));
}

}  // namespace blink
