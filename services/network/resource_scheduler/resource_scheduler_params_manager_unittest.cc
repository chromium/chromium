// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler/resource_scheduler_params_manager.h"

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Should remain synchronized with the values in
// resouce_scheduler_params_manager.cc.
constexpr base::TimeDelta kLowerBoundQueuingDuration = base::Seconds(15);
constexpr base::TimeDelta kUpperBoundQueuingDuration = base::Seconds(120);
constexpr int kHttpRttMultiplierForQueuingDuration = 30;

class ResourceSchedulerParamsManagerTest : public testing::Test {
 public:
  ResourceSchedulerParamsManagerTest() {}

  ResourceSchedulerParamsManagerTest(
      const ResourceSchedulerParamsManagerTest&) = delete;
  ResourceSchedulerParamsManagerTest& operator=(
      const ResourceSchedulerParamsManagerTest&) = delete;

  ~ResourceSchedulerParamsManagerTest() override {}

  void ReadConfigTestHelper(size_t num_ranges) {
    base::test::ScopedFeatureList scoped_feature_list;
    base::FieldTrialParams params;
    int index = 1;
    net::EffectiveConnectionType effective_connection_type =
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    while (effective_connection_type <
           net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G + num_ranges) {
      std::string index_str = base::NumberToString(index);
      params["EffectiveConnectionType" + index_str] =
          net::GetNameForEffectiveConnectionType(effective_connection_type);
      params["MaxDelayableRequests" + index_str] = index_str + "0";
      params["NonDelayableWeight" + index_str] = "0";
      effective_connection_type = static_cast<net::EffectiveConnectionType>(
          static_cast<int>(effective_connection_type) + 1);
      ++index;
    }

    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kThrottleDelayable, params);

    ResourceSchedulerParamsManager resource_scheduler_params_manager;

    effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    while (effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST) {
      if (effective_connection_type <
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G + num_ranges) {
        int type = static_cast<int>(effective_connection_type) - 1;
        EXPECT_EQ(type * 10u, resource_scheduler_params_manager
                                  .GetParamsForEffectiveConnectionType(
                                      effective_connection_type)
                                  .max_delayable_requests);
        EXPECT_EQ(0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(
                             effective_connection_type)
                         .non_delayable_weight);
      } else {
        VerifyDefaultParams(resource_scheduler_params_manager,
                            effective_connection_type);
      }
      effective_connection_type = static_cast<net::EffectiveConnectionType>(
          static_cast<int>(effective_connection_type) + 1);
    }
  }

  void VerifyDefaultParams(
      const ResourceSchedulerParamsManager& resource_scheduler_params_manager,
      net::EffectiveConnectionType effective_connection_type) const {
    switch (effective_connection_type) {
      case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
      case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
      case net::EFFECTIVE_CONNECTION_TYPE_4G:
        EXPECT_EQ(10u, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .max_delayable_requests);
        EXPECT_EQ(0.0, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .non_delayable_weight);
        EXPECT_FALSE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .delay_requests_on_multiplexed_connections);
        EXPECT_TRUE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .max_queuing_time.has_value());
        EXPECT_FALSE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .http_rtt_multiplier_for_proactive_throttling);
        return;

      case net::EFFECTIVE_CONNECTION_TYPE_3G:
        EXPECT_EQ(8u, resource_scheduler_params_manager
                          .GetParamsForEffectiveConnectionType(
                              effective_connection_type)
                          .max_delayable_requests);
        EXPECT_EQ(3.0, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .non_delayable_weight);
        EXPECT_TRUE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .delay_requests_on_multiplexed_connections);
        EXPECT_TRUE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .max_queuing_time.has_value());
        EXPECT_FALSE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .http_rtt_multiplier_for_proactive_throttling);
        return;

      case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
      case net::EFFECTIVE_CONNECTION_TYPE_2G:
        EXPECT_EQ(8u, resource_scheduler_params_manager
                          .GetParamsForEffectiveConnectionType(
                              effective_connection_type)
                          .max_delayable_requests);
        EXPECT_EQ(3.0, resource_scheduler_params_manager
                           .GetParamsForEffectiveConnectionType(
                               effective_connection_type)
                           .non_delayable_weight);
        EXPECT_TRUE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .delay_requests_on_multiplexed_connections);
        EXPECT_TRUE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .max_queuing_time.has_value());
        EXPECT_FALSE(
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(effective_connection_type)
                .http_rtt_multiplier_for_proactive_throttling);
        return;

      case net::EFFECTIVE_CONNECTION_TYPE_LAST:
        NOTREACHED_IN_MIGRATION();
        return;
    }
  }
};

