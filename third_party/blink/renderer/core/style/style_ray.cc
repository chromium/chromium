// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_ray.h"

#include "base/notreached.h"

namespace blink {

scoped_refptr<StyleRay> StyleRay::Create(float angle,
                                         RaySize size,
                                         bool contain) {
  return base::AdoptRef(new StyleRay(angle, size, contain));
}

StyleRay::StyleRay(float angle, RaySize size, bool contain)
    : angle_(angle), size_(size), contain_(contain) {}

bool StyleRay::IsEqualAssumingSameType(const BasicShape& o) const {
  const StyleRay& other = To<StyleRay>(o);
  return angle_ == other.angle_ && size_ == other.size_ &&
         contain_ == other.contain_;
}

void StyleRay::GetPath(Path&, const gfx::RectF&, float) {
  // ComputedStyle::ApplyMotionPathTransform cannot call GetPath
  // for rays as they may have infinite length.
  NOTREACHED();
}

}  // namespace blink
