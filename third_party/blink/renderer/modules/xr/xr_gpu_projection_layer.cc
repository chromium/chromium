// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_projection_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_gpu_projection_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_binding.h"

namespace blink {

XRGPUProjectionLayer::XRGPUProjectionLayer(XRGPUBinding* binding,
                                           const XRGPUProjectionLayerInit* init)
    : XRProjectionLayer(binding) {}

void XRGPUProjectionLayer::Trace(Visitor* visitor) const {
  XRProjectionLayer::Trace(visitor);
}

}  // namespace blink
