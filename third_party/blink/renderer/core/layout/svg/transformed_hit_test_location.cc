// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"

#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

namespace {

const HitTestLocation* InverseTransformLocationIfNeeded(
    const HitTestLocation& location,
    const AffineTransform& transform,
    base::Optional<HitTestLocation>& storage) {
  if (transform.IsIdentity())
    return &location;
  if (!transform.IsInvertible())
    return nullptr;
  const AffineTransform inverse = transform.Inverse();
  FloatPoint transformed_point = inverse.MapPoint(location.TransformedPoint());
  if (UNLIKELY(location.IsRectBasedTest())) {
    storage.emplace(transformed_point,
                    inverse.MapQuad(location.TransformedRect()));
  } else {
    storage.emplace(transformed_point);
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
