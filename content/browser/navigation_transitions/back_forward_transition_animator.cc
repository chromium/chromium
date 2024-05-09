// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animator.h"

#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/android/window_android.h"

namespace content {

namespace {

using NavigationDirection =
    BackForwardTransitionAnimationManager::NavigationDirection;
using SwitchSpringReason = PhysicsModel::SwitchSpringReason;

void ResetTransformForLayer(cc::slim::Layer* layer) {
  CHECK(layer);
  auto transform = layer->transform();
  transform.MakeIdentity();
  layer->SetTransform(transform);
}

void PutScreenshotBack(NavigationControllerImpl* controller,
                       std::unique_ptr<NavigationEntryScreenshot> screenshot) {
  if (auto* entry =
          controller->GetEntryWithUniqueID(screenshot->navigation_entry_id())) {
    auto* cache = controller->GetNavigationEntryScreenshotCache();
    cache->SetScreenshot(entry, std::move(screenshot));
  }
}

//========================== Fitted animation timeline =========================
//
// The animations for `OnGestureProgressed` are driven purely by user gestures.
// We use `gfx::KeyframeEffect` for progressing the animation in response by
// setting up a fitted animation timeline (one second) and mapping gesture
// progress to the corresponding time value.
//
// The timeline for the scrim animation is also a function of layer's position.
// We also use this fitted timeline for scrim.
//
// Note: The timing function is linear.

static constexpr base::TimeTicks kFittedStart;
static constexpr base::TimeDelta kFittedTimelineDuration = base::Seconds(1);

base::TimeTicks GetFittedTimeTicksForForegroundProgress(float progress) {
  return kFittedStart + kFittedTimelineDuration * progress;
}

// 0-indexed as the value will be stored in a bitset.
enum class TargetProperty {
  kScrim = 0,
  kCrossFade,
};

struct ScrimAndCrossFadeAnimaitonConfig {
  TargetProperty target_property;
  float start;
  float end;
  base::TimeDelta duration;
};

//============================= Crossfade animation ============================
static constexpr base::TimeDelta kCrossfadeDuration = base::Milliseconds(100);

static constexpr ScrimAndCrossFadeAnimaitonConfig kCrossFadeAnimation{
    .target_property = TargetProperty::kCrossFade,
    .start = 1.0f,
    .end = 0.0f,
    .duration = kCrossfadeDuration};

//=============================== Scrim animation ==============================
// The scim animations have two timelines:
// - The fist timeline for while the screenshot layer is moving across the
//   screen.
// - The second timeline while the screenshot layer is cross-fading into the
//   new content page.

static constexpr ScrimAndCrossFadeAnimaitonConfig
    kScrimAnimationDuringGestureProgress{
        .target_property = TargetProperty::kScrim,
        .start = 0.8f,
        .end = 0.3f,
        .duration = kFittedTimelineDuration};

static constexpr ScrimAndCrossFadeAnimaitonConfig
    kScrimAnimationDuringCrossFade{.target_property = TargetProperty::kScrim,
                                   .start = 0.3f,
                                   .end = 0.0f,
                                   .duration = kCrossfadeDuration};

void AddFloatModelToEffect(ScrimAndCrossFadeAnimaitonConfig config,
                           gfx::FloatAnimationCurve::Target* target,
                           gfx::KeyframeEffect& effect) {
  auto curve = gfx::KeyframedFloatAnimationCurve::Create();
  curve->AddKeyframe(gfx::FloatKeyframe::Create(/*time=*/base::TimeDelta(),
                                                /*value=*/config.start,
                                                /*timing_function=*/nullptr));
  curve->AddKeyframe(gfx::FloatKeyframe::Create(/*time=*/config.duration,
                                                /*value=*/config.end,
                                                /*timing_function=*/nullptr));
  curve->set_target(target);

  auto model = gfx::KeyframeModel::Create(
      /*curve=*/std::move(curve),
      /*keyframe_model_id=*/effect.GetNextKeyframeModelId(),
      /*target_property_id=*/
      static_cast<int>(config.target_property));

  effect.AddKeyframeModel(std::move(model));
}

}  // namespace

std::unique_ptr<BackForwardTransitionAnimator>
BackForwardTransitionAnimator::Factory::Create(
    WebContentsViewAndroid* web_contents_view_android,
    NavigationControllerImpl* controller,
    const ui::BackGestureEvent& gesture,
    NavigationDirection nav_direction,
    int destination_entry_id,
    BackForwardTransitionAnimationManagerAndroid* animation_manager) {
  return base::WrapUnique(new BackForwardTransitionAnimator(
      web_contents_view_android, controller, gesture, nav_direction,
      destination_entry_id, animation_manager));
}

BackForwardTransitionAnimator::~BackForwardTransitionAnimator() {
  WebContentsObserver::Observe(nullptr);

  ResetTransformForLayer(animation_manager_->web_contents_view_android()
                             ->parent_for_web_page_widgets());

  // TODO(crbug.com/40283503): If there is the old visual state hovering
  // above the RWHV layer, we need to remove that as well.

  if (ui_resource_layer_) {
    screenshot_scrim_->RemoveFromParent();
    screenshot_scrim_.reset();

    ui_resource_layer_->RemoveFromParent();
    ui_resource_layer_.reset();
  }

  if (old_surface_clone_) {
    old_surface_clone_->RemoveFromParent();
    old_surface_clone_.reset();
  }

  CHECK_NE(ui_resource_id_, cc::UIResourceClient::kUninitializedUIResourceId);
  RemoveWindowAndroidObserverAndDeleteUIResource(ui_resource_id_);

  if (navigation_state_ != NavigationState::kCommitted) {
    CHECK(screenshot_);
    PutScreenshotBack(animation_manager_->navigation_controller(),
                      std::move(screenshot_));
  } else {
    // If the navigation has committed then the destination entry is active. We
    // don't persist the screenshot for the active entry.
  }

  // This can happen if the navigation started for this gesture was committed
  // but another navigation or gesture started before the destination renderer
  // produced its first frame.
  if (new_render_widget_host_) {
    CHECK(state_ == State::kDisplayingInvokeAnimation ||
          state_ == State::kWaitingForNewRendererToDraw);
    UnregisterNewFrameActivationObserver();
  }
}

// protected.
BackForwardTransitionAnimator::BackForwardTransitionAnimator(
    WebContentsViewAndroid* web_contents_view_android,
    NavigationControllerImpl* controller,
    const ui::BackGestureEvent& gesture,
    NavigationDirection nav_direction,
    int destination_entry_id,
    BackForwardTransitionAnimationManagerAndroid* animation_manager)
    : nav_direction_(nav_direction),
      destination_entry_id_(destination_entry_id),
      animation_manager_(animation_manager),
      physics_model_(web_contents_view_android->GetNativeView()
                         ->GetPhysicalBackingSize()
                         .width(),
                     web_contents_view_android->GetNativeView()->GetDipScale()),
      latest_progress_gesture_(gesture),
      ignore_input_scope_(
          web_contents_view_android->web_contents()->IgnoreInputEvents(
              /*audit_callback=*/std::nullopt)) {
  state_ = State::kStarted;
  ProcessState();
}

void BackForwardTransitionAnimator::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  CHECK_EQ(state_, State::kStarted);
  // `gesture.progress()` goes from 0.0 to 1.0 when swipe from left to right,
  // and 1.0 to 0.0 from right to left.
  CHECK_GE(gesture.progress(), 0.f);
  CHECK_LE(gesture.progress(), 1.f);
  // TODO(crbug.com/40287990): Should check the number of KeyFrameModels
  // is 1 (for scrim).

