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

// Enum for various updates that can occur during a document lifecycle run. Each
// bit represents an independent bit of state that could change relevant to a
// running clip-path animation/transition.
enum UpdatesNeededForNextFrame {
  kNoMainFrameUpdates = 0,

  // Means that status before pre-paint will be kNeedsRepaint. At the moment,
  // this does not necessarily verify that RecalcCompositedStatus was actually
  // called, only that the state is marked for repainting.
  kPaintStatusReset = 1 << 0,
  kNeedsPaintPropertyUpdate = 1 << 2,

  // Checks whether ChromeClient::ScheduleAnimation is called. This is used to
  // determined whether an animation is causing new main frames or not.
  kScheduledAnimationUpdate = 1 << 3,

  // When true, checks for !DisplayItemClient::IsValid(). When that check is
  // true, the animation's owning PaintLayer will also be marked for repaint
  // (not explicitly checked), and so will the cached paint result of the
  // ClipPathMask. This should always be good enough to ensure a new
  // PaintWorkletDeferredImage is actually painted, though this is not
  // explicitly checked.
  kPaintInvalidated = 1 << 4,

  // Used in the case where an animation is running on main thread but does not
  // result in style mutation (e.g., because the two nearest keyfranes are
  // equal, the animation is discrete, or because the animation is using a step
  // timing function)
  kMainThreadAnimationFrameNoInvalidation = kScheduledAnimationUpdate,

  // The above, but the animation *did* mutate style.
  kMainThreadAnimationFrame = kMainThreadAnimationFrameNoInvalidation |
                              kNeedsPaintPropertyUpdate | kPaintInvalidated,

  // When a clip path animation is set pending, we expect a 'full-fat' update
  // where everything is dirtied.
  kAllUpdates = kMainThreadAnimationFrame | kPaintStatusReset,

  // Used in the case where there's a paint property change on a running cc
  // animation.
  kMainThreadPropertyInvalidation =
      kNeedsPaintPropertyUpdate | kPaintInvalidated
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

// TODO(clchambers): This should probably be subclassed at some point from
// ObjectInvalidatorTest, since it has most of the machinery we use. Either the
// animation-specific code can be added to RenderingTestChromeClient, or instead
// we can compromise and just call PendingAnimations more directly since it
// should be the same thing. Be sure to cleanup  the friend class decl in
// DisplayItemClient when this is done.
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

  // The next 4 methods are easy short-cuts for checking various cc clip path
  // invariants are held throughout the lifecycle, to make this file a bit more
  // readable at the cost of problem location in the test being 2-3 frames down
  // from the top of a stack trace. Right now, we check for animation updates,
  // composited paint status updates (naive checking for kNeedsRepaint), paint
  // property updates, and paint updates. View the code of these methods for a
  // description of what those invariants are and why they are enforced.

