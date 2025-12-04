// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_quad_layer.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_quad_layer_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRQuadLayer::XRQuadLayer(const XRQuadLayerInit* init,
                         XRGraphicsBinding* binding,
                         XRLayerDrawingContext* drawing_context)
    : XRShapedLayer(init, binding, drawing_context),
      width_(init->width()),
      height_(init->height()) {
  if (init->hasTransform()) {
    transform_ = MakeGarbageCollected<XRRigidTransform>(
        init->transform()->TransformMatrix());
  } else {
    transform_ = MakeGarbageCollected<XRRigidTransform>(gfx::Transform{});
  }
  CreateLayerBackend();
}

XRLayerType XRQuadLayer::LayerType() const {
  return XRLayerType::kQuadLayer;
}

void XRQuadLayer::setWidth(float width) {
  width_ = std::max(width, std::numeric_limits<float>::epsilon());
  SetModified(true);
}

void XRQuadLayer::setHeight(float height) {
  height_ = std::max(height, std::numeric_limits<float>::epsilon());
  SetModified(true);
}

void XRQuadLayer::setTransform(XRRigidTransform* value) {
  if (transform_ != value) {
    if (value) {
      transform_ = value;
    } else {
      transform_ = MakeGarbageCollected<XRRigidTransform>(gfx::Transform{});
    }
    SetModified(true);
  }
}

device::mojom::blink::XRLayerSpecificDataPtr
XRQuadLayer::CreateLayerSpecificData() const {
  auto quad_layer_data = device::mojom::blink::XRQuadLayerData::New();
  quad_layer_data->width = width();
  quad_layer_data->height = height();
  quad_layer_data->native_origin_from_layer = transform()->TransformMatrix();

  return device::mojom::blink::XRLayerSpecificData::NewQuad(
      std::move(quad_layer_data));
}

void XRQuadLayer::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  XRShapedLayer::Trace(visitor);
}

}  // namespace blink
