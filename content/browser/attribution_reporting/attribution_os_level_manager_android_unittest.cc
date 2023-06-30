// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/test/mock_content_browser_client.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AttributionOsLevelManager> manager_;
};

// Simple test to ensure that JNI calls work properly.
TEST_F(AttributionOsLevelManagerAndroidTest, Register) {
  const struct {
    const char* desc;
    absl::optional<AttributionInputEvent> input_event;
    bool should_use_os_web_source;
  } kTestCases[] = {
      {"trigger", absl::nullopt, false},
      {"os-source", AttributionInputEvent(), false},
      {"web-source", AttributionInputEvent(), true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    MockAttributionReportingContentBrowserClient browser_client;
    EXPECT_CALL(browser_client, ShouldUseOsWebSourceAttributionReporting())
        .WillRepeatedly(testing::Return(test_case.should_use_os_web_source));
    ScopedContentBrowserClientSetting setting(&browser_client);

    base::RunLoop run_loop;

    manager_->Register(
        OsRegistration(GURL("https://r.test"), /*debug_reporting=*/false,
                       url::Origin::Create(GURL("https://o.test")),
                       test_case.input_event, /*is_within_fenced_frame=*/false),
        /*is_debug_key_allowed=*/false,
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
