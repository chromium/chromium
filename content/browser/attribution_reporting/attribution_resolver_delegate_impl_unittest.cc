// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_resolver_delegate_impl.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "testing/gtest/include/gtest/gtest.h"

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
TEST(AttributionResolverDelegateImplTest, GetEventLevelReportTime) {
  constexpr base::Time kSourceTime;
  constexpr base::Time kTriggerTime = kSourceTime + base::Seconds(1);
  constexpr base::TimeDelta kEnd = base::Days(3);

  EXPECT_EQ(kSourceTime + kEnd,
            AttributionResolverDelegateImpl().GetEventLevelReportTime(
                *EventReportWindows::Create(/*start_time=*/base::Seconds(0),
                                            /*end_times=*/{kEnd}),
                kSourceTime, kTriggerTime));
}

TEST(AttributionResolverDelegateImplTest, GetAggregatableReportTime) {
  base::Time trigger_time = base::Time::Now();
  EXPECT_THAT(
      AttributionResolverDelegateImpl().GetAggregatableReportTime(trigger_time),
      AllOf(Ge(trigger_time), Lt(trigger_time + base::Minutes(10))));
}

TEST(AttributionResolverDelegateImplTest, NewReportID_IsValidGUID) {
  EXPECT_TRUE(AttributionResolverDelegateImpl().NewReportID().is_valid());
}

TEST(AttributionResolverDelegateImplTest,
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

    auto result = AttributionResolverDelegateImpl(AttributionNoiseMode::kNone)
                      .GetRandomizedResponse(
                          source.common_info().source_type(),
                          source.trigger_specs(), source.event_level_epsilon(),
                          /*attribution_scope_data=*/std::nullopt);
    ASSERT_TRUE(result.has_value());
    ASSERT_GT(result->rate(), 0);
    ASSERT_EQ(result->response(), std::nullopt);
  }
}

TEST(AttributionResolverDelegateImplTest,
     RandomizedResponse_ExceedsLimit_ReturnsError) {
  const struct {
    SourceType source_type;
    double max_navigation_info_gain = std::numeric_limits<double>::infinity();
    double max_event_info_gain = std::numeric_limits<double>::infinity();
    bool expected_ok;
  } kTestCases[] = {
      {
          .source_type = SourceType::kNavigation,
          .max_event_info_gain = 0.1,
          .expected_ok = true,
      },
      {
          .source_type = SourceType::kNavigation,
          .max_navigation_info_gain = 0.1,
          .expected_ok = false,
      },
      {
          .source_type = SourceType::kEvent,
          .max_navigation_info_gain = 0.1,
          .expected_ok = true,
      },
      {
          .source_type = SourceType::kEvent,
          .max_event_info_gain = 0.1,
          .expected_ok = false,
      },
  };

  for (const auto& test_case : kTestCases) {
    AttributionConfig config;
    config.privacy_math_config.max_channel_capacity_navigation =
        test_case.max_navigation_info_gain;
    config.privacy_math_config.max_channel_capacity_event =
        test_case.max_event_info_gain;

    auto delegate = AttributionResolverDelegateImpl::CreateForTesting(
        AttributionNoiseMode::kDefault, AttributionDelayMode::kDefault, config);

    const auto source =
        SourceBuilder().SetSourceType(test_case.source_type).BuildStored();

    auto result = delegate->GetRandomizedResponse(
        test_case.source_type, source.trigger_specs(),
        source.event_level_epsilon(),
        /*attribution_scope_data=*/std::nullopt);

    EXPECT_EQ(result.has_value(), test_case.expected_ok);
  }
}

}  // namespace
}  // namespace content
