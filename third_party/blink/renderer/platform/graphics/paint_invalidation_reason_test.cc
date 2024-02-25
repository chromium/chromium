// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"

#include <sstream>

#include "base/functional/function_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using ReasonFunction = base::FunctionRef<void(PaintInvalidationReason)>;

PaintInvalidationReason NextReason(PaintInvalidationReason r) {
  return static_cast<PaintInvalidationReason>(static_cast<unsigned>(r) + 1);
}

void ForReasons(PaintInvalidationReason min,
                PaintInvalidationReason max,
                ReasonFunction f) {
  for (auto i = min; i <= max; i = NextReason(i))
    f(i);
}

TEST(PaintInvalidationReasonTest, ToString) {
  ForReasons(PaintInvalidationReason::kNone, PaintInvalidationReason::kMax,
             [](PaintInvalidationReason r) {
               EXPECT_STRNE("", PaintInvalidationReasonToString(r));
             });

  EXPECT_STREQ("none",
               PaintInvalidationReasonToString(PaintInvalidationReason::kNone));
  EXPECT_STREQ("geometry", PaintInvalidationReasonToString(
                               PaintInvalidationReason::kLayout));
}

TEST(PaintInvalidationReasonTest, IsFullGeometryPaintInvalidationReason) {
  ForReasons(PaintInvalidationReason::kNone,
             PaintInvalidationReason::kNonFullMax,
             [](PaintInvalidationReason r) {
               EXPECT_FALSE(IsFullPaintInvalidationReason(r));
               EXPECT_FALSE(IsNonLayoutFullPaintInvalidationReason(r));
               EXPECT_FALSE(IsLayoutFullPaintInvalidationReason(r));
             });
  ForReasons(NextReason(PaintInvalidationReason::kNonFullMax),
             PaintInvalidationReason::kNonLayoutMax,
             [](PaintInvalidationReason r) {
               EXPECT_TRUE(IsFullPaintInvalidationReason(r));
               EXPECT_TRUE(IsNonLayoutFullPaintInvalidationReason(r));
               EXPECT_FALSE(IsLayoutFullPaintInvalidationReason(r));
             });
  ForReasons(NextReason(PaintInvalidationReason::kNonLayoutMax),
             PaintInvalidationReason::kLayoutMax,
             [](PaintInvalidationReason r) {
               EXPECT_TRUE(IsFullPaintInvalidationReason(r));
               EXPECT_FALSE(IsNonLayoutFullPaintInvalidationReason(r));
               EXPECT_TRUE(IsLayoutFullPaintInvalidationReason(r));
             });
  ForReasons(NextReason(PaintInvalidationReason::kLayoutMax),
             PaintInvalidationReason::kMax, [](PaintInvalidationReason r) {
               EXPECT_TRUE(IsFullPaintInvalidationReason(r));
               EXPECT_FALSE(IsNonLayoutFullPaintInvalidationReason(r));
               EXPECT_FALSE(IsLayoutFullPaintInvalidationReason(r));
             });
}

}  // namespace
}  // namespace blink
