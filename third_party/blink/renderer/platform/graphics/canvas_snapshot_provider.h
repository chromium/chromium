// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_H_

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class MemoryManagedPaintCanvas;

// This is an interface abstracting a class that can draw to a snapshot.
class PLATFORM_EXPORT CanvasSnapshotProvider {
 public:
  virtual ~CanvasSnapshotProvider() = default;

  virtual scoped_refptr<StaticBitmapImage> DoExternalDrawAndSnapshot(
      base::FunctionRef<void(MemoryManagedPaintCanvas&)> draw_callback,
      ImageOrientation orientation) = 0;
  virtual bool IsAccelerated() const = 0;
  virtual gfx::Size Size() const = 0;
  virtual viz::SharedImageFormat GetSharedImageFormat() const = 0;
  virtual gfx::ColorSpace GetColorSpace() const = 0;
  virtual SkAlphaType GetAlphaType() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool IsGpuContextLost() const = 0;
  virtual bool IsExternalBitmapProvider() const { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_H_
