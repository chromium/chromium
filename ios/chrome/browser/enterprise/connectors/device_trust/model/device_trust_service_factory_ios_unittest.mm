// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/device_signals/core/browser/mock_signals_aggregator.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "components/enterprise/device_trust/prefs.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/policy/core/common/management/management_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/enterprise/signals/model/ios_signals_aggregator_factory.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildMockSignalsAggregator(ProfileIOS*) {
  return std::make_unique<device_signals::MockSignalsAggregator>();
}

class DeviceTrustServiceFactoryIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(IOSSignalsAggregatorFactory::GetInstance(),
                              base::BindOnce(&BuildMockSignalsAggregator));
    builder.AddTestingFactory(
        DeviceTrustServiceFactoryIOS::GetInstance(),
        DeviceTrustServiceFactoryIOS::GetDefaultFactory());
    profile_ = std::move(builder).Build();
  }

  void SetManagementAuthority(policy::EnterpriseManagementAuthority authority) {
    policy::BrowserManagementService* management_service =
        policy::BrowserManagementServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(management_service);
    management_service->SetManagementAuthoritiesForTesting(authority);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Verifies that an unmanaged regular profile gets a service that is disabled.
TEST_F(DeviceTrustServiceFactoryIOSTest, CreateDisabledServiceForUnmanaged) {
  SetManagementAuthority(policy::EnterpriseManagementAuthority::NONE);
  enterprise_connectors::DeviceTrustService* service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsEnabled());
}

// Verifies that repeated lookups return the same profile-scoped service.
TEST_F(DeviceTrustServiceFactoryIOSTest, ReturnSameInstance) {
  enterprise_connectors::DeviceTrustService* first_service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get());
  ASSERT_TRUE(first_service);
  EXPECT_EQ(first_service,
            DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get()));
}

// Verifies that the service does not change when the management authority
// changes (e.g. after enrollment).
TEST_F(DeviceTrustServiceFactoryIOSTest,
       ReturnSameInstanceWhenManagementChanges) {
  SetManagementAuthority(policy::EnterpriseManagementAuthority::NONE);
  enterprise_connectors::DeviceTrustService* service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  SetManagementAuthority(policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  EXPECT_EQ(service,
            DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get()));
  SetManagementAuthority(policy::EnterpriseManagementAuthority::NONE);
  EXPECT_EQ(service,
            DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get()));
}

// Verifies that setting the allowlist policy enables the existing service.
TEST_F(DeviceTrustServiceFactoryIOSTest, EnableServiceWithManagedAllowlist) {
  enterprise_connectors::DeviceTrustService* service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get());
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsEnabled());
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_connectors::kUserContextAwareAccessSignalsAllowlistPref,
      base::ListValue().Append("https://example.com"));
  EXPECT_TRUE(service->IsEnabled());
  EXPECT_EQ(service,
            DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get()));
}

// Verifies that off-the-record profiles do not receive a service instance.
TEST_F(DeviceTrustServiceFactoryIOSTest, OffTheRecordReturnsNull) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);
  EXPECT_FALSE(DeviceTrustServiceFactoryIOS::GetForProfile(otr_profile));
}

// Verifies that testing profiles do not instantiate a service by default
// unless explicitly configured via `AddTestingFactory`.
TEST_F(DeviceTrustServiceFactoryIOSTest, TestingProfileReturnsNullByDefault) {
  std::unique_ptr<TestProfileIOS> unconfigured_profile =
      TestProfileIOS::Builder().Build();
  EXPECT_FALSE(
      DeviceTrustServiceFactoryIOS::GetForProfile(unconfigured_profile.get()));
}

}  // namespace
