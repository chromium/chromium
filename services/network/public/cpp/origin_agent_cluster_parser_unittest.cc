// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/origin_agent_cluster_parser.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(OriginAgentClusterHeaderTest, Parse) {
  EXPECT_EQ(ParseOriginAgentCluster(""), false);

  EXPECT_EQ(ParseOriginAgentCluster("?1"), true);
  EXPECT_EQ(ParseOriginAgentCluster("?0"), false);

  EXPECT_EQ(ParseOriginAgentCluster("?1;param"), true);
  EXPECT_EQ(ParseOriginAgentCluster("?1;param=value"), true);
  EXPECT_EQ(ParseOriginAgentCluster("?1;param=value;param2=value2"), true);

  EXPECT_EQ(ParseOriginAgentCluster("true"), false);
  EXPECT_EQ(ParseOriginAgentCluster("\"?1\""), false);
  EXPECT_EQ(ParseOriginAgentCluster("1"), false);
  EXPECT_EQ(ParseOriginAgentCluster("?2"), false);
  EXPECT_EQ(ParseOriginAgentCluster("(?1)"), false);
}

}  // namespace network
