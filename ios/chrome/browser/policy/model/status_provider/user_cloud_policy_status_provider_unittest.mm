// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/status_provider/user_cloud_policy_status_provider.h"

#import "base/containers/flat_set.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/task_environment.h"
#import "components/policy/core/browser/webui/policy_status_provider.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/policy/core/common/policy_types.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"

namespace {

constexpr char kTestClientId[] = "test_client_id";
constexpr char kTestUsername[] = "username@domain.com";
constexpr char kTestDomain[] = "domain.com";
constexpr char kAnnotatedAssetId[] = "test_annotated_asset_id";
constexpr char kAnnotatedLocation[] = "test_annotated_location";
constexpr char kTestDirectoryApiId[] = "test_directory_api_id";
constexpr char kTestGaiaId[] = "test_gaia_id";

policy::MockCloudPolicyClient* ConnectNewMockClient(
    policy::CloudPolicyCore* core) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  auto* client_ptr = client.get();
  core->Connect(std::move(client));
  return client_ptr;
}

}  // namespace

class UserCloudPolicyStatusProviderTest
    : public PlatformTest,
      public policy::PolicyStatusProvider::Observer,
      public UserCloudPolicyStatusProvider::Delegate {
 public:
  void SetUp() override {
    RegisterLocalStatePrefs(local_state_.registry());

    user_store_ = std::make_unique<policy::MockUserCloudPolicyStore>();
    user_core_ = std::make_unique<policy::CloudPolicyCore>(
        policy::dm_protocol::kChromeUserPolicyType, std::string(),
        user_store_.get(), base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());

    user_client_ = ConnectNewMockClient(user_core_.get());

    status_provider_ = std::make_unique<UserCloudPolicyStatusProvider>(
        this, user_core_.get(), identity_manager());
  }

  // policy::PolicyStatusProvider::Observer implementation:
  MOCK_METHOD0(OnPolicyStatusChanged, void());

  // UserCloudPolicyStatusProvider::Delegate implementation:
  MOCK_METHOD0(GetDeviceAffiliationIds, base::flat_set<std::string>());

  void SetPrimaryAccountAsFlex() {
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        kTestUsername, signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_is_subject_to_enterprise_policies(true);
    account.hosted_domain = kNoHostedDomainFound;
    identity_test_env_.UpdateAccountInfoForAccount(account);
  }

  // Sets the minimal viable user policy data to get status information.
  void SetMinimalViableUserPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
    user_store()->set_policy_data_for_testing(std::move(policy_data));
  }

  void StartRefreshScheduler() {
    user_core()->StartRefreshScheduler();
    user_core()->TrackRefreshDelayPref(
        &local_state_, policy::policy_prefs::kUserPolicyRefreshRate);
    ASSERT_TRUE(user_core()->refresh_scheduler());
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  policy::MockUserCloudPolicyStore* user_store() { return user_store_.get(); }

  policy::MockCloudPolicyClient* user_client() { return user_client_; }

  policy::CloudPolicyCore* user_core() { return user_core_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<policy::MockUserCloudPolicyStore> user_store_;
  std::unique_ptr<policy::CloudPolicyCore> user_core_;
  raw_ptr<policy::MockCloudPolicyClient> user_client_;
  std::unique_ptr<UserCloudPolicyStatusProvider> status_provider_;
};

// Test getting the status of a managed account when all the information is
// available.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_Full) {
  constexpr char kSharedAffiliationId[] = "kSharedAffiliationId";

  {
    // Set policy data in user level store.
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
    policy_data->set_device_id(kTestClientId);
    policy_data->set_username(kTestUsername);
    policy_data->set_annotated_asset_id(kAnnotatedAssetId);
    policy_data->set_annotated_location(kAnnotatedLocation);
    policy_data->set_directory_api_id(kTestDirectoryApiId);
    policy_data->set_gaia_id(kTestGaiaId);
    policy_data->set_timestamp(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    policy_data->add_user_affiliation_ids(kSharedAffiliationId);
    user_store()->set_policy_data_for_testing(std::move(policy_data));
  }

  ON_CALL(*this, GetDeviceAffiliationIds)
      .WillByDefault([kSharedAffiliationId]() {
        base::flat_set<std::string> affiliation_ids;
        affiliation_ids.insert(kSharedAffiliationId);
        return affiliation_ids;
      });

  // Set clients as managed.
  user_client()->SetStatus(policy::DM_STATUS_SUCCESS);
  user_client()->SetDMToken("test-dm-token");

  StartRefreshScheduler();

  // Update last refresh timestamp in scheduler.
  user_core()->refresh_scheduler()->RefreshSoon(
      policy::PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();

  const base::TimeDelta time_since_last_success_fetch = base::Hours(1);
  const std::u16string time_since_last_success_fetch_formatted =
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT,
                             time_since_last_success_fetch);

  // Advance time to emulate 1 hour between last refresh and now.
  task_environment_.FastForwardBy(time_since_last_success_fetch);

  // Set expected status.
  const base::Value::Dict expected_status =
      base::Value::Dict()
          .Set(policy::kClientIdKey, kTestClientId)
          .Set(policy::kDirectoryApiIdKey, kTestDirectoryApiId)
          .Set(policy::kUsernameKey, kTestUsername)
          .Set(policy::kAssetIdKey, kAnnotatedAssetId)
          .Set(policy::kLocationKey, kAnnotatedLocation)
          .Set(policy::kGaiaIdKey, kTestGaiaId)
          .Set("error", false)
          .Set("policiesPushAvailable", false)
          .Set("status", l10n_util::GetStringUTF16(IDS_POLICY_STORE_STATUS_OK))
          .Set("refreshInterval", "1 day")
          .Set("timeSinceLastRefresh", time_since_last_success_fetch_formatted)
          .Set("timeSinceLastFetchAttempt",
               time_since_last_success_fetch_formatted)
          .Set(policy::kDomainKey, kTestDomain)
          .Set("isAffiliated", true)
          .Set(policy::kFlexOrgWarningKey, false)
          .Set(policy::kPolicyDescriptionKey, "statusUser");

  base::Value::Dict returned_status = status_provider_->GetStatus();
  EXPECT_EQ(expected_status, returned_status);
}

// Test skipping the domain when there is no username.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_NoDomainIfNoUsername) {
  // Set user policy data with no username in it, enough to not return an empty
  // status payload and process the policy data.
  SetMinimalViableUserPolicyData();

  base::Value::Dict returned_status = status_provider_->GetStatus();
  EXPECT_FALSE(returned_status.FindString(policy::kDomainKey));
  // Sanity check to make sure that there is the other status information even
  // if the domain isn't set.
  EXPECT_TRUE(returned_status.FindString(policy::kPolicyDescriptionKey));
}

