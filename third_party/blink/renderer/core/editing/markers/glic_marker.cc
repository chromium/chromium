// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/glic_marker.h"

#include "base/time/time.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace blink {

constexpr static base::TimeDelta kAnimationDuration = base::Seconds(1);

float GetOpacity(float progress) {
  constexpr static float kOpacityStart = 0.f;
  constexpr static float kOpacityFinish = 1.f;
  double y = gfx::CubicBezier(0.2f, 0.f, 0.f, 1.f).Solve(progress);
  float opacity = kOpacityStart + (kOpacityFinish - kOpacityStart) * y;
  return opacity;
}

GlicMarker::GlicMarker(unsigned start_offset, unsigned end_offset)
    : DocumentMarker(start_offset, end_offset) {}

DocumentMarker::MarkerType GlicMarker::GetType() const {
  return DocumentMarker::kGlic;
}

Color GlicMarker::BackgroundColor() const {
  Color color =
      Color::FromRGBA32(shared_highlighting::kFragmentTextBackgroundColorARGB);
  color.SetAlpha(opacity_);
  return color;
}

bool GlicMarker::UpdateOpacityForDuration(base::TimeDelta duration) {
  double progress = duration / kAnimationDuration;
  bool is_last_frame = progress >= 1;
  opacity_ = is_last_frame ? GetOpacity(1.0) : GetOpacity(progress);
  return is_last_frame;
}
}  // namespace blink