  void EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus status,
      Element* element,
      UpdatesNeededForNextFrame updates) {
    GetDocument().View()->UpdateLifecycleToCompositingInputsClean(
        DocumentUpdateReason::kTest);

    LayoutObject* lo = element->GetLayoutObject();

    if (status != CompositedPaintStatus::kNoAnimation &&
        status != CompositedPaintStatus::kNotComposited) {
      // PaintLayer is required to paint a mask clip path.
      EXPECT_TRUE(lo->StyleRef().HasCurrentClipPathAnimation());
      EXPECT_TRUE(lo->HasLayer());
    }

    // Changes to a compositable clip-path animation should set
    // NeedsPaintPropertyUpdate. This is because we force a switch from
    // ClipPathClip (ordinary clipping) to ClipPathMask with its associated
    // ClipPathMaskEffect (SVG clipping). If this is not done, bad things
    // happen.
    EXPECT_EQ(lo->NeedsPaintPropertyUpdate(),
              !!(updates & kNeedsPaintPropertyUpdate));

    // Changes to a compositable animation should set kNeedsRepaint. This is
    // because for clip-path animations, the status value is cached to avoid
    // repeatedly calling the heavy check CheckCanStartAnimationOnCompositor in
    // the pre-paint tree walk, which can be very frequent and will occur during
    // hit tests. Note that this behavior, like the above, is not universal for
    // NPW animations. background-color will recompute its status at paint time
    // and this is not necessarily an issue because full paint invalidation on
    // an animated element should be fairly infrequent. Note that the composited
    // paint status can even be checked during the pre-paint tree walk even if
    // NeedsPaintPropertyUpdate is value, because InitPaintProperties checks the
    // status to ensure paint properties are populated for cc clip paths even
    // when they wouldn't ordinarily be needed.
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

    // Check the element's DisplayItemClient for invalidation - if we're
    // expecting an repaint, then IsValid() will be false. For a composited clip
    // path animation, this means that a new PaintWorkletDeferredImage is
    // created (or removed, for a fallback), which is necessary to ensure that
    // the latest keyframes are on the compositor and that we don't get stale
    // paint.
    EXPECT_EQ(static_cast<DisplayItemClient*>(lo)->IsValid(),
              !(updates & kPaintInvalidated));

    // Composited paint status should be resolved by this point. If it hasn't
    // been, that means paint properties haven't been updated.
    EXPECT_EQ(element->GetElementAnimations()->CompositedClipPathStatus(),
              status);

    // Various non-animation things can set scheduled animation updates. Reset
    // mocks here as we're interested about scheduling animation service
    // post-paint.
    Client()->ResetMocks();

    UpdateAllLifecyclePhasesForTest();

    switch (status) {
      case CompositedPaintStatus::kNoAnimation:
      case CompositedPaintStatus::kNotComposited:
        // For a fallback, the cached clip path animation candidate should be
        // cleared so we don't hold on to stale references. This also means that
        // the presence of a valid aniamtion in paint is a guarantee that we're
        // in the composited path.
        EXPECT_EQ(ClipPathClipper::GetClipPathAnimation(*lo), nullptr);
        // If a clip path is non-composited or non-existent, then the clip path
        // mask should not be set. If it is, it can cause a crash. Note that
        // this is not necessarily true, SVG clips can set this without a cc
        // clip path animation, which is not tested here.
        EXPECT_TRUE(!lo->FirstFragment().PaintProperties() ||
                    !lo->FirstFragment().PaintProperties()->ClipPathMask());
        // Non-composited animations SHOULD still be causing animation updates.
        // Additionally, style/layout code seems to trigger animation update for
        // the first frame after an animation cancel. Too few updates means a
        // fallback may not have been done properly (ie, paint is stuck.)
        EXPECT_EQ(!!(updates & kScheduledAnimationUpdate),
                  Client()->HasScheduledAnimation());
        // If the animation is still running on cc, it means that something went
        // wrong with a fallback. kNotComposited should always be coincident
        // with a pending cancel.
        EXPECT_FALSE(animation->HasActiveAnimationsOnCompositor());
        break;
      case CompositedPaintStatus::kComposited:
        // A compositable animation should always be cached in
        // ElementAnimations. We do this primarily because finding it every time
        // is an unnecessary expense. It requires walking through all keyframe
        // effects associated with an element until an animation that mutates
        // clip-paths is found.
        EXPECT_EQ(ClipPathClipper::GetClipPathAnimation(*lo), animation);
        // Composited clip-path animations depend on ClipPathMask() being set.
        // If this is not true, the animation will have no output (no
        // PaintWorkletDeferredImage for CC to update).
        EXPECT_TRUE(lo->FirstFragment().PaintProperties()->ClipPathMask());
        // Composited clip-path animations shouldn't cause further animation
        // updates after the first paint. Too many animation updates
        // mean that we're causing too many unnecessary main frames, undermining
        // perf.
        EXPECT_EQ(!!(updates & kScheduledAnimationUpdate),
                  Client()->HasScheduledAnimation());
        // The animation should be have been set up for compositing during
        // PreCommit.
        EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
        break;
      case CompositedPaintStatus::kNeedsRepaint:
        // kNeedsRepaint is only valid before pre-paint has been run.
        NOTREACHED();
    }

    // Clear paint invalidation reasons
    static_cast<DisplayItemClient*>(lo)->Validate();
  }

  // Given an element with a *CSS* defined compositable clip-path animation with
  // no disqualifiers, start the animation and advance to the first style change
  // to ensure all invariants hold. The animation pointer is returned for
  // further testing.
  Animation* StartAndVerifyEligibleClipPathAnimation(
      Element* element,
      int time_to_first_style_change_ms) {
    // Set up timing + PAC so that animation servicing works as expected.
    InitPaintArtifactCompositor();
    UpdateAndAdvanceTimeTo(0);

    // A CSS animation object won't be created until the lifecycle runs for the
    // first time. Because we need the animation object to verify invariants, we
    // run only style and layout first.
    EnsureCCClipPathInvariantsHoldStyleAndLayout(
        CompositedPaintStatus::kComposited, element,
        UpdatesNeededForNextFrame::kAllUpdates);

    // With the animation object created, we can then run paint and verify
    // animation-specific invariants, such as HasActiveAnimationsOnCompositor.
    Animation* animation = GetFirstAnimation(element);
    EnsureCCClipPathInvariantsHoldThroughoutPainting(
        CompositedPaintStatus::kComposited, element, animation,
        UpdatesNeededForNextFrame::kAllUpdates);
    StartAllWaitingAnimationsOnCompositor(element, 0);

    // Tick the animation before the first style change first. In cases of
    // over-invalidation, this helps differentiate whether the error relates to
    // Animation::TimeToEffectChange, or whether paint invalidation isn't being
    // properly prevented during style invalidation. Note that it is necessary
    // to check both - many things other than animations can cause lifecycle
    // updates (e.g. hit tests, javascript events, etc). Even if we're running
    // lifecycle anyway, we don't want to be causing unnecessary painting.
    UpdateAndAdvanceTimeTo(time_to_first_style_change_ms / 2);
    EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
        CompositedPaintStatus::kComposited, element, animation,
        UpdatesNeededForNextFrame::kNoMainFrameUpdates);

    // Style change, but no updates.
    UpdateAndAdvanceTimeTo(time_to_first_style_change_ms + 1);
    EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
        CompositedPaintStatus::kComposited, element, animation,
        UpdatesNeededForNextFrame::kNoMainFrameUpdates);

    return animation;
  }

  // Given an element with a *CSS* defined *non*-compositable clip-path
  // animation (due to one or more disqualifiers), start the animation and
  // advance to the first style change to ensure all invariants hold (ie, that
  // the behavior should be the same as main thread except for the extra work to
  // verify status from kNoAnimation -> kNeedsRepaint - > kNotComposited). The
  // animation pointer is returned for further testing.
  Animation* StartAndVerifyNonEligibleClipPathAnimation(
      Element* element,
      int time_to_first_style_change_ms) {
    // Set up timing + PAC so that animation servicing works as expected.
    InitPaintArtifactCompositor();
    UpdateAndAdvanceTimeTo(0);
    EnsureCCClipPathInvariantsHoldStyleAndLayout(
        CompositedPaintStatus::kNotComposited, element,
        UpdatesNeededForNextFrame::kAllUpdates);

    // Animation is not composited.
    Animation* animation = GetFirstAnimation(element);
    EnsureCCClipPathInvariantsHoldThroughoutPainting(
        CompositedPaintStatus::kNotComposited, element, animation,
        UpdatesNeededForNextFrame::kAllUpdates);

    // For the case of scheduled animation update, it may seem weird that we
    // schedule an update even when there is no keyframe change, but, if you
    // were to check the update time (we do not currently do this), for a
    // main-thread animation the update would be scheduled for the time to the
    // next timing change. The scheduled animation update here is, conceptually,
    // the same as the last one, just advanced by time_to_first_style_change_ms
    // / 2.
    UpdateAndAdvanceTimeTo(time_to_first_style_change_ms / 2);
    EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
        CompositedPaintStatus::kNotComposited, element, animation,
        UpdatesNeededForNextFrame::kMainThreadAnimationFrameNoInvalidation);

    // The style change here SHOULD invalidate both paint properties and paint.
    // Clip paths (on main thread) are accumulated as PaintOps when painting the
    // element's associated PaintLayer, and so a change to the clip means
    // re-painting and re-rasterizing the entire layer. If this does not happen,
    // it probably means something in the non-invalidation logic for clip paths
    // is too aggressive. See AdjustForCompositableAnimationPaint (note for more
    // complex scenarios, this can also happen with the clip-path paint
    // hierarchy has become inconsistent, see crbug.com/480422022. However, this
    // method is only called by during the start of tests, so if an invariant
    // breaks here, it is almost certainly in the aforementioned logic).
    UpdateAndAdvanceTimeTo(time_to_first_style_change_ms + 1);
    EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
        CompositedPaintStatus::kNotComposited, element, animation,
        UpdatesNeededForNextFrame::kMainThreadAnimationFrame);

    return animation;
  }

  void EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus status,
      Element* element,
      Animation* animation,
      UpdatesNeededForNextFrame updates) {
    EnsureCCClipPathInvariantsHoldStyleAndLayout(status, element, updates);
    EnsureCCClipPathInvariantsHoldThroughoutPainting(status, element, animation,
                                                     updates);
  }

  void UpdateAndAdvanceTimeTo(int ms, bool is_for_frame = true) {
    if (is_for_frame) {
      GetDocument().GetPage()->Animator().ServiceScriptedAnimations(
          base::TimeTicks() + base::Milliseconds(ms));
    } else {
      GetDocument().GetPage()->Animator().Clock().UpdateTime(
          base::TimeTicks() + base::Milliseconds(ms));
      GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                                   base::Milliseconds(ms));
    }
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

