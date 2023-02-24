// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/graphics_context_state.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"

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

GraphicsContextState::GraphicsContextState()
    : text_drawing_mode_(kTextModeFill),
      interpolation_quality_(kInterpolationDefault),
      save_count_(0),
      should_antialias_(true) {
  stroke_flags_.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags_.setStrokeWidth(SkFloatToScalar(stroke_data_.Thickness()));
  stroke_flags_.setStrokeCap(cc::PaintFlags::kDefault_Cap);
  stroke_flags_.setStrokeJoin(cc::PaintFlags::kDefault_Join);
  stroke_flags_.setStrokeMiter(SkFloatToScalar(stroke_data_.MiterLimit()));
  stroke_flags_.setFilterQuality(FilterQualityForPaint(interpolation_quality_));
  stroke_flags_.setAntiAlias(should_antialias_);
  fill_flags_.setFilterQuality(FilterQualityForPaint(interpolation_quality_));
  fill_flags_.setAntiAlias(should_antialias_);
}

GraphicsContextState::GraphicsContextState(const GraphicsContextState& other)
    : stroke_flags_(other.stroke_flags_),
      fill_flags_(other.fill_flags_),
      stroke_data_(other.stroke_data_),
      text_drawing_mode_(other.text_drawing_mode_),
      interpolation_quality_(other.interpolation_quality_),
      save_count_(0),
      should_antialias_(other.should_antialias_) {}

void GraphicsContextState::Copy(const GraphicsContextState& source) {
  this->~GraphicsContextState();
  new (this) GraphicsContextState(source);
}

const cc::PaintFlags& GraphicsContextState::StrokeFlags(
    const int stroked_path_length,
    const int dash_thickness,
    const bool closed_path) const {
  stroke_data_.SetupPaintDashPathEffect(&stroke_flags_, stroked_path_length,
                                        dash_thickness, closed_path);
  return stroke_flags_;
}

void GraphicsContextState::SetStrokeStyle(StrokeStyle style) {
  stroke_data_.SetStyle(style);
}

void GraphicsContextState::SetStrokeThickness(float thickness) {
  stroke_data_.SetThickness(thickness);
  stroke_flags_.setStrokeWidth(SkFloatToScalar(thickness));
}

void GraphicsContextState::SetStrokeColor(const Color& color) {
  stroke_flags_.setColor(color.Rgb());
  stroke_flags_.setShader(nullptr);
}

void GraphicsContextState::SetLineCap(LineCap cap) {
  stroke_data_.SetLineCap(cap);
  stroke_flags_.setStrokeCap(static_cast<cc::PaintFlags::Cap>(cap));
}

void GraphicsContextState::SetLineJoin(LineJoin join) {
  stroke_data_.SetLineJoin(join);
  stroke_flags_.setStrokeJoin(static_cast<cc::PaintFlags::Join>(join));
}

void GraphicsContextState::SetMiterLimit(float miter_limit) {
  stroke_data_.SetMiterLimit(miter_limit);
  stroke_flags_.setStrokeMiter(SkFloatToScalar(miter_limit));
}

void GraphicsContextState::SetFillColor(const Color& color) {
  fill_flags_.setColor(color.Rgb());
  fill_flags_.setShader(nullptr);
}

// Shadow. (This will need tweaking if we use draw loopers for other things.)
void GraphicsContextState::SetDrawLooper(sk_sp<SkDrawLooper> draw_looper) {
  // Grab a new ref for stroke.
  stroke_flags_.setLooper(draw_looper);
  // Pass the existing ref to fill (to minimize refcount churn).
  fill_flags_.setLooper(std::move(draw_looper));
}

void GraphicsContextState::SetLineDash(const DashArray& dashes,
                                       float dash_offset) {
  stroke_data_.SetLineDash(dashes, dash_offset);
}

void GraphicsContextState::SetInterpolationQuality(
    InterpolationQuality quality) {
  interpolation_quality_ = quality;
  stroke_flags_.setFilterQuality(FilterQualityForPaint(quality));
  fill_flags_.setFilterQuality(FilterQualityForPaint(quality));
}

void GraphicsContextState::SetShouldAntialias(bool should_antialias) {
  should_antialias_ = should_antialias;
  stroke_flags_.setAntiAlias(should_antialias);
  fill_flags_.setAntiAlias(should_antialias);
}

}  // namespace blink
