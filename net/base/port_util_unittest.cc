// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/port_util.h"

#include <array>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  const auto valid = std::to_array<std::vector<uint16_t>>({
      {},
      {1},
      {1, 2},
      {1, 2, 3},
      {10, 11, 12, 13},
  });

  for (size_t i = 0; i < std::size(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }
}

TEST(NetUtilTest, RestrictedAbusePortsTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRestrictAbusePorts,
      {{features::kPortsToRestrictForAbuse.name, "12345,23456,34567"}});
  EXPECT_TRUE(IsPortAllowedForScheme(443, "https"));
  for (int port : {12345, 23456, 34567}) {
    EXPECT_FALSE(IsPortAllowedForScheme(port, "https"));
  }
}

}  // namespace net
