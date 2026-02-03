// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_manager.h"

#import "base/json/json_reader.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/analysis_settings.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/connectors/core/reporting_constants.h"
#import "components/enterprise/connectors/core/reporting_test_utils.h"
#import "components/enterprise/connectors/core/service_provider_config.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/testing_pref_service.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

using enterprise_connectors::AnalysisConnector;

namespace {

constexpr char kNormalCloudAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["foo.com"], "tags": ["dlp", "malware"]}
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true
  }
])";

constexpr char kWildCardCloudAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]}
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true
  }
])";

constexpr char kDlpAndMalwareUrl[] = "https://foo.com";
constexpr char kNoAnalysisSettingsUrl[] = "https://bar.com";

class ConnectorsManagerTest : public PlatformTest {
 public:
  TestingPrefServiceSimple* prefs() { return &pref_service_; }

  void SetUp() override {
    // Setup required to register connectors prefs with `pref_service_`.
    RegisterProfilePrefs(prefs()->registry());
  }

  // Sets up the the AnalysisSettings pref based on the pref value passed in.
  void SetAnalysisConnectorSettings(const char* pref) {
    prefs()->Set(
        AnalysisConnectorPref(connector()),
        *base::JSONReader::Read(pref, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    prefs()->SetInteger(AnalysisConnectorScopePref(connector()),
                        policy::POLICY_SCOPE_MACHINE);
  }

  // Return FILE_DOWNLOADED as the type of the AnalysisConnector. For iOS we
  // only care about FILE_DOWNLOADED currently. It can be expanded to cover
  // other AnalysisConnector type in the future when we implement them.
  AnalysisConnector connector() { return FILE_DOWNLOADED; }

  void ValidateSettings(const AnalysisSettings& settings) {
    AnalysisSettings expectedSettings = GetExpectedSettings();
    ASSERT_EQ(settings.block_until_verdict,
              expectedSettings.block_until_verdict);
    ASSERT_EQ(settings.block_password_protected_files,
              expectedSettings.block_password_protected_files);
    ASSERT_EQ(settings.block_large_files, expectedSettings.block_large_files);
  }

  // Returns the expected AnalysisSettings corresponding to
  // kNormalCloudAnalysisSettingsPref and kWildCardCloudAnalysisSettingsPref.
  AnalysisSettings GetExpectedSettings() {
    AnalysisSettings settings;

    settings.block_until_verdict = BlockUntilVerdict::kBlock;
    settings.block_password_protected_files = true;
    settings.block_large_files = true;

    return settings;
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
            std::set<std::string>(kAllReportingEnabledEvents.begin(),
                                  kAllReportingEnabledEvents.end()));
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

TEST_F(ConnectorsManagerTest, AnalysisConnectorSettings) {
  ConnectorsManager manager(prefs(), GetServiceProviderConfig());

  EXPECT_FALSE(manager.IsAnalysisConnectorEnabled(connector()));
  EXPECT_TRUE(manager.GetAnalysisServiceProviderNames(connector()).empty());
  EXPECT_FALSE(
      manager.GetAnalysisSettings(GURL(kDlpAndMalwareUrl), connector()));
  EXPECT_FALSE(
      manager.GetAnalysisSettings(GURL(kNoAnalysisSettingsUrl), connector()));

  SetAnalysisConnectorSettings(kNormalCloudAnalysisSettingsPref);

  EXPECT_TRUE(manager.IsAnalysisConnectorEnabled(connector()));
  auto provider_names = manager.GetAnalysisServiceProviderNames(connector());
  EXPECT_EQ(provider_names, std::vector<std::string>({"google"}));

  EXPECT_FALSE(
      manager.GetAnalysisSettings(GURL(kNoAnalysisSettingsUrl), connector()));
  auto settings =
      manager.GetAnalysisSettings(GURL(kDlpAndMalwareUrl), connector());
  EXPECT_TRUE(settings.has_value());
  ValidateSettings(settings.value());

  SetAnalysisConnectorSettings(kWildCardCloudAnalysisSettingsPref);

  // Make sure it is still enabled after changing the settings.
  EXPECT_TRUE(manager.IsAnalysisConnectorEnabled(connector()));
  provider_names = manager.GetAnalysisServiceProviderNames(connector());
  EXPECT_EQ(provider_names, std::vector<std::string>({"google"}));

  // Make sure the settings is changed correctly for kNoAnalysisSettingsUrl.
  EXPECT_TRUE(
      manager.GetAnalysisSettings(GURL(kNoAnalysisSettingsUrl), connector()));
  settings =
      manager.GetAnalysisSettings(GURL(kNoAnalysisSettingsUrl), connector());
  EXPECT_TRUE(settings.has_value());
  ValidateSettings(settings.value());
}

}  // namespace enterprise_connectors
