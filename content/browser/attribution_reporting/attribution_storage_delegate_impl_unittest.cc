// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/privacy_math.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace {

using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::mojom::SourceType;
using ::testing::AllOf;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Lt;
using ::testing::SizeIs;

// This is more comprehensively tested in
// //components/attribution_reporting/event_report_windows_unittest.cc.
TEST(AttributionStorageDelegateImplTest, GetEventLevelReportTime) {
  constexpr base::Time kSourceTime;
  constexpr base::Time kTriggerTime = kSourceTime + base::Seconds(1);
  constexpr base::TimeDelta kEnd = base::Days(3);

  EXPECT_EQ(kSourceTime + kEnd,
            AttributionStorageDelegateImpl().GetEventLevelReportTime(
                *EventReportWindows::Create(/*start_time=*/base::Seconds(0),
                                            /*end_times=*/{kEnd}),
                kSourceTime, kTriggerTime));
}

TEST(AttributionStorageDelegateImplTest, GetAggregatableReportTime) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionStorageDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time), Lt(trigger_time + base::Minutes(10))));
}

TEST(AttributionStorageDelegateImplTest, NewReportID_IsValidGUID) {
  EXPECT_TRUE(AttributionStorageDelegateImpl().NewReportID().is_valid());
}

TEST(AttributionStorageDelegateImplTest,
     RandomizedResponse_NoNoiseModeReturnsRealRateAndNullResponse) {
  for (auto source_type : {SourceType::kNavigation, SourceType::kEvent}) {
    const auto source =
        SourceBuilder()
            .SetSourceType(source_type)
            .SetMaxEventLevelReports(
                source_type ==
                        attribution_reporting::mojom::SourceType::kNavigation
                    ? 3
                    : 1)
            .BuildStored();

    auto result = AttributionStorageDelegateImpl(AttributionNoiseMode::kNone)
                      .GetRandomizedResponse(source.common_info().source_type(),
                                             source.trigger_specs(),
                                             source.max_event_level_reports(),
                                             source.event_level_epsilon(),
                                             source.source_time());
    ASSERT_TRUE(result.has_value());
    ASSERT_GT(result->rate(), 0);
    ASSERT_EQ(result->response(), absl::nullopt);
  }
}

TEST(AttributionStorageDelegateImplTest,
     RandomizedResponse_ExceedsLimit_ReturnsError) {
  const struct {
    SourceType source_type;
    double max_navigation_info_gain = std::numeric_limits<double>::infinity();
    double max_event_info_gain = std::numeric_limits<double>::infinity();
    bool expected_ok;
  } kTestCases[] = {
      {
          .source_type = SourceType::kNavigation,
          .max_event_info_gain = 0,
          .expected_ok = true,
      },
      {
          .source_type = SourceType::kNavigation,
          .max_navigation_info_gain = 0,
          .expected_ok = false,
      },
      {
          .source_type = SourceType::kEvent,
          .max_navigation_info_gain = 0,
          .expected_ok = true,
      },
      {
          .source_type = SourceType::kEvent,
          .max_event_info_gain = 0,
          .expected_ok = false,
      },
  };

  for (const auto& test_case : kTestCases) {
    AttributionConfig config;
    config.event_level_limit.max_navigation_info_gain =
        test_case.max_navigation_info_gain;
    config.event_level_limit.max_event_info_gain =
        test_case.max_event_info_gain;

    auto delegate = AttributionStorageDelegateImpl::CreateForTesting(
        AttributionNoiseMode::kDefault, AttributionDelayMode::kDefault, config);

    const auto source =
        SourceBuilder().SetSourceType(test_case.source_type).BuildStored();

    auto result = delegate->GetRandomizedResponse(
        test_case.source_type, source.trigger_specs(),
        source.max_event_level_reports(), source.event_level_epsilon(),
        source.source_time());

    EXPECT_EQ(result.has_value(), test_case.expected_ok);
  }
}