/* -------------------------------------------- */
/*   1. ELIGIBLE COMPOSITABLE ANIMATION TESTS   */
/* Regression tests for known-good cases        */
/* -------------------------------------------- */

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

  StartAndVerifyEligibleClipPathAnimation(element, 1000);
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

  StartAndVerifyEligibleClipPathAnimation(element, 1000);
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

  StartAndVerifyEligibleClipPathAnimation(element, 1000);
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

  StartAndVerifyEligibleClipPathAnimation(element, 1000);
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

  StartAndVerifyEligibleClipPathAnimation(element, 1000);
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

  StartAndVerifyEligibleClipPathAnimation(element, 1000);
}

/* ----------------------------------------- */
/*         ANIMATION FALLBACK TESTS          */
/* For anims that fall back from the value   */
/* filter or fail standard compositability   */
/* checks in CheckCanStart*.                 */
/* ----------------------------------------- */

// Clip-path: initial is not composited and must fall back to main thread. This
// is done because initial results in a special keyframe value that the
// ClipPathPaintDefinition doesn't know what to do with. In future, we may
// interpret this as the same as clip-path: none, which is allowed.
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
            animation: clippath 4s;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  StartAndVerifyNonEligibleClipPathAnimation(element, 2000);
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

  StartAndVerifyNonEligibleClipPathAnimation(element, 1000);
}