  float progress_delta =
      gesture.progress() - latest_progress_gesture_.progress();
  const int width = animation_manager_->web_contents_view_android()
                        ->GetNativeView()
                        ->GetPhysicalBackingSize()
                        .width();
  const float movement = progress_delta * width;
  latest_progress_gesture_ = gesture;

  const PhysicsModel::Result result =
      physics_model_.OnGestureProgressed(movement, base::TimeTicks::Now());
  CHECK(!result.done);
  // The gesture animations are never considered "finished".
  bool animations_finished = SetLayerTransformationAndTickEffect(result);
  CHECK(!animations_finished);
}

void BackForwardTransitionAnimator::OnGestureCancelled() {
  CHECK_EQ(state_, State::kStarted);
  AdvanceAndProcessState(State::kDisplayingCancelAnimation);
}

void BackForwardTransitionAnimator::OnGestureInvoked() {
  CHECK_EQ(state_, State::kStarted);
  if (!StartNavigationAndTrackRequest()) {
    // We couldn't start the navigation. Cancel the animation.
    AdvanceAndProcessState(State::kDisplayingCancelAnimation);
    return;
  }
  // `StartNavigationAndTrackRequest()` sets `navigation_state_`.
  if (navigation_state_ == NavigationState::kBeforeUnloadDispatched) {
    AdvanceAndProcessState(State::kDisplayingCancelAnimation);
    return;
  }
  AdvanceAndProcessState(State::kDisplayingInvokeAnimation);
}

void BackForwardTransitionAnimator::OnDidNavigatePrimaryMainFramePreCommit(
    const NavigationRequest& navigation_request,
    RenderFrameHostImpl* old_host,
    RenderFrameHostImpl* new_host) {
  // Ignore all the subframe requests. Safe to do so as a start point because:
  // 1. TODO(crbug.com/40896219): We don't capture the screenshot for
  //    subframe navigations.
  // 2. (Implicitly) Because of 1, we don't animate subframe history
  //    navigations.
  // 3. TODO(crbug.com/41488906): For now, subframe navigations won't
  //    cancel the main frame history naivgations.
  //
  // Note: Also implicitly, all the subframes' DidFinishNavigation()s are
  // ignored.
  CHECK(navigation_request.IsInPrimaryMainFrame());

  bool skip_all_animations_and_self_destroy = false;

  switch (state_) {
    case State::kStarted:
      CHECK(!primary_main_frame_navigation_request_id_of_gesture_nav_);
      CHECK_EQ(navigation_state_, NavigationState::kNotStarted);
      // A new navigation finished in the primary main frame while the user is
      // swiping across the screen. For simplicity, destroy this class if the
      // new navigation was from the primary main frame.
      skip_all_animations_and_self_destroy = true;
      break;
    case State::kDisplayingInvokeAnimation: {
      // We can only get to `kDisplayingInvokeAnimation` if we have started
      // tracking the request.
      CHECK(primary_main_frame_navigation_request_id_of_gesture_nav_);

      if (navigation_state_ == NavigationState::kStarted) {
        if (navigation_request.GetNavigationId() !=
            primary_main_frame_navigation_request_id_of_gesture_nav_.value()) {
          // A previously pending navigation has committed since we started
          // tracking our gesture navigation. Ignore this committed navigation.
          return;
        }
        // Our gesture navigation has committed.
        navigation_state_ = NavigationState::kCommitted;
        physics_model_.OnNavigationFinished(/*navigation_committed=*/true);
        if (navigation_request.DidEncounterError()) {
          // TODO(crbug.com/41482489): Implement a different UX if we
          // decide not show the animation at all (i.e. abort animation early
          // when we receive the response header).
        }
        CloneOldSurfaceLayerAndRegisterNewFrameActivationObserver(old_host,
                                                                  new_host);
        // TODO(crbug.com/41492130): Handle the cross-origin server
        // redirect. We cannot show a cross-origin fullscreen overlay of a.com
        // if a.com redirect the user to b.com.
      } else {
        // Our navigation has already committed while a second navigation
        // commits. This can be a client redirect: A.com -> B.com and B.com's
        // document redirects to C.com, while we are still playing the post
        // commit-pending invoke animation to bring B.com's screenshot to the
        // center of the viewport.
        CHECK_EQ(navigation_state_, NavigationState::kCommitted);
        skip_all_animations_and_self_destroy = true;
      }
      break;
    }
    case State::kDisplayingCancelAnimation: {
      // We won't reach `NavigationState::kBeforeUnloadDispatched` because
      // if the request is blocked on BeforeUnload ack is cancelled, we will
      // receive `OnUnstartedNavigationCancelled()` where we advance
      // `navigation_state_` to `NavigationState::kCancelledBeforeStart`.

      CHECK(navigation_state_ == NavigationState::kNotStarted ||
            navigation_state_ == NavigationState::kCancelled ||
            navigation_state_ == NavigationState::kCancelledBeforeStart);

      // A navigation finished while we are displaying the cancel animation.
      // For simplicity, destroy `this` and reset everything.
      skip_all_animations_and_self_destroy = true;
      break;
    }
    case State::kWaitingForNewRendererToDraw:
      // Our navigation has already committed while a second navigation commits.
      // This can be a client redirect: A.com -> B.com and B.com's document
      // redirects to C.com, before B.com's renderer even submits a new frame.
      CHECK_EQ(navigation_state_, NavigationState::kCommitted);
      CHECK(primary_main_frame_navigation_request_id_of_gesture_nav_);
      skip_all_animations_and_self_destroy = true;
      break;
    case State::kDisplayingCrossFadeAnimation: {
      // Our navigation has already committed while a second navigation commits.
      // This can be a client redirect: A.com -> B.com and B.com's document
      // redirects to C.com, while we are cross-fading from B.com's screenshot
      // to whatever is underneath the screenshot.
      CHECK_EQ(navigation_state_, NavigationState::kCommitted);
      CHECK(primary_main_frame_navigation_request_id_of_gesture_nav_);
      skip_all_animations_and_self_destroy = true;
      break;
    }
    case State::kWaitingForBeforeUnloadResponse:
      NOTREACHED()
          << "The start of the second navigation will always cancel the "
             "navigation that's waiting for the renderer's BeforeUnload ack.";
      break;
    case State::kAnimationFinished:
      NOTREACHED()
          << "No navigations can commit during the animator's destruction "
             "because the destruction is atomic.";
      break;
  }

  // TODO(liuwilliam): We should return this bool and let the caller to destroy
  // the animator.
  if (skip_all_animations_and_self_destroy) {
    animation_manager_->SynchronouslyDestroyAnimator();
    // *DO NOT* add code after this. `this` will be destroyed at the end of
    // `SynchronouslyDestroyAnimator()`.
  }
}