class AttributionStorageDelegateImplTestFeatureConfigured
    : public testing::Test {
 public:
  AttributionStorageDelegateImplTestFeatureConfigured() {
    feature_list_.InitWithFeaturesAndParameters(
        {{attribution_reporting::features::kConversionMeasurement,
          {{"aggregate_report_min_delay", "1m"},
           {"aggregate_report_delay_span", "29m"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AttributionStorageDelegateImplTestFeatureConfigured,
       GetFeatureAggregatableReportTime) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionStorageDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time + base::Minutes(1)),
            Lt(trigger_time + base::Minutes(30))));
}

// Verifies that field test params are validated correctly.
class AttributionStorageDelegateImplTestInvalidFeatureConfigured
    : public testing::Test {
 public:
  AttributionStorageDelegateImplTestInvalidFeatureConfigured() {
    feature_list_.InitWithFeaturesAndParameters(
        {{attribution_reporting::features::kConversionMeasurement,
          {{"aggregate_report_min_delay", "-1m"},
           {"aggregate_report_delay_span", "-29m"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AttributionStorageDelegateImplTestInvalidFeatureConfigured,
       NegativeAggregateParams_DefaultsUsed) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionStorageDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time), Lt(trigger_time + base::Minutes(10))));
}

TEST(AttributionStorageDelegateImplTest,
     NullAggregatableReports_IncludeSourceRegistrationTime) {
  const auto trigger = DefaultTrigger();

  EXPECT_THAT(AttributionStorageDelegateImpl()
                  .GetNullAggregatableReports(
                      trigger, /*trigger_time=*/base::Time::Now(),
                      /*attributed_source_time=*/absl::nullopt)
                  .size(),
              Le(31u));

  base::Time attributed_source_time = base::Time::Now() - base::Days(1);
  auto null_reports =
      AttributionStorageDelegateImpl().GetNullAggregatableReports(
          trigger, /*trigger_time=*/base::Time::Now(), attributed_source_time);
  EXPECT_THAT(null_reports.size(), Lt(31u));

  auto same_source_time_report =
      base::ranges::find_if(null_reports, [&](const auto& null_report) {
        return RoundDownToWholeDaySinceUnixEpoch(
                   null_report.fake_source_time) ==
               RoundDownToWholeDaySinceUnixEpoch(attributed_source_time);
      });
  EXPECT_TRUE(same_source_time_report == null_reports.end());
}

TEST(AttributionStorageDelegateImplTest,
     NullAggregatableReports_ExcludeSourceRegistrationTime) {
  const auto trigger = TriggerBuilder()
                           .SetSourceRegistrationTimeConfig(
                               attribution_reporting::mojom::
                                   SourceRegistrationTimeConfig::kExclude)
                           .Build();

  EXPECT_THAT(AttributionStorageDelegateImpl()
                  .GetNullAggregatableReports(
                      trigger, /*trigger_time=*/base::Time::Now(),
                      /*attributed_source_time=*/absl::nullopt)
                  .size(),
              Le(1u));

  EXPECT_THAT(AttributionStorageDelegateImpl().GetNullAggregatableReports(
                  trigger, /*trigger_time=*/base::Time::Now(),
                  /*attributed_source_time=*/base::Time::Now() - base::Days(1)),
              IsEmpty());
}

TEST(AttributionStorageDelegateImplTest,
     NullAggregatableReports_WithTriggerContextId) {
  const auto trigger = TriggerBuilder()
                           .SetSourceRegistrationTimeConfig(
                               attribution_reporting::mojom::
                                   SourceRegistrationTimeConfig::kExclude)
                           .SetTriggerContextId("123")
                           .Build();

  EXPECT_THAT(AttributionStorageDelegateImpl().GetNullAggregatableReports(
                  trigger, /*trigger_time=*/base::Time::Now(),
                  /*attributed_source_time=*/absl::nullopt),
              SizeIs(1u));

  EXPECT_THAT(AttributionStorageDelegateImpl().GetNullAggregatableReports(
                  trigger, /*trigger_time=*/base::Time::Now(),
                  /*attributed_source_time=*/base::Time::Now() - base::Days(1)),
              IsEmpty());
}

}  // namespace
}  // namespace content
