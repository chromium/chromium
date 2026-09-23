// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/region_ops.h"

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace skia {

namespace {

// The ops are applied in list order. An add op unions its rect into the
// region and a subtract op removes its rect. Where rects overlap, the later
// op wins. So a point ends up in the region exactly when the last rect in the
// list that contains it is an add.
//
// A SequenceRegion describes one contiguous part of the list. `result` is the
// region that part produces when it is applied in order to an empty region.
// `covered` is the union of all its rects, added or subtracted. `result`
// always lies inside `covered`. If every op in the part is an add, the two
// are equal, so only `result` is stored and Covered() returns it.
//
// Correctness. A part of the list does not change a point that lies outside
// all of its rects. If a point lies inside at least one of its rects, and
// nothing after that part contains the point, then the last rect in the
// whole list that contains the point is in that part. So that part alone
// decides whether the point is in the region. Earlier ops do not matter. Now
// apply the first half and then the second half. A point outside every rect
// of the second half is in the region exactly when it is in `first.result`.
// A point inside a rect of the second half is in the region exactly when it
// is in `second.result`. So the answer is `first.result`, minus
// `second.covered`, plus `second.result`. This is what Build() does. It
// needs only `result` and `covered` from each half, so each half can be
// built on its own. The combined `covered` is the union of the two. The
// argument holds for any split point. Build() splits in the middle only to
// keep the recursion shallow: the depth is log2(n), 20 for a million rects.
//
// Cost. For n rects, one SkRegion::op() per rect costs O(n^2), because each
// op walks the whole region built so far. Grouping consecutive ops of the
// same kind into one op does not fix this. If adds and subtracts alternate,
// each group has one rect and each op still walks the whole region. Here,
// each region comes only from the rects of its own half. So the ops at one
// level of the recursion together handle each rect about once. There are
// about log2(n) levels, so the total is O(n log n) for regions whose size
// grows with the number of rects in them, which is the usual case.
//
// The shortcuts give the same answer as the general case with fewer ops. If
// the second half is all adds, its `covered` equals its `result`. Then the
// removal is redundant and one union is enough. If the first half is also
// all adds, `covered` is not built at all. If `second.result` is empty, for
// example because the second half has no adds, adding it does nothing, so
// one difference is enough.
struct SequenceRegion {
  const SkRegion& Covered() const { return all_adds ? result : covered; }

  SkRegion result;
  SkRegion covered;
  bool all_adds = false;
};

SequenceRegion Build(base::span<const RegionRectOp> ops) {
  SequenceRegion out;
  if (ops.size() == 1) {
    if (ops[0].subtract) {
      out.covered.setRect(ops[0].rect);
    } else {
      out.result.setRect(ops[0].rect);
      out.all_adds = true;
    }
    return out;
  }

  const size_t mid = ops.size() / 2;
  const SequenceRegion first = Build(ops.first(mid));
  const SequenceRegion second = Build(ops.subspan(mid));
  if (first.all_adds && second.all_adds) {
    out.result.op(first.result, second.result, SkRegion::kUnion_Op);
    out.all_adds = true;
    return out;
  }
  if (second.result.isEmpty()) {
    out.result.op(first.result, second.Covered(), SkRegion::kDifference_Op);
  } else if (second.all_adds) {
    out.result.op(first.result, second.result, SkRegion::kUnion_Op);
  } else {
    SkRegion first_outside_second;
    first_outside_second.op(first.result, second.covered,
                            SkRegion::kDifference_Op);
    out.result.op(first_outside_second, second.result, SkRegion::kUnion_Op);
  }
  out.covered.op(first.Covered(), second.Covered(), SkRegion::kUnion_Op);
  return out;
}

}  // namespace

SkRegion RegionFromRectOps(base::span<const RegionRectOp> ops) {
  if (ops.empty()) {
    return SkRegion();
  }
  return Build(ops).result;
}

}  // namespace skia
