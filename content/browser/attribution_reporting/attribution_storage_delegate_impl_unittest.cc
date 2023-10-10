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
#include "content/browser/attribution_reporting/combinatorics.h"
#include "content/browser/attribution_reporting/common_source_info.h"
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

using FakeReport = ::content::AttributionStorageDelegate::FakeReport;

constexpr base::TimeDelta kDefaultExpiry = base::Days(30);

void RunRandomFakeReportsTest(const SourceType source_type,
                              const int num_stars,
                              const int num_bars,
                              const int num_samples,
                              const double tolerance) {
  const auto source =
      SourceBuilder()
          .SetSourceType(source_type)
          .SetExpiry(kDefaultExpiry)
          .SetEventReportWindows(*EventReportWindows::FromDefaults(
              /*report_window=*/kDefaultExpiry, source_type))
          .SetMaxEventLevelReports(
              source_type ==
                      attribution_reporting::mojom::SourceType::kNavigation
                  ? 3
                  : 1)
          .BuildStored();

  base::flat_map<std::vector<FakeReport>, int> output_counts;
  for (int i = 0; i < num_samples; i++) {
    const AttributionStorageDelegateImpl delegate;
    const int64_t num_states =
        delegate.GetNumStates(source_type, source.event_report_windows(),
                              source.max_event_level_reports());
    std::vector<FakeReport> fake_reports = delegate.GetRandomFakeReports(
        source.common_info().source_type(), source.event_report_windows(),
        source.max_event_level_reports(), source.source_time(), num_states);
    output_counts[fake_reports]++;
  }

  // This is the coupon collector problem (see
  // https://en.wikipedia.org/wiki/Coupon_collector%27s_problem).
  // For n possible results:
  //
  // the expected number of trials needed to see all possible results is equal
  // to n * Sum_{i = 1,..,n} 1/i.
  //
  // The variance of the number of trials is equal to
  // Sum_{i = 1,.., n} (1 - p_i) / p_i^2,
  // where p_i = (n - i + 1) / n.
  //
  // The probability that t trials are not enough to see all possible results is
  // at most n^{-t/(n*ln(n)) + 1}.
  int64_t expected_num_combinations =
      BinomialCoefficient(num_stars + num_bars, num_stars);
  EXPECT_EQ(static_cast<int64_t>(output_counts.size()),
            expected_num_combinations);

  // For any of the n possible results, the expected number of times it is seen
  // is equal to 1/n. Moreover, for any possible result, the probability that it
  // is seen more than (1+alpha)*t/n times is at most p_high = exp(- D(1/n +
  // alpha/n || 1/n) * t).
  //
  // The probability that it is seen less than (1-alpha)*t/n times is at most
  // p_low = exp(-D(1/n - alpha/n || 1/n) * t,
  //
  // where D( x || y) = x * ln(x/y) + (1-x) * ln( (1-x) / (1-y) ).
  // See
  // https://en.wikipedia.org/wiki/Chernoff_bound#Additive_form_(absolute_error)
  // for details.
  //
  // Thus, the probability that the number of occurrences of one of the results
  // deviates from its expectation by alpha*t/n is at most
  // n * (p_high + p_low).
  int expected_counts =
      num_samples / static_cast<double>(expected_num_combinations);
  for (const auto& output_count : output_counts) {
    const double abs_error = expected_counts * tolerance;
    EXPECT_NEAR(output_count.second, expected_counts, abs_error);
  }
}

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
                                             source.event_report_windows(),
                                             source.max_event_level_reports(),
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
        test_case.source_type, source.event_report_windows(),
        source.max_event_level_reports(), source.source_time());

    EXPECT_EQ(result.has_value(), test_case.expected_ok);
  }
}

