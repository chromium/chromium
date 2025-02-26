// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_SKIA_GEOMETRY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_SKIA_GEOMETRY_UTILS_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

inline SkScalar WebCoreFloatToSkScalar(float f) {
  return SkFloatToScalar(std::isfinite(f) ? f : 0);
}

inline SkScalar WebCoreDoubleToSkScalar(double d) {
  return SkDoubleToScalar(std::isfinite(d) ? d : 0);
}

inline bool WebCoreFloatNearlyEqual(float a, float b) {
  return SkScalarNearlyEqual(WebCoreFloatToSkScalar(a),
                             WebCoreFloatToSkScalar(b));
}

inline SkPoint FloatPointToSkPoint(const gfx::PointF& point) {
  return SkPoint::Make(WebCoreFloatToSkScalar(point.x()),
                       WebCoreFloatToSkScalar(point.y()));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_SKIA_GEOMETRY_UTILS_H_
