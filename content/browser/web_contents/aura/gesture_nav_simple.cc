// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/gesture_nav_simple.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "cc/paint/paint_flags.h"
#include "components/vector_icons/vector_icons.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/web_contents/aura/types.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/preloading.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"

namespace content {

namespace {

// Parameters defining the arrow icon inside the affordance.
constexpr int kArrowSize = 20;
constexpr SkColor kArrowColorBeforeActivation = gfx::kGoogleBlue600;
constexpr SkColor kArrowColorAfterActivation = SK_ColorWHITE;

// Parameters defining the background circle of the affordance.
constexpr int kBackgroundRadius = 20;
constexpr SkColor kBackgroundColorBeforeActivation = SK_ColorWHITE;
constexpr SkColor kBackgroundColorAfterActivation = gfx::kGoogleBlue600;
constexpr int kBgShadowOffsetY = 2;
constexpr int kBgShadowBlurRadius = 8;
constexpr SkColor kBgShadowColor = SkColorSetA(SK_ColorBLACK, 0x4D);

// Parameters defining the affordance ripple. The ripple fades in and grows as
// the user drags the affordance until it reaches |kMaxRippleRadius|. If the
// overscroll is successful, the ripple will burst by fading out and growing to
// |kMaxRippleBurstRadius|.
constexpr int kMaxRippleRadius = 40;
constexpr SkColor kRippleColor = SkColorSetA(gfx::kGoogleBlue600, 0x4C);  // 30%

constexpr int kMaxRippleBurstRadius = 48;
constexpr gfx::Tween::Type kBurstAnimationTweenType =
    gfx::Tween::FAST_OUT_SLOW_IN;
constexpr auto kRippleBurstAnimationDuration = base::Milliseconds(200);

// Offset of the affordance when it is at the activation threshold. Since the
// affordance is initially out of content bounds, this is the offset of the
// farther side of the affordance (which equals 128 + 18).
constexpr int kAffordanceActivationOffset = 146;

// Extra offset of the affordance when it is dragged past the activation
// threshold.
constexpr int kAffordanceExtraOffset = 72;
constexpr gfx::Tween::Type kExtraDragTweenType =
    gfx::Tween::Type::FAST_OUT_SLOW_IN;

constexpr float kExtraAffordanceRatio =
    static_cast<float>(kAffordanceExtraOffset) /
    static_cast<float>(kAffordanceActivationOffset);

// Parameters defining animation when the affordance is aborted.
constexpr gfx::Tween::Type kAbortAnimationTweenType =
    gfx::Tween::FAST_OUT_SLOW_IN;
constexpr auto kAbortAnimationDuration = base::Milliseconds(300);

bool ShouldNavigateForward(NavigationController* controller,
                           OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST) &&
         controller->CanGoForward();
}

bool ShouldNavigateBack(NavigationController* controller, OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST) &&
         controller->CanGoBack();
}

bool ShouldReload(const NavigationController& controller, OverscrollMode mode) {
  return mode == OVERSCROLL_SOUTH;
}

NavigationDirection GetDirectionFromMode(OverscrollMode mode) {
  if (mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST)) {
    return NavigationDirection::BACK;
  }
  if (mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST)) {
    return NavigationDirection::FORWARD;
  }
  if (mode == OVERSCROLL_SOUTH) {
    return NavigationDirection::RELOAD;
  }
  return NavigationDirection::NONE;
}

// Records UMA histogram and also user action for the cancelled overscroll.
void RecordGestureOverscrollCancelled(NavigationDirection direction,
                                      OverscrollSource source) {
  DCHECK_NE(direction, NavigationDirection::NONE);
  DCHECK_NE(source, OverscrollSource::NONE);
  if (direction == NavigationDirection::BACK) {
    RecordAction(base::UserMetricsAction("Overscroll_Cancelled.Back"));
  } else if (direction == NavigationDirection::FORWARD) {
    RecordAction(base::UserMetricsAction("Overscroll_Cancelled.Forward"));
  } else {
    RecordAction(base::UserMetricsAction("Overscroll_Cancelled.Reload"));
  }
}

}  // namespace

// This class is responsible for creating, painting, and positioning the layer
// for the gesture nav affordance.
class Affordance : public ui::LayerDelegate, public gfx::AnimationDelegate {
 public:
  Affordance(GestureNavSimple* owner,
             OverscrollMode mode,
             const gfx::Rect& content_bounds,
             float max_drag_progress);

