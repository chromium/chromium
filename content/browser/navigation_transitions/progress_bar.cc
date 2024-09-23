// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/progress_bar.h"

#include "ui/android/progress_bar_config.h"

namespace content {

ProgressBar::ProgressBar(int width_physical,
                         const ui::ProgressBarConfig& config)
    : width_physical_(width_physical),
      height_physical_(config.height_physical),
      background_layer_(cc::slim::SolidColorLayer::Create()),
      first_pulse_layer_(cc::slim::SolidColorLayer::Create()),
      second_pulse_layer_(cc::slim::SolidColorLayer::Create()),
      hairline_layer_(cc::slim::SolidColorLayer::Create()) {
  background_layer_->SetBackgroundColor(config.background_color);
  background_layer_->SetIsDrawable(true);
  background_layer_->SetBounds(
      gfx::Size(width_physical_, config.height_physical));

  first_pulse_layer_->SetBackgroundColor(config.color);
  first_pulse_layer_->SetIsDrawable(true);
  background_layer_->AddChild(first_pulse_layer_);

  second_pulse_layer_->SetBackgroundColor(config.color);
  second_pulse_layer_->SetIsDrawable(true);
  background_layer_->AddChild(second_pulse_layer_);

  hairline_layer_->SetBackgroundColor(config.hairline_color);
  hairline_layer_->SetIsDrawable(true);
  hairline_layer_->SetBounds(
      gfx::Size(width_physical_, config.hairline_height_physical));
  hairline_layer_->SetPosition(gfx::PointF(0, config.height_physical));

  SetupAnimation();
}

ProgressBar::~ProgressBar() = default;

scoped_refptr<cc::slim::Layer> ProgressBar::GetLayer() const {
  return background_layer_;
}

void ProgressBar::Animate(base::TimeTicks frame_begin_time) {
  CHECK(!effect_.keyframe_models().empty());
  effect_.Tick(frame_begin_time);

  // The first pulse fires off at the beginning of the animation.
  float left =
      width_physical_ * static_cast<float>(pow(current_value_, 1.5f) - 0.5f);
  float right = width_physical_ * current_value_;
  // TODO(bokan/khushalsagar): This needs to account for RTL.
  first_pulse_layer_->SetBounds(gfx::Size(right - left, height_physical_));
  first_pulse_layer_->SetPosition(gfx::PointF(left, 0));

  // The second pulse fires off at some point after the first pulse has been
  // fired.
  constexpr float kSecondPulseStart = 1.1f;
  constexpr float kSecondPulseLength = 1.0f;
  if (current_value_ >= kSecondPulseStart) {
    float percentage =
        (current_value_ - kSecondPulseStart) / kSecondPulseLength;
    left = width_physical_ * static_cast<float>(pow(percentage, 2.5f) - 0.1f);
    right = width_physical_ * percentage;
    second_pulse_layer_->SetBounds(gfx::Size(right - left, height_physical_));
    second_pulse_layer_->SetPosition(gfx::PointF(left, 0));
  } else {
    second_pulse_layer_->SetBounds(gfx::Size(0, 0));
  }
}

void ProgressBar::SetupAnimation() {
  constexpr float kStartValue = 0.f;
  constexpr float kEndValue = 3.f;
  constexpr base::TimeDelta kDuration = base::Milliseconds(3000);

  auto curve = gfx::KeyframedFloatAnimationCurve::Create();
  curve->AddKeyframe(gfx::FloatKeyframe::Create(/*time=*/base::TimeDelta(),
                                                /*value=*/kStartValue,
                                                /*timing_function=*/nullptr));
  curve->AddKeyframe(gfx::FloatKeyframe::Create(/*time=*/kDuration,
                                                /*value=*/kEndValue,
                                                /*timing_function=*/nullptr));
  curve->set_target(this);

  auto model = gfx::KeyframeModel::Create(
      /*curve=*/std::move(curve),
      /*keyframe_model_id=*/effect_.GetNextKeyframeModelId(),
      /*target_property_id=*/1);
  model->set_iterations(std::numeric_limits<double>::infinity());

  effect_.AddKeyframeModel(std::move(model));
}

void ProgressBar::OnFloatAnimated(const float& value,
                                  int target_property_id,
                                  gfx::KeyframeModel* keyframe_model) {
  current_value_ = value;
}

}  // namespace content
