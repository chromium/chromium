// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_DRAWING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_DRAWING_CONTEXT_H_

#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class XRCompositionLayer;

class XRLayerDrawingContext : public GarbageCollected<XRLayerDrawingContext> {
 public:
  virtual enum XRGraphicsBinding::Api GraphicsApi() const = 0;

  virtual void OnFrameStart() = 0;
  virtual void OnFrameEnd() = 0;

  virtual void SetCompositionLayer(XRCompositionLayer* layer) = 0;

  virtual uint16_t TextureWidth() const = 0;
  virtual uint16_t TextureHeight() const = 0;
  virtual uint16_t TextureArrayLength() const = 0;

  virtual bool TextureWasQueried() const = 0;

  virtual void Trace(Visitor* visitor) const {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_DRAWING_CONTEXT_H_
