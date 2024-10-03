// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestProfileDmToken[] = "profile_dm_token";
constexpr char kTestBrowserDmToken[] = "browser_dm_token";
constexpr char kTestClientId[] = "client_id";

class ConnectorsServiceTest : public PlatformTest {
 public:
  void SetUp() override {
    // Setup required to access a profile DM token.
    enterprise_management::PolicyData profile_policy_data;
    profile_policy_data.set_request_token(kTestProfileDmToken);

    auto store = std::make_unique<policy::MockUserCloudPolicyStore>();
    store->set_policy_data_for_testing(
        std::make_unique<enterprise_management::PolicyData>(
            std::move(profile_policy_data)));

    auto cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(store), base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    auto profile_builder = TestProfileIOS::Builder();
    profile_builder.SetUserCloudPolicyManager(std::move(cloud_policy_manager));
    profile_ = std::move(profile_builder).Build();

    // Setup required to access a profile DM token.
    fake_browser_dm_token_storage_.SetDMToken(kTestBrowserDmToken);
    fake_browser_dm_token_storage_.SetClientId(kTestClientId);

    // Setup required to register connectors prefs with `pref_service_`.
    RegisterProfilePrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return &pref_service_; }

  TestProfileIOS* profile() { return profile_.get(); }

 private:
  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  policy::FakeBrowserDMTokenStorage fake_browser_dm_token_storage_;
};

}  // namespace

TEST_F(ConnectorsServiceTest, GetPrefs) {
  ConnectorsService connectors_service{/*off_the_record=*/false, prefs(),
                                       /*user_cloud_policy_client=*/nullptr};
  const ConnectorsService const_connectors_service{
      /*off_the_record=*/false, prefs(),
      /*user_cloud_policy_client=*/nullptr};

  PrefService* prefs = connectors_service.GetPrefs();
  const PrefService* const_prefs = const_connectors_service.GetPrefs();

  ASSERT_TRUE(prefs);
  ASSERT_TRUE(const_prefs);
  ASSERT_EQ(prefs, const_prefs);
}

TEST_F(ConnectorsServiceTest, GetProfileDmToken) {
  prefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_USER);
  ConnectorsService connectors_service{
      /*off_the_record=*/false, prefs(),
      /*user_cloud_policy_client=*/profile()->GetUserCloudPolicyManager()};

  auto profile_dm_token =
      connectors_service.GetDmToken(kEnterpriseRealTimeUrlCheckScope);
  ASSERT_TRUE(profile_dm_token.has_value());
  ASSERT_EQ(profile_dm_token->value, kTestProfileDmToken);
  ASSERT_EQ(profile_dm_token->scope, policy::POLICY_SCOPE_USER);
}

TEST_F(ConnectorsServiceTest, GetBrowserDmToken) {
  prefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_MACHINE);
  ConnectorsService connectors_service{
      /*off_the_record=*/false, prefs(),
      /*user_cloud_policy_client=*/profile()->GetUserCloudPolicyManager()};

  auto browser_dm_token =
      connectors_service.GetDmToken(kEnterpriseRealTimeUrlCheckScope);
  ASSERT_TRUE(browser_dm_token.has_value());
  ASSERT_EQ(browser_dm_token->value, kTestBrowserDmToken);
  ASSERT_EQ(browser_dm_token->scope, policy::POLICY_SCOPE_MACHINE);
}

TEST_F(ConnectorsServiceTest, ConnectorsEnabled) {
  ASSERT_TRUE(
      ConnectorsServiceFactory::GetForProfile(profile())->ConnectorsEnabled());
  ASSERT_FALSE(ConnectorsServiceFactory::GetForProfile(
                   profile()->GetOffTheRecordChromeBrowserState())
                   ->ConnectorsEnabled());
  ASSERT_TRUE(
      ConnectorsService(
          /*off_the_record=*/false, prefs(),
          /*user_cloud_policy_client=*/profile()->GetUserCloudPolicyManager())
          .ConnectorsEnabled());
  ASSERT_FALSE(
      ConnectorsService(
          /*off_the_record=*/true, prefs(),
          /*user_cloud_policy_client=*/profile()->GetUserCloudPolicyManager())
          .ConnectorsEnabled());
}

TEST_F(ConnectorsServiceTest, RealTimeUrlCheck) {
  auto service = ConnectorsService(
      /*off_the_record=*/false, prefs(),
      /*user_cloud_policy_client=*/profile()->GetUserCloudPolicyManager());

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);

  prefs()->SetInteger(
      kEnterpriseRealTimeUrlCheckMode,
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  prefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_MACHINE);
  ASSERT_TRUE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(*service.GetDMTokenForRealTimeUrlCheck(), kTestBrowserDmToken);
  ASSERT_EQ(
      service.GetAppliedRealTimeUrlCheck(),
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);

  prefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_USER);
  ASSERT_TRUE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(*service.GetDMTokenForRealTimeUrlCheck(), kTestProfileDmToken);
  ASSERT_EQ(
      service.GetAppliedRealTimeUrlCheck(),
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
}

TEST_F(ConnectorsServiceTest, RealTimeUrlCheck_OffTheRecord) {
  auto service = ConnectorsService(
      /*off_the_record=*/true, prefs(),
      /*user_cloud_policy_client=*/profile()->GetUserCloudPolicyManager());

  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);

  prefs()->SetInteger(
      kEnterpriseRealTimeUrlCheckMode,
      EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED);
  prefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_MACHINE);
  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);

  prefs()->SetInteger(kEnterpriseRealTimeUrlCheckScope,
                      policy::POLICY_SCOPE_USER);
  ASSERT_FALSE(service.GetDMTokenForRealTimeUrlCheck().has_value());
  ASSERT_EQ(service.GetAppliedRealTimeUrlCheck(),
            EnterpriseRealTimeUrlCheckMode::REAL_TIME_CHECK_DISABLED);
}

}  // namespace enterprise_connectors
