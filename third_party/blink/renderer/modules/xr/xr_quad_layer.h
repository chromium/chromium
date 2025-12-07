// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_QUAD_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_QUAD_LAYER_H_

#include <optional>

#include "third_party/blink/renderer/modules/xr/xr_shaped_layer.h"

namespace blink {

class XRQuadLayerInit;
class XRRigidTransform;

class XRQuadLayer : public XRShapedLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRQuadLayer(const XRQuadLayerInit* init,
              XRGraphicsBinding* binding,
              XRLayerDrawingContext* drawing_context);
  ~XRQuadLayer() override = default;

  XRLayerType LayerType() const override;

  XRRigidTransform* transform() const { return transform_.Get(); }
  void setTransform(XRRigidTransform* value);

  float width() const { return width_; }
  void setWidth(float width);
  float height() const { return height_; }
  void setHeight(float height);

  void Trace(Visitor*) const override;

 protected:
  device::mojom::blink::XRLayerSpecificDataPtr CreateLayerSpecificData()
      const override;

 private:
  Member<XRRigidTransform> transform_{nullptr};
  float width_{0.0};
  float height_{0.0};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_QUAD_LAYER_H_
