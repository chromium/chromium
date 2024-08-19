// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animator.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"
#include "content/browser/navigation_transitions/progress_bar.h"
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
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/android/window_android.h"
#include "ui/events/back_gesture_event.h"

namespace content {

namespace {

using CacheHitOrMissReason = NavigationTransitionData::CacheHitOrMissReason;

using NavigationDirection =
    BackForwardTransitionAnimationManager::NavigationDirection;
using AnimationStage = BackForwardTransitionAnimationManager::AnimationStage;
using SwitchSpringReason = PhysicsModel::SwitchSpringReason;
using SwipeEdge = ui::BackGestureEventSwipeEdge;

void ResetTransformForLayer(cc::slim::Layer* layer) {
  CHECK(layer);
  auto transform = layer->transform();
  transform.MakeIdentity();
  layer->SetTransform(transform);
}

bool ShouldUseFallbackScreenshot(
    BackForwardTransitionAnimationManagerAndroid* animation_manager,
    NavigationEntryImpl* destination_entry) {
  bool use_fallback_screenshot = true;
  auto* screenshot = static_cast<NavigationEntryScreenshot*>(
      destination_entry->GetUserData(NavigationEntryScreenshot::kUserDataKey));
  auto cache_hit_or_miss_reason =
      destination_entry->navigation_transition_data()
          .cache_hit_or_miss_reason();

  if (screenshot) {
    gfx::Size screenshot_size = screenshot->dimensions_without_compression();
    gfx::Size screen_size = animation_manager->web_contents_view_android()
                                ->GetNativeView()
                                ->GetPhysicalBackingSize();
    use_fallback_screenshot = screenshot_size != screen_size;
    if (screenshot_size != screen_size) {
      cache_hit_or_miss_reason = NavigationTransitionData::
          CacheHitOrMissReason::kCacheMissScreenshotOrientation;
    } else {
      CHECK_EQ(cache_hit_or_miss_reason.value(),
               NavigationTransitionData::CacheHitOrMissReason::kCacheHit);
    }
  }

  // TODO(crbug.com/355454946): Consider other ways to capture `kCacheColdStart`
  // metric.
  UMA_HISTOGRAM_ENUMERATION("Navigation.GestureTransition.CacheHitOrMissReason",
                            cache_hit_or_miss_reason.value_or(
                                CacheHitOrMissReason::kCacheMissColdStart));

  return use_fallback_screenshot;
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
// The scrim range is from 0.2 to 0 in dark mode and 0.1 to 0 in light mode. The
// scrim value is a linear function of the top layer's position.
static constexpr ScrimAndCrossFadeAnimaitonConfig kScrimAnimationLightMode{
    .target_property = TargetProperty::kScrim,
    .start = 0.1f,
    .end = 0.0f,
    .duration = kFittedTimelineDuration};

static constexpr ScrimAndCrossFadeAnimaitonConfig kScrimAnimationDarkMode{
    .target_property = TargetProperty::kScrim,
    .start = 0.2f,
    .end = 0.0f,
    .duration = kFittedTimelineDuration};

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
    SwipeEdge initiating_edge,
    NavigationEntryImpl* destination_entry,
    BackForwardTransitionAnimationManagerAndroid* animation_manager) {
  return base::WrapUnique(new BackForwardTransitionAnimator(
      web_contents_view_android, controller, gesture, nav_direction,
      initiating_edge, destination_entry, animation_manager));
}

BackForwardTransitionAnimator::~BackForwardTransitionAnimator() {
  CHECK(IsTerminalState()) << ToString(state_);

  ResetTransformForLayer(animation_manager_->web_contents_view_android()
                             ->parent_for_web_page_widgets());

  // TODO(crbug.com/40283503): If there is the old visual state hovering
  // above the RWHV layer, we need to remove that as well.

  if (screenshot_layer_) {
    screenshot_scrim_->RemoveFromParent();
    screenshot_scrim_.reset();

    screenshot_layer_->RemoveFromParent();
    screenshot_layer_.reset();
  }

  if (embedder_live_content_clone_) {
    CHECK(!old_surface_clone_);
    embedder_live_content_clone_->RemoveFromParent();
    embedder_live_content_clone_.reset();
  } else if (old_surface_clone_) {
    old_surface_clone_->RemoveFromParent();
    old_surface_clone_.reset();
  }

  if (!use_fallback_screenshot_) {
    CHECK_NE(ui_resource_id_, cc::UIResourceClient::kUninitializedUIResourceId);
    DeleteUIResource(ui_resource_id_);

    if (navigation_state_ != NavigationState::kCommitted) {
      CHECK(screenshot_);
      animation_manager_->navigation_controller()
          ->GetNavigationEntryScreenshotCache()
          ->SetScreenshot(nullptr, std::move(screenshot_),
                          is_copied_from_embedder_);
    } else {
      // If the navigation has committed then the destination entry is active.
      // We don't persist the screenshot for the active entry.
    }
  }

  // This can happen if the navigation started for this gesture was committed
  // but another navigation or gesture started before the destination renderer
  // produced its first frame.
  if (new_render_widget_host_) {
    CHECK_EQ(state_, State::kAnimationAborted) << ToString(state_);
    UnregisterNewFrameActivationObserver();
  }
}

// protected.
BackForwardTransitionAnimator::BackForwardTransitionAnimator(
    WebContentsViewAndroid* web_contents_view_android,
    NavigationControllerImpl* controller,
    const ui::BackGestureEvent& gesture,
    NavigationDirection nav_direction,
    SwipeEdge initiating_edge,
    NavigationEntryImpl* destination_entry,
    BackForwardTransitionAnimationManagerAndroid* animation_manager)
    : nav_direction_(nav_direction),
      initiating_edge_(initiating_edge),
      destination_entry_id_(destination_entry->GetUniqueID()),
      animation_manager_(animation_manager),
      is_copied_from_embedder_(destination_entry->navigation_transition_data()
                                   .is_copied_from_embedder()),
      use_fallback_screenshot_(
          ShouldUseFallbackScreenshot(animation_manager_, destination_entry)),
      fallback_ux_config_(animation_manager_->web_contents_view_android()
                              ->web_contents()
                              ->GetDelegate()
                              ->GetBackForwardTransitionFallbackUXConfig()),
      physics_model_(GetViewportWidthPx(),
                     web_contents_view_android->GetNativeView()->GetDipScale()),
      latest_progress_gesture_(gesture) {
  state_ = State::kStarted;
  ProcessState();
}

void BackForwardTransitionAnimator::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  CHECK_EQ(state_, State::kStarted);
  // `gesture.progress()` goes from 0.0 to 1.0 regardless of the edge being
  // swiped.
  CHECK_GE(gesture.progress(), 0.f);
  CHECK_LE(gesture.progress(), 1.f);
  // TODO(crbug.com/40287990): Should check the number of KeyFrameModels
  // is 1 (for scrim).

