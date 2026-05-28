// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_H_

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// This is an interface abstracting a class that can draw to a snapshot.
class PLATFORM_EXPORT CanvasSnapshotProvider {
 public:
  virtual ~CanvasSnapshotProvider() = default;

  virtual gfx::Size Size() const = 0;
  virtual viz::SharedImageFormat GetSharedImageFormat() const = 0;
  virtual gfx::ColorSpace GetColorSpace() const = 0;
  virtual SkAlphaType GetAlphaType() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool IsGpuContextLost() const = 0;
  virtual bool IsExternalBitmapProvider() const { return false; }

  // Helper structure and function for caching CanvasSnapshotProviders.
  struct Info {
    SkAlphaType alpha_type;
    gfx::ColorSpace color_space;
    viz::SharedImageFormat format;
    gfx::Size size;

    bool Matches(const CanvasSnapshotProvider::Info& info) const {
      return info.size == size && info.alpha_type == alpha_type &&
             info.color_space == color_space &&
             // TODO(crbug.com/40767377): Restore strict format checks once the
             // CanvasResourceProvider no longer swaps BGRA/RGBA sometimes.
             (info.format == format ||
              (info.format == viz::SinglePlaneFormat::kRGBA_8888 &&
               format == viz::SinglePlaneFormat::kBGRA_8888) ||
              (info.format == viz::SinglePlaneFormat::kBGRA_8888 &&
               format == viz::SinglePlaneFormat::kRGBA_8888));
    }
  };

  Info GetInfo() const {
    return {
        .alpha_type = GetAlphaType(),
        .color_space = GetColorSpace(),
        .format = GetSharedImageFormat(),
        .size = Size(),
    };
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_H_
