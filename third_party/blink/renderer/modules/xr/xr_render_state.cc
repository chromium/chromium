// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "third_party/blink/renderer/modules/xr/xr_render_state.h"

#include <algorithm>
#include <cmath>

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_render_state_init.h"
#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"

namespace blink {

namespace {
// The WebXR spec specifies that the min and max are up the UA, but have to be
// within 0 and Pi.  Using those exact numbers can lead to floating point math
// errors, so set them slightly inside those numbers.
constexpr double kMinFieldOfView = 0.01;
constexpr double kMaxFieldOfView = 3.13;
constexpr double kDefaultFieldOfView = M_PI * 0.5;
}  // namespace

XRRenderState::XRRenderState(bool immersive) : immersive_(immersive) {
  if (!immersive_)
    inline_vertical_fov_ = kDefaultFieldOfView;
}

void XRRenderState::Update(const XRRenderStateInit* init) {
  if (init->hasDepthNear()) {
    depth_near_ = std::max(0.0, init->depthNear());
  }
  if (init->hasDepthFar()) {
    depth_far_ = std::max(0.0, init->depthFar());
  }
  if (init->hasBaseLayer()) {
    needs_layers_update_ |= base_layer_ != init->baseLayer();
    base_layer_ = init->baseLayer();
    layers_ = MakeGarbageCollected<FrozenArray<XRLayer>>();
  }
  if (init->hasLayers()) {
    if (!init->layers() || init->layers()->size() != layers_->size() ||
        !std::equal(init->layers()->begin(), init->layers()->end(),
                    layers_->begin())) {
      needs_layers_update_ = true;
    }

    base_layer_ = nullptr;
    layers_ = init->layers() ? MakeGarbageCollected<FrozenArray<XRLayer>>(
                                   HeapVector<Member<XRLayer>>(*init->layers()))
                             : MakeGarbageCollected<FrozenArray<XRLayer>>();
  }
  if (init->hasInlineVerticalFieldOfView()) {
    double fov = init->inlineVerticalFieldOfView();

    // Clamp the value between our min and max.
    fov = std::max(kMinFieldOfView, fov);
    fov = std::min(kMaxFieldOfView, fov);
    inline_vertical_fov_ = fov;
  }
}

XRLayer* XRRenderState::GetFirstLayer() const {
  if (base_layer_) {
    return base_layer_.Get();
  }
  if (!layers_->empty()) {
    return layers_->at(0);
  }
  return nullptr;
}

HTMLCanvasElement* XRRenderState::output_canvas() const {
  if (base_layer_) {
    return base_layer_->output_canvas();
  }
  return nullptr;
}

std::optional<double> XRRenderState::inlineVerticalFieldOfView() const {
  if (immersive_)
    return std::nullopt;
  return inline_vertical_fov_;
}

bool XRRenderState::HasActiveLayer() const {
  return base_layer_ || (layers_ && !layers_->empty());
}

void XRRenderState::OnFrameStart() {
  if (base_layer_) {
    base_layer_->OnFrameStart();
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->OnFrameStart();
    }
  }
}

void XRRenderState::OnFrameEnd() {
  if (base_layer_) {
    base_layer_->OnFrameEnd();
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->OnFrameEnd();
    }
  }
}

void XRRenderState::OnResize() {
  if (base_layer_) {
    base_layer_->OnResize();
  }

  if (layers_) {
    for (XRLayer* layer : *layers_) {
      layer->OnResize();
    }
  }
}

XRFrameTransportDelegate* XRRenderState::GetTransportDelegate() {
  if (XRLayer* first_layer = GetFirstLayer(); first_layer) {
    return first_layer->LayerClient()->GetTransportDelegate();
  }
  return nullptr;
}

bool XRRenderState::NeedLayersUpdate() {
  return needs_layers_update_;
}

void XRRenderState::OnLayersUpdated() {
  needs_layers_update_ = false;
}

void XRRenderState::Trace(Visitor* visitor) const {
  visitor->Trace(base_layer_);
  visitor->Trace(layers_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
