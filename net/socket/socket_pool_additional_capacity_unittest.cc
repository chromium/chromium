// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_pool_additional_capacity.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(SocketPoolAdditionalCapacityTest, CreateWithDisabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kTcpSocketPoolLimitRandomization);
  EXPECT_EQ(SocketPoolAdditionalCapacity::Create(),
            SocketPoolAdditionalCapacity::CreateForTest(0.0, 0, 0.0, 0.0));
}

TEST(SocketPoolAdditionalCapacityTest, CreateWithEnabledFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTcpSocketPoolLimitRandomization,
      {
          {
              "TcpSocketPoolLimitRandomizationBase",
              "0.1",
          },
          {
              "TcpSocketPoolLimitRandomizationCapacity",
              "2",
          },
          {
              "TcpSocketPoolLimitRandomizationMinimum",
              "0.3",
          },
          {
              "TcpSocketPoolLimitRandomizationNoise",
              "0.4",
          },
      });
  EXPECT_EQ(SocketPoolAdditionalCapacity::Create(),
            SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 0.4));
}

TEST(SocketPoolAdditionalCapacityTest, CreateForTest) {
  EXPECT_EQ(std::string(
                SocketPoolAdditionalCapacity::CreateForTest(0.1, 2, 0.3, 0.4)),
            "SocketPoolAdditionalCapacity(base:1.000000e-01,capacity:2,minimum:"
            "3.000000e-01,noise:4.000000e-01)");
}

}  // namespace

}  // namespace net
