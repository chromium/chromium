// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>

#import "base/command_line.h"
#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/test/scoped_command_line.h"
#import "base/values.h"
#import "components/enterprise/browser/enterprise_switches.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/policy/core/common/mock_configuration_policy_provider.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_types.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/variations/pref_names.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/policy/model/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_service_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/url_util.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

class VariationsRestrictParameterPolicyTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kEnableChromeBrowserCloudManagement);
    ASSERT_TRUE(state_directory_.CreateUniqueTempDir());
    enterprise_policy_helper_ = std::make_unique<EnterprisePolicyTestHelper>(
        state_directory_.GetPath());
    ASSERT_TRUE(enterprise_policy_helper_->GetProfile());
  }

 protected:
  base::test::ScopedCommandLine scoped_command_line_;
  base::ScopedTempDir state_directory_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper_;
};

}  // namespace

TEST_F(VariationsRestrictParameterPolicyTest, AffectsVariationsURL) {
  PrefService* local_state = enterprise_policy_helper_->GetLocalState();
  ASSERT_NE(PrefService::INITIALIZATION_STATUS_WAITING,
            local_state->GetInitializationStatus());

  auto enabled_state_provider =
      std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/false,
                                                          /*enabled=*/false);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          local_state, enabled_state_provider.get(),
          /*backup_registry_key=*/std::wstring(),
          /*user_data_dir=*/base::FilePath(),
          metrics::StartupVisibility::kUnknown);

  std::unique_ptr<variations::VariationsService> variations_service =
      variations::VariationsService::Create(
          std::make_unique<IOSChromeVariationsServiceClient>(), local_state,
          metrics_state_manager.get(),
          /*disable_network_switch=*/"dummy-disable-background-switch",
          network::TestNetworkConnectionTracker::CreateGetter());

  // Without policy set, no "restrict" parameter is present in HTTPS URL,
  // and insecure HTTP fallback is permitted.
  GURL https_url = variations_service->GetVariationsServerURL(
      variations::VariationsService::USE_HTTPS);
  std::string value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(https_url, "restrict", &value));
  EXPECT_FALSE(
      variations_service
          ->GetVariationsServerURL(variations::VariationsService::USE_HTTP)
          .is_empty());

  // Set the VariationsRestrictParameter policy.
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kVariationsRestrictParameter,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM, base::Value("restricted_corp"),
                 nullptr);
  enterprise_policy_helper_->GetPolicyProvider()->UpdateChromePolicy(
      policy_map);

  // Verify policy handler mapped policy to Local State pref.
  EXPECT_EQ(
      "restricted_corp",
      local_state->GetString(variations::prefs::kVariationsRestrictParameter));

  // Variations HTTPS URL must now contain "restrict=restricted_corp".
  https_url = variations_service->GetVariationsServerURL(
      variations::VariationsService::USE_HTTPS);
  EXPECT_TRUE(net::GetValueForKeyInQuery(https_url, "restrict", &value));
  EXPECT_EQ("restricted_corp", value);

  // Insecure HTTP fallback must be disabled when restrict mode is active.
  EXPECT_TRUE(
      variations_service
          ->GetVariationsServerURL(variations::VariationsService::USE_HTTP)
          .is_empty());

  // Clear policy to verify dynamic removal.
  policy_map.Clear();
  enterprise_policy_helper_->GetPolicyProvider()->UpdateChromePolicy(
      policy_map);

  EXPECT_EQ("", local_state->GetString(
                    variations::prefs::kVariationsRestrictParameter));
  https_url = variations_service->GetVariationsServerURL(
      variations::VariationsService::USE_HTTPS);
  value.clear();
  EXPECT_FALSE(net::GetValueForKeyInQuery(https_url, "restrict", &value));
  EXPECT_FALSE(
      variations_service
          ->GetVariationsServerURL(variations::VariationsService::USE_HTTP)
          .is_empty());
}