/* ----------------------------------------- */
/*     SPECIAL ANIMATION FALLBACK TESTS      */
/* For animation fallback outside the usual  */
/* flow of rechecking status on pending.     */
/* ----------------------------------------- */

// These cases primarily are here to prevent broken painting or stuck
// animations. Most fallback cases are either expected to set the animation
// pending (in which case, the ordinary handling is sufficient), or are not
// actually a reason an animation can't continue to run on the compositor.
// Clip-path animations have special handling to ensure cases that absolutely
// need to fall back are handled with care.

// These tests exist as pairs, one for the "normal" fallback case, and one for
// the case where the disqualifying factor is added after the animation has
// already started. This is important to test to ensure that animations that
// start composited but then have a disqualifying factor added later properly
// fall back, rather than getting stuck in a broken state. ie, that the paint
// status and the animation composinting state are updated atomically.

// TODO(clchambers): I hope one day most of these tests won't exist.
// will-change: contents doesn't even apply to NPW-based clip-path animations,
// since we don't need render surfaces. The only case we really care about is
// when a clip-path animation is shared with a transform animation (which may
// need a surface). Backdrop-filter should just be fixed, since masks already
// work with backdrop filter, this behavior was just never moved to svg clips.
// In the long term, fragmentation for clip-paths should simply be properly
// defined so we don't need to fall back (see crbug.com/40241353), but in the
// short term, it needs to be handled better than having a code block in the
// middle of the pre-paint tree walk. See crbug.com/488268869. Perspective
// transforms / child transform anims with clip-path: none will always be an
// issue until/unless this feature is completely rewritten to clip differently
// however, for various perf reasons. (ie, we don't want to allocate unbounded
// mask tiles on cc). Though I envision a better way to handle that case.

