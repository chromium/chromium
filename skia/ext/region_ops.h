// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_REGION_OPS_H_
#define SKIA_EXT_REGION_OPS_H_

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace skia {

// One step in building a region: add `rect` to it, or subtract `rect` from
// it.
struct SK_API RegionRectOp {
  SkIRect rect;
  bool subtract = false;
};

// Returns the region that results from applying `ops` in order to an empty
// region, so that later ops win where rects overlap. Equivalent to calling
// SkRegion::op() once per rect with kUnion_Op or kDifference_Op, but takes
// O(n log n) region operations for n rects instead of O(n^2), however adds
// and subtracts interleave.
SK_API SkRegion RegionFromRectOps(base::span<const RegionRectOp> ops);

}  // namespace skia

#endif  // SKIA_EXT_REGION_OPS_H_