  Affordance(const Affordance&) = delete;
  Affordance& operator=(const Affordance&) = delete;

  ~Affordance() override;

  // Sets drag progress. 0 means no progress. 1 means full progress. Values more
  // than 1 are also allowed for when the user drags beyond the completion
  // threshold.
  void SetDragProgress(float progress);

  // Aborts the affordance and animates it back. This will delete |this|
  // instance after the animation.
  void Abort();

  // Completes the affordance by doing a ripple burst animation. This will
  // delete |this| instance after the animation.
  void Complete();

  // Returns the root layer of the affordance.
  ui::Layer* root_layer() { return &root_layer_; }

  // Returns whether the affordance is performing abort or complete animation.
  bool IsFinishing() const { return state_ != State::DRAGGING; }

 private:
  enum class State { DRAGGING, ABORTING, COMPLETING };

  // Helper function that returns the origin for painted layer according to the
  // overscroll mode.
  gfx::Point GetPaintedLayerOrigin(const gfx::Rect& content_bounds) const;

  void UpdatePaintedLayer();
  void SchedulePaint();
  void SetAbortProgress(float progress);
  void SetCompleteProgress(float progress);

  // Helper function that returns the affordance progress according to the
  // current values of different progress values (drag progress and abort
  // progress). 1 means the affordance is at the activation threshold.
  float GetAffordanceProgress() const;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  const raw_ptr<GestureNavSimple> owner_;

  const OverscrollMode mode_;

  // Maximum value for drag progress that can be reached if the user drags
  // entire width of the page/screen.
  const float max_drag_progress_;

  // Root layer of the affordance. This is used to clip the affordance to the
  // content bounds.
  ui::Layer root_layer_;

  // Layer that actually paints the affordance.
  ui::Layer painted_layer_;

  // Image icon of the arrow inside the affordance.
  raw_ptr<const gfx::VectorIcon> arrow_icon_ = nullptr;

  // Values that determine current state of the affordance.
  State state_ = State::DRAGGING;
  float drag_progress_ = 0.f;
  float abort_progress_ = 0.f;
  float complete_progress_ = 0.f;

  std::unique_ptr<gfx::LinearAnimation> animation_;
};

Affordance::Affordance(GestureNavSimple* owner,
                       OverscrollMode mode,
                       const gfx::Rect& content_bounds,
                       float max_drag_progress)
    : owner_(owner),
      mode_(mode),
      max_drag_progress_(max_drag_progress),
      root_layer_(ui::LAYER_NOT_DRAWN),
      painted_layer_(ui::LAYER_TEXTURED) {
  DCHECK(mode_ == OVERSCROLL_EAST || mode_ == OVERSCROLL_WEST ||
         mode_ == OVERSCROLL_SOUTH);
  if (mode_ == OVERSCROLL_EAST) {
    arrow_icon_ = &vector_icons::kBackArrowIcon;
  } else if (mode_ == OVERSCROLL_WEST) {
    arrow_icon_ = &vector_icons::kForwardArrowIcon;
  } else if (mode_ == OVERSCROLL_SOUTH) {
    arrow_icon_ = &vector_icons::kReloadIcon;
  }

  DCHECK(arrow_icon_);
  root_layer_.SetBounds(content_bounds);
  root_layer_.SetMasksToBounds(true);

  painted_layer_.SetFillsBoundsOpaquely(false);
  gfx::Rect painted_layer_bounds(2 * kMaxRippleBurstRadius,
                                 2 * kMaxRippleBurstRadius);
  painted_layer_bounds.set_origin(GetPaintedLayerOrigin(content_bounds));
  painted_layer_.SetBounds(painted_layer_bounds);
  painted_layer_.set_delegate(this);

  root_layer_.Add(&painted_layer_);
}

Affordance::~Affordance() {}

void Affordance::SetDragProgress(float progress) {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_LE(0.f, progress);

  if (drag_progress_ == progress) {
    return;
  }
  drag_progress_ = progress;

  UpdatePaintedLayer();
  SchedulePaint();
}

void Affordance::Abort() {
  DCHECK_EQ(State::DRAGGING, state_);

  state_ = State::ABORTING;

  animation_ = std::make_unique<gfx::LinearAnimation>(
      GetAffordanceProgress() * kAbortAnimationDuration,
      gfx::LinearAnimation::kDefaultFrameRate, this);
  animation_->Start();
}

