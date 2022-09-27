// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"

#include <sstream>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PaintInvalidationReasonTest, ToString) {
  for (auto i = PaintInvalidationReason::kNone;
       i <= PaintInvalidationReason::kMax;
       i = static_cast<PaintInvalidationReason>(static_cast<int>(i) + 1))
    EXPECT_STRNE("", PaintInvalidationReasonToString(i));

  EXPECT_STREQ("none",
               PaintInvalidationReasonToString(PaintInvalidationReason::kNone));
  EXPECT_STREQ("full",
               PaintInvalidationReasonToString(PaintInvalidationReason::kFull));
}

TEST(PaintInvalidationReasonTest, StreamOutput) {
  for (auto i = PaintInvalidationReason::kNone;
       i <= PaintInvalidationReason::kMax;
       i = static_cast<PaintInvalidationReason>(static_cast<int>(i) + 1)) {
    std::stringstream string_stream;
    string_stream << i;
    EXPECT_EQ(PaintInvalidationReasonToString(i), string_stream.str());
  }
}

}  // namespace blink
