// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include "base/check.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Layer;
class PaintCanvas;
}  // namespace cc

namespace blink {

class CanvasResourceProvider;

class PLATFORM_EXPORT CanvasResourceHost {
 public:
  virtual ~CanvasResourceHost() = default;

  virtual void NotifyGpuContextLost() = 0;
  // TODO(crbug.com/399587138): Delete once `cc::Layer` related code is moved to
  // `CanvasRenderingContext2D`. `IsContextLost()` is only needed by
  // `IsResourceValid()`, which is needed by `PrepareTransferableResource()`,
  // which is needed by cc_layer_, which is only used by
  // CanvasRenderingContext2D.
  virtual bool IsContextLost() const = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;

  virtual CanvasResourceProvider* GetResourceProviderForCanvas2D() const = 0;

  virtual void ResetResourceProviderForCanvas2D() = 0;

  virtual bool IsPageVisible() const = 0;

  virtual bool IsPrinting() const { return false; }
  virtual bool IsHibernating() const { return false; }

  // Called when the CC texture layer that this instance is holding (if any)
  // should be cleared. Subclasses that can hold a CC texture layer should
  // override this method. Should only be called if the context is
  // CanvasRenderingContext2D.
  virtual void ClearCanvas2DLayerTexture() {}

  virtual void SetTransferToGPUTextureWasInvoked() {}
  virtual bool TransferToGPUTextureWasInvoked() { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
