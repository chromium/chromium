// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_PATH_CLIPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_PATH_CLIPPER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class DisplayItemClient;
class GraphicsContext;
class HitTestLocation;
class LayoutObject;

class CORE_EXPORT ClipPathClipper {
  STATIC_ONLY(ClipPathClipper);

 public:
  static void PaintClipPathAsMaskImage(GraphicsContext&,
                                       const LayoutObject&,
                                       const DisplayItemClient&);

  // Returns the reference box used by CSS clip-path. For HTML objects,
  // this is the border box of the element. For SVG objects this is the
  // object bounding box.
  static gfx::RectF LocalReferenceBox(const LayoutObject&);

  // Returns the bounding box of the computed clip path, which could be
  // smaller or bigger than the reference box. Returns nullopt if the
  // clip path is invalid.
  static absl::optional<gfx::RectF> LocalClipPathBoundingBox(
      const LayoutObject&);

  // The argument |clip_path_owner| is the layout object that owns the
  // ClipPathOperation we are currently processing. Usually it is the
  // same as the layout object getting clipped, but in the case of nested
  // clip-path, it could be one of the SVG clip path in the chain.
  // Returns the path if the clip-path can use path-based clip.
  static absl::optional<Path> PathBasedClip(
      const LayoutObject& clip_path_owner,
      const bool is_in_block_fragmentation);

  // Returns true if `location` intersects the LayoutObject's clip-path.
  // `reference_box` is used to resolve 'objectBoundingBox' units/percentages,
  // and can differ from the reference box of the passed LayoutObject.
  static bool HitTest(const LayoutObject&,
                      const gfx::RectF& reference_box,
                      const HitTestLocation& location);

  // Like the above, but derives the reference box from the LayoutObject using
  // `LocalReferenceBox()`.
  static bool HitTest(const LayoutObject&, const HitTestLocation& location);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_PATH_CLIPPER_H_
