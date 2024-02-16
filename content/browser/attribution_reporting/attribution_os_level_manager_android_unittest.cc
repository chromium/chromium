// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

class AttributionOsLevelManagerAndroidTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<AttributionOsLevelManagerAndroid>();
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<AttributionOsLevelManager> manager_;
};

TEST_F(AttributionOsLevelManagerAndroidTest, GetMeasurementStatusTimeMetric) {
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectTotalCount("Conversions.GetMeasurementStatusTime", 1);
}

// Simple test to ensure that JNI calls work properly.
TEST_F(AttributionOsLevelManagerAndroidTest, Register) {
  const struct {
    const char* desc;
    std::optional<AttributionInputEvent> input_event;
    bool should_use_os_web_source;
    bool should_use_os_web_trigger;
    size_t items_count;
  } kTestCases[] = {
      {"os-trigger-single", std::nullopt, false, false, 1},
      {"os-trigger-multi", std::nullopt, false, false, 3},
      {"web-trigger-single", std::nullopt, false, true, 1},
      {"web-trigger-multi", std::nullopt, false, true, 3},
      {"os-source-single", AttributionInputEvent(), false, false, 1},
      {"os-source-multi", AttributionInputEvent(), false, false, 3},
      {"web-source-single", AttributionInputEvent(), true, false, 1},
      {"web-source-multi", AttributionInputEvent(), true, false, 3},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    MockAttributionReportingContentBrowserClient browser_client;
    EXPECT_CALL(browser_client,
                ShouldUseOsWebSourceAttributionReporting(testing::_))
        .WillRepeatedly(testing::Return(test_case.should_use_os_web_source));
    EXPECT_CALL(browser_client,
                ShouldUseOsWebTriggerAttributionReporting(testing::_))
        .WillRepeatedly(testing::Return(test_case.should_use_os_web_trigger));
    ScopedContentBrowserClientSetting setting(&browser_client);

    base::RunLoop run_loop;

    std::vector<attribution_reporting::OsRegistrationItem> items;
    items.reserve(test_case.items_count);
    std::vector<bool> is_debug_key_allowed;
    is_debug_key_allowed.reserve(test_case.items_count);
    for (size_t i = 0; i < test_case.items_count; ++i) {
      items.emplace_back(GURL("https://r.test"), /*debug_reporting=*/false);
      is_debug_key_allowed.push_back(false);
    }
    manager_->Register(
        OsRegistration(std::move(items),
                       url::Origin::Create(GURL("https://o.test")),
                       test_case.input_event, /*is_within_fenced_frame=*/false,
                       /*render_frame_id=*/GlobalRenderFrameHostId()),
        is_debug_key_allowed,
        base::BindLambdaForTesting([&](const OsRegistration&, bool success) {
          // We don't check `success` here because the measurement API may or
          // may not be available depending on the Android version.
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

// Simple test to ensure that JNI calls work properly.
TEST_F(AttributionOsLevelManagerAndroidTest, ClearData) {
  const BrowsingDataFilterBuilder::Mode kModes[] = {
      BrowsingDataFilterBuilder::Mode::kDelete,
      BrowsingDataFilterBuilder::Mode::kPreserve,
  };

  for (BrowsingDataFilterBuilder::Mode mode : kModes) {
    SCOPED_TRACE(static_cast<int>(mode));
    base::RunLoop run_loop;

    manager_->ClearData(
        /*delete_begin=*/base::Time::Min(),
        /*delete_end=*/base::Time::Max(),
        /*origins=*/{url::Origin::Create(GURL("https://o.test"))},
        /*domains=*/{"d.test"}, mode,
        /*delete_rate_limit_data=*/false, run_loop.QuitClosure());

    run_loop.Run();
  }
}

}  // namespace
}  // namespace content
