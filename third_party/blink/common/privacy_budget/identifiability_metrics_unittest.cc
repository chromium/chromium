// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"

#include <cstdint>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_Basic) {
  const uint8_t kInput[] = {1, 2, 3, 4, 5};
  auto digest = IdentifiabilityDigestOfBytes(kInput);

  // Due to our requirement that the digest be stable and persistable, this test
  // should always pass once the code reaches the stable branch.
  EXPECT_EQ(UINT64_C(0x7cd845f1db5ad659), digest);
}

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_Padding) {
  const uint8_t kTwoBytes[] = {1, 2};
  const std::vector<uint8_t> kLong(16 * 1024, 'x');

  // Ideally we should be using all 64-bits or at least the 56 LSBs.
  EXPECT_EQ(UINT64_C(0xb74c74c9fcf0505a),
            IdentifiabilityDigestOfBytes(kTwoBytes));
  EXPECT_EQ(UINT64_C(0x76b3567105dc5253), IdentifiabilityDigestOfBytes(kLong));
}

TEST(IdentifiabilityMetricsTest, IdentifiabilityDigestOfBytes_EdgeCases) {
  const std::vector<uint8_t> kEmpty;
  const uint8_t kOneByte[] = {1};

  // As before, these tests should always pass.
  EXPECT_EQ(UINT64_C(0x9ae16a3b2f90404f), IdentifiabilityDigestOfBytes(kEmpty));
  EXPECT_EQ(UINT64_C(0x6209312a69a56947),
            IdentifiabilityDigestOfBytes(kOneByte));
}

}  // namespace blink
