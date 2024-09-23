// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_PROGRESS_BAR_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_PROGRESS_BAR_H_

#include "cc/slim/solid_color_layer.h"
#include "content/common/content_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"

namespace ui {
struct ProgressBarConfig;
}

namespace content {

// Provides an indeterminate progress bar displayed between navigation start and
// commit.
class CONTENT_EXPORT ProgressBar : public gfx::FloatAnimationCurve::Target {
 public:
  ProgressBar(int width_physical, const ui::ProgressBarConfig& config);
  ~ProgressBar() override;

  scoped_refptr<cc::slim::Layer> GetLayer() const;
  void Animate(base::TimeTicks frame_begin_time);

 private:
  // gfx::FloatAnimationCurve::Target implementation.
  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  void SetupAnimation();

  // The width of the complete progress bar.
  const int width_physical_;

  // The height of the pulses drawn within the progress bar.
  const int height_physical_;

  scoped_refptr<cc::slim::SolidColorLayer> background_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> first_pulse_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> second_pulse_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> hairline_layer_;

  float current_value_ = 0.f;
  gfx::KeyframeEffect effect_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_PROGRESS_BAR_H_
