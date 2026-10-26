// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_user_permission_service_factory.h"

#import <memory>
#import <utility>

#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/pref_names.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class IOSUserPermissionServiceFactoryTest : public PlatformTest {
 protected:
  void CreateProfile(bool is_managed_user = false) {
    TestProfileIOS::Builder builder;
    if (is_managed_user) {
      auto policy_data = std::make_unique<enterprise_management::PolicyData>();
      policy_data->set_managed_by("example.com");
      policy_store_.set_policy_data_for_testing(std::move(policy_data));
    }
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
        std::make_unique<policy::MockPolicyService>(), &schema_registry_,
        &policy_store_));
    profile_ = std::move(builder).Build();
  }

  base::test::TaskEnvironment task_environment_;
  policy::SchemaRegistry schema_registry_;
  policy::MockCloudPolicyStore policy_store_{
      policy::dm_protocol::GetChromeUserPolicyType()};
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that `GetForProfile` returns a non-null UserPermissionService instance
// for a given profile and returns the same instance on subsequent calls.
TEST_F(IOSUserPermissionServiceFactoryTest, GetForProfile) {
  CreateProfile();
  device_signals::UserPermissionService* service1 =
      IOSUserPermissionServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service1);

  device_signals::UserPermissionService* service2 =
      IOSUserPermissionServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(service1, service2);
}

// Tests that an unmanaged profile on an unmanaged device cannot collect
// signals without consent.
TEST_F(IOSUserPermissionServiceFactoryTest,
       CanCollectSignals_UnmanagedProfile) {
  CreateProfile(/*is_managed_user=*/false);
  device_signals::UserPermissionService* service =
      IOSUserPermissionServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->ShouldCollectConsent());
  EXPECT_FALSE(service->HasUserConsented());
  EXPECT_EQ(service->CanCollectSignals(),
            device_signals::UserPermission::kMissingConsent);
}

// Tests that a managed profile on iOS implicitly has consent and can collect
// signals.
TEST_F(IOSUserPermissionServiceFactoryTest, CanCollectSignals_ManagedProfile) {
  CreateProfile(/*is_managed_user=*/true);
  device_signals::UserPermissionService* service =
      IOSUserPermissionServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->ShouldCollectConsent());
  EXPECT_TRUE(service->HasUserConsented());
  EXPECT_EQ(service->CanCollectSignals(),
            device_signals::UserPermission::kGranted);
}

// Tests that setting the permanent consent pref grants signal collection.
TEST_F(IOSUserPermissionServiceFactoryTest, CanCollectSignals_ConsentPref) {
  CreateProfile(/*is_managed_user=*/false);
  profile_->GetPrefs()->SetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived, true);

  device_signals::UserPermissionService* service =
      IOSUserPermissionServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->HasUserConsented());
  EXPECT_EQ(service->CanCollectSignals(),
            device_signals::UserPermission::kGranted);
}