// TODO(crbug.com/449152897): Backdrop-filter and clip path paint worklet
// images are not rasterized correctly. We fall back in this case to prevent
// broken painting.
TEST_F(ClipPathPaintDefinitionTest, FallbackForCoincidentBackdropFilter) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(30% at 20% 20%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
        .bdfilter {
            backdrop-filter: invert(1);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation bdfilter"));

  StartAndVerifyNonEligibleClipPathAnimation(element, 1000);
}

// This is a variation of the above test. The backdrop-filter is added later,
// rather than immediately. This ensures the animation is still functioning
// properly in this case.
TEST_F(ClipPathPaintDefinitionTest, FallbackForLateBackdropFilter) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(30% at 20% 20%);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
        .bdfilter {
            backdrop-filter: invert(1);
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  // Advance the animation time and add a backdrop-filter.
  UpdateAndAdvanceTimeTo(1250);
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation bdfilter"));

  // Next main frame should proceed ordinarily
  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kMainThreadPropertyInvalidation);

  // However, the animation will fall back during pre-paint.
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Animation should still producve frames on main as normal
  UpdateAndAdvanceTimeTo(2001);

  // Main thread should still be producing frames.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kMainThreadAnimationFrame);
}

// When clip-path: none exists as part of an animation, we use the cull rect to
// constrain the animation bounds. This is done for perf reasons. However, with
// a perspective transform, the CullRectUpdater will early-out as it can't
// estimate the maximum painting bounds. Because of this, we have to fall back -
// and we have to do it before the CullRectUpdater even runs.
TEST_F(ClipPathPaintDefinitionTest, FallbackForClipPathNoneWithPerspective) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(10% at 30% 30%);
            }
            75% {
                clip-path: circle(30% at 30% 30%);
            }
            100% {
                clip-path: none;
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
        .perspectivetf {
            transform: perspective(200px);
        }
    </style>
    <div id="parent">
        <div id="target" style="width: 100px; height: 100px;">
        </div>
    </div>
  )HTML");

  Element* parent = GetElementById("parent");
  Element* element = GetElementById("target");
  parent->setAttribute(html_names::kClassAttr, AtomicString("perspectivetf"));
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  StartAndVerifyNonEligibleClipPathAnimation(element, 1000);
}

// This is a variation of the above test. The perspective transform is added
// later, rather than immediately. This ensures the animation is still
// functioning properly in this case.
TEST_F(ClipPathPaintDefinitionTest,
       FallbackForClipPathNoneWithDelayedPerspective) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(10% at 30% 30%);
            }
            75% {
                clip-path: circle(30% at 30% 30%);
            }
            100% {
                clip-path: none;
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
        .perspectivetf {
            transform: perspective(200px);
        }
    </style>
    <div id="parent">
        <div id="target" style="width: 100px; height: 100px;">
        </div>
    </div>
  )HTML");

  Element* parent = GetElementById("parent");
  Element* element = GetElementById("target");

  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  // Advance the animation time and add the perspective transform to the parent.
  UpdateAndAdvanceTimeTo(1250);
  parent->setAttribute(html_names::kClassAttr, AtomicString("perspectivetf"));

  // Next main frame should proceed ordinarily
  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kMainThreadPropertyInvalidation);

  // However, the animation will fall back during pre-paint.
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Animation should still producve frames on main as normal
  UpdateAndAdvanceTimeTo(2001);

  // Main thread should still be producing frames.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kMainThreadAnimationFrame);
}

// Same as the above - with a descendant transform animation, the
// CullRectUpdater can't estimate bounds, since the paint area will be updated
// on the compositor thread (in theory - you could calculate the maximum
// transformations of any given tf anim and then propagate those changes, but
// that would be extremely complex and this is not done for that reason).
// Because of this, we fall back for the same reasons.
TEST_F(ClipPathPaintDefinitionTest,
       FallbackWithNoneKeyframeAndChildTransformAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(10% at 30% 30%);
            }
            75% {
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
            animation: transform 8s steps(2, jump-end);
        }
    </style>
    <div id="target" style="width: 100px; height: 100px;">
      <div id="child" style="width: 50px; height: 50px;">
      </div>
    </div>
  )HTML");
  InitPaintArtifactCompositor();

  Element* child = GetElementById("child");
  child->setAttribute(html_names::kClassAttr, AtomicString("child-animation"));

  UpdateAllLifecyclePhasesForTest();
  StartAllWaitingAnimationsOnCompositor(child, 0);

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  Animation* animation =
      StartAndVerifyNonEligibleClipPathAnimation(element, 1000);

  UpdateAndAdvanceTimeTo(2001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kMainThreadAnimationFrame);
}

