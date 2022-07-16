// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_SHAPE_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_SHAPE_HELPERS_H_

// Code that is useful for both FloatPolygon and FloatQuad.

#include "third_party/blink/renderer/platform/geometry/float_size.h"

namespace blink {
inline float Determinant(const FloatSize& a, const FloatSize& b) {
  return a.width() * b.height() - a.height() * b.width();
}
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_SHAPE_HELPERS_H_
