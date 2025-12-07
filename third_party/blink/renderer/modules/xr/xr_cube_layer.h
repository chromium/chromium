// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CUBE_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CUBE_LAYER_H_

#include "third_party/blink/renderer/modules/xr/xr_shaped_layer.h"

namespace blink {

class XRCubeLayerInit;
class DOMPointReadOnly;

class XRCubeLayer : public XRShapedLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRCubeLayer(const XRCubeLayerInit* init,
              XRGraphicsBinding* binding,
              XRLayerDrawingContext* drawing_context);
  ~XRCubeLayer() override = default;

  XRLayerType LayerType() const override;

  DOMPointReadOnly* orientation() const { return orientation_.Get(); }
  void setOrientation(DOMPointReadOnly* orientation);

  void Trace(Visitor*) const override;

 private:
  device::mojom::blink::XRLayerSpecificDataPtr CreateLayerSpecificData()
      const override;

  Member<DOMPointReadOnly> orientation_{nullptr};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CUBE_LAYER_H_
