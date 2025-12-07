// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SHAPED_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SHAPED_LAYER_H_

#include "third_party/blink/renderer/modules/xr/xr_composition_layer.h"

namespace blink {

class XRGraphicsBinding;
class XRLayerInit;
class XRSpace;

class XRShapedLayer : public XRCompositionLayer {
 public:
  XRShapedLayer(const XRLayerInit* init,
                XRGraphicsBinding* binding,
                XRLayerDrawingContext* drawing_context);
  ~XRShapedLayer() override = default;

  // onredraw event handler
  DEFINE_ATTRIBUTE_EVENT_LISTENER(redraw, kRedraw)

  // space attribute
  XRSpace* space() const { return xr_space_; }
  void setSpace(XRSpace* space);

  // xr layer init parameters
  bool isStatic() const override;
  bool clearOnAccess() const { return clear_on_access_; }

  void Trace(Visitor*) const override;

 protected:
  bool IsRedrawEventSupported() const override;
  void UpdateLayerBackend() override;
  device::mojom::blink::XRNativeOriginInformationPtr NativeOrigin()
      const override;

 private:
  Member<XRSpace> xr_space_;
  uint16_t texture_width_;
  uint16_t texture_height_;
  bool is_static_;
  bool clear_on_access_;
};

}  //  namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SHAPED_LAYER_H_
