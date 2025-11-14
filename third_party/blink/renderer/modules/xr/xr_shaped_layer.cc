// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_shaped_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRShapedLayer::XRShapedLayer(const XRLayerInit* init,
                             XRGraphicsBinding* binding,
                             XRLayerDrawingContext* drawing_context)
    : XRCompositionLayer(binding, drawing_context),
      xr_space_(init->space()),
      texture_width_(init->viewPixelWidth()),
      texture_height_(init->viewPixelHeight()),
      is_static_(init->isStatic()),
      clear_on_access_(init->clearOnAccess()) {
  SetLayout(init->layout());
  SetMipLevels(init->mipLevels());
}

bool XRShapedLayer::isStatic() const {
  return is_static_;
}

void XRShapedLayer::setSpace(XRSpace* space) {
  xr_space_ = space;
  SetModified(true);
}

device::mojom::blink::XRNativeOriginInformationPtr XRShapedLayer::NativeOrigin()
    const {
  return xr_space_->NativeOrigin();
}

void XRShapedLayer::UpdateLayerBackend() {
  if (auto* layer_manager = session()->LayerManager(); layer_manager) {
    device::mojom::blink::XRLayerMutableDataPtr mutable_data =
        device::mojom::blink::XRLayerMutableData::New();
    mutable_data->blend_texture_source_alpha = blendTextureSourceAlpha();
    mutable_data->opacity = opacity();
    mutable_data->native_origin_information = NativeOrigin();

    // Layer Specific data.
    mutable_data->layer_data = CreateLayerSpecificData();
    layer_manager->UpdateCompositionLayer(layer_id(), std::move(mutable_data));
  }
}

bool XRShapedLayer::IsRedrawEventSupported() const {
  return true;
}

void XRShapedLayer::Trace(Visitor* visitor) const {
  visitor->Trace(xr_space_);
  XRCompositionLayer::Trace(visitor);
}

}  // namespace blink
