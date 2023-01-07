// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/storage_partition_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Test that the Less comparison function is implemented properly to uniquely
// identify storage partitions used as keys in a std::map.
TEST(StoragePartitionConfigTest, OperatorLess) {
  StoragePartitionConfig c1(std::string(), std::string(), false);
  StoragePartitionConfig c2(std::string(), std::string(), false);
  StoragePartitionConfig c3(std::string(), std::string(), true);
  StoragePartitionConfig c4("a", std::string(), true);
  StoragePartitionConfig c5("b", std::string(), true);
  StoragePartitionConfig c6(std::string(), "abc", false);
  StoragePartitionConfig c7(std::string(), "abc", true);
  StoragePartitionConfig c8("a", "abc", false);
  StoragePartitionConfig c9("a", "abc", true);

  // Let's ensure basic comparison works.
  EXPECT_TRUE(c1 < c3);
  EXPECT_TRUE(c1 < c4);
  EXPECT_TRUE(c3 < c4);
  EXPECT_TRUE(c4 < c5);
  EXPECT_TRUE(c4 < c8);
  EXPECT_TRUE(c6 < c4);
  EXPECT_TRUE(c6 < c7);
  EXPECT_TRUE(c8 < c9);

  // Now, ensure antisymmetry for each pair we've tested.
  EXPECT_FALSE(c3 < c1);
  EXPECT_FALSE(c4 < c1);
  EXPECT_FALSE(c4 < c3);
  EXPECT_FALSE(c5 < c4);
  EXPECT_FALSE(c8 < c4);
  EXPECT_FALSE(c4 < c6);
  EXPECT_FALSE(c7 < c6);
  EXPECT_FALSE(c9 < c8);

  // Check for irreflexivity.
  EXPECT_FALSE(c1 < c1);
  EXPECT_TRUE(c8 == c8);
  EXPECT_FALSE(c8 != c8);

  // Check for transitivity.
  EXPECT_TRUE(c1 < c4);

  // Let's enforce that two identical elements obey strict weak ordering.
  EXPECT_TRUE(!(c1 < c2) && !(c2 < c1));
}

}  // namespace content