  float progress_delta =
      gesture.progress() - latest_progress_gesture_.progress();
  const float movement = progress_delta * GetViewportWidthPx();
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
  StartInputSuppression();
  AdvanceAndProcessState(State::kDisplayingCancelAnimation);
}

void BackForwardTransitionAnimator::OnGestureInvoked() {
  CHECK_EQ(state_, State::kStarted);

  StartInputSuppression();

  if (!StartNavigationAndTrackRequest()) {
    // `BackForwardTransitionAnimationManagerAndroid` will destroy `this` upon
    // return if the animation is aborted.
    if (state_ != State::kAnimationAborted) {
      AdvanceAndProcessState(State::kDisplayingCancelAnimation);
    }
    return;
  }

  // `StartNavigationAndTrackRequest()` sets `navigation_state_`.
  if (navigation_state_ == NavigationState::kBeforeUnloadDispatched) {
    AdvanceAndProcessState(State::kDisplayingCancelAnimation);
    return;
  }

  CHECK_EQ(navigation_state_, NavigationState::kStarted);
  AdvanceAndProcessState(State::kDisplayingInvokeAnimation);
}

// TODO(https://crbug.com/357094180): We should cancel the transition if a
// unrelated request shows a beforeunload dialog.
void BackForwardTransitionAnimator::OnNavigationCancelledBeforeStart(
    NavigationHandle* navigation_handle) {
  if (!tracked_request_ ||
      tracked_request_->navigation_id != navigation_handle->GetNavigationId()) {
    // A unrelated request is cancelled before start.
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

void BackForwardTransitionAnimator::OnContentForNavigationEntryShown() {
  // Might be called multiple times if user swipes again before NTP fade
  // has finished.
  if (state_ != State::kWaitingForContentForNavigationEntryShown) {
    return;
  }
  // The embedder has finished cross-fading from the screenshot to the new
  // content. Unregister `this` from the `RenderWidgetHost` to stop the
  // `OnRenderWidgetHostDestroyed()` notification.
  CHECK(new_render_widget_host_);
  new_render_widget_host_->RemoveObserver(animation_manager_);
  new_render_widget_host_ = nullptr;
  AdvanceAndProcessState(State::kAnimationFinished);
}

AnimationStage BackForwardTransitionAnimator::GetCurrentAnimationStage() {
  switch (state_) {
    case State::kDisplayingInvokeAnimation:
      return AnimationStage::kInvokeAnimation;
    case State::kAnimationFinished:
    case State::kAnimationAborted:
      return AnimationStage::kNone;
    default:
      return AnimationStage::kOther;
  }
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

      if (progress_bar_) {
        progress_bar_->Animate(frame_begin_time);
      }
      break;
    }
    case State::kDisplayingCrossFadeAnimation: {
      // The cross-fade model.
      CHECK_EQ(effect_.keyframe_models().size(), 1U);
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
    case State::kWaitingForContentForNavigationEntryShown:
    case State::kAnimationFinished:
    case State::kAnimationAborted:
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
      case State::kWaitingForContentForNavigationEntryShown:
      case State::kAnimationFinished:
      case State::kAnimationAborted:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else {
    animation_manager_->web_contents_view_android()
        ->GetTopLevelNativeWindow()
        ->SetNeedsAnimate();
  }
}

void BackForwardTransitionAnimator::OnRenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  if (widget_host != new_render_widget_host_) {
    return;
  }
  // The subscribed `RenderWidgetHost` is getting destroyed. We must cancel the
  // transition and reset everything. This can happen for a client redirect,
  // where Viz never activates a frame from the committed renderer.
  CHECK_EQ(state_, State::kWaitingForNewRendererToDraw);
  CHECK_EQ(navigation_state_, NavigationState::kCommitted);
  AbortAnimation();
}

// This is only called after we subscribe to the new `RenderWidgetHost` when the
// navigation is ready to commit, meaning this method won't be called for
// 204/205/Download navigations, and won't be called if the navigation is
// cancelled.
void BackForwardTransitionAnimator::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  CHECK(tracked_request_);
  // We shouldn't get this notification for subframe navigations because we
  // never subscribe to the `RenderWidgetHost` for subframes.
  //
  // This is for simplicity: non-OOPIF / VideoSubmitter subframes share the same
  // `RenderWidgetHost` with the embedder thus it's difficult to differentiate
  // the frames submitted from a subframe vs from its embedder. For subframe
  // navigations, we play the cross-fade animation as soon as the invoke
  // animation has finished (see `DidFinishNavigation()`'s treatment for
  // subframes).
  CHECK(tracked_request_->is_primary_main_frame);

  // `new_render_widget_host_` and
  // `primary_main_frame_navigation_entry_item_sequence_number_` are set when
  // the navigation is ready to commit.
  CHECK(new_render_widget_host_);
  CHECK_NE(primary_main_frame_navigation_entry_item_sequence_number_,
           cc::RenderFrameMetadata::kInvalidItemSequenceNumber);

  // Viz can activate the frame before the DidCommit message arrives at the
  // browser (kStarted), since we start to get this notification when the
  // browser tells the renderer to commit the navigation.
  CHECK(navigation_state_ == NavigationState::kCommitted ||
        navigation_state_ == NavigationState::kStarted);

  // Again this notification is only received after the browser tells the
  // renderer to commit the navigation. So we must have started playing the
  // invoke animation, or the invoke animation has finished.
  CHECK(state_ == State::kDisplayingInvokeAnimation ||
        state_ == State::kWaitingForNewRendererToDraw)
      << ToString(state_);

  CHECK(!viz_has_activated_first_frame_)
      << "OnRenderFrameMetadataChangedAfterActivation can only be called once.";

  if (new_render_widget_host_->render_frame_metadata_provider()
          ->LastRenderFrameMetadata()
          .primary_main_frame_item_sequence_number !=
      primary_main_frame_navigation_entry_item_sequence_number_) {
    // We shouldn't dismiss the screenshot if the activated frame isn't what we
    // are expecting.
    return;
  }

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

// We only use `DidStartNavigation()` for signalling that the renderer has acked
// the BeforeUnload message to proceed (begin) the navigation.
void BackForwardTransitionAnimator::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!tracked_request_) {
    // We could reach here for an early-commit navigation:
    // - The animator only tracks the request's ID after `GoToIndex()` returns.
    // - In early commit, `DidStartNavigation()` is called during `GoToIndex()`.
    //
    // Early return here and let `StartNavigationAndTrackRequest()` to set the
    // `navigation_state_`.
    return;
  }