void Affordance::Complete() {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_LE(1.f, drag_progress_);

  state_ = State::COMPLETING;

  animation_ = std::make_unique<gfx::LinearAnimation>(
      kRippleBurstAnimationDuration, gfx::LinearAnimation::kDefaultFrameRate,
      this);
  animation_->Start();
}

gfx::Point Affordance::GetPaintedLayerOrigin(
    const gfx::Rect& content_bounds) const {
  DCHECK_NE(OVERSCROLL_NONE, mode_);
  gfx::Point origin;
  if (mode_ == OVERSCROLL_SOUTH) {
    origin.set_x(
        std::max(0, content_bounds.width() / 2 - kMaxRippleBurstRadius));
    origin.set_y(-kMaxRippleBurstRadius - kBackgroundRadius);
  } else {
    origin.set_y(
        std::max(0, content_bounds.height() / 2 - kMaxRippleBurstRadius));
    if (mode_ == OVERSCROLL_EAST) {
      origin.set_x(-kMaxRippleBurstRadius - kBackgroundRadius);
    } else {
      origin.set_x(content_bounds.width() - kMaxRippleBurstRadius +
                   kBackgroundRadius);
    }
  }
  return origin;
}

void Affordance::UpdatePaintedLayer() {
  const float offset = GetAffordanceProgress() * kAffordanceActivationOffset;
  gfx::Transform transform;
  if (mode_ == OVERSCROLL_EAST) {
    transform.Translate(offset, 0);
  } else if (mode_ == OVERSCROLL_WEST) {
    transform.Translate(-offset, 0);
  } else {
    transform.Translate(0, offset);
  }
  painted_layer_.SetTransform(transform);
}

void Affordance::SchedulePaint() {
  painted_layer_.SchedulePaint(gfx::Rect(painted_layer_.size()));
}

void Affordance::SetAbortProgress(float progress) {
  DCHECK_EQ(State::ABORTING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (abort_progress_ == progress) {
    return;
  }
  abort_progress_ = progress;

  UpdatePaintedLayer();
  SchedulePaint();
}

void Affordance::SetCompleteProgress(float progress) {
  DCHECK_EQ(State::COMPLETING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (complete_progress_ == progress) {
    return;
  }
  complete_progress_ = progress;

  painted_layer_.SetOpacity(gfx::Tween::CalculateValue(kBurstAnimationTweenType,
                                                       1 - complete_progress_));
  SchedulePaint();
}

float Affordance::GetAffordanceProgress() const {
  // Apply tween on the drag progress.
  float affordance_progress = drag_progress_;
  if (drag_progress_ >= 1.f) {
    float extra_progress = max_drag_progress_ == 1.f
                               ? 1.f
                               : std::min(1.f, (drag_progress_ - 1.f) /
                                                   (max_drag_progress_ - 1.f));
    affordance_progress =
        1 + gfx::Tween::CalculateValue(kExtraDragTweenType, extra_progress) *
                kExtraAffordanceRatio;
  }

  // Apply abort progress, if any.
  affordance_progress *=
      1 - gfx::Tween::CalculateValue(kAbortAnimationTweenType, abort_progress_);

  return affordance_progress;
}

void Affordance::OnPaintLayer(const ui::PaintContext& context) {
  DCHECK(drag_progress_ >= 1.f || state_ != State::COMPLETING);
  DCHECK(abort_progress_ == 0.f || state_ == State::ABORTING);
  DCHECK(complete_progress_ == 0.f || state_ == State::COMPLETING);

  ui::PaintRecorder recorder(context, painted_layer_.size());
  gfx::Canvas* canvas = recorder.canvas();

  gfx::Point center_point(kMaxRippleBurstRadius, kMaxRippleBurstRadius);
  float progress = std::min(1.f, GetAffordanceProgress());

  // Draw the ripple.
  cc::PaintFlags ripple_flags;
  ripple_flags.setAntiAlias(true);
  ripple_flags.setStyle(cc::PaintFlags::kFill_Style);
  ripple_flags.setColor(kRippleColor);
  float ripple_radius;
  if (state_ == State::COMPLETING) {
    const float burst_progress = gfx::Tween::CalculateValue(
        kBurstAnimationTweenType, complete_progress_);
    ripple_radius = kMaxRippleRadius +
                    burst_progress * (kMaxRippleBurstRadius - kMaxRippleRadius);
  } else {
    ripple_radius =
        kBackgroundRadius + progress * (kMaxRippleRadius - kBackgroundRadius);
  }
  canvas->DrawCircle(center_point, ripple_radius, ripple_flags);

  // Draw the arrow background circle with the shadow.
  cc::PaintFlags bg_flags;
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);
  bg_flags.setColor(progress >= 1.0f ? kBackgroundColorAfterActivation
                                     : kBackgroundColorBeforeActivation);
  gfx::ShadowValues shadow;
  shadow.emplace_back(gfx::Vector2d(0, kBgShadowOffsetY), kBgShadowBlurRadius,
                      kBgShadowColor);
  bg_flags.setLooper(gfx::CreateShadowDrawLooper(shadow));
  canvas->DrawCircle(center_point, kBackgroundRadius, bg_flags);

  // Draw the arrow.
  const int arrow_x = center_point.x() - kArrowSize / 2;
  const int arrow_y = center_point.y() - kArrowSize / 2;
  canvas->DrawImageInt(
      gfx::CreateVectorIcon(*arrow_icon_, kArrowSize,
                            progress >= 1.0f ? kArrowColorAfterActivation
                                             : kArrowColorBeforeActivation),
      arrow_x, arrow_y);
}

void Affordance::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                            float new_device_scale_factor) {}

