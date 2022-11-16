// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRCompositionLayer::XRCompositionLayer(XRSession* session) : XRLayer(session) {}

const String& XRCompositionLayer::layout() const {
  return layout_;
}

bool XRCompositionLayer::blendTextureSourceAlpha() const {
  return blend_texture_source_alpha_;
}

void XRCompositionLayer::setBlendTextureSourceAlpha(bool value) {
  blend_texture_source_alpha_ = value;
}

absl::optional<bool> XRCompositionLayer::chromaticAberrationCorrection() const {
  return chromatic_aberration_correction_;
}

void XRCompositionLayer::setChromaticAberrationCorrection(
    absl::optional<bool> value) {
  chromatic_aberration_correction_ = value;
}

bool XRCompositionLayer::forceMonoPresentation() const {
  return force_mono_presentation_;
}

void XRCompositionLayer::setForceMonoPresentation(bool value) {
  force_mono_presentation_ = value;
}

float XRCompositionLayer::opacity() const {
  return opacity_;
}

void XRCompositionLayer::setOpacity(float value) {
  opacity_ = value;
}

uint16_t XRCompositionLayer::mipLevels() const {
  return mip_levels_;
}

bool XRCompositionLayer::needsRedraw() const {
  return needs_redraw_;
}

void XRCompositionLayer::destroy() const {
  NOTIMPLEMENTED();
}

void XRCompositionLayer::Trace(Visitor* visitor) const {
  XRLayer::Trace(visitor);
}

}  // namespace blink
