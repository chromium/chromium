// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_equirect_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_equirect_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {
XREquirectLayer::XREquirectLayer(const XREquirectLayerInit* init,
                                 XRGraphicsBinding* binding,
                                 XRLayerDrawingContext* drawing_context)
    : XRShapedLayer(init, binding, drawing_context),
      radius_(init->radius()),
      central_horizontal_angle_(init->centralHorizontalAngle()),
      upper_vertical_angle_(init->upperVerticalAngle()),
      lower_vertical_angle_(init->lowerVerticalAngle()) {
  if (init->hasTransform()) {
    transform_ = MakeGarbageCollected<XRRigidTransform>(
        init->transform()->TransformMatrix());
  } else {
    transform_ = MakeGarbageCollected<XRRigidTransform>(gfx::Transform{});
  }
  CreateLayerBackend();
}

XRLayerType XREquirectLayer::LayerType() const {
  return XRLayerType::kEquirectLayer;
}

void XREquirectLayer::setRadius(float radius) {
  radius_ = radius;
  SetModified(true);
}

void XREquirectLayer::setCentralHorizontalAngle(float angle) {
  central_horizontal_angle_ = angle;
  SetModified(true);
}

void XREquirectLayer::setUpperVerticalAngle(float angle) {
  upper_vertical_angle_ = angle;
  SetModified(true);
}

void XREquirectLayer::setLowerVerticalAngle(float angle) {
  lower_vertical_angle_ = angle;
  SetModified(true);
}

void XREquirectLayer::setTransform(XRRigidTransform* value) {
  if (transform_ != value) {
    transform_ = value;
    SetModified(true);
  }
}

device::mojom::blink::XRLayerSpecificDataPtr
XREquirectLayer::CreateLayerSpecificData() const {
  auto equirect_layer_data = device::mojom::blink::XREquirectLayerData::New();
  equirect_layer_data->radius = radius();
  equirect_layer_data->central_horizontal_angle = centralHorizontalAngle();
  equirect_layer_data->upper_vertical_angle = upperVerticalAngle();
  equirect_layer_data->lower_vertical_angle = lowerVerticalAngle();
  equirect_layer_data->native_origin_from_layer =
      transform()->TransformMatrix();
  return device::mojom::blink::XRLayerSpecificData::NewEquirect(
      std::move(equirect_layer_data));
}

void XREquirectLayer::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  XRShapedLayer::Trace(visitor);
}

}  // namespace blink
