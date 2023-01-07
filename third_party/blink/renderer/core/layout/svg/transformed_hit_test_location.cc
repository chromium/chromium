// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"

#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

const HitTestLocation* InverseTransformLocationIfNeeded(
    const HitTestLocation& location,
    const AffineTransform& transform,
    absl::optional<HitTestLocation>& storage) {
  if (transform.IsIdentity())
    return &location;
  if (!transform.IsInvertible())
    return nullptr;
  const AffineTransform inverse = transform.Inverse();
  gfx::PointF transformed_point = inverse.MapPoint(location.TransformedPoint());
  if (UNLIKELY(location.IsRectBasedTest())) {
    storage.emplace(transformed_point,
                    inverse.MapQuad(location.TransformedRect()));
  } else {
    gfx::RectF mapped_rect =
        inverse.MapRect(gfx::RectF(location.BoundingBox()));
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
  return &*storage;
}

}  // namespace

TransformedHitTestLocation::TransformedHitTestLocation(
    const HitTestLocation& location,
    const AffineTransform& transform)
    : location_(
          InverseTransformLocationIfNeeded(location, transform, storage_)) {}

}  // namespace blink