void BackForwardTransitionAnimator::OnNavigationCancelledBeforeStart(
    NavigationHandle* navigation_handle) {
  if (!primary_main_frame_navigation_request_id_of_gesture_nav_.has_value() ||
      primary_main_frame_navigation_request_id_of_gesture_nav_.value() !=
          navigation_handle->GetNavigationId()) {
    return;
  }

  // For now only a BeforeUnload can defer the start of a navigation.
  //
  // NOTE: Even if the renderer acks the BeforeUnload message to proceed the
  // navigation, the navigation can still fail (see the early out in
  // BeginNavigationImpl()). However the animator's `navigation_state_` will
  // remain `NavigationState::kBeforeUnloadDispatched` because we only advance
  // from `NavigationState::kBeforeUnloadDispatched` to the next state at
  // `DidStartNavigation()`. In other words, if for any reason the navigation
  // fails after the renderer's ack, the below CHECK_EQ still holds.
  CHECK_EQ(navigation_state_, NavigationState::kBeforeUnloadDispatched);
  navigation_state_ = NavigationState::kCancelledBeforeStart;

  if (state_ == State::kWaitingForBeforeUnloadResponse) {
    // The cancel animation has already finished.
    AdvanceAndProcessState(State::kAnimationFinished);
  } else {
    // Let the cancel animation finish playing. We will advance to
    // `State::kAnimationFinished`.
    CHECK_EQ(state_, State::kDisplayingCancelAnimation);
  }
}

// This is only called after we subscribe to the new `RenderWidgetHost` in
// `OnDidNavigatePrimaryMainFramePreCommit()`, meaning this method, just like
// `OnDidNavigatePrimaryMainFramePreCommit()`, won't be called for
// 204/205/Download navigations, and won't be called if the navigation is
// cancelled.
//
// The manager won't be notified by the
// `OnRenderFrameMetadataChangedAfterActivation()`s that arrive earlier than
// `OnDidNavigatePrimaryMainFramePreCommit()` either if the renderer is too busy
// to reply the DidCommit message while viz has already activated a new frame
// for the new page. See
// `CloneOldSurfaceLayerAndRegisterNewFrameActivationObserver()` on how we guard
// this case.
//
// TODO(crbug.com/41488142): Should consider subscribe to FCP. FCP
// works mainframe as well as subframes navigations, with the exceptions of
// same-doc navigations.
void BackForwardTransitionAnimator::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  // `OnDidNavigatePrimaryMainFramePreCommit()` is the prerequisite for this
  // API.
  CHECK(new_render_widget_host_);

  // The navigation must have successfully committed, resulting us swapping the
  // `RenderWidgetHostView`s thus getting this notification.
  CHECK_EQ(navigation_state_, NavigationState::kCommitted);

  // Again this notification is only received after the
  // `OnDidNavigatePrimaryMainFramePreCommit()`. So we must have started playing
  // the invoke animation, or the invoke animation has finished.
  CHECK(state_ == State::kDisplayingInvokeAnimation ||
        state_ == State::kWaitingForNewRendererToDraw)
      << ToString(state_);

  CHECK(!viz_has_activated_first_frame_)
      << "OnRenderFrameMetadataChangedAfterActivation can only be called once.";
  viz_has_activated_first_frame_ = true;

  // No longer interested in any other compositor frame submission
  // notifications. We can safely dismiss the previewed screenshot now.
  UnregisterNewFrameActivationObserver();

  if (state_ == State::kWaitingForNewRendererToDraw) {
    // Only display the crossfade animation if the old page is completely out of
    // the viewport.
    AdvanceAndProcessState(State::kDisplayingCrossFadeAnimation);
  }
}

