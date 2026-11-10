// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/observation_table.h"

#include <cstdint>

#include "base/test/gtest_util.h"
#include "content/services/resource_broker/observation_record.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(ObservationTableTest, KeepsNewestAndCountsEvictions) {
  ObservationTable table(/*max_entries=*/3);
  for (uint64_t size = 1; size <= 5; ++size) {
    ObservationRecord record;
    record.body_size = size;
    table.AddRecord(record);
  }

  ASSERT_EQ(table.size(), 3u);
  EXPECT_EQ(table.eviction_count(), 2u);
  EXPECT_EQ(table.records()[0].body_size, 5u);
  EXPECT_EQ(table.records()[1].body_size, 4u);
  EXPECT_EQ(table.records()[2].body_size, 3u);
}

TEST(ObservationTableTest, ZeroMaxEntriesCheckFails) {
  EXPECT_CHECK_DEATH(ObservationTable(0));
}

}  // namespace

}  // namespace content