// Test that the returned status information is empty when there is no active
// policy data and no flex account.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_NotManaged) {
  base::Value::Dict returned_status = status_provider_->GetStatus();
  EXPECT_TRUE(returned_status.empty());
}

// Test that the affiliation status is false when tha affiliation IDs don't
// match.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_AffiliationIds_NoMatch) {
  constexpr char kAffiliationId1[] = "kAffiliationId1";
  constexpr char kAffiliationId2[] = "kAffiliationId2";

  {
    // Set user affiliations ids in user level store.
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_state(enterprise_management::PolicyData::ACTIVE);
    policy_data->add_user_affiliation_ids(kAffiliationId1);
    user_store()->set_policy_data_for_testing(std::move(policy_data));
  }

  ON_CALL(*this, GetDeviceAffiliationIds).WillByDefault([kAffiliationId2]() {
    base::flat_set<std::string> affiliation_ids;
    affiliation_ids.insert(kAffiliationId2);
    return affiliation_ids;
  });

  // Set clients as managed.
  user_client()->SetStatus(policy::DM_STATUS_SUCCESS);
  user_client()->SetDMToken("test-dm-token");

  base::Value::Dict returned_status = status_provider_->GetStatus();
  auto affiliation_value = returned_status.FindBool("isAffiliated");
  ASSERT_TRUE(affiliation_value);
  EXPECT_FALSE(*affiliation_value);
}

// Test that the flex warning is set to true when the account is a flex account,
// even when there is no active policy data.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_FlexWarning) {
  SetPrimaryAccountAsFlex();

  // Test flex status.
  base::Value::Dict returned_status = status_provider_->GetStatus();
  auto flex_warning_value =
      returned_status.FindBool(policy::kFlexOrgWarningKey);
  ASSERT_TRUE(flex_warning_value);
  EXPECT_TRUE(*flex_warning_value);
}

// Test that OnStoreLoaded observed from store triggers OnPolicyStatusChanged.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_Notify_OnStoreLoaded) {
  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      observation{this};
  observation.Observe(status_provider_.get());

  EXPECT_CALL(*this, OnPolicyStatusChanged).Times(1);
  user_store()->NotifyStoreLoaded();
}

// Test that OnStoreError observed from store triggers OnPolicyStatusChanged.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_Notify_OnStoreError) {
  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      observation{this};
  observation.Observe(status_provider_.get());

  EXPECT_CALL(*this, OnPolicyStatusChanged).Times(1);
  user_store()->NotifyStoreError();
}

// Test that OnClientError observed from client triggers OnPolicyStatusChanged.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_Notify_OnClientError) {
  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      observation{this};
  observation.Observe(status_provider_.get());

  EXPECT_CALL(*this, OnPolicyStatusChanged).Times(1);
  user_client()->NotifyClientError();
}

// Test that OnPolicyFetched observed from client triggers
// OnPolicyStatusChanged.
TEST_F(UserCloudPolicyStatusProviderTest, GetStatus_Notify_OnPolicyFetched) {
  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      observation{this};
  observation.Observe(status_provider_.get());

  EXPECT_CALL(*this, OnPolicyStatusChanged).Times(1);
  user_client()->NotifyPolicyFetched();
}

// Test that OnRegistrationStateChanged observed from client triggers
// OnPolicyStatusChanged.
TEST_F(UserCloudPolicyStatusProviderTest,
       GetStatus_Notify_OnRegistrationStateChanged) {
  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      observation{this};
  observation.Observe(status_provider_.get());

  EXPECT_CALL(*this, OnPolicyStatusChanged).Times(1);
  user_client()->NotifyRegistrationStateChanged();
}

// Test that the observer is reset when connecting a new client.
TEST_F(UserCloudPolicyStatusProviderTest, ConnectNewClient) {
  base::ScopedObservation<policy::PolicyStatusProvider,
                          policy::PolicyStatusProvider::Observer>
      observation{this};
  observation.Observe(status_provider_.get());

  // Disconnect the current client.
  user_core()->Disconnect();
  // Connect a new client.
  policy::MockCloudPolicyClient* new_client =
      ConnectNewMockClient(user_core_.get());

  // Verify that the status provider listens to the new client and can observe
  // client errors.
  EXPECT_CALL(*this, OnPolicyStatusChanged).Times(1);
  new_client->NotifyClientError();
}