  if (tracked_request_->navigation_id != navigation_handle->GetNavigationId()) {
    return;
  }

  CHECK_EQ(navigation_state_, NavigationState::kBeforeUnloadDispatched);
  navigation_state_ = NavigationState::kBeforeUnloadAckedProceed;

  CHECK(state_ == State::kWaitingForBeforeUnloadResponse ||
        state_ == State::kDisplayingCancelAnimation);

  AdvanceAndProcessState(State::kDisplayingInvokeAnimation);
}

void BackForwardTransitionAnimator::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(!navigation_handle->IsSameDocument());

  if (!tracked_request_ ||
      tracked_request_->navigation_id != navigation_handle->GetNavigationId()) {
    // A unrelated navigation is ready to commit. This is possible with
    // NavigationQueuing. We ignore the unrelated navigation request.
    return;
  }

  if (!tracked_request_->is_primary_main_frame) {
    // We don't subscribe to the new widget host for subframes, nor clone the
    // old surface layer.
    return;
  }

  SubscribeToNewRenderWidgetHost(
      static_cast<NavigationRequest*>(navigation_handle));

  // Clone the Surface of the outgoing page for same-RFH navigations. We need to
  // this sooner for these navigations since the SurfaceID is updated when
  // sending the commit message.
  // For cross-RFH navigations, this is done as a part of processing the
  // DidCommit ack from the renderer.
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  auto* old_rfh = RenderFrameHostImpl::FromID(
      navigation_request->GetPreviousRenderFrameHostId());
  auto* new_rfh = navigation_request->GetRenderFrameHost();

  // Ignore early swap cases for example crashed pages. They are same-RFH
  // navigations but the current SurfaceID of this RFH doesn't refer to content
  // from the old Document.
  if (navigation_request->early_render_frame_host_swap_type() ==
          NavigationRequest::EarlyRenderFrameHostSwapType::kNone &&
      old_rfh == new_rfh) {
    MaybeCloneOldSurfaceLayer(old_rfh->GetView());
  }
}