void BackForwardTransitionAnimator::OnRootWindowVisibilityChanged(
    bool visible) {
  if (!visible) {
    animation_manager_->SynchronouslyDestroyAnimator();
  }
}

void BackForwardTransitionAnimator::OnDetachCompositor() {
  animation_manager_->SynchronouslyDestroyAnimator();
}

void BackForwardTransitionAnimator::OnAnimate(
    base::TimeTicks frame_begin_time) {
  bool animation_finished = false;

  switch (state_) {
    case State::kDisplayingCancelAnimation: {
      PhysicsModel::Result result = physics_model_.OnAnimate(frame_begin_time);
      std::ignore = SetLayerTransformationAndTickEffect(result);
      animation_finished = result.done;
      break;
    }
    case State::kDisplayingInvokeAnimation: {
      PhysicsModel::Result result = physics_model_.OnAnimate(frame_begin_time);
      animation_finished = SetLayerTransformationAndTickEffect(result);
      break;
    }
    case State::kDisplayingCrossFadeAnimation: {
      // One cross-fade and one scrim models.
      CHECK_EQ(effect_.keyframe_models().size(), 2U);
      effect_.Tick(frame_begin_time);
      // `Tick()` has the side effect of removing all the finished models. At
      // the last frame of `OnFloatAnimated()`, the model is still running, but
      // is immediately removed after the `Tick()` WITHOUT advancing to the
      // finished or pending deletion state.
      animation_finished = effect_.keyframe_models().empty();
      break;
    }
    case State::kStarted:
    case State::kWaitingForBeforeUnloadResponse:
    case State::kWaitingForNewRendererToDraw:
    case State::kAnimationFinished:
      return;
  }

  if (animation_finished) {
    switch (state_) {
      case State::kDisplayingInvokeAnimation: {
        CHECK_EQ(navigation_state_, NavigationState::kCommitted);
        OnInvokeAnimationDisplayed();
        break;
      }
      case State::kDisplayingCancelAnimation: {
        OnCancelAnimationDisplayed();
        break;
      }
      case State::kDisplayingCrossFadeAnimation: {
        OnCrossFadeAnimationDisplayed();
        break;
      }
      case State::kStarted:
      case State::kWaitingForBeforeUnloadResponse:
      case State::kWaitingForNewRendererToDraw:
      case State::kAnimationFinished:
        NOTREACHED();
        break;
    }
  } else {
    animation_manager_->web_contents_view_android()
        ->GetTopLevelNativeWindow()
        ->SetNeedsAnimate();
  }
}

// We only use `DidStartNavigation()` for signalling that the renderer has acked
// the BeforeUnload message to proceed (begin) the navigation.
void BackForwardTransitionAnimator::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!primary_main_frame_navigation_request_id_of_gesture_nav_.has_value()) {
    // We could reach here for an early-commit navigation:
    // - The animator only tracks the request's ID after `GoToIndex()` returns.
    // - In early commit, `DidStartNavigation()` is called during `GoToIndex()`.
    //
    // Early return here and let `StartNavigationAndTrackRequest()` to set the
    // `navigation_state_`.
    return;
  }
  int64_t tracked_request_id =
      primary_main_frame_navigation_request_id_of_gesture_nav_.value();
  if (tracked_request_id != navigation_handle->GetNavigationId()) {
    return;
  }

  CHECK_EQ(navigation_state_, NavigationState::kBeforeUnloadDispatched);
  navigation_state_ = NavigationState::kBeforeUnloadAckedProceed;

  CHECK(state_ == State::kWaitingForBeforeUnloadResponse ||
        state_ == State::kDisplayingCancelAnimation);

  AdvanceAndProcessState(State::kDisplayingInvokeAnimation);
}

// We only use `DidFinishNavigation()` for navigations that never commit
// (204/205/downloads), or the cancelled / replaced navigations. For a committed
// navigation, everything is set in `OnDidNavigatePrimaryMainFramePreCommit()`,
// which is before the old `RenderViewHost` is swapped out.
void BackForwardTransitionAnimator::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If we haven't started tracking a navigation, or if `navigation_handle`
  // isn't what we tracked, or if this `navigation_handle` has committed, ignore
  // it.
  if (!primary_main_frame_navigation_request_id_of_gesture_nav_.has_value() ||
      primary_main_frame_navigation_request_id_of_gesture_nav_.value() !=
          navigation_handle->GetNavigationId()) {
    return;
  }
  if (navigation_handle->HasCommitted()) {
    CHECK_EQ(navigation_state_, NavigationState::kCommitted);
    return;
  }

  CHECK_EQ(state_, State::kDisplayingInvokeAnimation);
  CHECK_EQ(navigation_state_, NavigationState::kStarted);
  navigation_state_ = NavigationState::kCancelled;
  physics_model_.OnNavigationFinished(/*navigation_committed=*/false);
  // 204/205/Download, or the ongoing navigation is cancelled. We need
  // to animate the old page back.
  //
  // TODO(crbug.com/41482488): We might need a better UX than
  // just display the cancel animation.
  AdvanceAndProcessState(State::kDisplayingCancelAnimation);
}

void BackForwardTransitionAnimator::RenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  if (widget_host != new_render_widget_host_) {
    return;
  }
  // Our new widget host is about to be destroyed. This can happen for a client
  // redirect, where we never get the
  // `OnRenderFrameMetadataChangedAfterActivation()` of any frame of a committed
  // renderer. The screenshot isn't dismissed even after the gesture navigation
  // is committed. Destroy `this` and reset everything.
  CHECK_EQ(state_, State::kWaitingForNewRendererToDraw);
  CHECK_EQ(navigation_state_, NavigationState::kCommitted);
  animation_manager_->SynchronouslyDestroyAnimator();
}

