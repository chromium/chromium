// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/blur_transition_animation_manager.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace content {

namespace {

// Configurable params for the animation defined under the feature
// kAndroidNavigationBlurTransitionAnimation.

static base::TimeDelta GetBlurHoldDuration() {
  static const base::TimeDelta duration = base::Milliseconds(
      features::kAndroidNavigationAnimationBlurHoldDuration.Get());
  return duration;
}

static base::TimeDelta GetBlurFadeOutDuration() {
  static const base::TimeDelta duration = base::Milliseconds(
      features::kAndroidNavigationAnimationFadeOutDuration.Get());
  return duration;
}

static float GetBlurSigma() {
  static const float sigma = base::saturated_cast<float>(
      features::kAndroidNavigationAnimationBlurSigma.Get());
  return sigma;
}

class WebContentsViewAndroidDelegateImpl
    : public BlurTransitionAnimationManager::WebContentsViewAndroidDelegate {
 public:
  explicit WebContentsViewAndroidDelegateImpl(WebContentsImpl* web_contents)
      : web_contents_(web_contents) {}
  ~WebContentsViewAndroidDelegateImpl() override = default;

  // WebContentsViewAndroidDelegate:
  bool ShouldShowBlurTransitionAnimation(
      NavigationHandle* navigation_handle) override {
    if (auto* view = GetView()) {
      return view->ShouldShowBlurTransitionAnimation(navigation_handle);
    }
    return false;
  }

  BackForwardTransitionAnimationManager*
  GetBackForwardTransitionAnimationManager() override {
    if (auto* view = GetView()) {
      return view->GetBackForwardTransitionAnimationManager();
    }
    return nullptr;
  }

  gfx::NativeView GetNativeView() override {
    if (auto* view = GetView()) {
      return view->GetNativeView();
    }
    return gfx::NativeView();
  }

  ui::WindowAndroid* GetWindowAndroid() override {
    if (auto* view = GetView()) {
      return view->GetTopLevelNativeWindow();
    }
    return nullptr;
  }

  viz::SurfaceId GetCurrentSurfaceId() override {
    if (!web_contents_) {
      return viz::SurfaceId();
    }
    auto* rwhv = static_cast<RenderWidgetHostViewAndroid*>(
        web_contents_->GetRenderWidgetHostView());
    if (!rwhv) {
      return viz::SurfaceId();
    }
    return rwhv->GetCurrentSurfaceId();
  }

  std::optional<SkColor> GetThemeColor() override {
    if (!web_contents_) {
      return std::nullopt;
    }
    return web_contents_->GetThemeColor();
  }

 private:
  WebContentsViewAndroid* GetView() const {
    if (!web_contents_) {
      return nullptr;
    }
    return static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  }

  raw_ptr<WebContentsImpl> web_contents_;
};

}  // namespace

// static
void BlurTransitionAnimationManager::CreateForWebContents(
    WebContents* web_contents) {
  if (FromWebContents(web_contents)) {
    return;
  }
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new BlurTransitionAnimationManager(web_contents)));
}

// static
BlurTransitionAnimationManager* BlurTransitionAnimationManager::FromWebContents(
    WebContents* web_contents) {
  return static_cast<BlurTransitionAnimationManager*>(
      web_contents->GetUserData(UserDataKey()));
}

// static
const void* BlurTransitionAnimationManager::UserDataKey() {
  static const int kUserDataKey = 0;
  return &kUserDataKey;
}

