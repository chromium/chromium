// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/merkle_audit_proof.h"

#include "base/check.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace ct {
namespace {

TEST(MerkleAuditProofTest, CalculatesAuditPathLengthCorrectly) {
  // Test all leaves up to a tree size of 4:
  EXPECT_EQ(0u, CalculateAuditPathLength(0, 1));
  EXPECT_EQ(1u, CalculateAuditPathLength(0, 2));
  EXPECT_EQ(1u, CalculateAuditPathLength(1, 2));
  EXPECT_EQ(2u, CalculateAuditPathLength(0, 3));
  EXPECT_EQ(2u, CalculateAuditPathLength(1, 3));
  EXPECT_EQ(1u, CalculateAuditPathLength(2, 3));
  EXPECT_EQ(2u, CalculateAuditPathLength(0, 4));
  EXPECT_EQ(2u, CalculateAuditPathLength(1, 4));
  EXPECT_EQ(2u, CalculateAuditPathLength(2, 4));
  EXPECT_EQ(2u, CalculateAuditPathLength(3, 4));
  // Boundary cases for a larger tree size:
  EXPECT_EQ(9u, CalculateAuditPathLength(0, 257));
  EXPECT_EQ(9u, CalculateAuditPathLength(255, 257));
  EXPECT_EQ(1u, CalculateAuditPathLength(256, 257));
  // Example from CT over DNS draft RFC:
  EXPECT_EQ(20u, CalculateAuditPathLength(123456, 999999));
  // Test data from
  // https://github.com/google/certificate-transparency/blob/af98904302724c29aa6659ca372d41c9687de2b7/python/ct/crypto/merkle_test.py:
  EXPECT_EQ(22u, CalculateAuditPathLength(848049, 3630887));
}

TEST(MerkleAuditProofDeathTest, DiesIfLeafIndexIsGreaterThanOrEqualToTreeSize) {
#ifdef OFFICIAL_BUILD
  // The official build does not print the reason a CHECK failed.
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] = "leaf_index < tree_size";
#endif

  EXPECT_DEATH_IF_SUPPORTED(CalculateAuditPathLength(0, 0), kErrorRegex);
  EXPECT_DEATH_IF_SUPPORTED(CalculateAuditPathLength(10, 10), kErrorRegex);
  EXPECT_DEATH_IF_SUPPORTED(CalculateAuditPathLength(11, 10), kErrorRegex);
}

}  // namespace
}  // namespace ct
}  // namespace net
