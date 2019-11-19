// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/display/gl_cursor_feedback.h"

#include <math.h>

#include <array>

#include "base/logging.h"
#include "remoting/client/display/canvas.h"
#include "remoting/client/display/gl_cursor_feedback_texture.h"
#include "remoting/client/display/gl_math.h"
#include "remoting/client/display/gl_render_layer.h"
#include "remoting/client/display/gl_texture_ids.h"

namespace {

const float kAnimationDurationMs = 300.f;

// This function is for calculating the size of the feedback animation circle at
// the moment when the animation progress is |progress|.
// |progress|: [0, 1], indicating the progress of the animation.
// Returns a coefficient in [0, 1]. It will be multiplied with the maximum
// diameter of the feedback circle to get the current diameter of the feedback
// circle.
float GetExpansionCoefficient(float progress) {
  DCHECK(progress >= 0 && progress <= 1);

  // Decelerating expansion. This is conforming to the material design spec.
  // More time will be spent showing the larger circle and the animation will
  // look more rapid given the same time duration.
  auto get_unnormalized_coeff = [](float progress) {
    static const float kExpansionBase = 400.f;
    return 1.f - pow(kExpansionBase, -progress);
  };
  static const float kExpansionNormalization = get_unnormalized_coeff(1);
  return get_unnormalized_coeff(progress) / kExpansionNormalization;
}

}  // namespace

namespace remoting {

GlCursorFeedback::GlCursorFeedback() {}

GlCursorFeedback::~GlCursorFeedback() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GlCursorFeedback::SetCanvas(base::WeakPtr<Canvas> canvas) {
  if (!canvas) {
    layer_.reset();
    return;
  }
  layer_.reset(new GlRenderLayer(kGlCursorFeedbackTextureId, canvas));
  GlCursorFeedbackTexture* texture = GlCursorFeedbackTexture::GetInstance();
  layer_->SetTexture(texture->GetTexture().data(),
                     GlCursorFeedbackTexture::kTextureWidth,
                     GlCursorFeedbackTexture::kTextureWidth, 0);
}

void GlCursorFeedback::StartAnimation(float x, float y, float diameter) {
  cursor_x_ = x;
  cursor_y_ = y;
  max_diameter_ = diameter;
  animation_start_time_ = base::TimeTicks::Now();
}

bool GlCursorFeedback::Draw() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!layer_ || animation_start_time_.is_null()) {
    return false;
  }
  float progress =
      (base::TimeTicks::Now() - animation_start_time_).InMilliseconds() /
      kAnimationDurationMs;
  if (progress > 1) {
    animation_start_time_ = base::TimeTicks();
    return false;
  }
  float diameter = GetExpansionCoefficient(progress) * max_diameter_;
  std::array<float, 8> positions;
  FillRectangleVertexPositions(cursor_x_ - diameter / 2,
                               cursor_y_ - diameter / 2, diameter, diameter,
                               &positions);
  layer_->SetVertexPositions(positions);

  // Linear fade-out.
  layer_->Draw(1.f - progress);
  return true;
}

int GlCursorFeedback::GetZIndex() {
  return Drawable::ZIndex::CURSOR_FEEDBACK;
}

base::WeakPtr<Drawable> GlCursorFeedback::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