// This is a variation of the above test. The descendant transform animation is
// added later, rather than immediately. This ensures the clip-path animation
// still falls back correctly in this case.
TEST_F(ClipPathPaintDefinitionTest,
       FallbackWithNoneKeyframeAndDelayedChildTransformAnimation) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(10% at 30% 30%);
            }
            75% {
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
            animation: transform 4s steps(2, jump-end);
        }
    </style>
    <div id="target" style="width: 100px; height: 100px;">
      <div id="child" style="width: 50px; height: 50px;">
      </div>
    </div>
  )HTML");

  Element* element = GetElementById("target");
  Element* child = GetElementById("child");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  // Advance the animation time and add a descendant transform animation.
  UpdateAndAdvanceTimeTo(1250);
  child->setAttribute(html_names::kClassAttr, AtomicString("child-animation"));

  // Next main frame should proceed ordinarily.
  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kMainThreadPropertyInvalidation);

  // However, the animation will fall back during pre-paint.
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // So we get no other unexpected updates.
  StartAllWaitingAnimationsOnCompositor(child, 1250);

  // Main thread should still be producing frames.
  UpdateAndAdvanceTimeTo(2001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kMainThreadAnimationFrame);
}

// Offset-path/offset-position technically don't belong here as they do result
// in the animation being set pending commit, however, this occurs as a special
// behavior in KeyframeEffect::ApplyEffect. Offset-position on its own doesn't
// do anything, however it's easiest to test since offset-path almost always
// invalidates paint when it is added.
TEST_F(ClipPathPaintDefinitionTest, CoincidentTransformWithOffsetPosition) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
                transform: translateX(0px);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
                transform: translateX(100px);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }

        .offsetposition {
            offset-position: left top;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation offsetposition"));

  StartAndVerifyNonEligibleClipPathAnimation(element, 1000);
}

// This is a variation of the above test. will-change: contents is added later,
// rather than immediately. This ensures the animation is still functioning
// properly in this case. As implied above - this fallback could in theory be
// removed, but we keep it around for now to avoid potentially weird situations
// where will-change contents is added later and then something is done to get
// the animation to restart on the compositor (maybe a bounds change of a tf
// anim). In this case, paint status will be COMPOSITED but the failure reasons
// will not be kNoFailure, causing a stuck animation. In the future, we should
// explicitly handle this rather than hitting it with a hammer in the pre-paint
// tree walk.
TEST_F(ClipPathPaintDefinitionTest,
       CoincidentTransformDelayedWithOffsetPosition) {
  SetBodyInnerHTML(R"HTML(
    <style>
        @keyframes clippath {
            0% {
                clip-path: circle(50% at 50% 50%);
                transform: translateX(0px);
            }
            100% {
                clip-path: circle(30% at 30% 30%);
                transform: translateX(100px);
            }
        }
        .animation {
            animation: clippath 4s steps(4, jump-end);
        }
        .offsetposition {
            offset-position: left top;
        }
    </style>
    <div id ="target" style="width: 100px; height: 100px">
    </div>
  )HTML");

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  // Advance the animation time and add offset-position.
  UpdateAndAdvanceTimeTo(1250);

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation offsetposition"));

  // Nothing except the property update caused by offset-position happens until
  // the next call to KeyframeEffect::ApplyEffect
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate);

  // Next frame, the animation falls back immediately.
  UpdateAndAdvanceTimeTo(2001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);
}

// Will-change: contents is a very old disqualifier for composited animations.
// Setting up the transform/opacity/filter animations requires allocating render
// surfaces, which is expensive, and we'd like to avoid that work if the
// contents are just going to change anyway. It's unclear how this work compares
// to native paint worklet, which without a synthesized clip (something that is
// only created for effects with render surface reasons), do not create a
// textures at all. In the future, this may be allowed, depending on perf
// testing.
TEST_F(ClipPathPaintDefinitionTest, FallbackForWillChangeContents) {
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
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation willchangecontents"));

  StartAndVerifyNonEligibleClipPathAnimation(element, 1000);
}