BlurTransitionAnimationManager::BlurTransitionAnimationManager(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

BlurTransitionAnimationManager::~BlurTransitionAnimationManager() {
  SignalExit(TransitionExitReason::kRenderProcessGone,
             /*should_animate_out=*/false);
  UnregisterWindowObserver();
}

BlurTransitionAnimationManager::WebContentsViewAndroidDelegate*
BlurTransitionAnimationManager::GetWebContentsViewAndroidDelegate() {
  if (!web_contents_view_android_delegate_) {
    web_contents_view_android_delegate_ =
        std::make_unique<WebContentsViewAndroidDelegateImpl>(
            static_cast<WebContentsImpl*>(web_contents()));
  }
  return web_contents_view_android_delegate_.get();
}

void BlurTransitionAnimationManager::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // If there's an existing animation from a previous navigation, stop it
  // regardless of whether the new navigation will show one. This prevents
  // stale blur layers from persisting.
  SignalExit(TransitionExitReason::kNavigationInterrupted,
             /*should_animate_out=*/false);

  // Do not apply to page refreshes or same page navigations.
  // We also exclude back-forward cache navigations because they are typically
  // near-instant, and adding a transition animation could interfere with the
  // user's perception of that immediate feedback.
  if (navigation_handle->IsSameDocument() ||
      navigation_handle->IsServedFromBackForwardCache() ||
      navigation_handle->GetReloadType() != ReloadType::NONE) {
    return;
  }

  // To avoid interfering with animations and custom routing logic implemented
  // by websites, this transition is restricted to cross-site navigations.
  const GURL& prev_url = web_contents()->GetLastCommittedURL();
  if (prev_url.is_valid() &&
      navigation_handle->GetURL().host() == prev_url.host()) {
    return;
  }

  auto* delegate = GetWebContentsViewAndroidDelegate();
  if (!delegate) {
    return;
  }

  // If a native back/forward gesture animation is active, we should not
  // interfere with it.
  auto* animation_manager =
      delegate->GetBackForwardTransitionAnimationManager();
  if (animation_manager &&
      animation_manager->GetCurrentAnimationStage() !=
          BackForwardTransitionAnimationManager::AnimationStage::kNone) {
    return;
  }

  bool should_show_animation =
      delegate->ShouldShowBlurTransitionAnimation(navigation_handle);

  if (!should_show_animation) {
    return;
  }

  const viz::SurfaceId& surface_id = delegate->GetCurrentSurfaceId();
  if (!surface_id.is_valid()) {
    return;
  }

  auto* native_view = delegate->GetNativeView();
  if (!native_view) {
    return;
  }

  navigation_id_ = navigation_handle->GetNavigationId();

  if (auto* rfh = navigation_handle->GetRenderFrameHost()) {
    target_rfh_id_ = rfh->GetGlobalId();
  }

  blur_layer_ = cc::slim::SurfaceLayer::Create();
  blur_layer_->SetIsDrawable(true);
  blur_layer_->SetSurfaceId(surface_id,
                            cc::DeadlinePolicy::UseSpecifiedDeadline(0));
  blur_layer_->SetStretchContentToFillBounds(true);
  blur_layer_->SetBounds(native_view->GetPhysicalBackingSize());

  // Prepare the layer with opacity and blur, but don't make layer visible yet.
  blur_layer_->SetOpacity(1.0f);
  blur_layer_->SetFilters(
      {cc::slim::Filter::CreateBlur(GetBlurSigma(), SkTileMode::kClamp)});

  // Prepare fallback solid color layer that returns user to old navigation
  // experience with theme background color if blur layer duration expires.
  fallback_color_layer_ = cc::slim::SolidColorLayer::Create();
  fallback_color_layer_->SetIsDrawable(true);
  fallback_color_layer_->SetBounds(native_view->GetPhysicalBackingSize());
  SkColor4f background_color = SkColors::kWhite;
  if (auto theme_color = delegate->GetThemeColor()) {
    background_color = SkColor4f::FromColor(*theme_color);
  }
  fallback_color_layer_->SetBackgroundColor(background_color);
  fallback_color_layer_->SetOpacity(0.0f);

  SetAnimationState(AnimationState::kNone);
}

void BlurTransitionAnimationManager::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (new_state != RenderFrameHost::LifecycleState::kActive ||
      !render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_id_ == 0 || !blur_layer_ ||
      render_frame_host->GetGlobalId() != target_rfh_id_) {
    return;
  }

  auto* delegate = GetWebContentsViewAndroidDelegate();
  if (!delegate) {
    DestroyLayer();
    return;
  }

  ShowBlurTransitionLayer();

  SetAnimationState(AnimationState::kBlurShown);
  blur_hold_timer_.Start(
      FROM_HERE, GetBlurHoldDuration(),
      base::BindOnce(&BlurTransitionAnimationManager::OnBlurHoldTimerExpired,
                     weak_factory_.GetWeakPtr()));
}

void BlurTransitionAnimationManager::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation that just finished is the one we started a blur
  // transition for, and it did not commit, we must hide the blur layer.
  if (navigation_handle->GetNavigationId() == navigation_id_ &&
      (!navigation_handle->HasCommitted())) {
    SignalExit(TransitionExitReason::kNavigationInterrupted,
               /*should_animate_out=*/true);
  }
}

