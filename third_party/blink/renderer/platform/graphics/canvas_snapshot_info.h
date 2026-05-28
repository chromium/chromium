// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_INFO_H_

#include "base/functional/function_ref.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

struct PLATFORM_EXPORT CanvasSnapshotInfo {
  SkAlphaType alpha_type;
  gfx::ColorSpace color_space;
  viz::SharedImageFormat format;
  gfx::Size size;

  bool Matches(const CanvasSnapshotInfo& info) const {
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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_INFO_H_