TEST(AttributionStorageDelegateImplTest, GetFakeReportsForSequenceIndex) {
  constexpr base::Time kImpressionTime = base::Time();
  constexpr base::TimeDelta kExpiry = base::Days(9);

  constexpr base::Time kEarlyReportTime1 = kImpressionTime + base::Days(2);
  constexpr base::Time kEarlyReportTime2 = kImpressionTime + base::Days(7);
  constexpr base::Time kExpiryReportTime = kImpressionTime + kExpiry;

  const struct {
    SourceType source_type;
    int sequence_index;
    std::vector<FakeReport> expected;
  } kTestCases[] = {
      // Event sources only have 3 output states, so we can enumerate them:
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 1,
          .expected = {{
              .trigger_data = 0,
              .trigger_time = kExpiryReportTime - base::Milliseconds(1),
              .report_time = kExpiryReportTime,
          }},
      },
      {
          .source_type = SourceType::kEvent,
          .sequence_index = 2,
          .expected = {{
              .trigger_data = 1,
              .trigger_time = kExpiryReportTime - base::Milliseconds(1),
              .report_time = kExpiryReportTime,
          }},
      },
      // Navigation sources have 2925 output states, so pick interesting ones:
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 0,
          .expected = {},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 20,
          .expected = {{
              .trigger_data = 3,
              .trigger_time = kEarlyReportTime1 - base::Milliseconds(1),
              .report_time = kEarlyReportTime1,
          }},
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 41,
          .expected =
              {
                  {
                      .trigger_data = 4,
                      .trigger_time = kEarlyReportTime1 - base::Milliseconds(1),
                      .report_time = kEarlyReportTime1,
                  },
                  {
                      .trigger_data = 2,
                      .trigger_time = kEarlyReportTime1 - base::Milliseconds(1),
                      .report_time = kEarlyReportTime1,
                  },
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 50,
          .expected =
              {
                  {
                      .trigger_data = 4,
                      .trigger_time = kEarlyReportTime1 - base::Milliseconds(1),
                      .report_time = kEarlyReportTime1,
                  },
                  {
                      .trigger_data = 4,
                      .trigger_time = kEarlyReportTime1 - base::Milliseconds(1),
                      .report_time = kEarlyReportTime1,
                  },
              },
      },
      {
          .source_type = SourceType::kNavigation,
          .sequence_index = 1268,
          .expected =
              {
                  {
                      .trigger_data = 1,
                      .trigger_time = kExpiryReportTime - base::Milliseconds(1),
                      .report_time = kExpiryReportTime,
                  },
                  {
                      .trigger_data = 6,
                      .trigger_time = kEarlyReportTime2 - base::Milliseconds(1),
                      .report_time = kEarlyReportTime2,
                  },
                  {
                      .trigger_data = 7,
                      .trigger_time = kEarlyReportTime1 - base::Milliseconds(1),
                      .report_time = kEarlyReportTime1,
                  },
              },
      },
  };

  for (const auto& test_case : kTestCases) {
    const auto source =
        SourceBuilder(kImpressionTime)
            .SetSourceType(test_case.source_type)
            .SetExpiry(kExpiry)
            .SetEventReportWindows(*EventReportWindows::FromDefaults(
                /*report_window=*/kExpiry, test_case.source_type))
            .SetMaxEventLevelReports(
                test_case.source_type ==
                        attribution_reporting::mojom::SourceType::kNavigation
                    ? 3
                    : 1)
            .BuildStored();
    EXPECT_EQ(
        test_case.expected,
        AttributionStorageDelegateImpl().GetFakeReportsForSequenceIndex(
            source.common_info().source_type(), source.event_report_windows(),
            source.max_event_level_reports(), source.source_time(),
            test_case.sequence_index))
        << test_case.sequence_index;
  }
}

TEST(AttributionStorageDelegateImplTest,
     GetRandomFakeReports_Event_MatchesExpectedDistribution) {
  // The probability that not all of the 3 states are seen after `num_samples`
  // trials is at most ~1e-14476, which is 0 for all practical purposes, so the
  // `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most 1e-9.
  RunRandomFakeReportsTest(SourceType::kEvent,
                           /*num_stars=*/1,
                           /*num_bars=*/2,
                           /*num_samples=*/100'000,
                           /*tolerance=*/0.03);
}

TEST(AttributionStorageDelegateImplTest,
     GetRandomFakeReports_Navigation_MatchesExpectedDistribution) {
  // The probability that not all of the 2925 states are seen after
  // `num_samples` trials is at most ~1e-19, which is 0 for all practical
  // purposes, so the `expected_num_combinations` check should always pass.
  //
  // For the distribution check, the probability of failure with `tolerance` is
  // at most .0002.
  RunRandomFakeReportsTest(SourceType::kNavigation,
                           /*num_stars=*/3,
                           /*num_bars=*/24,
                           /*num_samples=*/150'000,
                           /*tolerance=*/0.9);
}

TEST(AttributionStorageDelegateImplTest,
     ComputeChannelCapacity_DefaultWindows) {
  const struct {
    SourceType source_type;
    double expected;
  } kTestCases[] = {
      {.source_type = SourceType::kNavigation, .expected = 11.46173},
      {.source_type = SourceType::kEvent, .expected = 1.58493}};
  for (const auto& test_case : kTestCases) {
    const auto source =
        SourceBuilder(base::Time())
            .SetSourceType(test_case.source_type)
            .SetEventReportWindows(*EventReportWindows::FromDefaults(
                /*report_window=*/base::Days(30), test_case.source_type))
            .SetMaxEventLevelReports(
                test_case.source_type ==
                        attribution_reporting::mojom::SourceType::kNavigation
                    ? 3
                    : 1)
            .BuildStored();

    const AttributionStorageDelegateImpl delegate;

    double value =
        std::round(ComputeChannelCapacity(
                       delegate.GetNumStates(test_case.source_type,
                                             source.event_report_windows(),
                                             source.max_event_level_reports()),
                       delegate.GetRandomizedResponseRate(
                           test_case.source_type, source.event_report_windows(),
                           source.max_event_level_reports())) *
                   100000.0) /
        100000.0;
    EXPECT_EQ(test_case.expected, value);
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
  base::test::ScopedFeatureList scoped_feature_list(
      attribution_reporting::features::
          kAttributionReportingNullAggregatableReports);

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
  base::test::ScopedFeatureList scoped_feature_list(
      attribution_reporting::features::
          kAttributionReportingNullAggregatableReports);

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

}  // namespace
}  // namespace content
