// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/region_ops.h"

#include <algorithm>
#include <vector>

#include "base/test/insecure_random_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace skia {

namespace {

RegionRectOp Add(int x, int y, int w, int h) {
  return {SkIRect::MakeXYWH(x, y, w, h), false};
}
RegionRectOp Subtract(int x, int y, int w, int h) {
  return {SkIRect::MakeXYWH(x, y, w, h), true};
}

// The one-op-per-rect construction that RegionFromRectOps() replaces.
SkRegion NaiveRegionFromRectOps(const std::vector<RegionRectOp>& ops) {
  SkRegion result;
  for (const RegionRectOp& op : ops) {
    result.op(op.rect,
              op.subtract ? SkRegion::kDifference_Op : SkRegion::kUnion_Op);
  }
  return result;
}

}  // namespace

TEST(RegionOpsTest, Empty) {
  EXPECT_TRUE(RegionFromRectOps({}).isEmpty());
  // Only subtracts.
  EXPECT_TRUE(RegionFromRectOps({{Subtract(0, 0, 10, 10)}}).isEmpty());
  // Empty rects contribute nothing.
  EXPECT_TRUE(
      RegionFromRectOps({{Add(5, 5, 0, 0), Add(5, 5, 10, 0)}}).isEmpty());
}

TEST(RegionOpsTest, LaterOpsWin) {
  // A subtract punches a hole in an earlier add.
  SkRegion region =
      RegionFromRectOps({{Add(0, 0, 100, 20), Subtract(40, 0, 20, 20)}});
  EXPECT_TRUE(region.contains(10, 10));
  EXPECT_FALSE(region.contains(50, 10));
  EXPECT_TRUE(region.contains(90, 10));

  // A later add fills the hole again.
  region = RegionFromRectOps(
      {{Add(0, 0, 100, 20), Subtract(40, 0, 20, 20), Add(45, 5, 10, 10)}});
  EXPECT_FALSE(region.contains(42, 10));
  EXPECT_TRUE(region.contains(50, 10));

  // A subtract before any add removes nothing.
  region = RegionFromRectOps({{Subtract(40, 0, 20, 20), Add(0, 0, 100, 20)}});
  EXPECT_TRUE(region == SkRegion(SkIRect::MakeWH(100, 20)));
}

TEST(RegionOpsTest, MatchesOneOpPerRect) {
  base::test::InsecureRandomGenerator rng;
  rng.ReseedForTesting(12345);
  // Uniform-ish integer in [lo, hi].
  auto rand_int = [&rng](int lo, int hi) {
    return lo + static_cast<int>(rng.RandUint32() % (hi - lo + 1));
  };
  for (int iteration = 0; iteration < 300; ++iteration) {
    // Runs of same-kind ops of random length (often 1), including empty
    // rects.
    std::vector<RegionRectOp> ops;
    bool subtract = iteration % 2 == 0;
    const int count = 1 + iteration;
    const int max_run_length = 1 + iteration % 6;
    while (static_cast<int>(ops.size()) < count) {
      for (int n = rand_int(1, max_run_length);
           n > 0 && static_cast<int>(ops.size()) < count; --n) {
        ops.push_back({SkIRect::MakeXYWH(rand_int(-20, 200), rand_int(-20, 200),
                                         std::max(0, rand_int(-5, 60)),
                                         std::max(0, rand_int(-5, 60))),
                       subtract});
      }
      subtract = !subtract;
    }
    EXPECT_TRUE(RegionFromRectOps(ops) == NaiveRegionFromRectOps(ops))
        << "iteration " << iteration;
  }
}

TEST(RegionOpsTest, ManyRects) {
  // 20,000 disjoint added tiles followed by 20,000 subtracted holes, and then
  // the same tiles and holes interleaved. Both are quadratic with one
  // SkRegion::op() per rect.
  std::vector<RegionRectOp> ops;
  for (int i = 0; i < 20000; ++i) {
    ops.push_back(Add((i % 200) * 20, (i / 200) * 20, 18, 18));
  }
  for (int i = 0; i < 20000; ++i) {
    ops.push_back(Subtract((i % 200) * 20 + 4, (i / 200) * 20 + 4, 4, 4));
  }
  SkRegion region = RegionFromRectOps(ops);
  EXPECT_TRUE(region.contains(1, 1));
  EXPECT_FALSE(region.contains(5, 5));
  EXPECT_FALSE(region.contains(19, 1));
  EXPECT_TRUE(region.contains(20 * 199 + 1, 20 * 99 + 1));
  EXPECT_FALSE(region.contains(20 * 199 + 5, 20 * 99 + 5));

  std::vector<RegionRectOp> interleaved;
  for (int i = 0; i < 20000; ++i) {
    interleaved.push_back(ops[i]);
    interleaved.push_back(ops[20000 + i]);
  }
  EXPECT_TRUE(RegionFromRectOps(interleaved) == region);
}

}  // namespace skia
