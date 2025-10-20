// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"

#include "base/notimplemented.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_layer_layout.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_layer_drawing_context.h"
#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"

namespace blink {

XRCompositionLayer::XRCompositionLayer(XRGraphicsBinding* binding,
                                       XRLayerDrawingContext* drawing_context)
    : XRLayer(binding->session()),
      binding_(binding),
      drawing_context_(drawing_context) {
  CHECK(drawing_context_);
  drawing_context_->SetCompositionLayer(this);
}

V8XRLayerLayout XRCompositionLayer::layout() const {
  return V8XRLayerLayout(V8XRLayerLayout::Enum::kDefault);
}

bool XRCompositionLayer::blendTextureSourceAlpha() const {
  return blend_texture_source_alpha_;
}

void XRCompositionLayer::setBlendTextureSourceAlpha(bool value) {
  blend_texture_source_alpha_ = value;
}

std::optional<bool> XRCompositionLayer::chromaticAberrationCorrection() const {
  return chromatic_aberration_correction_;
}

void XRCompositionLayer::setChromaticAberrationCorrection(
    std::optional<bool> value) {
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

void XRCompositionLayer::SetNeedsRedraw(bool needsRedraw) {
  needs_redraw_ = needsRedraw;
}

void XRCompositionLayer::SetLayout(V8XRLayerLayout layout) {
  layout_ = layout.AsEnum();
}

void XRCompositionLayer::SetMipLevels(uint16_t mipLevels) {
  mip_levels_ = mipLevels;
}

uint16_t XRCompositionLayer::textureWidth() const {
  return drawing_context_->TextureWidth();
}

uint16_t XRCompositionLayer::textureHeight() const {
  return drawing_context_->TextureHeight();
}

uint16_t XRCompositionLayer::textureArrayLength() const {
  return drawing_context_->TextureArrayLength();
}

void XRCompositionLayer::OnFrameStart() {
  drawing_context_->OnFrameStart();
}

void XRCompositionLayer::OnFrameEnd() {
  drawing_context_->OnFrameEnd();

  XRFrameProvider* frame_provider = session()->xr()->frameProvider();

  if (IsModified()) {
    if (XRProjectionLayer* layer = DynamicTo<XRProjectionLayer>(this); layer) {
      frame_provider->UpdateLayerViewports(layer);
      SetModified(false);
    }
  }

  frame_provider->SubmitLayer(layer_id(), drawing_context_,
                              drawing_context_->TextureWasQueried());
}

XrLayerClient* XRCompositionLayer::LayerClient() {
  return drawing_context();
}

void XRCompositionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(binding_);
  visitor->Trace(drawing_context_);
  XRLayer::Trace(visitor);
}

}  // namespace blink