void BackForwardTransitionAnimator::OnFloatAnimated(
    const float& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  TargetProperty property = static_cast<TargetProperty>(target_property_id);
  switch (property) {
    case TargetProperty::kScrim: {
      CHECK(screenshot_scrim_);
      auto scrim = SkColors::kBlack;
      scrim.fA = value;
      screenshot_scrim_->SetBackgroundColor(scrim);
      return;
    }
    case TargetProperty::kCrossFade: {
      CHECK(ui_resource_layer_);
      // Scrim (second timeline) and the crossfade model.
      CHECK_EQ(effect_.keyframe_models().size(), 2u);
      ui_resource_layer_->SetOpacity(value);
      return;
    }
  }
  NOTREACHED();
}

void BackForwardTransitionAnimator::OnCancelAnimationDisplayed() {
  CHECK_EQ(effect_.keyframe_models().size(), 1U);
  CHECK_EQ(effect_.keyframe_models()[0]->TargetProperty(),
           static_cast<int>(TargetProperty::kScrim));
  if (navigation_state_ == NavigationState::kBeforeUnloadDispatched) {
    AdvanceAndProcessState(State::kWaitingForBeforeUnloadResponse);
    return;
  }
  effect_.RemoveAllKeyframeModels();
  AdvanceAndProcessState(State::kAnimationFinished);
}

void BackForwardTransitionAnimator::OnInvokeAnimationDisplayed() {
  // There is no `old_surface_clone_` when navigating from a crashed page.
  if (old_surface_clone_) {
    old_surface_clone_->RemoveFromParent();
    old_surface_clone_.reset();
  }

  // The first scrim timeline is a function of the top layer's position. At the
  // end of the invoke animation, the top layer is completely out of the
  // viewport, so the `KeyFrameModel` for the scrim should also be exhausted and
  // removed.
  CHECK(effect_.keyframe_models().empty());
  if (viz_has_activated_first_frame_) {
    AdvanceAndProcessState(State::kDisplayingCrossFadeAnimation);
  } else {
    AdvanceAndProcessState(State::kWaitingForNewRendererToDraw);
  }
}

void BackForwardTransitionAnimator::OnCrossFadeAnimationDisplayed() {
  CHECK(effect_.keyframe_models().empty());
  AdvanceAndProcessState(State::kAnimationFinished);
}

// static.
bool BackForwardTransitionAnimator::CanAdvanceTo(State from, State to) {
  switch (from) {
    case State::kStarted:
      return to == State::kDisplayingCancelAnimation ||
             to == State::kDisplayingInvokeAnimation;
    case State::kWaitingForBeforeUnloadResponse:
      return to == State::kDisplayingInvokeAnimation ||
             to == State::kAnimationFinished;
    case State::kDisplayingInvokeAnimation:
      return to == State::kDisplayingCrossFadeAnimation ||
             to == State::kWaitingForNewRendererToDraw ||
             // A second navigation replaces the current one, or the user hits
             // the stop button.
             to == State::kDisplayingCancelAnimation;
    case State::kWaitingForNewRendererToDraw:
      return to == State::kDisplayingCrossFadeAnimation;
    case State::kDisplayingCrossFadeAnimation:
      return to == State::kAnimationFinished;
    case State::kDisplayingCancelAnimation:
      return to == State::kAnimationFinished ||
             // The cancel animation has finished for a dispatched BeforeUnload
             // message.
             to == State::kWaitingForBeforeUnloadResponse ||
             // The renderer acks the BeforeUnload message to proceed the
             // navigation, BEFORE the cancel animation finishes.
             to == State::kDisplayingInvokeAnimation;
    case State::kAnimationFinished:
      NOTREACHED_NORETURN();
  }
}

// static.
std::string BackForwardTransitionAnimator::ToString(State state) {
  switch (state) {
    case State::kStarted:
      return "kStarted";
    case State::kDisplayingCancelAnimation:
      return "kDisplayingCancelAnimation";
    case State::kDisplayingInvokeAnimation:
      return "kDisplayingInvokeAnimation";
    case State::kWaitingForNewRendererToDraw:
      return "kWaitingForNewRendererToDraw";
    case State::kDisplayingCrossFadeAnimation:
      return "kDisplayingCrossFadeAnimation";
    case State::kAnimationFinished:
      return "kAnimationFinished";
    case State::kWaitingForBeforeUnloadResponse:
      return "kWaitingForBeforeUnloadResponse";
  }
  NOTREACHED_NORETURN();
}

// static.
std::string BackForwardTransitionAnimator::ToString(NavigationState state) {
  switch (state) {
    case NavigationState::kNotStarted:
      return "kNotStarted";
    case NavigationState::kBeforeUnloadDispatched:
      return "kBeforeUnloadDispatched";
    case NavigationState::kBeforeUnloadAckedProceed:
      return "kBeforeUnloadAckedProceed";
    case NavigationState::kCancelledBeforeStart:
      return "kCancelledBeforeStart";
    case NavigationState::kStarted:
      return "kStarted";
    case NavigationState::kCommitted:
      return "kCommitted";
    case NavigationState::kCancelled:
      return "kCancelled";
  }
  NOTREACHED_NORETURN();
}

void BackForwardTransitionAnimator::
    InitializeEffectForGestureProgressAnimation() {
  // The KeyFrameModel for scrim is added when we set up the screenshot layer,
  // at which we must have no models yet.
  CHECK(effect_.keyframe_models().empty());

  // First scrim timeline for the screenshot layer's transform.
  AddFloatModelToEffect(kScrimAnimationDuringGestureProgress, this, effect_);
}

void BackForwardTransitionAnimator::InitializeEffectForCrossfadeAnimation() {
  // At the the end if the invoke animation and before the cross-fade, the scrim
  // model for the first timeline is finished (and removed).
  CHECK(effect_.keyframe_models().empty());

  AddFloatModelToEffect(kCrossFadeAnimation, this, effect_);

  // Second scrim timeline for the cross-fade animation.
  AddFloatModelToEffect(kScrimAnimationDuringCrossFade, this, effect_);
}

