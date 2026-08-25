// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKCOLORSPACE_EXT_H_
#define SKIA_EXT_SKCOLORSPACE_EXT_H_

#include "third_party/skia/include/core/SkColorSpace.h"

namespace skia {

// Return true if `a` and `b` are approximately equal. This uses the same
// per-parameter comparisons (0.001f for transfer functions and 0.01f for XYZ
// matrices) used internally by SkColorSpace (e.g, to snap nearby color space to
// sRGB).
SK_API bool ApproximatelyEqual(const SkColorSpace* a, const SkColorSpace* b);

}  // namespace skia

#endif  // SKIA_EXT_SKCOLORSPACE_EXT_H_
