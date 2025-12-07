// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CYLINDER_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CYLINDER_LAYER_H_

#include <optional>

#include "third_party/blink/renderer/modules/xr/xr_shaped_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

class XRCylinderLayerInit;
class XRRigidTransform;

class XRCylinderLayer : public XRShapedLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRCylinderLayer(const XRCylinderLayerInit* init,
                  XRGraphicsBinding* binding,
                  XRLayerDrawingContext* drawing_context);
  ~XRCylinderLayer() override = default;

  XRLayerType LayerType() const override;

  XRRigidTransform* transform() const { return transform_.Get(); }
  void setTransform(XRRigidTransform* value);
  float radius() const { return radius_; }
  void setRadius(float radius_);
  float centralAngle() const { return central_angle_; }
  void setCentralAngle(float central_angle);
  float aspectRatio() const { return aspect_ratio_; }
  void setAspectRatio(float aspect_ratio);

  void Trace(Visitor*) const override;

 protected:
  device::mojom::blink::XRLayerSpecificDataPtr CreateLayerSpecificData()
      const override;

 private:
  Member<XRRigidTransform> transform_{nullptr};

  float radius_;
  float central_angle_;
  float aspect_ratio_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CYLINDER_LAYER_H_
