// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace gfx {
class RectF;
}  // namespace gfx

namespace blink {

class DisplayItemClient;
class GraphicsContext;
class LayoutObject;
class SVGResource;
class SVGResourceClient;

enum class EMaskType : uint8_t;

class SVGMaskPainter {
  STATIC_ONLY(SVGMaskPainter);

 public:
  static void Paint(GraphicsContext& context,
                    const LayoutObject& layout_object,
                    const DisplayItemClient& display_item_client);
  static PaintRecord PaintResource(SVGResource* mask_resource,
                                   SVGResourceClient& client,
                                   const gfx::RectF& reference_box,
                                   float zoom);
  static gfx::RectF ResourceBounds(SVGResource* mask_resource,
                                   SVGResourceClient& client,
                                   const gfx::RectF& reference_box,
                                   float zoom);
  static EMaskType MaskType(SVGResource* mask_resource,
                            SVGResourceClient& client);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_