void Affordance::AnimationEnded(const gfx::Animation* animation) {
  owner_->OnAffordanceAnimationEnded();
}

void Affordance::AnimationProgressed(const gfx::Animation* animation) {
  switch (state_) {
    case State::DRAGGING:
      NOTREACHED_IN_MIGRATION();
      break;
    case State::ABORTING:
      SetAbortProgress(animation->GetCurrentValue());
      break;
    case State::COMPLETING:
      SetCompleteProgress(animation->GetCurrentValue());
      break;
  }
}

void Affordance::AnimationCanceled(const gfx::Animation* animation) {
  NOTREACHED_IN_MIGRATION();
}

GestureNavSimple::GestureNavSimple(WebContentsImpl* web_contents)
    : web_contents_(web_contents) {}

GestureNavSimple::~GestureNavSimple() {}

void GestureNavSimple::OnAffordanceAnimationEnded() {
  affordance_ = nullptr;
}

gfx::Size GestureNavSimple::GetDisplaySize() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(web_contents_->GetNativeView())
      .size();
}

bool GestureNavSimple::OnOverscrollUpdate(float delta_x, float delta_y) {
  if (!affordance_ || affordance_->IsFinishing()) {
    return false;
  }
  float delta = std::abs(mode_ == OVERSCROLL_SOUTH ? delta_y : delta_x);
  DCHECK_LE(delta, max_delta_);
  affordance_->SetDragProgress(delta / completion_threshold_);
  return true;
}

void GestureNavSimple::OnOverscrollComplete(OverscrollMode overscroll_mode) {
  if (mode_ == OverscrollMode::OVERSCROLL_NONE) {
    // Previous mode change has been ignored because of overscroll-behavior
    // value; so, ignore this, too.
    return;
  }

  DCHECK_EQ(mode_, overscroll_mode);

  mode_ = OVERSCROLL_NONE;
  OverscrollSource overscroll_source = source_;
  source_ = OverscrollSource::NONE;
  if (!affordance_ || affordance_->IsFinishing()) {
    return;
  }

  affordance_->Complete();

  NavigationControllerImpl& controller = web_contents_->GetController();
  NavigationDirection direction = NavigationDirection::NONE;
  if (ShouldNavigateForward(&controller, overscroll_mode)) {
    controller.GoForward();
    direction = NavigationDirection::FORWARD;
  } else if (ShouldNavigateBack(&controller, overscroll_mode)) {
    controller.GoBack();
    direction = NavigationDirection::BACK;
  } else if (ShouldReload(controller, overscroll_mode)) {
    controller.Reload(ReloadType::NORMAL, true);
    direction = NavigationDirection::RELOAD;
  }

  if (direction != NavigationDirection::NONE) {
    if (direction == NavigationDirection::BACK) {
      RecordAction(base::UserMetricsAction("Overscroll_Navigated.Back"));
    } else if (direction == NavigationDirection::FORWARD) {
      RecordAction(base::UserMetricsAction("Overscroll_Navigated.Forward"));
    } else {
      RecordAction(base::UserMetricsAction("Overscroll_Navigated.Reload"));
    }
  } else {
    RecordGestureOverscrollCancelled(GetDirectionFromMode(overscroll_mode),
                                     overscroll_source);
  }
}

