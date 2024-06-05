// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_entry_builder.h"

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;

// Tests that calling `SetMetric` repeatedly on an `UkmEntryBuilder` updates the
// value stored for the metric.
TEST(UkmEntryBuilderTest, BuilderAllowsUpdatingMetrics) {
  UkmEntryBuilder builder(kInvalidSourceId, "Kangaroo.Jumped");
  builder.SetMetric("Length", 4);
  EXPECT_THAT(builder.GetEntryForTesting()->metrics, ElementsAre(Pair(_, 4)));

  builder.SetMetric("Length", 5);
  EXPECT_THAT(builder.GetEntryForTesting()->metrics, ElementsAre(Pair(_, 5)));
}

}  // namespace ukm