void BackForwardTransitionAnimator::AdvanceAndProcessState(State state) {
  CHECK(CanAdvanceTo(state_, state))
      << "Cannot advance from " << ToString(state_) << " to "
      << ToString(state);
  state_ = state;
  ProcessState();
}

void BackForwardTransitionAnimator::ProcessState() {
  switch (state_) {
    case State::kStarted: {
      NavigationControllerImpl* nav_controller =
          animation_manager_->navigation_controller();
      auto* destination_entry =
          nav_controller->GetEntryWithUniqueID(destination_entry_id_);
      CHECK(destination_entry);
      auto* cache = nav_controller->GetNavigationEntryScreenshotCache();
      SetupForScreenshotPreview(cache->RemoveScreenshot(destination_entry));
      // Become a WCO as soon as this class is created, because we want to
      // observe all navigations while this class is controlling the UI. This
      // allows us to ensure the visuals displayed align with the active page
      // and URL in the URL bar.
      WebContentsObserver::Observe(
          animation_manager_->web_contents_view_android()->web_contents());
      // Become a `WindowAndroidObserver` right away as well.
      CHECK(animation_manager_->web_contents_view_android()
                ->GetTopLevelNativeWindow());
      animation_manager_->web_contents_view_android()
          ->GetTopLevelNativeWindow()
          ->AddObserver(this);
      break;
      // `this` will be waiting for the `OnGestureProgressed` call.
    }
    case State::kDisplayingCancelAnimation: {
      if (navigation_state_ == NavigationState::kNotStarted) {
        // When the user lifts the finger and signals not to start the
        // navigation.
        physics_model_.SwitchSpringForReason(
            SwitchSpringReason::kGestureCancelled);
      } else if (navigation_state_ ==
                 NavigationState::kBeforeUnloadDispatched) {
        // Notify the physics model we need to animate the active page back to
        // the center of the viewport because the browser has asked the renderer
        // to ack the BeforeUnload message. The renderer may need to show a
        // prompt to ask for the user input.
        physics_model_.SwitchSpringForReason(
            SwitchSpringReason::kBeforeUnloadDispatched);
      } else if (navigation_state_ == NavigationState::kCancelledBeforeStart) {
        // The user has interacted with the prompt to not start the navigation.
        // We are waiting for the ongoing cancel animation to finish.
      } else if (navigation_state_ == NavigationState::kCancelled) {
        // When the ongoing navigaion is cancelled because the user hits stop or
        // the navigation was replaced by another navigation,
        // `OnDidFinishNavigation()` has already notified the physics model to
        // switch to the cancel spring.
      } else {
        NOTREACHED() << ToString(navigation_state_);
      }
      CHECK(animation_manager_->web_contents_view_android()
                ->GetTopLevelNativeWindow());
      animation_manager_->web_contents_view_android()
          ->GetTopLevelNativeWindow()
          ->SetNeedsAnimate();
      break;
    }
    case State::kDisplayingInvokeAnimation: {
      if (navigation_state_ == NavigationState::kBeforeUnloadAckedProceed) {
        // Notify the physics model that the renderer has ack'ed BeforeUnload
        // and the navigation shall proceed.
        physics_model_.SwitchSpringForReason(
            SwitchSpringReason::kBeforeUnloadAckProceed);
        navigation_state_ = NavigationState::kStarted;
      } else {
        // Else, we must have started the navigation.
        CHECK_EQ(navigation_state_, NavigationState::kStarted);
        physics_model_.SwitchSpringForReason(
            SwitchSpringReason::kGestureInvoked);
      }
      CHECK(animation_manager_->web_contents_view_android()
                ->GetTopLevelNativeWindow());
      animation_manager_->web_contents_view_android()
          ->GetTopLevelNativeWindow()
          ->SetNeedsAnimate();
      break;
    };
    case State::kWaitingForBeforeUnloadResponse: {
      // No-op. Waiting for the renderer's ack before we can proceed with the
      // navigation and animation or cancel everything.
      break;
    }
    case State::kWaitingForNewRendererToDraw:
      // No-op. Waiting for `OnRenderFrameMetadataChangedAfterActivation()`.
      break;
    case State::kDisplayingCrossFadeAnimation: {
      // Before we start displaying the crossfade animation,
      // `parent_for_web_page_widgets()` is completely out of the viewport. This
      // layer is reused for new content. For this reason, before we can start
      // the cross-fade we need to bring it back to the center of the viewport.
      ResetTransformForLayer(animation_manager_->web_contents_view_android()
                                 ->parent_for_web_page_widgets());
      ResetTransformForLayer(ui_resource_layer_.get());

      // Move the screenshot to the very top, so we can cross-fade from the
      // screenshot (top) into the active page (bottom).
      CHECK(ui_resource_layer_->parent());
      ui_resource_layer_->RemoveFromParent();
      animation_manager_->web_contents_view_android()
          ->AddScreenshotLayerForNavigationTransitions(
              ui_resource_layer_.get(), /*screenshot_layer_on_top=*/true);

      InitializeEffectForCrossfadeAnimation();

      CHECK(animation_manager_->web_contents_view_android()
                ->GetTopLevelNativeWindow());
      animation_manager_->web_contents_view_android()
          ->GetTopLevelNativeWindow()
          ->SetNeedsAnimate();
      break;
    }
    case State::kAnimationFinished: {
      animation_manager_->SynchronouslyDestroyAnimator();
      break;
      // DO NOT add code after this state. `OnAnimationsFinished` call will
      // erase the current instance of `this` from `animation_manager_`.
    }
  }
}

