// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/user_delegate_ios.h"

#import <memory>
#import <set>
#import <string>
#import <utility>

#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/enterprise/device_trust/core/fake_device_trust_connector_service.h"
#import "components/enterprise/device_trust/prefs.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/policy_types.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr char kUserEmail[] = "someEmail@example.com";
constexpr char kOtherUserEmail[] = "someOtherUser@example.com";
constexpr GaiaId::Literal kOtherUserGaiaId("some-other-user-gaia");
constexpr char kAffiliationId1[] = "affiliation-id-1";
constexpr char kAffiliationId2[] = "affiliation-id-2";

base::ListValue GetUrls() {
  base::ListValue trusted_urls;
  trusted_urls.Append("https://www.example.com");
  trusted_urls.Append("example2.example.com");
  return trusted_urls;
}

}  // namespace

class UserDelegateIOSTest : public PlatformTest {
 public:
  base::flat_set<std::string> GetDeviceAffiliationIds() const {
    return device_affiliation_ids_;
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    enterprise_connectors::RegisterDeviceTrustConnectorProfilePrefs(
        pref_service_.registry());
  }

  void CreateDelegate(bool is_managed_user = true,
                      base::flat_set<std::string> user_affiliation_ids = {},
                      base::flat_set<std::string> device_affiliation_ids = {}) {
    device_affiliation_ids_ = std::move(device_affiliation_ids);
    if (is_managed_user || !user_affiliation_ids.empty()) {
      auto policy_data = std::make_unique<enterprise_management::PolicyData>();
      if (is_managed_user) {
        policy_data->set_managed_by("example.com");
      }
      for (const std::string& id : user_affiliation_ids) {
        policy_data->add_user_affiliation_ids(id);
      }
      policy_store_.set_policy_data_for_testing(std::move(policy_data));
    } else {
      policy_store_.set_policy_data_for_testing(nullptr);
    }

    profile_policy_connector_ = std::make_unique<ProfilePolicyConnectorMock>(
        std::make_unique<policy::MockPolicyService>(), &schema_registry_,
        &policy_store_);
    fake_dt_connector_service_ = std::make_unique<
        enterprise_connectors::FakeDeviceTrustConnectorService>(&pref_service_);
    user_delegate_ = std::make_unique<UserDelegateIOS>(
        profile_policy_connector_.get(),
        base::BindRepeating(&UserDelegateIOSTest::GetDeviceAffiliationIds,
                            base::Unretained(this)),
        identity_test_env_.identity_manager(),
        fake_dt_connector_service_.get());
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  policy::SchemaRegistry schema_registry_;
  policy::MockCloudPolicyStore policy_store_{
      policy::dm_protocol::GetChromeUserPolicyType()};
  base::flat_set<std::string> device_affiliation_ids_;
  std::unique_ptr<ProfilePolicyConnectorMock> profile_policy_connector_;
  std::unique_ptr<enterprise_connectors::FakeDeviceTrustConnectorService>
      fake_dt_connector_service_;
  std::unique_ptr<UserDelegateIOS> user_delegate_;
};

// Tests that IsAffiliated returns true when user and device affiliation IDs
// match.
TEST_F(UserDelegateIOSTest, IsAffiliated_True) {
  CreateDelegate(/*is_managed_user=*/true, {kAffiliationId1},
                 {kAffiliationId1});
  EXPECT_TRUE(user_delegate_->IsAffiliated());
}

// Tests that IsAffiliated returns false when user and device affiliation IDs
// do not match.
TEST_F(UserDelegateIOSTest, IsAffiliated_False) {
  CreateDelegate(/*is_managed_user=*/true, {kAffiliationId1},
                 {kAffiliationId2});
  EXPECT_FALSE(user_delegate_->IsAffiliated());
}

// Tests that IsManagedUser returns false when the user is not managed.
TEST_F(UserDelegateIOSTest, IsManagedUser_False) {
  CreateDelegate(/*is_managed_user=*/false);
  EXPECT_FALSE(user_delegate_->IsManagedUser());
}

// Tests that IsManagedUser returns true when the user is managed.
TEST_F(UserDelegateIOSTest, IsManagedUser_True) {
  CreateDelegate(/*is_managed_user=*/true);
  EXPECT_TRUE(user_delegate_->IsManagedUser());
}

// Tests that IsSameUser returns false when the identity manager is nullptr.
TEST_F(UserDelegateIOSTest, IsSameUser_NullManager) {
  CreateDelegate();
  user_delegate_ = std::make_unique<UserDelegateIOS>(
      profile_policy_connector_.get(),
      base::BindRepeating(&UserDelegateIOSTest::GetDeviceAffiliationIds,
                          base::Unretained(this)),
      nullptr, fake_dt_connector_service_.get());

  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);
  EXPECT_FALSE(user_delegate_->IsSameUser(account.GetGaiaId()));
}

// Tests that IsSameUser returns false when given a different user.
TEST_F(UserDelegateIOSTest, IsSameUser_DifferentUser) {
  identity_test_env_.MakePrimaryAccountAvailable(kUserEmail,
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.MakeAccountAvailable(
      kOtherUserEmail, {.set_cookie = true, .gaia_id = kOtherUserGaiaId});

  CreateDelegate();
  EXPECT_FALSE(user_delegate_->IsSameUser(kOtherUserGaiaId));
}

// Tests that IsSameUser returns false when there is no primary user.
TEST_F(UserDelegateIOSTest, IsSameUser_NoPrimaryUser) {
  identity_test_env_.MakeAccountAvailable(
      kOtherUserEmail, {.set_cookie = true, .gaia_id = kOtherUserGaiaId});

  CreateDelegate();
  EXPECT_FALSE(user_delegate_->IsSameUser(kOtherUserGaiaId));
}

// Tests that IsSameUser returns true when given the same user.
TEST_F(UserDelegateIOSTest, IsSameUser_SameUser) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      kUserEmail, signin::ConsentLevel::kSignin);

  CreateDelegate();
  EXPECT_TRUE(user_delegate_->IsSameUser(account.GetGaiaId()));
}

// Tests that GetPolicyScopesNeedingSignals returns an empty set when no
// policies are enabled.
TEST_F(UserDelegateIOSTest, GetPolicyScopesNeedingSignals_Empty) {
  CreateDelegate();
  EXPECT_EQ(user_delegate_->GetPolicyScopesNeedingSignals(),
            std::set<policy::PolicyScope>());
}

// Tests that GetPolicyScopesNeedingSignals returns user scope when the
// user-level DTC policy is enabled.
TEST_F(UserDelegateIOSTest, GetPolicyScopesNeedingSignals_User) {
  CreateDelegate();
  fake_dt_connector_service_->UpdateInlinePolicy(
      GetUrls(), enterprise_connectors::DTCPolicyLevel::kUser);
  EXPECT_EQ(user_delegate_->GetPolicyScopesNeedingSignals(),
            std::set<policy::PolicyScope>({policy::POLICY_SCOPE_USER}));
}

// Tests that IsCollectSignalsConsentRequired returns true on iOS.
TEST_F(UserDelegateIOSTest, IsCollectSignalsConsentRequired) {
  CreateDelegate();
  EXPECT_TRUE(user_delegate_->IsCollectSignalsConsentRequired());
}