void BlurTransitionAnimationManager::DidFirstVisuallyNonEmptyPaint() {
  // If the page is ready before the blur hold timer expires, start the reveal.
  // Since other paths reset `navigation_id_`, this specifically handles the
  // direct transition from the blur layer to fade out.
  if (navigation_id_ != 0) {
    SignalExit(TransitionExitReason::kFinished,
               /*should_animate_out=*/true);
    return;
  }

  if (animation_state_ != AnimationState::kFadeOut &&
      (blur_layer_ || fallback_color_layer_)) {
    // If the timer already expired, we still need to trigger the final
    // fade out of the fallback layer now that the new page is ready.
    StartFadeOut();
  }
}

void BlurTransitionAnimationManager::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  SignalExit(TransitionExitReason::kRenderProcessGone,
             /*should_animate_out=*/false);
}

void BlurTransitionAnimationManager::SignalExit(
    TransitionExitReason exit_reason,
    bool should_animate_out) {
  ResetNavigationState(exit_reason);

  if (!blur_layer_ && !fallback_color_layer_) {
    return;
  }

  // If we are already fading out, we only interrupt if we need to force stop.
  if (should_animate_out && animation_state_ != AnimationState::kFadeOut) {
    StartFadeOut();
  } else if (!should_animate_out) {
    DestroyLayer();
  }
}

void BlurTransitionAnimationManager::StartFadeOut() {
  blur_hold_timer_.Stop();

  // If we haven't started fading to the fallback color yet, ensure the
  // fallback layer is hidden so it doesn't "pop" in during the reveal.
  if (animation_state_ != AnimationState::kFadeToFallbackColor &&
      animation_state_ != AnimationState::kFallbackShown &&
      fallback_color_layer_) {
    fallback_color_layer_->SetOpacity(0.0f);
  }

  SetAnimationState(AnimationState::kFadeOut);
  RequestAnimate();
}

void BlurTransitionAnimationManager::OnBlurHoldTimerExpired() {
  ResetNavigationState(TransitionExitReason::kAnimationTimerExpired);
  SetAnimationState(AnimationState::kFadeToFallbackColor);
  RequestAnimate();
}

void BlurTransitionAnimationManager::OnAnimate(
    base::TimeTicks frame_begin_time) {
  if (animation_state_ == AnimationState::kNone ||
      animation_state_ == AnimationState::kBlurShown ||
      animation_state_ == AnimationState::kFallbackShown ||
      (!blur_layer_ && !fallback_color_layer_)) {
    UnregisterWindowObserver();
    return;
  }

  if (animation_phase_start_time_.is_null()) {
    animation_phase_start_time_ = frame_begin_time;
  }

  base::TimeDelta elapsed = frame_begin_time - animation_phase_start_time_;
  float progress = 0.0f;
  float alpha = 0.0f;

  switch (animation_state_) {
    case AnimationState::kFadeOut: {
      progress = std::clamp(elapsed / GetBlurFadeOutDuration(), 0.0, 1.0);
      // We scale the starting opacity (not necessarily 1.0) down to zero over
      // the full duration of the fade out phase. This ensures that the fade out
      // always takes the allocated duration of time, providing a consistent
      // visual experience for the transition regardless of the starting state.
      alpha = gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, progress);
      float adjusted_opacity_multiplier = 1.0f - alpha;

      if (blur_layer_) {
        blur_layer_->SetOpacity(initial_blur_opacity_ *
                                adjusted_opacity_multiplier);
      }
      if (fallback_color_layer_) {
        fallback_color_layer_->SetOpacity(initial_fallback_opacity_ *
                                          adjusted_opacity_multiplier);
      }
      break;
    }
    case AnimationState::kFadeToFallbackColor: {
      progress = std::clamp(elapsed / GetBlurFadeOutDuration(), 0.0, 1.0);
      alpha = gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, progress);
      // Keep the blur layer at its initial opacity while the fallback
      // fades in over it to ensure the new page isn't visible during the
      // transition.
      if (blur_layer_) {
        blur_layer_->SetOpacity(initial_blur_opacity_);
      }
      if (fallback_color_layer_) {
        fallback_color_layer_->SetOpacity(initial_fallback_opacity_ +
                                          (1.0f - initial_fallback_opacity_) *
                                              alpha);
      }
      break;
    }
    case AnimationState::kBlurShown:
    case AnimationState::kFallbackShown:
    case AnimationState::kNone:
      NOTREACHED();
  }

  if (progress >= 1.0f) {
    if (animation_state_ == AnimationState::kFadeOut) {
      DestroyLayer();
    } else if (animation_state_ == AnimationState::kFadeToFallbackColor) {
      SetAnimationState(AnimationState::kFallbackShown);
      UnregisterWindowObserver();
      // Drop the old page's surface now that it's completely covered.
      if (blur_layer_) {
        blur_layer_->RemoveFromParent();
        blur_layer_.reset();
      }
    }
  } else {
    RequestAnimate();
  }
}