TEST_F(ResourceSchedulerParamsManagerTest, VerifyAllDefaultParams) {
  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  for (int effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST;
       ++effective_connection_type) {
    VerifyDefaultParams(
        resource_scheduler_params_manager,
        static_cast<net::EffectiveConnectionType>(effective_connection_type));
  }
}

// Verify that the params are parsed correctly when
// kDelayRequestsOnMultiplexedConnections is enabled.
TEST_F(ResourceSchedulerParamsManagerTest,
       DelayRequestsOnMultiplexedConnections) {
  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  for (int effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST;
       ++effective_connection_type) {
    net::EffectiveConnectionType ect =
        static_cast<net::EffectiveConnectionType>(effective_connection_type);
    if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G ||
        effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_2G ||
        effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_3G) {
      EXPECT_EQ(8u, resource_scheduler_params_manager
                        .GetParamsForEffectiveConnectionType(ect)
                        .max_delayable_requests);
      EXPECT_EQ(3.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .delay_requests_on_multiplexed_connections);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .max_queuing_time.has_value());

    } else {
      VerifyDefaultParams(
          resource_scheduler_params_manager,
          static_cast<net::EffectiveConnectionType>(effective_connection_type));
    }
  }
}

// Verify that the params are parsed correctly when
// kDelayRequestsOnMultiplexedConnections is disabled.
TEST_F(ResourceSchedulerParamsManagerTest,
       DisableDelayRequestsOnMultiplexedConnections) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDelayRequestsOnMultiplexedConnections);

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  for (int effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST;
       ++effective_connection_type) {
    net::EffectiveConnectionType ect =
        static_cast<net::EffectiveConnectionType>(effective_connection_type);
    if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G ||
        effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_2G ||
        effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_3G) {
      EXPECT_EQ(8u, resource_scheduler_params_manager
                        .GetParamsForEffectiveConnectionType(ect)
                        .max_delayable_requests);
      EXPECT_EQ(3.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_FALSE(resource_scheduler_params_manager
                       .GetParamsForEffectiveConnectionType(ect)
                       .delay_requests_on_multiplexed_connections);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .max_queuing_time.has_value());

    } else {
      VerifyDefaultParams(
          resource_scheduler_params_manager,
          static_cast<net::EffectiveConnectionType>(effective_connection_type));
    }
  }
}

TEST_F(ResourceSchedulerParamsManagerTest, MaxQueuingTime) {
  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  for (int effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST;
       ++effective_connection_type) {
    net::EffectiveConnectionType ect =
        static_cast<net::EffectiveConnectionType>(effective_connection_type);
    base::TimeDelta typical_http_rtt =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(ect);

    if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G ||
        effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_2G) {
      EXPECT_EQ(8u, resource_scheduler_params_manager
                        .GetParamsForEffectiveConnectionType(ect)
                        .max_delayable_requests);
      EXPECT_EQ(3.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .delay_requests_on_multiplexed_connections);
      EXPECT_EQ(typical_http_rtt * kHttpRttMultiplierForQueuingDuration,
                resource_scheduler_params_manager
                    .GetParamsForEffectiveConnectionType(ect)
                    .max_queuing_time);

    } else if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_3G) {
      EXPECT_EQ(8u, resource_scheduler_params_manager
                        .GetParamsForEffectiveConnectionType(ect)
                        .max_delayable_requests);
      EXPECT_EQ(3.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .delay_requests_on_multiplexed_connections);
      EXPECT_EQ(kLowerBoundQueuingDuration,
                resource_scheduler_params_manager
                    .GetParamsForEffectiveConnectionType(ect)
                    .max_queuing_time);

    } else if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_4G) {
      EXPECT_EQ(10u, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .max_delayable_requests);
      EXPECT_EQ(0.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_FALSE(resource_scheduler_params_manager
                       .GetParamsForEffectiveConnectionType(ect)
                       .delay_requests_on_multiplexed_connections);
      EXPECT_EQ(kLowerBoundQueuingDuration,
                resource_scheduler_params_manager
                    .GetParamsForEffectiveConnectionType(ect)
                    .max_queuing_time);
    } else {
      EXPECT_EQ(10u, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .max_delayable_requests);
      EXPECT_EQ(0.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_FALSE(resource_scheduler_params_manager
                       .GetParamsForEffectiveConnectionType(ect)
                       .delay_requests_on_multiplexed_connections);
      EXPECT_EQ(kUpperBoundQueuingDuration,
                resource_scheduler_params_manager
                    .GetParamsForEffectiveConnectionType(ect)
                    .max_queuing_time);
    }
  }
}

