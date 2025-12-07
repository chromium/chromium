// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_projection_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"

namespace blink {

XRProjectionLayer::XRProjectionLayer(XRGraphicsBinding* binding,
                                     XRLayerDrawingContext* drawing_context)
    : XRCompositionLayer(binding, drawing_context) {
  CreateLayerBackend();
  // Ensure correct viewports are sent to the runtime on the first frame.
  SetModified(true);
}

XRLayerType XRProjectionLayer::LayerType() const {
  return XRLayerType::kProjectionLayer;
}

bool XRProjectionLayer::ignoreDepthValues() const {
  return ignore_depth_values_;
}

std::optional<float> XRProjectionLayer::fixedFoveation() const {
  return fixed_foveation_;
}

void XRProjectionLayer::setFixedFoveation(std::optional<float> value) {
  fixed_foveation_ = value;
}

XRRigidTransform* XRProjectionLayer::deltaPose() const {
  return delta_pose_.Get();
}

void XRProjectionLayer::setDeltaPose(XRRigidTransform* value) {
  delta_pose_ = value;
}

void XRProjectionLayer::UpdateLayerBackend() {
  session()->xr()->frameProvider()->UpdateLayerViewports(this);
}

device::mojom::blink::XRNativeOriginInformationPtr
XRProjectionLayer::NativeOrigin() const {
  return device::mojom::blink::XRNativeOriginInformation::NewReferenceSpaceType(
      device::mojom::blink::XRReferenceSpaceType::kLocal);
}

device::mojom::blink::XRLayerSpecificDataPtr
XRProjectionLayer::CreateLayerSpecificData() const {
  return device::mojom::blink::XRLayerSpecificData::NewProjection(
      device::mojom::blink::XRProjectionLayerData::New());
}

void XRProjectionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(delta_pose_);
  XRCompositionLayer::Trace(visitor);
}

}  // namespace blink
