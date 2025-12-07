// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cylinder_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_cylinder_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {
XRCylinderLayer::XRCylinderLayer(const XRCylinderLayerInit* init,
                                 XRGraphicsBinding* binding,
                                 XRLayerDrawingContext* drawing_context)
    : XRShapedLayer(init, binding, drawing_context),
      radius_(ExcludeNegativeAndNoise(init->radius())),
      central_angle_(ExcludeNegativeAndNoise(init->centralAngle())),
      aspect_ratio_(init->aspectRatio()) {
  if (init->hasTransform()) {
    transform_ = MakeGarbageCollected<XRRigidTransform>(
        init->transform()->TransformMatrix());
  } else {
    transform_ = MakeGarbageCollected<XRRigidTransform>(gfx::Transform{});
  }

  CreateLayerBackend();
}

XRLayerType XRCylinderLayer::LayerType() const {
  return XRLayerType::kCylinderLayer;
}

void XRCylinderLayer::setRadius(float radius) {
  radius_ = ExcludeNegativeAndNoise(radius);
  SetModified(true);
}

void XRCylinderLayer::setCentralAngle(float central_angle) {
  // Central angle should be in range [0.f, 2*pi].
  central_angle_ =
      std::clamp(ExcludeNegativeAndNoise(central_angle), 0.f, kTwoPiFloat);
  SetModified(true);
}

void XRCylinderLayer::setAspectRatio(float aspect_ratio) {
  aspect_ratio_ = std::max(aspect_ratio, std::numeric_limits<float>::epsilon());
  SetModified(true);
}

void XRCylinderLayer::setTransform(XRRigidTransform* value) {
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
XRCylinderLayer::CreateLayerSpecificData() const {
  auto cylinder_layer_data = device::mojom::blink::XRCylinderLayerData::New();
  cylinder_layer_data->radius = radius();
  cylinder_layer_data->central_angle = centralAngle();
  cylinder_layer_data->aspect_ratio = aspectRatio();
  cylinder_layer_data->native_origin_from_layer =
      transform()->TransformMatrix();
  return device::mojom::blink::XRLayerSpecificData::NewCylinder(
      std::move(cylinder_layer_data));
}

void XRCylinderLayer::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  XRShapedLayer::Trace(visitor);
}

}  // namespace blink
