// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_CLIP_PATH_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_CLIP_PATH_VALUE_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"

namespace cc {
class PaintCanvas;
}

namespace gfx {
class RectF;
class Vector2dF;
}  // namespace gfx

namespace blink {

class ContouredRect;

class CORE_EXPORT CompositorClipPathValue {
 public:
  CompositorClipPathValue() = default;
  CompositorClipPathValue(const CompositorClipPathValue& other) = default;
  CompositorClipPathValue& operator=(const CompositorClipPathValue& other) =
      default;

  void Draw(cc::PaintCanvas* canvas) const;
  void Interpolate(const CompositorClipPathValue& other,
                   float progress,
                   CompositorClipPathValue* result) const;
  const SkRect& GetBounds() const;
  const std::optional<BasicShape::ShapeType>& GetShapeType() const {
    return shape_type_;
  }

  // True if the BasicShape types are interpolable, or if both values are
  // geometry boxes, false otherwise.
  bool AreShapeTypesInterpolable(const CompositorClipPathValue& other) const;
  // True iff the SKIA paths of the value pair are interpolatable in SKIA, false
  // otherwise. SKIA paths are interpolatable if they contain equal verbs and
  // equal weights. Full criteria can be found in SkPath::isInterpolatable() in
  // SkPath.h. A true here does not imply the values are interpolatable, type
  // information is not compared.
  bool ArePathsInterpolatable(const CompositorClipPathValue& other) const;

  // Returns true if all points are roughly equal. SkScalarNearlyEqual is used
  // for comparison, and the default tolerance is used.
  bool IsRoughlyEqualForTesting(const CompositorClipPathValue& other) const;

  static CompositorClipPathValue FromShape(const BasicShape& shape,
                                           const gfx::RectF& reference_box,
                                           const gfx::Vector2dF& clip_offset,
                                           float zoom);
  static CompositorClipPathValue FromGeometryBox(
      ContouredRect& box,
      const gfx::Vector2dF& clip_offset);
  static CompositorClipPathValue FromRectF(const gfx::RectF& rect);

 private:
  CompositorClipPathValue(
      const SkPath& path,
      const std::optional<SkRRect>& rrect,
      const std::optional<BasicShape::ShapeType>& shape_type,
      bool is_geometry_box);

  void InterpolateDiscretely(const CompositorClipPathValue& other,
                             float progress,
                             CompositorClipPathValue* result) const;
  static SkRRect InterpolateRRects(const SkRRect& from,
                                   const SkRRect& to,
                                   float progress);

  SkPath path_;
  std::optional<SkRRect> rrect_;
  std::optional<BasicShape::ShapeType> shape_type_;
  bool is_geometry_box_;
};

template <>
struct VectorTraits<CompositorClipPathValue>
    : VectorTraitsBase<CompositorClipPathValue> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_CLIP_PATH_VALUE_H_
