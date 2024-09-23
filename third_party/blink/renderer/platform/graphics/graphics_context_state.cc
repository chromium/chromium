// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/graphics_context_state.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"

namespace blink {

static inline cc::PaintFlags::FilterQuality FilterQualityForPaint(
    InterpolationQuality quality) {
  // The filter quality "selected" here will primarily be used when painting a
  // primitive using one of the PaintFlags below. For the most part this will
  // not affect things that are part of the Image class hierarchy (which use
  // the unmodified m_interpolationQuality.)
  return quality != kInterpolationNone ? cc::PaintFlags::FilterQuality::kLow
                                       : cc::PaintFlags::FilterQuality::kNone;
}

GraphicsContextState::GraphicsContextState() {
  stroke_flags_.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags_.setFilterQuality(FilterQualityForPaint(interpolation_quality_));
  stroke_flags_.setDynamicRangeLimit(dynamic_range_limit_);
  stroke_flags_.setAntiAlias(should_antialias_);
  fill_flags_.setFilterQuality(FilterQualityForPaint(interpolation_quality_));
  fill_flags_.setDynamicRangeLimit(dynamic_range_limit_);
  fill_flags_.setAntiAlias(should_antialias_);
}

GraphicsContextState::GraphicsContextState(const GraphicsContextState& other)
    : stroke_flags_(other.stroke_flags_),
      fill_flags_(other.fill_flags_),
      text_drawing_mode_(other.text_drawing_mode_),
      interpolation_quality_(other.interpolation_quality_),
      dynamic_range_limit_(other.dynamic_range_limit_),
      save_count_(0),
      should_antialias_(other.should_antialias_) {}

void GraphicsContextState::Copy(const GraphicsContextState& source) {
  this->~GraphicsContextState();
  new (this) GraphicsContextState(source);
}

void GraphicsContextState::SetStrokeThickness(float thickness) {
  stroke_flags_.setStrokeWidth(SkFloatToScalar(thickness));
}

void GraphicsContextState::SetStroke(const StrokeData& stroke_data) {
  stroke_data.SetupPaint(&stroke_flags_);
}

void GraphicsContextState::SetStrokeColor(const Color& color) {
  stroke_flags_.setColor(color.toSkColor4f());
  stroke_flags_.setShader(nullptr);
}

void GraphicsContextState::SetFillColor(const Color& color) {
  fill_flags_.setColor(color.toSkColor4f());
  fill_flags_.setShader(nullptr);
}

// Shadow. (This will need tweaking if we use draw loopers for other things.)
void GraphicsContextState::SetDrawLooper(sk_sp<cc::DrawLooper> draw_looper) {
  // Grab a new ref for stroke.
  stroke_flags_.setLooper(draw_looper);
  // Pass the existing ref to fill (to minimize refcount churn).
  fill_flags_.setLooper(std::move(draw_looper));
}

void GraphicsContextState::SetInterpolationQuality(
    InterpolationQuality quality) {
  interpolation_quality_ = quality;
  stroke_flags_.setFilterQuality(FilterQualityForPaint(quality));
  fill_flags_.setFilterQuality(FilterQualityForPaint(quality));
}

void GraphicsContextState::SetDynamicRangeLimit(DynamicRangeLimit limit) {
  dynamic_range_limit_ = limit;
  stroke_flags_.setDynamicRangeLimit(limit);
  fill_flags_.setDynamicRangeLimit(limit);
}

void GraphicsContextState::SetShouldAntialias(bool should_antialias) {
  should_antialias_ = should_antialias;
  stroke_flags_.setAntiAlias(should_antialias);
  fill_flags_.setAntiAlias(should_antialias);
}

}  // namespace blink
