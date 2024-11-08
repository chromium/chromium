// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"

#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/enterprise/connectors/core/service_provider_config.h"
#import "components/prefs/testing_pref_service.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

class ConnectorsManagerTest : public PlatformTest {
 public:
  TestingPrefServiceSimple* prefs() { return &pref_service_; }

  void SetUp() override {
    // Setup required to register connectors prefs with `pref_service_`.
    RegisterProfilePrefs(prefs()->registry());
  }

 private:
  TestingPrefServiceSimple pref_service_;
};

}  // namespace

TEST_F(ConnectorsManagerTest, ReportingSettings) {
  // All reporting settings tests are done in the same test so as to validate
  // that `ConnectorsManager` is able to properly observe the pref as it
  // changes.

  ConnectorsManager manager(prefs(), GetServiceProviderConfig());

  EXPECT_FALSE(manager.IsReportingConnectorEnabled());
  EXPECT_FALSE(manager.GetReportingSettings());
  EXPECT_TRUE(manager.GetReportingServiceProviderNames().empty());

  test::SetOnSecurityEventReporting(prefs(), /*enabled=*/true);

  EXPECT_TRUE(manager.IsReportingConnectorEnabled());
  auto settings = manager.GetReportingSettings();
  EXPECT_TRUE(settings.has_value());
  EXPECT_EQ(settings->enabled_event_names,
            std::set<std::string>(kAllReportingEvents.begin(),
                                  kAllReportingEvents.end()));
  EXPECT_TRUE(settings->enabled_opt_in_events.empty());
  auto provider_names = manager.GetReportingServiceProviderNames();
  EXPECT_EQ(provider_names, std::vector<std::string>({"google"}));

  test::SetOnSecurityEventReporting(
      prefs(), /*enabled=*/true,
      /*enabled_event_names=*/{"passwordReuseEvent", "interstitialEvent"},
      /*enabled_opt_in_events=*/
      {
          {"loginEvent", {"foo.com", "bar.com"}},
          {"passwordBreachEvent", {"baz.com"}},
      },
      /*machine_scope=*/false);

  EXPECT_TRUE(manager.IsReportingConnectorEnabled());
  settings = manager.GetReportingSettings();
  EXPECT_TRUE(settings.has_value());
  EXPECT_EQ(settings->enabled_event_names,
            std::set<std::string>({"passwordReuseEvent", "interstitialEvent"}));
  EXPECT_TRUE(settings->enabled_opt_in_events.count("loginEvent"));
  EXPECT_EQ(settings->enabled_opt_in_events["loginEvent"],
            std::vector<std::string>({"foo.com", "bar.com"}));
  EXPECT_TRUE(settings->enabled_opt_in_events.count("passwordBreachEvent"));
  EXPECT_EQ(settings->enabled_opt_in_events["passwordBreachEvent"],
            std::vector<std::string>({"baz.com"}));
  provider_names = manager.GetReportingServiceProviderNames();
  EXPECT_EQ(provider_names, std::vector<std::string>({"google"}));
}

}  // namespace enterprise_connectors
