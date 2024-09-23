// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PROJECTION_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PROJECTION_LAYER_H_

#include <optional>

#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"

namespace blink {

class XRRigidTransform;

class XRProjectionLayer : public XRCompositionLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRProjectionLayer(XRGraphicsBinding* binding);
  ~XRProjectionLayer() override = default;

  uint16_t textureWidth() const;
  uint16_t textureHeight() const;
  uint16_t textureArrayLength() const;

  bool ignoreDepthValues() const;
  std::optional<float> fixedFoveation() const;
  void setFixedFoveation(std::optional<float> value);
  XRRigidTransform* deltaPose() const;
  void setDeltaPose(XRRigidTransform* value);

  void Trace(Visitor*) const override;

 private:
  uint16_t texture_width_{0L};
  uint16_t texture_height_{0L};
  uint16_t texture_array_length_{1L};
  bool ignore_depth_values_{true};
  std::optional<float> fixed_foveation_{std::nullopt};
  Member<XRRigidTransform> delta_pose_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PROJECTION_LAYER_H_