void BackForwardTransitionAnimator::SetupForScreenshotPreview(
    std::unique_ptr<NavigationEntryScreenshot> screenshot) {
  CHECK(screenshot);
  CHECK_EQ(screenshot->navigation_entry_id(), destination_entry_id_);
  screenshot_ = std::move(screenshot);

  // The layers can be reused. We need to make sure there is no ongoing
  // transform on the layer of the current `WebContents`'s view.
  auto transform = animation_manager_->web_contents_view_android()
                       ->parent_for_web_page_widgets()
                       ->transform();
  CHECK(transform.IsIdentity()) << transform.ToString();

  ui_resource_id_ = CreateUIResource(screenshot_.get());
  ui_resource_layer_ = cc::slim::UIResourceLayer::Create();
  ui_resource_layer_->SetIsDrawable(true);
  ui_resource_layer_->SetUIResourceId(ui_resource_id_);

  screenshot_scrim_ = cc::slim::SolidColorLayer::Create();
  screenshot_scrim_->SetBounds(screenshot_->GetDimensions());
  screenshot_scrim_->SetIsDrawable(true);
  screenshot_scrim_->SetBackgroundColor(SkColors::kTransparent);
  // This makes sure `screenshot_scrim_` is drawn on top of
  // `ui_resource_layer_`.
  ui_resource_layer_->AddChild(screenshot_scrim_);
  screenshot_scrim_->SetContentsOpaque(false);

  // Insert a new `cc::slim::UIResourceLayer` into the existing layer tree.
  //
  // `WebContentsViewAndroid::view_->GetLayer()`
  //            |
  //            |- `old_surface_clone_` (only set during the invoke animation).
  //            |- `parent_for_web_page_widgets_` (RWHVAndroid, Overscroll etc).
  //            |
  //            |- `NavigationEntryScreenshot`

  bool screenshot_on_top_of_web_page =
      nav_direction_ == NavigationDirection::kForward;
  animation_manager_->web_contents_view_android()
      ->AddScreenshotLayerForNavigationTransitions(
          ui_resource_layer_.get(), screenshot_on_top_of_web_page);

  // Reposition the new layer and set its bounds.
  ui_resource_layer_->SetPosition(gfx::PointF(0.f, 0.f));
  ui_resource_layer_->SetBounds(animation_manager_->web_contents_view_android()
                                    ->GetNativeView()
                                    ->GetPhysicalBackingSize());

  // Set up `effect_`.
  InitializeEffectForGestureProgressAnimation();

  // Calling `OnGestureProgressed` manually. This will ask the physics model to
  // move the layers to their respective initial positions.
  OnGestureProgressed(latest_progress_gesture_);
}

bool BackForwardTransitionAnimator::StartNavigationAndTrackRequest() {
  CHECK(screenshot_);
  CHECK(!primary_main_frame_navigation_request_id_of_gesture_nav_.has_value());
  CHECK_EQ(navigation_state_, NavigationState::kNotStarted);

  NavigationControllerImpl* nav_controller =
      animation_manager_->navigation_controller();

  int index = nav_controller->GetEntryIndexWithUniqueID(destination_entry_id_);
  if (index == -1) {
    return false;
  }

  // TODO(https://crbug.com/325331788): `GoToIndex()` can return a bool to
  // signal the creation of the navigation request.
  nav_controller->GoToIndex(index);

  auto& frame_tree = nav_controller->frame_tree();
  // TODO(https://crbug.com/325331788): This request might not be the one we
  // started.
  if (auto* request = frame_tree.root()->navigation_request()) {
    // TOOD(https://crbug.com/1518341): We shouldn't animate same-doc history
    // navigations because we don't capture the screenshots for them.
    CHECK(!request->IsSameDocument());
    // If the request isn't associated with a navigation entry, we shouldn't
    // start the navigation.
    //
    // TODO(https://crbug.com/325331788): Turn this early return into a CHECK.
    if (!request->GetNavigationEntry()) {
      return false;
    }
    int request_entry_id = request->GetNavigationEntry()->GetUniqueID();
    // If the request's navigation entry ID does not match the entry ID of the
    // screenshot of the destination, we shouldn't start the navigation.
    //
    // TODO(crbug.com/325331788): Turn this early return into a CHECK.
    if (destination_entry_id_ != request_entry_id) {
      return false;
    }

    primary_main_frame_navigation_request_id_of_gesture_nav_ =
        request->GetNavigationId();
    if (request->IsNavigationStarted()) {
      navigation_state_ = NavigationState::kStarted;
    } else {
      CHECK(request->IsWaitingForBeforeUnload());
      navigation_state_ = NavigationState::kBeforeUnloadDispatched;
    }
    return true;
  }
  // TOOD(https://crbug.com/1518341): Collect the requests for the same-doc
  // navigations.
  // TODO(crbug.com/41490714): Collect subframe requests from the
  // subframe FrameTreeNodes.
  return false;
}

cc::UIResourceId BackForwardTransitionAnimator::CreateUIResource(
    cc::UIResourceClient* client) {
  // A Window is detached from the NativeView if the tab is not currently
  // displayed. It would be an error to use any of the APIs in this file.
  ui::WindowAndroid* window = animation_manager_->web_contents_view_android()
                                  ->GetTopLevelNativeWindow();
  CHECK(window);
  // Guaranteed to have a compositor as long as the window is attached.
  ui::WindowAndroidCompositor* compositor = window->GetCompositor();
  CHECK(compositor);
  return static_cast<CompositorImpl*>(compositor)->CreateUIResource(client);
}

void BackForwardTransitionAnimator::
    RemoveWindowAndroidObserverAndDeleteUIResource(
        cc::UIResourceId resource_id) {
  ui::WindowAndroid* window = animation_manager_->web_contents_view_android()
                                  ->GetTopLevelNativeWindow();
  CHECK(window);
  window->RemoveObserver(this);
  ui::WindowAndroidCompositor* compositor = window->GetCompositor();
  CHECK(compositor);
  static_cast<CompositorImpl*>(compositor)->DeleteUIResource(ui_resource_id_);
}