void BlurTransitionAnimationManager::RegisterWindowObserver() {
  if (is_window_observer_registered_) {
    return;
  }
  if (auto* delegate = GetWebContentsViewAndroidDelegate()) {
    if (auto* native_view = delegate->GetNativeView()) {
      if (auto* window = native_view->GetWindowAndroid()) {
        window->AddObserver(this);
        is_window_observer_registered_ = true;
      }
    }
  }
}

void BlurTransitionAnimationManager::UnregisterWindowObserver() {
  if (!is_window_observer_registered_) {
    return;
  }
  if (auto* delegate = GetWebContentsViewAndroidDelegate()) {
    if (auto* native_view = delegate->GetNativeView()) {
      if (auto* window = native_view->GetWindowAndroid()) {
        window->RemoveObserver(this);
      }
    }
  }
  is_window_observer_registered_ = false;
}

void BlurTransitionAnimationManager::RequestAnimate() {
  RegisterWindowObserver();
  if (auto* delegate = GetWebContentsViewAndroidDelegate()) {
    if (auto* window = delegate->GetWindowAndroid()) {
      window->SetNeedsAnimate();
    }
  }
}

void BlurTransitionAnimationManager::RecordExitReason(
    TransitionExitReason exit_reason) {
  base::UmaHistogramEnumeration("Navigation.BlurTransitionAnimation.ExitReason",
                                exit_reason);
  last_exit_reason_ = exit_reason;
}

void BlurTransitionAnimationManager::SetAnimationState(AnimationState state) {
  animation_state_ = state;
  animation_phase_start_time_ = base::TimeTicks();
  // Save the opacities of the layers at animation state change so animation
  // bases off current opacity levels rather than jumping to random values.
  initial_blur_opacity_ = blur_layer_ ? blur_layer_->opacity() : 0.0f;
  initial_fallback_opacity_ =
      fallback_color_layer_ ? fallback_color_layer_->opacity() : 0.0f;
}

void BlurTransitionAnimationManager::ResetNavigationState(
    TransitionExitReason exit_reason) {
  if (navigation_id_ != 0) {
    RecordExitReason(exit_reason);
    navigation_id_ = 0;
    target_rfh_id_ = GlobalRenderFrameHostId();
  }
}

void BlurTransitionAnimationManager::ShowBlurTransitionLayer() {
  auto* delegate = GetWebContentsViewAndroidDelegate();
  if (!delegate) {
    return;
  }
  auto native_view = delegate->GetNativeView();
  if (!native_view) {
    return;
  }
  if (auto* layer_tree = native_view->GetLayer()) {
    if (blur_layer_) {
      layer_tree->AddChild(blur_layer_);
    }
    if (fallback_color_layer_) {
      layer_tree->AddChild(fallback_color_layer_);
    }
  }
}

void BlurTransitionAnimationManager::HideBlurTransitionLayer() {
  if (blur_layer_ && blur_layer_->parent()) {
    blur_layer_->RemoveFromParent();
  }
  if (fallback_color_layer_ && fallback_color_layer_->parent()) {
    fallback_color_layer_->RemoveFromParent();
  }
}

void BlurTransitionAnimationManager::DestroyLayer() {
  blur_hold_timer_.Stop();
  HideBlurTransitionLayer();
  // Releasing the layer here drops the reference to the viz::SurfaceId,
  // allowing the old page's pixel buffers to be garbage collected.
  blur_layer_.reset();
  fallback_color_layer_.reset();
  if (animation_state_ != AnimationState::kNone) {
    animation_state_ = AnimationState::kNone;
    UnregisterWindowObserver();
  }
}

}  // namespace content
