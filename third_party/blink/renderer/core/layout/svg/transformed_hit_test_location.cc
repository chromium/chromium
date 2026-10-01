// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

template <typename TransformType>
void LocationTransformHelper(const HitTestLocation& location,
                             const TransformType& transform,
                             std::optional<HitTestLocation>& storage) {
  gfx::PointF transformed_point =
      transform.MapPoint(location.TransformedPoint());
  if (location.IsRectBasedTest()) [[unlikely]] {
    storage.emplace(transformed_point,
                    transform.MapQuad(location.TransformedRect()));
  } else {
    gfx::RectF mapped_rect =
        transform.MapRect(gfx::RectF(location.BoundingBox()));
    if (mapped_rect.width() < 1 || mapped_rect.height() < 1) {
      // Specify |bounding_box| argument even if |location| is not rect-based.
      // Without it, HitTestLocation would have 1x1 bounding box, and it would
      // be mapped to NxN screen pixels if scaling factor is N.
      storage.emplace(transformed_point,
                      PhysicalRect::EnclosingRect(mapped_rect));
    } else {
      storage.emplace(transformed_point);
    }
  }
}

// Maps |location| from the space of |transform|'s parent node into the local
// space of |transform|, using the same projection that rendering uses
// (including 3D components and inherited perspective).
const HitTestLocation* ProjectLocationIfNeeded(
    const HitTestLocation& location,
    const TransformPaintPropertyNode& transform,
    std::optional<HitTestLocation>& storage) {
  gfx::Transform projection;
  if (!GeometryMapper::SourceToDestinationProjection(*transform.Parent(),
                                                     transform, projection)) {
    return nullptr;
  }
  if (projection.IsIdentity()) {
    return &location;
  }
  LocationTransformHelper(location, projection, storage);
  return &*storage;
}

const HitTestLocation* InverseTransformLocationIfNeeded(
    const HitTestLocation& location,
    const AffineTransform& transform,
    std::optional<HitTestLocation>& storage) {
  if (transform.IsIdentity()) {
    return &location;
  }
  if (!transform.IsInvertible()) {
    return nullptr;
  }
  const AffineTransform inverse = transform.Inverse();
  LocationTransformHelper(location, inverse, storage);
  return &*storage;
}

const HitTestLocation* TransformLocationIfNeeded(
    const HitTestLocation& location,
    const AffineTransform& transform,
    std::optional<HitTestLocation>& storage) {
  if (transform.IsIdentity()) {
    return &location;
  }
  LocationTransformHelper(location, transform, storage);
  return &*storage;
}

}  // namespace

TransformedHitTestLocation::TransformedHitTestLocation(
    const HitTestLocation& location,
    const AffineTransform& transform)
    : location_(
          InverseTransformLocationIfNeeded(location, transform, storage_)) {}

TransformedHitTestLocation::TransformedHitTestLocation(
    const HitTestLocation& location,
    const AffineTransform& transform,
    InverseTag)
    : location_(TransformLocationIfNeeded(location, transform, storage_)) {}

TransformedHitTestLocation::TransformedHitTestLocation(
    const HitTestLocation& location,
    const LayoutObject& object)
    : location_(nullptr) {
  if (TransformHelper::HasCss3DTransform(object)) [[unlikely]] {
    // TODO(crbug.com/41310059): This falls back to the flattened transform
    // below when paint properties are not available (e.g. for descendants of
    // a <clipPath>), but the spec defines how 3D transforms apply there.
    if (const ObjectPaintProperties* properties =
            object.FirstFragment().PaintProperties()) {
      if (const TransformPaintPropertyNode* transform =
              properties->Transform()) {
        location_ = ProjectLocationIfNeeded(location, *transform, storage_);
        return;
      }
    }
  }
  location_ = InverseTransformLocationIfNeeded(
      location, object.LocalToSVGParentTransform(), storage_);
}

}  // namespace blink