bool BackForwardTransitionAnimator::SetLayerTransformationAndTickEffect(
    const PhysicsModel::Result& result) {
  ui_resource_layer_->SetTransform(
      gfx::Transform::MakeTranslation(result.background_offset_physical, 0.f));

  const auto foreground_transform =
      gfx::Transform::MakeTranslation(result.foreground_offset_physical, 0.f);
  animation_manager_->web_contents_view_android()
      ->parent_for_web_page_widgets()
      ->SetTransform(foreground_transform);

  if (old_surface_clone_) {
    CHECK_EQ(navigation_state_, NavigationState::kCommitted);
    CHECK_EQ(state_, State::kDisplayingInvokeAnimation);
    old_surface_clone_->SetTransform(foreground_transform);
  }

  float screenshot_layer_progress =
      result.foreground_offset_physical /
      animation_manager_->web_contents_view_android()
          ->GetNativeView()
          ->GetPhysicalBackingSize()
          .width();
  CHECK_GE(screenshot_layer_progress, 0.f);
  CHECK_LE(screenshot_layer_progress, 1.f);
  effect_.Tick(
      GetFittedTimeTicksForForegroundProgress(screenshot_layer_progress));
  return result.done && effect_.keyframe_models().empty();
}

void BackForwardTransitionAnimator::
    CloneOldSurfaceLayerAndRegisterNewFrameActivationObserver(
        RenderFrameHostImpl* old_host,
        RenderFrameHostImpl* new_host) {
  auto* old_view =
      static_cast<RenderWidgetHostViewAndroid*>(old_host->GetView());
  CHECK(old_view);
  if (old_host == new_host) {
    // The RFH for the old page is early-swapped out. This can only happen to
    // navigation from a crashed page.
    //
    // TODO(crbug.com/40283503): The Clank's interstitial page isn't
    // drawn by `old_view`. We need to address as part of "navigating from NTP"
    // animation.
  } else {
    CloneOldSurfaceLayer(old_host->GetView());
  }
  CHECK(new_host);
  auto* new_widget_host = new_host->GetRenderWidgetHost();
  // We must have a live new widget.
  CHECK(new_widget_host);
  // `render_frame_metadata_provider()` is guaranteed non-null.
  std::optional<viz::LocalSurfaceId> last_frame_local_surface_id =
      static_cast<RenderWidgetHostImpl*>(new_widget_host)
          ->render_frame_metadata_provider()
          ->LastRenderFrameMetadata()
          .local_surface_id;
  auto* new_view =
      static_cast<RenderWidgetHostViewBase*>(new_widget_host->GetView());
  if (last_frame_local_surface_id.has_value() &&
      last_frame_local_surface_id.value().is_valid() &&
      last_frame_local_surface_id.value().embed_token() ==
          new_view->GetLocalSurfaceId().embed_token() &&
      last_frame_local_surface_id.value().IsSameOrNewerThan(
          new_view->GetLocalSurfaceId())) {
    // This can happen where the renderer's main thread is very busy to reply
    // `DidCommitNavigation()` back to the browser, but viz has already
    // activated the first frame. Because the browser hasn't received the
    // `DidCommitNavigation()` message, this animation manager hasn't subscribed
    // to the new widget host, therefore missed out on the first
    // `OnRenderFrameMetadataChangedAfterActivation()`. The screenshot will stay
    // until the next `OnRenderFrameMetadataChangedAfterActivation()`
    // notification.
    //
    // In this case we inspect the `LocalSurfaceId` of activated frame. If the
    // ID is greater than what the browser is embedding, we know viz has already
    // activated a frame. We don't need to subscribe to the new widget host
    // for `OnRenderFrameMetadataChangedAfterActivation()` at all.
    CHECK(!viz_has_activated_first_frame_);
    viz_has_activated_first_frame_ = true;
    return;
  }

  // We subscribe to `new_widget_host` to get notified when the new renderer
  // draws a new frame, so we can start cross-fading from the preview screenshot
  // to the new page's live content.
  //
  // TODO(crbug.com/41483162): This won't work for same-doc navigations.
  // We need to listen to `OnLocalSurfaceIdChanged` when we bump the `SurfaceId`
  // for same-doc navigations.
  CHECK(!new_render_widget_host_);
  new_render_widget_host_ = RenderWidgetHostImpl::From(new_widget_host);
  new_render_widget_host_->AddObserver(this);
  new_render_widget_host_->render_frame_metadata_provider()->AddObserver(this);
}

void BackForwardTransitionAnimator::CloneOldSurfaceLayer(
    RenderWidgetHostViewBase* old_main_frame_view) {
  // The old View must be still alive (and its renderer).
  CHECK(old_main_frame_view);

  CHECK(!old_surface_clone_);

  old_surface_clone_ = cc::slim::SurfaceLayer::Create();
  const auto* old_surface_layer =
      static_cast<RenderWidgetHostViewAndroid*>(old_main_frame_view)
          ->GetSurfaceLayer();
  // Use a zero deadline because this is a copy of a surface being actively
  // shown. The surface textures are ready (i.e. won't be GC'ed) because
  // `old_surface_clone_` references to them.
  old_surface_clone_->SetSurfaceId(old_surface_layer->surface_id(),
                                   cc::DeadlinePolicy::UseSpecifiedDeadline(0));
  old_surface_clone_->SetPosition(old_surface_layer->position());
  old_surface_clone_->SetBounds(old_surface_layer->bounds());
  old_surface_clone_->SetTransform(old_surface_layer->transform());
  old_surface_clone_->SetIsDrawable(true);
  auto* parent_for_web_widgets = animation_manager_->web_contents_view_android()
                                     ->parent_for_web_page_widgets();
  CHECK_EQ(animation_manager_->web_contents_view_android()
               ->GetNativeView()
               ->GetLayer(),
           parent_for_web_widgets->parent());

  parent_for_web_widgets->parent()->AddChild(old_surface_clone_);
}

void BackForwardTransitionAnimator::UnregisterNewFrameActivationObserver() {
  new_render_widget_host_->render_frame_metadata_provider()->RemoveObserver(
      this);
  new_render_widget_host_->RemoveObserver(this);
  new_render_widget_host_ = nullptr;
}

}  // namespace content