void GestureNavSimple::OnOverscrollModeChange(OverscrollMode old_mode,
                                              OverscrollMode new_mode,
                                              OverscrollSource source,
                                              cc::OverscrollBehavior behavior) {
  DCHECK(old_mode == OverscrollMode::OVERSCROLL_NONE ||
         new_mode == OverscrollMode::OVERSCROLL_NONE);

  // Do not start a new gesture-nav if overscroll-behavior-x is not auto.
  if ((new_mode == OverscrollMode::OVERSCROLL_EAST ||
       new_mode == OverscrollMode::OVERSCROLL_WEST) &&
      behavior.x != cc::OverscrollBehavior::Type::kAuto) {
    return;
  }

  // Do not start a new pull-to-refresh if overscroll-behavior-y is not auto.
  if (new_mode == OverscrollMode::OVERSCROLL_SOUTH &&
      behavior.y != cc::OverscrollBehavior::Type::kAuto) {
    return;
  }

  if (old_mode != OverscrollMode::OVERSCROLL_NONE &&
      mode_ == OverscrollMode::OVERSCROLL_NONE) {
    // Previous mode change has been ignored because of overscroll-behavior
    // value; so, ignore this one, too.
    return;
  }

  DCHECK_EQ(mode_, old_mode);
  if (mode_ == new_mode) {
    return;
  }
  mode_ = new_mode;

  NavigationControllerImpl& controller = web_contents_->GetController();
  if (!ShouldNavigateForward(&controller, mode_) &&
      !ShouldNavigateBack(&controller, mode_) &&
      !ShouldReload(controller, mode_)) {
    // If there is an overscroll in progress - record its cancellation.
    if (affordance_ && !affordance_->IsFinishing()) {
      RecordGestureOverscrollCancelled(GetDirectionFromMode(old_mode), source_);
      affordance_->Abort();
    }
    source_ = OverscrollSource::NONE;
    return;
  }

  DCHECK_NE(OverscrollSource::NONE, source);
  source_ = source;

  if (ShouldNavigateBack(&controller, mode_)) {
    web_contents_->BackNavigationLikely(
        preloading_predictor::kBackGestureNavigation,
        WindowOpenDisposition::CURRENT_TAB);
  }

  const bool is_touchpad = source == OverscrollSource::TOUCHPAD;
  const float start_threshold =
      is_touchpad ? OverscrollConfig::kStartTouchpadThresholdDips
                  : OverscrollConfig::kStartTouchscreenThresholdDips;
  const float complete_percent =
      is_touchpad ? OverscrollConfig::kCompleteTouchpadThresholdPercent
                  : OverscrollConfig::kCompleteTouchscreenThresholdPercent;

  const gfx::Size size = GetDisplaySize();
  const int max_size = std::max(size.width(), size.height());

  completion_threshold_ = max_size * complete_percent - start_threshold;
  DCHECK_LE(0, completion_threshold_) << " for display size " << size.ToString()
                                      << " and is_touchpad=" << is_touchpad;

  max_delta_ = max_size - start_threshold;
  DCHECK_LE(0, max_delta_);

  aura::Window* window = web_contents_->GetNativeView();
  affordance_ = std::make_unique<Affordance>(
      this, mode_, window->bounds(), max_delta_ / completion_threshold_);

  // Adding the affordance as a child of the content window is not sufficient,
  // because it is possible for a new layer to be parented on top of the
  // affordance layer (e.g. when the navigated-to page is displayed while the
  // completion animation is in progress). So instead, it is installed on top of
  // the content window as its sibling. Note that the affordance itself makes
  // sure that its contents are clipped to the bounds given to it.
  ui::Layer* parent = window->layer()->parent();
  parent->Add(affordance_->root_layer());
  parent->StackAtTop(affordance_->root_layer());
}

std::optional<float> GestureNavSimple::GetMaxOverscrollDelta() const {
  if (affordance_) {
    return max_delta_;
  }
  return std::nullopt;
}

}  // namespace content