// Verify that the params are parsed correctly when
// kDelayRequestsOnMultiplexedConnections and kThrottleDelayable are enabled.
TEST_F(ResourceSchedulerParamsManagerTest, MultipleFieldTrialsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  // Configure kDelayRequestsOnMultiplexedConnections experiment params.
  base::FieldTrialParams params_multiplex;
  params_multiplex["MaxEffectiveConnectionType"] = "3G";

  // Configure kThrottleDelayable experiment params.
  base::FieldTrialParams params_throttle_delayable;
  params_throttle_delayable["EffectiveConnectionType1"] = "3G";
  params_throttle_delayable["MaxDelayableRequests1"] = "12";
  params_throttle_delayable["NonDelayableWeight1"] = "3.0";
  params_throttle_delayable["EffectiveConnectionType2"] = "4G";
  params_throttle_delayable["MaxDelayableRequests2"] = "14";
  params_throttle_delayable["NonDelayableWeight2"] = "4.0";
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::kDelayRequestsOnMultiplexedConnections, params_multiplex},
       {features::kThrottleDelayable, params_throttle_delayable}},
      {});

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  // Verify the parsed params.
  for (int effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
       effective_connection_type < net::EFFECTIVE_CONNECTION_TYPE_LAST;
       ++effective_connection_type) {
    net::EffectiveConnectionType ect =
        static_cast<net::EffectiveConnectionType>(effective_connection_type);
    if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G ||
        effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_2G) {
      EXPECT_EQ(8u, resource_scheduler_params_manager
                        .GetParamsForEffectiveConnectionType(ect)
                        .max_delayable_requests);
      EXPECT_EQ(3.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .delay_requests_on_multiplexed_connections);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .max_queuing_time.has_value());

    } else if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_3G) {
      EXPECT_EQ(12u, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .max_delayable_requests);
      EXPECT_EQ(3.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .delay_requests_on_multiplexed_connections);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .max_queuing_time.has_value());

    } else if (effective_connection_type == net::EFFECTIVE_CONNECTION_TYPE_4G) {
      EXPECT_EQ(14u, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .max_delayable_requests);
      EXPECT_EQ(4.0, resource_scheduler_params_manager
                         .GetParamsForEffectiveConnectionType(ect)
                         .non_delayable_weight);
      EXPECT_FALSE(resource_scheduler_params_manager
                       .GetParamsForEffectiveConnectionType(ect)
                       .delay_requests_on_multiplexed_connections);
      EXPECT_TRUE(resource_scheduler_params_manager
                      .GetParamsForEffectiveConnectionType(ect)
                      .max_queuing_time.has_value());

    } else {
      VerifyDefaultParams(resource_scheduler_params_manager, ect);
    }
  }
}

// Test that a configuration with bad strings does not break the parser, and
// the parser stops reading the configuration after it encounters the first
// missing index.
TEST_F(ResourceSchedulerParamsManagerTest, ReadInvalidConfigTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  // Skip configuration parameters for index 2 to test that the parser stops
  // when it cannot find the parameters for an index.
  for (int range_index : {1, 3, 4}) {
    std::string index_str = base::NumberToString(range_index);
    params["EffectiveConnectionType" + index_str] = "Slow-2G";
    params["MaxDelayableRequests" + index_str] = index_str + "0";
    params["NonDelayableWeight" + index_str] = "0";
  }
  // Add some bad configuration strigs to ensure that the parser does not break.
  params["BadConfigParam1"] = "100";
  params["BadConfigParam2"] = "100";

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kThrottleDelayable, params);

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  // Only the first configuration parameter must be read because a match was not
  // found for index 2. The configuration parameters with index 3 and 4 must be
  // ignored, even though they are valid configuration parameters.
  EXPECT_EQ(10u, resource_scheduler_params_manager
                     .GetParamsForEffectiveConnectionType(
                         net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                     .max_delayable_requests);
  EXPECT_EQ(0.0, resource_scheduler_params_manager
                     .GetParamsForEffectiveConnectionType(
                         net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                     .non_delayable_weight);

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_2G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_3G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_4G);
}

// Test that a configuration with 2 ranges is read correctly.
TEST_F(ResourceSchedulerParamsManagerTest, ReadValidConfigTest2) {
  ReadConfigTestHelper(2);
}