// This is a variation of the above test. will-change: contents is added later,
// rather than immediately. This ensures the animation is still functioning
// properly in this case. As implied above - this fallback could in theory be
// removed, but we keep it around for now to avoid potentially weird situations
// where will-change contents is added later and then something is done to get
// the animation to restart on the compositor (maybe a bounds change of a tf
// anim). In this case, paint status will be COMPOSITED but the failure reasons
// will not be kNoFailure, causing a stuck animation. In the future, we should
// explicitly handle this rather than hitting it with a hammer in the pre-paint
// tree walk.
TEST_F(ClipPathPaintDefinitionTest, FallbackForDelayedWillChangeContents) {
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

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  // Advance the animation time and add will-change: contents.
  UpdateAndAdvanceTimeTo(1250);

  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation willchangecontents"));

  // Next main frame should proceed ordinarily.
  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kComposited, element,
      UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate);

  // However, the animation will fall back during pre-paint.
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Main thread should still be producing frames.
  UpdateAndAdvanceTimeTo(2001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kMainThreadAnimationFrame);
}

/* ----------------------------------------- */
/*       ANIMATION STATE CHANGE TESTS        */
/* For anims mutated by WAAPI, or CSS anims  */
/* directly mutated by JS                    */
/* ----------------------------------------- */

// Test the case where we reverse a clip-path animation using javascript. In
// this case, we will need to resync the animation with cc, but compositing
// status should not change except being transiently set as needing a repaint.
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

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  UpdateAndAdvanceTimeTo(2000 + 1);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kNoMainFrameUpdates);

  // Reverse the animation.
  animation->updatePlaybackRate(-1);

  // Run lifecycle once more: animation should still be composited. Because it's
  // the same animation, it shouldn't schedule an animation update. We do
  // however, create a new paint worklet here, even though it contains no new
  // information. In future, this could potentially be optimized out, but doing
  // so would be complex and could cause errors.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kPaintStatusReset |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate |
          UpdatesNeededForNextFrame::kPaintInvalidated));

  // Advance animation back (since it is reversed) to the 2nd frame.
  UpdateAndAdvanceTimeTo(2000 + 1 + 1000 + 2);

  // Run lifecycle once more: repaints should be avoided even with negative
  // playback rate.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kNoMainFrameUpdates);
}

// Cancelling a clip path animation with web animations API should properly
// clear all state.
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

  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  animation->cancel();

  // Cancelling the animation should reset status and the clippath properties.
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNoAnimation, element, animation,
      static_cast<UpdatesNeededForNextFrame>(
          UpdatesNeededForNextFrame::kPaintStatusReset |
          UpdatesNeededForNextFrame::kNeedsPaintPropertyUpdate |
          UpdatesNeededForNextFrame::kPaintInvalidated));
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
      UpdatesNeededForNextFrame::kMainThreadAnimationFrameNoInvalidation);

  UpdateAndAdvanceTimeTo(700 + 1001);
  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kMainThreadAnimationFrame);
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

  Element* element = GetElementById("target");
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation oldsize"));

  // The two properties should play fine together.
  Animation* animation = StartAndVerifyEligibleClipPathAnimation(element, 1000);

  // A new size should trigger an animation update. iT will also, necessarily,
  // invalidate paint.
  element->setAttribute(html_names::kClassAttr,
                        AtomicString("animation newsize"));

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kComposited, element, animation,
      UpdatesNeededForNextFrame::kPaintInvalidated);
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
  // retarget, and resolve the clip path status of this brand new transition as
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

// Like TransitionRetarget, except the transition runs for such short a time
// that it is cancelled within a second hit-test before the main frame.
TEST_F(ClipPathPaintDefinitionTest, TransitionRetargetVerySmallDuration) {
  SetBodyInnerHTML(R"HTML(
    <style>
        #target {
            transition: 1s clip-path ease-in-out;
            clip-path: circle(25% at 50% 50%);
        }
        #target.transition {
             clip-path: circle(50% at 50% 50%);
        }
        #irrelevant {
            width: 10px;
            height: 10px;
        }
        #irrelevant.update {
            width: 20px;
            height: 20px;
        }
    </style>
    <div id="target" style="width: 100px; height: 100px"></div>
    <div id="irrelevant"></div>
  )HTML");

  Element* element = GetElementById("target");
  Element* irrelevant = GetElementById("irrelevant");
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

  // Advance some small time such that cancelling the transition will require
  // reversing it, but small enough that the next main frame may not catch it.
  // (Since this is a test, we can control when exactly that is - this comment
  // is included mainly to to associate this test with a real-world behavior)
  UpdateAndAdvanceTimeTo(13);
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

  // Invalidate layout so that a 2nd hit test can cause an animation update.
  // See: Document::UpdateStyleAndLayoutTreeForThisDocument.
  irrelevant->setAttribute(html_names::kClassAttr, AtomicString("update"));

  // Simulate another animation update on demand. Although infrequent, it
  // sometimes occurs that there are two lifecycle updates from hit tests back
  // to back. This seems to happen more often if I mix event types, but the
  // necessary condition is that layout is invalidated as a result of the first
  // hit test.
  UpdateAndAdvanceTimeTo(26, false);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);

  // Aa status recalc + pre-paint results in kNotComposited. Because the
  // transition was not idle at the time of the status recalc, we don't get
  // kNoAnimation.
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

