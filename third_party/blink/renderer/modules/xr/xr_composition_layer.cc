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
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

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
  return V8XRLayerLayout(layout_);
}

bool XRCompositionLayer::blendTextureSourceAlpha() const {
  return blend_texture_source_alpha_;
}

void XRCompositionLayer::setBlendTextureSourceAlpha(bool value) {
  blend_texture_source_alpha_ = value;
  SetModified(true);
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
  SetModified(true);
}

uint16_t XRCompositionLayer::mipLevels() const {
  return mip_levels_;
}

void XRCompositionLayer::destroy() const {
  NOTIMPLEMENTED();
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

  if (IsModified()) {
    UpdateLayerBackend();
    SetModified(false);
  }

  XRFrameProvider* frame_provider = session()->xr()->frameProvider();
  frame_provider->SubmitLayer(layer_id(), drawing_context_,
                              drawing_context_->TextureWasQueried());

  // Reset needs redraw state because texture was requested and submitted.
  if (drawing_context_->TextureWasQueried()) {
    SetNeedsRedraw(false);
  }
}

XrLayerClient* XRCompositionLayer::LayerClient() {
  return drawing_context();
}

device::mojom::blink::XRCompositionLayerDataPtr
XRCompositionLayer::CreateLayerData() const {
  auto layer_data = device::mojom::blink::XRCompositionLayerData::New();
  // Readonly data.
  layer_data->read_only_data = device::mojom::blink::XRLayerReadOnlyData::New();
  layer_data->read_only_data->layer_id = layer_id();
  layer_data->read_only_data->texture_width = textureWidth();
  layer_data->read_only_data->texture_height = textureHeight();
  layer_data->read_only_data->is_static = isStatic();
  layer_data->read_only_data->layout = V8ToMojomLayerLayout(layout_);
  // Mutable data.
  layer_data->mutable_data = device::mojom::blink::XRLayerMutableData::New();
  layer_data->mutable_data->blend_texture_source_alpha =
      blendTextureSourceAlpha();
  layer_data->mutable_data->opacity = opacity();
  layer_data->mutable_data->native_origin_information = NativeOrigin();

  // Layer Specific data.
  layer_data->mutable_data->layer_data = CreateLayerSpecificData();

  return layer_data;
}

bool XRCompositionLayer::isStatic() const {
  return false;
}

void XRCompositionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(binding_);
  visitor->Trace(drawing_context_);
  XRLayer::Trace(visitor);
}

}  // namespace blink
