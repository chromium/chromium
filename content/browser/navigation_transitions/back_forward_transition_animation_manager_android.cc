// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"

#include <stdint.h>

#include <algorithm>

#include "base/android/callback_android.h"
#include "base/debug/crash_logging.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_delegate.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/BackForwardTransitionAnimationManagerImpl_jni.h"

namespace content {

namespace {

// TODO(crbug.com/353766658): Move these shorthands to a proper header file.
using NavigationDirection =
    BackForwardTransitionAnimationManager::NavigationDirection;

using AnimationStage = BackForwardTransitionAnimationManager::AnimationStage;
using SwipeEdge = ui::BackGestureEventSwipeEdge;
using AnimationAbortReason =
    BackForwardTransitionAnimator::AnimationAbortReason;

}  // namespace

BackForwardTransitionAnimationManagerAndroid::
    BackForwardTransitionAnimationManagerAndroid(
        WebContentsViewAndroid* web_contents_view_android,
        NavigationControllerImpl* navigation_controller)
    : web_contents_view_android_(web_contents_view_android),
      navigation_controller_(navigation_controller),
      animator_factory_(
          std::make_unique<BackForwardTransitionAnimator::Factory>()) {}

BackForwardTransitionAnimationManagerAndroid::
    ~BackForwardTransitionAnimationManagerAndroid() {
  // `this` must be destroyed before the `NavigationController`.
  CHECK(navigation_controller_);
  if (animator_) {
    animator_->AbortAnimation(AnimationAbortReason::kAnimationManagerDestroyed);
    DestroyAnimator();
  }
  if (!java_object_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_BackForwardTransitionAnimationManagerImpl_destroy(env, java_object_);
  }
}

base::android::ScopedJavaLocalRef<jobject>
BackForwardTransitionAnimationManagerAndroid::GetJavaObject(JNIEnv* env) {
  if (java_object_.is_null()) {
    java_object_.Reset(Java_BackForwardTransitionAnimationManagerImpl_create(
        env, reinterpret_cast<intptr_t>(this)));
  }
  return base::android::ScopedJavaLocalRef<jobject>(env, java_object_);
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureStarted(
    const ui::BackGestureEvent& gesture,
    SwipeEdge edge,
    NavigationDirection navigation_direction) {

  if (animator_) {
    // It's possible for a user to start a second gesture when the first gesture
    // animation is still on-going (aka "chained back"). For now, abort the
    // previous animation (impl's dtor will reset the layer's position and
    // reclaim all the resources).
    //
    // TODO(crbug.com/40261105): We need a proper UX to support this.
    animator_->AbortAnimation(AnimationAbortReason::kChainedBack);
    DestroyAnimator();
  }

  std::optional<int> index =
      navigation_direction == NavigationDirection::kForward
          ? navigation_controller_->GetIndexForGoForward()
          : navigation_controller_->GetIndexForGoBack();
  CHECK(index.has_value())
      << "The embedder should only delegate the history navigation task to "
         "this manager if there is a destination entry.";
  auto* destination_entry = navigation_controller_->GetEntryAtIndex(*index);
  CHECK(destination_entry)
      << "The embedder should only delegate the history navigation task to "
         "this manager if there is a destination entry.";

  // Each previous gesture should finished with `OnGestureCancelled()` or
  // `OnGestureInvoked()`. In both cases we reset `destination_entry_id_` to
  // -1.
  CHECK_EQ(destination_entry_id_, NavigationTransitionData::kInvalidId);
  destination_entry_id_ =
      destination_entry->navigation_transition_data().unique_id();

  if (!ShouldAnimateNavigationTransition(navigation_direction, edge)) {
    TRACE_EVENT(
        "browser,navigation",
        "BackForwardTransitionAnimationManagerAndroid::OnGestureStarted");
    return;
  }

  CHECK(animator_factory_);
  animator_ = animator_factory_->Create(
      web_contents_view_android_.get(), navigation_controller_.get(), gesture,
      navigation_direction, edge, destination_entry,
      MaybeCopyContentAreaAsBitmapSync(), this);

  // Become a WCO as soon as this class is created, because we want to
  // observe all navigations while this class is controlling the UI. This
  // allows us to ensure the visuals displayed align with the active page
  // and URL in the URL bar.
  WebContentsObserver::Observe(
      this->web_contents_view_android()->web_contents());
  auto* window = web_contents_view_android()->GetTopLevelNativeWindow();
  CHECK(window);
  window->AddObserver(this);
  web_contents_view_android()->GetNativeView()->AddObserver(this);

  OnAnimationStageChanged();
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  if (animator_) {
    animator_->OnGestureProgressed(gesture);
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureCancelled() {
  CHECK_NE(destination_entry_id_, NavigationTransitionData::kInvalidId);
  if (animator_) {
    animator_->OnGestureCancelled();
    MaybeDestroyAnimator();
  }
  destination_entry_id_ = NavigationTransitionData::kInvalidId;
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureInvoked() {
  CHECK_NE(destination_entry_id_, NavigationTransitionData::kInvalidId);
  if (animator_) {
    animator_->OnGestureInvoked();
    MaybeDestroyAnimator();
  } else {
    int index =
        NavigationTransitionUtils::FindEntryIndexForNavigationTransitionID(
            navigation_controller_, destination_entry_id_);
    if (index != -1) {
      navigation_controller_->GoToIndex(index);
    }
  }
  destination_entry_id_ = NavigationTransitionData::kInvalidId;
}

void BackForwardTransitionAnimationManagerAndroid::
    OnContentForNavigationEntryShown() {
  if (animator_) {
    animator_->OnContentForNavigationEntryShown();
    MaybeDestroyAnimator();
  }
}

AnimationStage
BackForwardTransitionAnimationManagerAndroid::GetCurrentAnimationStage() {
  return animator_ ? animator_->GetCurrentAnimationStage()
                   : AnimationStage::kNone;
}

void BackForwardTransitionAnimationManagerAndroid::SetFavicon(
    const SkBitmap& favicon) {
  auto* entry = web_contents_view_android_->web_contents()
                    ->GetController()
                    .GetLastCommittedEntry();
  CHECK(entry);
  entry->navigation_transition_data().set_favicon(favicon);
}

void BackForwardTransitionAnimationManagerAndroid::OnDetachedFromWindow() {
  // The WebContentsViewAndroid's native view is detached from the top level
  // window. We must abort the transition.
  CHECK(animator_);
  animator_->AbortAnimation(AnimationAbortReason::kDetachedFromWindow);
  DestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnRootWindowVisibilityChanged(bool visible) {
  CHECK(animator_);
  if (!visible) {
    animator_->AbortAnimation(
        AnimationAbortReason::kRootWindowVisibilityChanged);
    DestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnDetachCompositor() {
  CHECK(animator_);
  animator_->AbortAnimation(AnimationAbortReason::kCompositorDetached);
  DestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::OnAnimate(
    base::TimeTicks frame_begin_time) {
  animator_->OnAnimate(frame_begin_time);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::RenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  animator_->OnRenderWidgetHostDestroyed(widget_host);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnRenderFrameMetadataChangedAfterActivation(
        base::TimeTicks activation_time) {
  animator_->OnRenderFrameMetadataChangedAfterActivation(activation_time);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  animator_->DidStartNavigation(navigation_handle);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  animator_->ReadyToCommitNavigation(navigation_handle);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  animator_->DidFinishNavigation(navigation_handle);
  MaybeDestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) {
  CHECK(animator_);
  animator_->AbortAnimation(
      AnimationAbortReason::kPrimaryMainFrameRenderProcessDestroyed);
  DestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnDidNavigatePrimaryMainFramePreCommit(
        NavigationRequest* navigation_request,
        RenderFrameHostImpl* old_host,
        RenderFrameHostImpl* new_host) {
  if (animator_) {
    animator_->OnDidNavigatePrimaryMainFramePreCommit(navigation_request,
                                                      old_host, new_host);
    MaybeDestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::
    OnNavigationCancelledBeforeStart(NavigationHandle* navigation_handle) {
  if (animator_) {
    animator_->OnNavigationCancelledBeforeStart(navigation_handle);
    MaybeDestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnAnimationStageChanged() {
  if (auto* delegate =
          web_contents_view_android()->web_contents()->GetDelegate()) {
    delegate->DidBackForwardTransitionAnimationChange();
  }
}

void BackForwardTransitionAnimationManagerAndroid::
    OnPhysicalBackingSizeChanged() {
  if (!animator_) {
    return;
  }
  animator_->AbortAnimation(AnimationAbortReason::kPhysicalSizeChanged);
  DestroyAnimator();
}

void BackForwardTransitionAnimationManagerAndroid::OnBeforeUnloadDialogShown(
    int64_t navigation_id) {
  if (!animator_) {
    return;
  }
  animator_->OnBeforeUnloadDialogShown(navigation_id);
  MaybeDestroyAnimator();
}

SkBitmap BackForwardTransitionAnimationManagerAndroid::
    MaybeCopyContentAreaAsBitmapSync() {
  return web_contents_view_android()
      ->web_contents()
      ->GetDelegate()
      ->MaybeCopyContentAreaAsBitmapSync();
}

SkBitmap BackForwardTransitionAnimationManagerAndroid::
    GetBackForwardTransitionFallbackUXInternalPageIcon() {
  return web_contents_view_android()
      ->web_contents()
      ->GetDelegate()
      ->GetBackForwardTransitionFallbackUXInternalPageIcon();
}

void BackForwardTransitionAnimationManagerAndroid::MaybeDestroyAnimator() {
  CHECK(animator_);
  if (animator_->IsTerminalState()) {
    DestroyAnimator();
  }
}

void BackForwardTransitionAnimationManagerAndroid::DestroyAnimator() {
  CHECK(animator_);
  WebContentsObserver::Observe(nullptr);
  auto* window = web_contents_view_android()->GetTopLevelNativeWindow();
  CHECK(window);
  window->RemoveObserver(this);
  web_contents_view_android()->GetNativeView()->RemoveObserver(this);
  animator_.reset();
  OnAnimationStageChanged();
}

// JNI entry points for the Java peer.
void BackForwardTransitionAnimationManagerAndroid::OnGestureStarted(
    JNIEnv* env,
    float progress,
    ui::BackGestureEventSwipeEdge edge,
    bool forward,
    bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackStarted", "gesture mode", is_gesture_mode);
  auto* window =
      web_contents_view_android()->GetNativeView()->GetWindowAndroid();
  CHECK(window);
  CHECK(window->GetCompositor());
  OnGestureStarted(
      ui::BackGestureEvent(progress), edge,
      forward ? NavigationDirection::kForward : NavigationDirection::kBackward);
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureProgressed(
    JNIEnv* env,
    float progress,
    bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackProgressed", "gesture mode", is_gesture_mode);
  OnGestureProgressed(ui::BackGestureEvent(std::clamp(progress, 0.f, 1.f)));
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureCancelled(
    JNIEnv* env,
    bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackCancelled", "gesture mode", is_gesture_mode);
  OnGestureCancelled();
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureInvoked(
    JNIEnv* env,
    bool is_gesture_mode) {
  SCOPED_CRASH_KEY_BOOL("OnBackInvoked", "gesture mode", is_gesture_mode);
  OnGestureInvoked();
}

static bool
JNI_BackForwardTransitionAnimationManagerImpl_ShouldAnimateNavigationTransition(
    JNIEnv* env,
    bool forward,
    ui::BackGestureEventSwipeEdge edge) {
  return BackForwardTransitionAnimationManager::
      ShouldAnimateNavigationTransition(forward
                                            ? NavigationDirection::kForward
                                            : NavigationDirection::kBackward,
                                        edge);
}

static bool
JNI_BackForwardTransitionAnimationManagerImpl_ShouldAnimateBackForwardTransitions(
    JNIEnv* env) {
  return BackForwardTransitionAnimationManager::
      ShouldAnimateBackForwardTransitions();
}

static base::OnceClosure
JNI_BackForwardTransitionAnimationManagerImpl_SetMinRequiredPhysicalRamMbForTesting(
    int32_t mb) {
  return base::DoNothingWithBoundArgs(
      NavigationTransitionConfig::
          SetMinRequiredPhysicalRamMbForTesting(  // IN-TEST
              mb));
}

}  // namespace content

DEFINE_JNI(BackForwardTransitionAnimationManagerImpl)
