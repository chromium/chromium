// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_shaped_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRShapedLayer::XRShapedLayer(const XRLayerInit* init,
                             XRGraphicsBinding* binding)
    : XRCompositionLayer(binding),
      xr_space_(init->space()),
      texture_width_(init->viewPixelWidth()),
      texture_height_(init->viewPixelHeight()),
      is_static_(init->isStatic()),
      clear_on_access_(init->clearOnAccess()) {
  SetLayout(init->layout());
  SetMipLevels(init->mipLevels());
}

bool XRShapedLayer::InitializeLayer() const {
  return false;
}

void XRShapedLayer::setSpace(XRSpace* space) {
  xr_space_ = space;
  OnUpdateLayerData();
}

uint16_t XRShapedLayer::textureWidth() const {
  return texture_width_;
}

uint16_t XRShapedLayer::textureHeight() const {
  return texture_height_;
}

void XRShapedLayer::Trace(Visitor* visitor) const {
  visitor->Trace(xr_space_);
  XRCompositionLayer::Trace(visitor);
}

}  // namespace blink
