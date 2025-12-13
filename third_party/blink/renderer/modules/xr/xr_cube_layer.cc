// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cube_layer.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_cube_layer_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRCubeLayer::XRCubeLayer(const XRCubeLayerInit* init,
                         XRGraphicsBinding* binding,
                         XRLayerDrawingContext* drawing_context)
    : XRShapedLayer(init, binding, drawing_context) {
  if (init->hasOrientation()) {
    orientation_ = init->orientation();
  } else {
    orientation_ = DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }
  CreateLayerBackend();
}

XRLayerType XRCubeLayer::LayerType() const {
  return XRLayerType::kCubeLayer;
}

void XRCubeLayer::setOrientation(DOMPointReadOnly* orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    SetModified(true);
  }
}

device::mojom::blink::XRLayerSpecificDataPtr
XRCubeLayer::CreateLayerSpecificData() const {
  auto cube_layer_data = device::mojom::blink::XRCubeLayerData::New();
  cube_layer_data->orientation =
      gfx::Quaternion(orientation_->x(), orientation_->y(), orientation_->z(),
                      orientation_->w());

  return device::mojom::blink::XRLayerSpecificData::NewCube(
      std::move(cube_layer_data));
}

void XRCubeLayer::Trace(Visitor* visitor) const {
  visitor->Trace(orientation_);
  XRShapedLayer::Trace(visitor);
}

}  // namespace blink
