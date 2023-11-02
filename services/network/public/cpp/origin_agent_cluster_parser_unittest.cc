// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/origin_agent_cluster_parser.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(OriginAgentClusterHeaderTest, Parse) {
  using mojom::OriginAgentClusterValue;

  EXPECT_EQ(ParseOriginAgentCluster(""), OriginAgentClusterValue::kAbsent);

  EXPECT_EQ(ParseOriginAgentCluster("?1"), OriginAgentClusterValue::kTrue);
  EXPECT_EQ(ParseOriginAgentCluster("?0"), OriginAgentClusterValue::kFalse);

  EXPECT_EQ(ParseOriginAgentCluster("?1;param"),
            OriginAgentClusterValue::kTrue);
  EXPECT_EQ(ParseOriginAgentCluster("?1;param=value"),
            OriginAgentClusterValue::kTrue);
  EXPECT_EQ(ParseOriginAgentCluster("?1;param=value;param2=value2"),
            OriginAgentClusterValue::kTrue);
  EXPECT_EQ(ParseOriginAgentCluster("?0;param=value"),
            OriginAgentClusterValue::kFalse);

  EXPECT_EQ(ParseOriginAgentCluster("true"), OriginAgentClusterValue::kAbsent);
  EXPECT_EQ(ParseOriginAgentCluster("\"?1\""),
            OriginAgentClusterValue::kAbsent);
  EXPECT_EQ(ParseOriginAgentCluster("1"), OriginAgentClusterValue::kAbsent);
  EXPECT_EQ(ParseOriginAgentCluster("?2"), OriginAgentClusterValue::kAbsent);
  EXPECT_EQ(ParseOriginAgentCluster("(?1)"), OriginAgentClusterValue::kAbsent);
}

}  // namespace network
