// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_projection_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRProjectionLayer::XRProjectionLayer(XRGraphicsBinding* binding)
    : XRCompositionLayer(binding) {}

uint16_t XRProjectionLayer::textureWidth() const {
  return texture_width_;
}

uint16_t XRProjectionLayer::textureHeight() const {
  return texture_height_;
}

uint16_t XRProjectionLayer::textureArrayLength() const {
  return texture_array_length_;
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

void XRProjectionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(delta_pose_);
  XRCompositionLayer::Trace(visitor);
}

}  // namespace blink
