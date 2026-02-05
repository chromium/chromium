// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/blur_transition_animation_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "cc/slim/surface_layer.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace content {

namespace {
constexpr base::TimeDelta kBlurFadeInDuration = base::Milliseconds(100);
constexpr base::TimeDelta kBlurFadeOutDuration = base::Milliseconds(200);
constexpr float kBlurSigma = 12.0f;

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

 private:
  WebContentsViewAndroid* GetView() const {
    if (!web_contents_) {
      return nullptr;
    }
    return reinterpret_cast<WebContentsViewAndroid*>(web_contents_->GetView());
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
  StopFadeInAnimation(TransitionExitReason::kRenderProcessGone,
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
  StopFadeInAnimation(TransitionExitReason::kNavigationInterrupted,
                      /*should_animate_out=*/false);

  // Do not apply to page refreshes or same page navigations.
  // We also exclude back-forward cache navigations because they are typically
  // near-instant, and adding a transition animation could interfere with the
  // user's perception of that immediate feedback.
  if (navigation_handle->IsSameDocument() ||
      navigation_handle->IsServedFromBackForwardCache()) {
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

  blur_layer_ = cc::slim::SurfaceLayer::Create();
  blur_layer_->SetIsDrawable(true);
  blur_layer_->SetSurfaceId(surface_id,
                            cc::DeadlinePolicy::UseSpecifiedDeadline(0));
  blur_layer_->SetStretchContentToFillBounds(true);
  blur_layer_->SetBounds(native_view->GetPhysicalBackingSize());
  blur_layer_->SetFilters(
      {cc::slim::Filter::CreateBlur(kBlurSigma, SkTileMode::kClamp)});

  // Start with 0 opacity for fade-in.
  blur_layer_->SetOpacity(0.0f);

  animation_state_ = AnimationState::kFadeIn;
  is_first_frame_of_animation_ = true;
  initial_animation_offset_ = base::TimeDelta();
  RegisterWindowObserver();

  ShowBlurTransitionLayer(blur_layer_);

  if (auto* window = delegate->GetWindowAndroid()) {
    window->SetNeedsAnimate();
  }
}

void BlurTransitionAnimationManager::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation that just finished is the one we started a blur
  // transition for, and it did not commit, we must hide the blur layer.
  if (navigation_handle->GetNavigationId() == navigation_id_ &&
      (!navigation_handle->HasCommitted())) {
    StopFadeInAnimation(TransitionExitReason::kNavigationInterrupted,
                        /*should_animate_out=*/true);
  }
}

void BlurTransitionAnimationManager::DidFirstVisuallyNonEmptyPaint() {
  StopFadeInAnimation(TransitionExitReason::kFinished,
                      /*should_animate_out=*/true);
}

void BlurTransitionAnimationManager::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  StopFadeInAnimation(TransitionExitReason::kRenderProcessGone,
                      /*should_animate_out=*/false);
}

void BlurTransitionAnimationManager::StopFadeInAnimation(
    TransitionExitReason exit_reason,
    bool should_animate_out) {
  // `navigation_id_` is reset only here. If it is non-zero, it means an
  // animation was running and we haven't recorded the exit reason yet.
  if (navigation_id_ != 0) {
    RecordExitReason(exit_reason);
    navigation_id_ = 0;
  }

  if (!blur_layer_) {
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
  float current_opacity = blur_layer_->opacity();
  // Calculate the equivalent elapsed time to start the fade-out from the
  // current opacity. This ensures that if the fade-in was interrupted
  // mid-way, the fade-out begins seamlessly from that same opacity.
  initial_animation_offset_ = (1.0f - current_opacity) * kBlurFadeOutDuration;

  animation_state_ = AnimationState::kFadeOut;
  is_first_frame_of_animation_ = true;

  RegisterWindowObserver();

  if (auto* delegate = GetWebContentsViewAndroidDelegate()) {
    if (auto* window = delegate->GetWindowAndroid()) {
      window->SetNeedsAnimate();
    }
  }
}

void BlurTransitionAnimationManager::OnAnimate(
    base::TimeTicks frame_begin_time) {
  if (animation_state_ == AnimationState::kNone || !blur_layer_) {
    UnregisterWindowObserver();
    return;
  }

  if (is_first_frame_of_animation_) {
    animation_start_time_ = frame_begin_time - initial_animation_offset_;
    is_first_frame_of_animation_ = false;
  }

  base::TimeDelta elapsed = frame_begin_time - animation_start_time_;
  float progress = 0.0f;
  float opacity = 0.0f;

  if (animation_state_ == AnimationState::kFadeOut) {
    progress = std::clamp(elapsed / kBlurFadeOutDuration, 0.0, 1.0);
    // Fade out: 1.0 -> 0.0
    opacity = 1.0f - gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, progress);
  } else if (animation_state_ == AnimationState::kFadeIn) {
    progress = std::clamp(elapsed / kBlurFadeInDuration, 0.0, 1.0);
    // Fade in: 0.0 -> 1.0
    opacity = gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, progress);
  }

  blur_layer_->SetOpacity(opacity);

  if (progress >= 1.0f) {
    if (animation_state_ == AnimationState::kFadeOut) {
      DestroyLayer();
    } else {
      // Fade in complete.
      animation_state_ = AnimationState::kNone;
      UnregisterWindowObserver();
    }
  } else {
    if (auto* delegate = GetWebContentsViewAndroidDelegate()) {
      if (auto* window = delegate->GetWindowAndroid()) {
        window->SetNeedsAnimate();
      }
    }
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

void BlurTransitionAnimationManager::RecordExitReason(
    TransitionExitReason exit_reason) {
  base::UmaHistogramEnumeration("Navigation.BlurTransitionAnimation.ExitReason",
                                exit_reason);
  last_exit_reason_ = exit_reason;
}

void BlurTransitionAnimationManager::ShowBlurTransitionLayer(
    scoped_refptr<cc::slim::Layer> layer) {
  auto* delegate = GetWebContentsViewAndroidDelegate();
  if (!delegate) {
    return;
  }
  auto native_view = delegate->GetNativeView();
  if (!native_view) {
    return;
  }
  if (auto* layer_tree = native_view->GetLayer()) {
    layer_tree->AddChild(std::move(layer));
  }
}

void BlurTransitionAnimationManager::HideBlurTransitionLayer() {
  if (blur_layer_ && blur_layer_->parent()) {
    blur_layer_->RemoveFromParent();
  }
}

void BlurTransitionAnimationManager::DestroyLayer() {
  HideBlurTransitionLayer();
  // Releasing the layer here drops the reference to the viz::SurfaceId,
  // allowing the old page's pixel buffers to be garbage collected.
  blur_layer_.reset();
  if (animation_state_ != AnimationState::kNone) {
    animation_state_ = AnimationState::kNone;
    UnregisterWindowObserver();
  }
}

}  // namespace content
