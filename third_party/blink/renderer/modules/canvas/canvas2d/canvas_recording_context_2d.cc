// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_recording_context_2d.h"

#include <cmath>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

CanvasRecordingContext2D::CanvasRecordingContext2D() {
  state_stack_.push_back(MakeGarbageCollected<CanvasRenderingContext2DState>());
}

double CanvasRecordingContext2D::shadowOffsetX() const {
  return GetState().ShadowOffset().x();
}

void CanvasRecordingContext2D::setShadowOffsetX(double x) {
  if (!std::isfinite(x)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowOffset().x() == x) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowOffsetX,
                                                x);
  }
  state.SetShadowOffsetX(ClampTo<float>(x));
}

double CanvasRecordingContext2D::shadowOffsetY() const {
  return GetState().ShadowOffset().y();
}

void CanvasRecordingContext2D::setShadowOffsetY(double y) {
  if (!std::isfinite(y)) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowOffset().y() == y) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowOffsetY,
                                                y);
  }
  state.SetShadowOffsetY(ClampTo<float>(y));
}

double CanvasRecordingContext2D::shadowBlur() const {
  return GetState().ShadowBlur();
}

void CanvasRecordingContext2D::setShadowBlur(double blur) {
  if (!std::isfinite(blur) || blur < 0) {
    return;
  }
  CanvasRenderingContext2DState& state = GetState();
  if (state.ShadowBlur() == blur) {
    return;
  }
  if (identifiability_study_helper_.ShouldUpdateBuilder()) [[unlikely]] {
    identifiability_study_helper_.UpdateBuilder(CanvasOps::kSetShadowBlur,
                                                blur);
  }
  state.SetShadowBlur(ClampTo<float>(blur));
}

void CanvasRecordingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(state_stack_);
  CanvasPath::Trace(visitor);
}

}  // namespace blink
