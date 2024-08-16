// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_PROJECTION_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_PROJECTION_LAYER_H_

#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"

namespace blink {

class XRGPUBinding;
class XRGPUProjectionLayerInit;

class XRGPUProjectionLayer final : public XRProjectionLayer {
 public:
  XRGPUProjectionLayer(XRGPUBinding*, const XRGPUProjectionLayerInit*);
  ~XRGPUProjectionLayer() override = default;

  void Trace(Visitor*) const override;

 private:
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_PROJECTION_LAYER_H_