// - For a primary main frame navigation, we only use `DidFinishNavigation()`
// for navigations that never commit (204/205/downloads), or the cancelled /
// replaced navigations. For a committed navigation, everything is set in
// `OnDidNavigatePrimaryMainFramePreCommit()`, which is before the old
// `RenderViewHost` is swapped out.
//
// - For subframe navigation, we bring the fallback UX to the full viewport when
// the subframe navigation commits.
void BackForwardTransitionAnimator::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If we haven't started tracking a navigation, or if `navigation_handle`
  // isn't what we tracked, or if this `navigation_handle` has committed, ignore
  // it.
  //
  // TODO(https://crbug.com/357060513): If we are tracking a subframe request
  // from subframe A while subframe B navigates, the request in subframe B is
  // ignored completely. We should decide what to do before launch.
  if (!tracked_request_ ||
      tracked_request_->navigation_id != navigation_handle->GetNavigationId()) {
    return;
  }

  if (navigation_handle->HasCommitted()) {
    if (navigation_handle->IsInPrimaryMainFrame()) {
      // If this is a committed primary main frame navigation request, we must
      // have already set the states in
      // `OnDidNavigatePrimaryMainFramePreCommit()`.
      CHECK(tracked_request_->is_primary_main_frame);
      CHECK_EQ(navigation_state_, NavigationState::kCommitted);
    } else {
      // If this is a committed subframe request, animate the fallback UX to
      // occupy the full viewport.
      CHECK(!tracked_request_->is_primary_main_frame);
      navigation_state_ = NavigationState::kCommitted;
      physics_model_.OnNavigationFinished(/*navigation_committed=*/true);
      CHECK_EQ(state_, State::kDisplayingInvokeAnimation);
      // Signals that when the invoke animation finishes, play the cross-fade
      // animation directly.
      viz_has_activated_first_frame_ = true;
    }
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

void BackForwardTransitionAnimator::OnDidNavigatePrimaryMainFramePreCommit(
    NavigationRequest* navigation_request,
    RenderFrameHostImpl* old_host,
    RenderFrameHostImpl* new_host) {
  // If a navigation commits in the primary main frame while we are tracking the
  // subframe requests, abort the animation immediately.
  if (tracked_request_ && !tracked_request_->is_primary_main_frame) {
    AbortAnimation();
    return;
  }

  CHECK(navigation_request->IsInPrimaryMainFrame());

  bool skip_all_animations = false;

  switch (state_) {
    case State::kStarted:
      CHECK(!tracked_request_);
      CHECK_EQ(navigation_state_, NavigationState::kNotStarted);
      // A new navigation finished in the primary main frame while the user is
      // swiping across the screen. For simplicity, destroy this class if the
      // new navigation was from the primary main frame.
      skip_all_animations = true;
      break;
    case State::kDisplayingInvokeAnimation: {
      // We can only get to `kDisplayingInvokeAnimation` if we have started
      // tracking the request.
      CHECK(tracked_request_);

      if (navigation_state_ == NavigationState::kStarted) {
        if (tracked_request_->navigation_id !=
            navigation_request->GetNavigationId()) {
          // A previously pending navigation has committed since we started
          // tracking our gesture navigation. Ignore this committed navigation.
          return;
        }

        // Before we display the crossfade animation to show the new page, we
        // need to check if the new page matches the origin of the screenshot.
        // We are not allowed to cross-fade from a screenshot of A.com to a page
        // of B.com.
        bool land_on_error_page = navigation_request->DidEncounterError();
        bool different_commit_origin = false;

        const auto& original_url = navigation_request->GetOriginalRequestURL();
        const auto& committed_url = navigation_request->GetURL();

        // The origin comparison is tricky because we do not know the precise
        // origin of the initial `NavigationRequest` (which depends on response
        // headers like CSP sandbox). It is reasonable to allow the animation to
        // proceed if the origins derived from the URL remains same-origin at
        // the end of the navigation, even if there is a sandboxing difference
        // that leads to an opaque origin. Also, URLs that can inherit origins
        // (e.g., about:blank) do not generally redirect, so it should be safe
        // to ignore inherited origins. Thus, we compare origins derived from
        // the URLs, after first checking whether the URL itself remains
        // unchanged (to account for URLs with opaque origins that won't appear
        // equal to each other, like data: URLs). This addresses concerns about
        // converting between URLs and origins (see
        // https://chromium.googlesource.com/chromium/src/+/main/docs/security/origin-vs-url.md).
        if (original_url != committed_url) {
          different_commit_origin =
              !url::Origin::Create(original_url)
                   .IsSameOriginWith(url::Origin::Create(committed_url));
        }

        if (!land_on_error_page && different_commit_origin) {
          skip_all_animations = true;
          break;
        }

        // Our gesture navigation has committed.
        navigation_state_ = NavigationState::kCommitted;
        physics_model_.OnNavigationFinished(/*navigation_committed=*/true);
        if (land_on_error_page) {
          // TODO(crbug.com/41482489): Implement a different UX if we
          // decide not show the animation at all (i.e. abort animation early
          // when we receive the response header).
        }
        // We need to check if hosts have changed, since they could have stayed
        // the same if the old page was early-swapped out, which can happen in
        // navigations from a crashed page.
        //
        // This is done sooner (in ReadyToCommit) for same-RFH navigations
        // since the SurfaceID changes before DidCommit for these navigations.
        if (old_host != new_host) {
          MaybeCloneOldSurfaceLayer(old_host->GetView());
        }
      } else {
        // Our navigation has already committed while a second navigation
        // commits. This can be a client redirect: A.com -> B.com and B.com's
        // document redirects to C.com, while we are still playing the post
        // commit-pending invoke animation to bring B.com's screenshot to the
        // center of the viewport.
        CHECK_EQ(navigation_state_, NavigationState::kCommitted);
        skip_all_animations = true;
      }
      break;
    }
    case State::kDisplayingCancelAnimation: {
      // We won't reach `NavigationState::kBeforeUnloadDispatched` because
      // if the request is blocked on BeforeUnload ack is cancelled, we will
      // receive `OnUnstartedNavigationCancelled()` where we advance
      // `navigation_state_` to `NavigationState::kCancelledBeforeStart`.

      CHECK(navigation_state_ == NavigationState::kNotStarted ||
            navigation_state_ == NavigationState::kBeforeUnloadDispatched ||
            navigation_state_ == NavigationState::kCancelled ||
            navigation_state_ == NavigationState::kCancelledBeforeStart)
          << ToString(navigation_state_);

      // A navigation finished while we are displaying the cancel animation.
      // For simplicity, destroy `this` and reset everything.
      skip_all_animations = true;
      break;
    }
    case State::kWaitingForNewRendererToDraw:
      // Our navigation has already committed while a second navigation commits.
      // This can be a client redirect: A.com -> B.com and B.com's document
      // redirects to C.com, before B.com's renderer even submits a new frame.
      CHECK_EQ(navigation_state_, NavigationState::kCommitted);
      CHECK(tracked_request_);
      skip_all_animations = true;
      break;
    case State::kWaitingForContentForNavigationEntryShown:
      // Our navigation has already committed while waiting for a native
      // entry to be finished drawing by the embedder.
      CHECK_EQ(navigation_state_, NavigationState::kCommitted);
      CHECK(tracked_request_);
      skip_all_animations = true;
      break;
    case State::kDisplayingCrossFadeAnimation: {
      // Our navigation has already committed while a second navigation commits.
      // This can be a client redirect: A.com -> B.com and B.com's document
      // redirects to C.com, while we are cross-fading from B.com's screenshot
      // to whatever is underneath the screenshot.
      CHECK_EQ(navigation_state_, NavigationState::kCommitted);
      CHECK(tracked_request_);
      skip_all_animations = true;
      break;
    }
    case State::kWaitingForBeforeUnloadResponse:
      skip_all_animations = true;
      break;
    case State::kAnimationFinished:
    case State::kAnimationAborted:
      NOTREACHED_IN_MIGRATION()
          << "No navigations can commit during the animator's destruction "
             "because the destruction is atomic.";
      break;
  }

  if (skip_all_animations) {
    AbortAnimation();
  }
}

void BackForwardTransitionAnimator::AbortAnimation() {
  AdvanceAndProcessState(State::kAnimationAborted);
}

bool BackForwardTransitionAnimator::IsTerminalState() {
  return state_ == State::kAnimationFinished ||
         state_ == State::kAnimationAborted;
}

void BackForwardTransitionAnimator::OnFloatAnimated(
    const float& value,
    int target_property_id,
    gfx::KeyframeModel* keyframe_model) {
  TargetProperty property = static_cast<TargetProperty>(target_property_id);
  CHECK_EQ(effect_.keyframe_models().size(), 1u);
  switch (property) {
    case TargetProperty::kScrim: {
      CHECK(screenshot_scrim_);
      auto scrim = SkColors::kBlack;
      scrim.fA = value;
      screenshot_scrim_->SetBackgroundColor(scrim);
      return;
    }
    case TargetProperty::kCrossFade: {
      CHECK(screenshot_layer_);
      screenshot_layer_->SetOpacity(value);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
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

  if (progress_bar_) {
    progress_bar_->GetLayer()->RemoveFromParent();
    progress_bar_.reset();
  }

  // The scrim timeline is a function of the top layer's position. At the end of
  // the invoke animation, the top layer is completely out of the viewport, so
  // the `KeyFrameModel` for the scrim should also be exhausted and removed.
  CHECK(effect_.keyframe_models().empty());
  if (is_copied_from_embedder_) {
    AdvanceAndProcessState(State::kWaitingForContentForNavigationEntryShown);
  } else if (viz_has_activated_first_frame_) {
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
             to == State::kDisplayingInvokeAnimation ||
             to == State::kAnimationAborted;
    case State::kWaitingForBeforeUnloadResponse:
      return to == State::kDisplayingInvokeAnimation ||
             to == State::kAnimationFinished || to == State::kAnimationAborted;
    case State::kDisplayingInvokeAnimation:
      return to == State::kDisplayingCrossFadeAnimation ||
             to == State::kWaitingForNewRendererToDraw ||
             // A second navigation replaces the current one, or the user hits
             // the stop button.
             to == State::kDisplayingCancelAnimation ||
             to == State::kWaitingForContentForNavigationEntryShown ||
             to == State::kAnimationAborted;
    case State::kWaitingForNewRendererToDraw:
      return to == State::kDisplayingCrossFadeAnimation ||
             to == State::kAnimationAborted;
    case State::kWaitingForContentForNavigationEntryShown:
      return to == State::kAnimationFinished || to == State::kAnimationAborted;
    case State::kDisplayingCrossFadeAnimation:
      return to == State::kAnimationFinished || to == State::kAnimationAborted;
    case State::kDisplayingCancelAnimation:
      return to == State::kAnimationFinished ||
             // The cancel animation has finished for a dispatched BeforeUnload
             // message.
             to == State::kWaitingForBeforeUnloadResponse ||
             // The renderer acks the BeforeUnload message to proceed the
             // navigation, BEFORE the cancel animation finishes.
             to == State::kDisplayingInvokeAnimation ||
             to == State::kAnimationAborted;
    case State::kAnimationFinished:
    case State::kAnimationAborted:
      NOTREACHED();
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
    case State::kWaitingForContentForNavigationEntryShown:
      return "kWaitingForContentForNavigationEntryShown";
    case State::kDisplayingCrossFadeAnimation:
      return "kDisplayingCrossFadeAnimation";
    case State::kAnimationFinished:
      return "kAnimationFinished";
    case State::kWaitingForBeforeUnloadResponse:
      return "kWaitingForBeforeUnloadResponse";
    case State::kAnimationAborted:
      return "kAnimationAborted";
  }
  NOTREACHED();
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
  NOTREACHED();
}

void BackForwardTransitionAnimator::
    InitializeEffectForGestureProgressAnimation() {
  // The KeyFrameModel for scrim is added when we set up the screenshot layer,
  // at which we must have no models yet.
  CHECK(effect_.keyframe_models().empty());

  const blink::web_pref::WebPreferences& web_prefs =
      animation_manager_->web_contents_view_android()
          ->web_contents()
          ->GetOrCreateWebPreferences();

  if (web_prefs.preferred_color_scheme ==
      blink::mojom::PreferredColorScheme::kDark) {
    AddFloatModelToEffect(kScrimAnimationDarkMode, this, effect_);
  } else {
    AddFloatModelToEffect(kScrimAnimationLightMode, this, effect_);
  }
}

void BackForwardTransitionAnimator::InitializeEffectForCrossfadeAnimation() {
  // Before we add the cross-fade model, the scrim model must have finished.
  CHECK(effect_.keyframe_models().empty());

  AddFloatModelToEffect(kCrossFadeAnimation, this, effect_);
}

void BackForwardTransitionAnimator::AdvanceAndProcessState(State state) {
  CHECK(CanAdvanceTo(state_, state))
      << "Cannot advance from " << ToString(state_) << " to "
      << ToString(state);
  auto previous_animation_stage = GetCurrentAnimationStage();
  state_ = state;
  if (previous_animation_stage != GetCurrentAnimationStage()) {
    animation_manager_->OnAnimationStageChanged();
  }
  ProcessState();
}

void BackForwardTransitionAnimator::ProcessState() {
  switch (state_) {
    case State::kStarted: {
      SetupForScreenshotPreview();
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
        NOTREACHED_IN_MIGRATION() << ToString(navigation_state_);
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
      SetupProgressBar();
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
    case State::kWaitingForContentForNavigationEntryShown:
      // No-op.
      break;
    case State::kDisplayingCrossFadeAnimation: {
      // Before we start displaying the crossfade animation,
      // `parent_for_web_page_widgets()` is completely out of the viewport. This
      // layer is reused for new content. For this reason, before we can start
      // the cross-fade we need to bring it back to the center of the viewport.
      ResetTransformForLayer(animation_manager_->web_contents_view_android()
                                 ->parent_for_web_page_widgets());
      ResetTransformForLayer(screenshot_layer_.get());

      // Move the screenshot to the very top, so we can cross-fade from the
      // screenshot (top) into the active page (bottom).
      InsertLayersInOrder();

      InitializeEffectForCrossfadeAnimation();

      CHECK(animation_manager_->web_contents_view_android()
                ->GetTopLevelNativeWindow());
      animation_manager_->web_contents_view_android()
          ->GetTopLevelNativeWindow()
          ->SetNeedsAnimate();
      break;
    }
    case State::kAnimationFinished:
    case State::kAnimationAborted:
      break;
  }
}

void BackForwardTransitionAnimator::SetupForScreenshotPreview() {
  NavigationControllerImpl* nav_controller =
      animation_manager_->navigation_controller();
  auto* destination_entry =
      nav_controller->GetEntryWithUniqueID(destination_entry_id_);
  CHECK(destination_entry);
  auto* preview = static_cast<NavigationEntryScreenshot*>(
      destination_entry->GetUserData(NavigationEntryScreenshot::kUserDataKey));
  CHECK(use_fallback_screenshot_ ||
        preview->navigation_entry_id() == destination_entry_id_);

  if (!use_fallback_screenshot_) {
    auto* cache = nav_controller->GetNavigationEntryScreenshotCache();
    screenshot_ = cache->RemoveScreenshot(destination_entry);
  }

  // The layers can be reused. We need to make sure there is no ongoing
  // transform on the layer of the current `WebContents`'s view.
  auto transform = animation_manager_->web_contents_view_android()
                       ->parent_for_web_page_widgets()
                       ->transform();
  CHECK(transform.IsIdentity()) << transform.ToString();

  if (use_fallback_screenshot_) {
    // For now, the fallback screenshot is only a solid color, without the
    // rounded rectangle and favicon.
    //
    // TODO(crbug/40260440): Implement the UX's spec using the favicon.
    auto screenshot_layer = cc::slim::SolidColorLayer::Create();
    screenshot_layer->SetBackgroundColor(fallback_ux_config_.background_color);
    screenshot_layer_ = std::move(screenshot_layer);
  } else {
    ui_resource_id_ = CreateUIResource(screenshot_.get());
    auto screenshot_layer = cc::slim::UIResourceLayer::Create();
    screenshot_layer->SetUIResourceId(ui_resource_id_);
    screenshot_layer_ = std::move(screenshot_layer);
  }
  screenshot_layer_->SetIsDrawable(true);
  screenshot_layer_->SetPosition(gfx::PointF(0.f, 0.f));
  screenshot_layer_->SetBounds(animation_manager_->web_contents_view_android()
                                   ->GetNativeView()
                                   ->GetPhysicalBackingSize());

  screenshot_scrim_ = cc::slim::SolidColorLayer::Create();
  screenshot_scrim_->SetBounds(screenshot_layer_->bounds());
  screenshot_scrim_->SetIsDrawable(true);
  screenshot_scrim_->SetBackgroundColor(SkColors::kTransparent);

  // Makes sure `screenshot_scrim_` is drawn on top of `screenshot_layer_`.
  screenshot_layer_->AddChild(screenshot_scrim_);
  screenshot_scrim_->SetContentsOpaque(false);

  MaybeCopyContentAreaAsBitmap();

  // This inserts the screenshot layer into the layer tree.
  InsertLayersInOrder();

  // Set up `effect_`.
  InitializeEffectForGestureProgressAnimation();

  // Calling `OnGestureProgressed` manually. This will ask the physics model to
  // move the layers to their respective initial positions.
  OnGestureProgressed(latest_progress_gesture_);
}

void BackForwardTransitionAnimator::SetupProgressBar() {
  const auto& progress_bar_config =
      animation_manager_->web_contents_view_android()
          ->GetNativeView()
          ->GetWindowAndroid()
          ->GetProgressBarConfig();
  if (!progress_bar_config.ShouldDisplay()) {
    return;
  }

  progress_bar_ =
      std::make_unique<ProgressBar>(GetViewportWidthPx(), progress_bar_config);

  // The progress bar should draw on top of the scrim (if any).
  screenshot_layer_->AddChild(progress_bar_->GetLayer());
}

bool BackForwardTransitionAnimator::StartNavigationAndTrackRequest() {
  CHECK(use_fallback_screenshot_ || screenshot_);
  CHECK(!tracked_request_);
  CHECK_EQ(navigation_state_, NavigationState::kNotStarted);

  NavigationControllerImpl* nav_controller =
      animation_manager_->navigation_controller();

  int index = nav_controller->GetEntryIndexWithUniqueID(destination_entry_id_);
  if (index == -1) {
    return false;
  }

  std::vector<base::WeakPtr<NavigationRequest>> requests =
      nav_controller->GoToIndexAndReturnAllRequests(index);
  if (requests.empty()) {
    // The gesture did not create any navigation requests.
    return false;
  }

  for (const auto& request : requests) {
    if (request->IsInPrimaryMainFrame()) {
      TrackRequest(std::move(request));
      return true;
    }
  }

  if (requests.size() > 1U) {
    AbortAnimation();
    return false;
  }

  CHECK(!tracked_request_);
  CHECK_EQ(navigation_state_, NavigationState::kNotStarted);
  TrackRequest(std::move(requests[0]));
  return true;
}

void BackForwardTransitionAnimator::TrackRequest(
    base::WeakPtr<NavigationRequest> created_request) {
  CHECK(created_request);
  // The resulting `NavigationRequest` must be associated with the intended
  // `NavigationEntry`, to safely start the animation.
  //
  // NOTE: A `NavigationRequest` does not always have a `NavigationEntry`, since
  // the entry can be deleted at any time (e.g., clearing history), even during
  // a pending navigation. It's fine to CHECK the entry here because we just
  // created the requests in the same stack. No code yet had a chance to delete
  // the entry.
  CHECK(created_request->GetNavigationEntry());

  int request_entry_id = created_request->GetNavigationEntry()->GetUniqueID();

  // `destination_entry_id_` is initialized in the same stack as
  // `GoToIndexAndReturnAllRequests()`. Thus they must equal.
  CHECK_EQ(destination_entry_id_, request_entry_id);

  tracked_request_ = TrackedRequest{
      .navigation_id = created_request->GetNavigationId(),
      .is_primary_main_frame = created_request->IsInPrimaryMainFrame(),
  };

  if (created_request->IsNavigationStarted()) {
    navigation_state_ = NavigationState::kStarted;
    if (created_request->IsSameDocument() &&
        created_request->IsInPrimaryMainFrame()) {
      // For same-doc navigations, we clone the old surface layer and subscribe
      // to the widget host immediately after sending the "CommitNavigation"
      // message. Once the browser receives the renderer's "DidCommitNavigation"
      // message, it is too late to make a clone or subscribe to the widget
      // host.
      MaybeCloneOldSurfaceLayer(
          created_request->GetRenderFrameHost()->GetView());
      SubscribeToNewRenderWidgetHost(created_request.get());
    }
  } else {
    CHECK(!created_request->IsSameDocument());
    CHECK(created_request->IsWaitingForBeforeUnload());
    navigation_state_ = NavigationState::kBeforeUnloadDispatched;
  }
  created_request->set_was_initiated_by_animated_transition();
}

BackForwardTransitionAnimator::ComputedAnimationValues
BackForwardTransitionAnimator::ComputeAnimationValues(
    const PhysicsModel::Result& result) {
  ComputedAnimationValues values;

  values.progress =
      std::abs(result.foreground_offset_physical) / GetViewportWidthPx();

  if (nav_direction_ == NavigationDirection::kForward) {
    // The physics model assumes the background comes in from slightly outside
    // the viewport. But in forward navigations the live page is in the
    // background, it starts fully in the viewport, and moves slightly
    // offscreen. So shift the live page so that it starts in the viewport.
    float start_from_origin =
        -PhysicsModel::kScreenshotInitialPositionRatio * GetViewportWidthPx();
    values.live_page_offset =
        result.background_offset_physical + start_from_origin;
    // The physics model assumes the foreground starts fully in the viewport and
    // slides out. In a forward navigation the foreground is the screenshot and
    // comes from fully out of the viewport so offset it by the viewport width
    // to make it animate from fully out to fully in.
    values.screenshot_offset =
        result.foreground_offset_physical - GetViewportWidthPx();
  } else {
    values.live_page_offset = result.foreground_offset_physical;
    values.screenshot_offset = result.background_offset_physical;
  }

  // Swipes from the right edge will travel in the opposite direction.
  if (initiating_edge_ == SwipeEdge::RIGHT) {
    values.live_page_offset *= -1;
    values.screenshot_offset *= -1;
  }

  CHECK_GE(values.progress, 0.f);
  CHECK_LE(values.progress, 1.f);

  return values;
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

void BackForwardTransitionAnimator::DeleteUIResource(
    cc::UIResourceId resource_id) {
  ui::WindowAndroid* window = animation_manager_->web_contents_view_android()
                                  ->GetTopLevelNativeWindow();
  CHECK(window);
  ui::WindowAndroidCompositor* compositor = window->GetCompositor();
  CHECK(compositor);
  static_cast<CompositorImpl*>(compositor)->DeleteUIResource(ui_resource_id_);
}

bool BackForwardTransitionAnimator::SetLayerTransformationAndTickEffect(
    const PhysicsModel::Result& result) {
  // Mirror for RTL if needed and swap the layers for forward navigations.
  ComputedAnimationValues values = ComputeAnimationValues(result);

  screenshot_layer_->SetTransform(
      gfx::Transform::MakeTranslation(values.screenshot_offset, 0.f));

  const auto live_page_transform =
      gfx::Transform::MakeTranslation(values.live_page_offset, 0.f);
  animation_manager_->web_contents_view_android()
      ->parent_for_web_page_widgets()
      ->SetTransform(live_page_transform);

  if (old_surface_clone_) {
    CHECK(navigation_state_ == NavigationState::kCommitted ||
          navigation_state_ == NavigationState::kStarted)
        << ToString(navigation_state_);
    CHECK_EQ(state_, State::kDisplayingInvokeAnimation);
    old_surface_clone_->SetTransform(live_page_transform);
  } else if (embedder_live_content_clone_) {
    embedder_live_content_clone_->SetTransform(live_page_transform);
  }

  effect_.Tick(GetFittedTimeTicksForForegroundProgress(values.progress));
  return result.done && effect_.keyframe_models().empty();
}

void BackForwardTransitionAnimator::MaybeCloneOldSurfaceLayer(
    RenderWidgetHostViewBase* old_main_frame_view) {
  // The old View must be still alive (and its renderer).
  CHECK(old_main_frame_view);

  CHECK(!old_surface_clone_);

  if (embedder_live_content_clone_) {
    return;
  }

  const auto* old_surface_layer =
      static_cast<RenderWidgetHostViewAndroid*>(old_main_frame_view)
          ->GetSurfaceLayer();
  old_surface_clone_ = cc::slim::SurfaceLayer::Create();
  // Use a zero deadline because this is a copy of a surface being actively
  // shown. The surface textures are ready (i.e. won't be GC'ed) because
  // `old_surface_clone_` references to them.
  old_surface_clone_->SetSurfaceId(old_surface_layer->surface_id(),
                                   cc::DeadlinePolicy::UseSpecifiedDeadline(0));
  old_surface_clone_->SetPosition(old_surface_layer->position());
  old_surface_clone_->SetBounds(old_surface_layer->bounds());
  old_surface_clone_->SetTransform(old_surface_layer->transform());
  old_surface_clone_->SetIsDrawable(true);

  // Inserts the clone layer into the layer tree.
  InsertLayersInOrder();
}

void BackForwardTransitionAnimator::MaybeCopyContentAreaAsBitmap() {
  SkBitmap bitmap = animation_manager_->MaybeCopyContentAreaAsBitmapSync();
  if (bitmap.empty()) {
    return;
  }
  embedder_live_content_clone_ = cc::slim::UIResourceLayer::Create();
  embedder_live_content_clone_->SetBitmap(bitmap);
  embedder_live_content_clone_->SetIsDrawable(true);
  embedder_live_content_clone_->SetPosition(gfx::PointF(0.f, 0.f));
  embedder_live_content_clone_->SetBounds(
      animation_manager_->web_contents_view_android()
          ->GetNativeView()
          ->GetPhysicalBackingSize());
}

// TODO(crbug.com/350750205): Refactor this function and
// `OnRenderFrameMetadataChangedAfterActivation` to the manager
void BackForwardTransitionAnimator::SubscribeToNewRenderWidgetHost(
    NavigationRequest* navigation_request) {
  CHECK(!new_render_widget_host_);

  if (!navigation_request->GetNavigationEntry()) {
    // Error case: The navigation entry is deleted when the navigation is ready
    // to commit. Abort the transition.
    AbortAnimation();
    return;
  }

  auto* new_host = navigation_request->GetRenderFrameHost();
  CHECK(new_host);
  new_render_widget_host_ = new_host->GetRenderWidgetHost();
  new_render_widget_host_->AddObserver(animation_manager_);

  CHECK_EQ(primary_main_frame_navigation_entry_item_sequence_number_,
           cc::RenderFrameMetadata::kInvalidItemSequenceNumber);

  if (is_copied_from_embedder_) {
    // The embedder will be responsible for cross-fading from the screenshot
    // to the new content. We don't register
    // `RenderFrameMetadataProvider::Observer` and do not set
    // `primary_main_frame_navigation_entry_item_sequence_number_`.
    return;
  }

  new_render_widget_host_->render_frame_metadata_provider()->AddObserver(
      animation_manager_);
  FrameNavigationEntry* frame_nav_entry =
      static_cast<NavigationEntryImpl*>(
          navigation_request->GetNavigationEntry())
          ->GetFrameEntry(new_host->frame_tree_node());
  // This is a session history of the primary main frame. We must have a
  // valid `FrameNavigationEntry`.
  CHECK(frame_nav_entry);
  CHECK_NE(frame_nav_entry->item_sequence_number(), -1);
  primary_main_frame_navigation_entry_item_sequence_number_ =
      frame_nav_entry->item_sequence_number();
}

void BackForwardTransitionAnimator::UnregisterNewFrameActivationObserver() {
  new_render_widget_host_->render_frame_metadata_provider()->RemoveObserver(
      animation_manager_);
  new_render_widget_host_->RemoveObserver(animation_manager_);
  new_render_widget_host_ = nullptr;
}

int BackForwardTransitionAnimator::GetViewportWidthPx() const {
  return animation_manager_->web_contents_view_android()
      ->GetNativeView()
      ->GetPhysicalBackingSize()
      .width();
}

void BackForwardTransitionAnimator::StartInputSuppression() {
  CHECK(!ignore_input_scope_);

  ignore_input_scope_.emplace(animation_manager_->web_contents_view_android()
                                  ->web_contents()
                                  ->IgnoreInputEvents(
                                      /*audit_callback=*/std::nullopt));
}

void BackForwardTransitionAnimator::InsertLayersInOrder() {
  // The layer order when navigating backwards (successive lines decrease in
  // z-order):
  //
  //   WebContentsViewAndroid::view_->GetLayer()
  //      |- `embedder_live_content_clone_`
  //      |- `old_surface_clone_` (only set during the invoke animation
  //           and when `embedder_live_content_clone_` is not set).
  //      |- parent_for_web_page_widgets_ (RWHVAndroid, Overscroll etc).
  //      |-   progress_bar_ (child of screenshot_layer_,
  //                          only during invoke animation)
  //      |-   screenshot_scrim_ (child of screenshot_layer_)
  //      |- screenshot_layer_
  //
  // And when navigating forwards:
  //
  //   WebContentsViewAndroid::view_->GetLayer()
  //      |-   progress_bar_
  //      |-   screenshot_scrim_
  //      |- screenshot_layer_
  //      |- old_surface_clone_
  //      |- parent_for_web_page_widgets_
  //
  // Finally, in both cases -- when the navigation is about to complete -- the
  // screenshot layer is placed over top of the new live page so that the cross
  // fade animation can smoothly transition to the live page:
  //
  //   WebContentsViewAndroid::view_->GetLayer()
  //      |-   screenshot_scrim_
  //      |- screenshot_layer_
  //      |- parent_for_web_page_widgets_

  // This class' layers are removed and reinserted relative to the
  // parent_for_web_page_widgets layer to ensure the ordering is always
  // up-to-date after this call. Remove both layers first, before any
  // re-inserting, to avoid having to bookkeep the changing
  // web_page_widgets_index.
  CHECK(screenshot_layer_);
  if (screenshot_layer_->parent()) {
    screenshot_layer_->RemoveFromParent();
  }
  if (embedder_live_content_clone_) {
    embedder_live_content_clone_->RemoveFromParent();
  } else if (old_surface_clone_) {
    old_surface_clone_->RemoveFromParent();
  }

  cc::slim::Layer* parent_layer =
      animation_manager_->web_contents_view_android()
          ->parent_for_web_page_widgets()
          ->parent();
  const std::vector<scoped_refptr<cc::slim::Layer>> layers =
      parent_layer->children();
  auto itr =
      base::ranges::find(layers, animation_manager_->web_contents_view_android()
                                     ->parent_for_web_page_widgets());
  CHECK(itr != layers.end());
  std::ptrdiff_t web_page_widgets_index = std::distance(layers.begin(), itr);

  // The screenshot layer is shown below the live web page when navigating
  // backwards and above it when navigating forwards. The screenshot is always
  // on top when cross-fading.
  bool screenshot_on_top = nav_direction_ == NavigationDirection::kForward ||
                           state_ == State::kDisplayingCrossFadeAnimation;
  std::ptrdiff_t screenshot_index =
      screenshot_on_top ? web_page_widgets_index + 1 : web_page_widgets_index;
  parent_layer->InsertChild(screenshot_layer_.get(), screenshot_index);

  if (!screenshot_on_top) {
    ++web_page_widgets_index;
  }

  if (embedder_live_content_clone_) {
    // The embedder live content clone is used only when there is a visible
    // native view corresponding to the currently committed navigation entry.
    parent_layer->InsertChild(embedder_live_content_clone_.get(),
                              web_page_widgets_index + 1);
  } else if (old_surface_clone_) {
    // The old page clone is used only when the old live page is swapped out so
    // may be null at other times.

    // The clone is no longer needed when cross-fading - the screenshot layer
    // must always be on top at this time.
    CHECK_NE(state_, State::kDisplayingCrossFadeAnimation);

    // Since the clone represents the old live page it must maintain the
    // ordering relative to the screenshot noted above but must also be shown
    // above the live web page layer. Since the web page widget is already
    // ordered relative to the screenshot, order it directly on top of it.
    parent_layer->InsertChild(old_surface_clone_.get(),
                              web_page_widgets_index + 1);
  }
}

}  // namespace content
