// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_EQUIRECT_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_EQUIRECT_LAYER_H_

#include <optional>

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_equirect_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_shaped_layer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XREquirectLayerInit;
class XRRigidTransform;

class XREquirectLayer : public XRShapedLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XREquirectLayer(const XREquirectLayerInit* init,
                  XRGraphicsBinding* binding,
                  XRLayerDrawingContext* drawing_context);
  ~XREquirectLayer() override = default;

  XRLayerType LayerType() const override;

  XRRigidTransform* transform() const { return transform_.Get(); }
  void setTransform(XRRigidTransform* value);
  float radius() const { return radius_; }
  void setRadius(float radius);
  float centralHorizontalAngle() const { return central_horizontal_angle_; }
  void setCentralHorizontalAngle(float angle);
  float upperVerticalAngle() const { return upper_vertical_angle_; }
  void setUpperVerticalAngle(float angle);
  float lowerVerticalAngle() const { return lower_vertical_angle_; }
  void setLowerVerticalAngle(float angle);

  void Trace(Visitor*) const override;

 protected:
  device::mojom::blink::XRLayerSpecificDataPtr CreateLayerSpecificData()
      const override;

 private:
  Member<XRRigidTransform> transform_{nullptr};

  float radius_;
  float central_horizontal_angle_;
  float upper_vertical_angle_;
  float lower_vertical_angle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_EQUIRECT_LAYER_H_