// Test that a configuration with 3 ranges is read correctly.
TEST_F(ResourceSchedulerParamsManagerTest, ReadValidConfigTest3) {
  ReadConfigTestHelper(3);
}

TEST_F(ResourceSchedulerParamsManagerTest, ThrottleDelayableDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kThrottleDelayable);

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_2G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_3G);
  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_4G);
}

TEST_F(ResourceSchedulerParamsManagerTest,
       MaxDelayableRequestsAndNonDelayableWeightSet) {
  base::test::ScopedFeatureList scoped_feature_list;

  base::FieldTrialParams params;

  params["EffectiveConnectionType1"] = "Slow-2G";
  size_t max_delayable_requests_slow_2g = 2u;
  double non_delayable_weight_slow_2g = 2.0;
  params["MaxDelayableRequests1"] =
      base::NumberToString(max_delayable_requests_slow_2g);
  params["NonDelayableWeight1"] =
      base::NumberToString(non_delayable_weight_slow_2g);

  params["EffectiveConnectionType2"] = "3G";
  size_t max_delayable_requests_3g = 4u;
  double non_delayable_weight_3g = 0.0;
  params["MaxDelayableRequests2"] =
      base::NumberToString(max_delayable_requests_3g);
  params["NonDelayableWeight2"] = base::NumberToString(non_delayable_weight_3g);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kThrottleDelayable, params);

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  EXPECT_EQ(max_delayable_requests_slow_2g,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                .max_delayable_requests);
  EXPECT_EQ(non_delayable_weight_slow_2g,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                .non_delayable_weight);
  EXPECT_TRUE(resource_scheduler_params_manager
                  .GetParamsForEffectiveConnectionType(
                      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                  .max_queuing_time.has_value());

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_2G);

  EXPECT_EQ(max_delayable_requests_3g,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_3G)
                .max_delayable_requests);
  EXPECT_EQ(non_delayable_weight_3g, resource_scheduler_params_manager
                                         .GetParamsForEffectiveConnectionType(
                                             net::EFFECTIVE_CONNECTION_TYPE_3G)
                                         .non_delayable_weight);
  EXPECT_TRUE(resource_scheduler_params_manager
                  .GetParamsForEffectiveConnectionType(
                      net::EFFECTIVE_CONNECTION_TYPE_3G)
                  .max_queuing_time.has_value());

  VerifyDefaultParams(resource_scheduler_params_manager,
                      net::EFFECTIVE_CONNECTION_TYPE_4G);
}

TEST_F(ResourceSchedulerParamsManagerTest,
       ProactivelyThrottleLowPriorityRequests) {
  const double kDelaySlow2G = 1.5;
  const double kDelay2G = 1.6;
  const double kDelay4G = 0.5;

  base::test::ScopedFeatureList scoped_feature_list;

  base::FieldTrialParams params;
  params["http_rtt_multiplier_for_proactive_throttling_Slow-2G"] =
      base::NumberToString(kDelaySlow2G);
  params["http_rtt_multiplier_for_proactive_throttling_2G"] =
      base::NumberToString(kDelay2G);
  params["http_rtt_multiplier_for_proactive_throttling_4G"] =
      base::NumberToString(kDelay4G);

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kProactivelyThrottleLowPriorityRequests, params);

  ResourceSchedulerParamsManager resource_scheduler_params_manager;

  EXPECT_EQ(kDelaySlow2G, resource_scheduler_params_manager
                              .GetParamsForEffectiveConnectionType(
                                  net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G)
                              .http_rtt_multiplier_for_proactive_throttling);
  EXPECT_EQ(kDelay2G, resource_scheduler_params_manager
                          .GetParamsForEffectiveConnectionType(
                              net::EFFECTIVE_CONNECTION_TYPE_2G)
                          .http_rtt_multiplier_for_proactive_throttling);
  EXPECT_FALSE(resource_scheduler_params_manager
                   .GetParamsForEffectiveConnectionType(
                       net::EFFECTIVE_CONNECTION_TYPE_3G)
                   .http_rtt_multiplier_for_proactive_throttling.has_value());
  EXPECT_EQ(kDelay4G,
            resource_scheduler_params_manager
                .GetParamsForEffectiveConnectionType(
                    net::EFFECTIVE_CONNECTION_TYPE_4G)
                .http_rtt_multiplier_for_proactive_throttling.value());
}

}  // unnamed namespace

}  // namespace network