/* ----------------------------------------- */
/*       ANIMATION BOUNDING RECT TESTS       */
/* ----------------------------------------- */

// The animation bounding rect is the rect that contains all keyframes,
// including underlying value (for delays) and potential extrapolation (for
// complex easing). This is needed so that we won't create unbounded paint
// chunks (or mask textures) for synthesized clips, which causes perf issues.

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

// Fragmentless boxes do not get the paint property updates that clip-path
// animations depend on, however, since there is nothing to animation in
// this case, there is no issue. This test and AnimationOnTableCol mainly
// check that no status (D)CHECKs.
TEST_F(ClipPathPaintDefinitionTest, AnimationOnTableColGroup) {
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
            /* We explicitly don't go past the first step here, because
               changes in the clip-path property keep the layout object
               marked for a paint property update */
            animation: clippath 4s steps(4, jump-end);
        }
        .invalidatepaint {
            background-color: red;
        }
    </style>
    <table>
      <colgroup id="target">
        <col>
        <col>
      </colgroup>
      <tr>
        <td id="incidental"></td>
        <td></td>
      </tr>
    </table>
  )HTML");

  Element* element = GetElementById("target");
  Element* incidental = GetElementById("incidental");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  // Init clock.
  UpdateAndAdvanceTimeTo(0);

  // Fallback should occur in pre-paint, rather than leaving a persistent
  // kNeedsRepaint. Currently, this requires all updates, but in the future,
  // animations like this may be made to not tick as they can't possibly
  // have a visual output. If that''s the case, UpdatesNeededForNextFrame
  // may need to be adjusted.
  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kNotComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);
  Animation* animation = GetFirstAnimation(element);
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  // Init clock.
  UpdateAndAdvanceTimeTo(50);

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  incidental->setAttribute(html_names::kClassAttr,
                           AtomicString("invalidatepaint"));
  UpdateAndAdvanceTimeTo(100);

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);
}

// See AnimationOnTableColGroup for a description of what this tests for.
TEST_F(ClipPathPaintDefinitionTest, AnimationOnTableCol) {
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
            animation: clippath 4s steps(4, jump-end);
        }
        .invalidatepaint {
            background-color: red;
        }
    </style>
    <table>
      <colgroup id="parent">
        <col id="target">
        <col>
      </colgroup>
      <tr>
        <td></td>
        <td></td>
      </tr>
    </table>
  )HTML");

  Element* element = GetElementById("target");
  Element* parent = GetElementById("parent");
  element->setAttribute(html_names::kClassAttr, AtomicString("animation"));

  UpdateAndAdvanceTimeTo(0);

  EnsureCCClipPathInvariantsHoldStyleAndLayout(
      CompositedPaintStatus::kNotComposited, element,
      UpdatesNeededForNextFrame::kAllUpdates);
  Animation* animation = GetFirstAnimation(element);
  EnsureCCClipPathInvariantsHoldThroughoutPainting(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kAllUpdates);

  UpdateAllLifecyclePhasesForTest();

  UpdateAndAdvanceTimeTo(50);

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);

  // Invaidate the group so that the pre-paint tree walk will reach the actual
  // col.
  parent->setAttribute(html_names::kClassAttr, AtomicString("invalidatepaint"));
  UpdateAndAdvanceTimeTo(100);

  EnsureCCClipPathInvariantsHoldThroughoutLifecycle(
      CompositedPaintStatus::kNotComposited, element, animation,
      UpdatesNeededForNextFrame::kScheduledAnimationUpdate);
}

}  // namespace blink
