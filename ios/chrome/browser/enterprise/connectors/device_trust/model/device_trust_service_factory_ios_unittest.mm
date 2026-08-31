// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/mock_signals_aggregator.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/policy/core/common/management/management_service.h"
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

    management_service_ =
        policy::BrowserManagementServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(management_service_);
    management_service_->SetManagementAuthoritiesForTesting(
        policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  }

  void SetManagementAuthority(policy::EnterpriseManagementAuthority authority) {
    management_service_->SetManagementAuthoritiesForTesting(authority);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<policy::BrowserManagementService> management_service_ = nullptr;
};

// Verifies that the factory creates a service for a managed regular profile
// even when the connector is not enabled by policy.
TEST_F(DeviceTrustServiceFactoryIOSTest, CreateService) {
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

  enterprise_connectors::DeviceTrustService* second_service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get());
  EXPECT_EQ(first_service, second_service);
}

// Verifies that unmanaged profiles do not instantiate a service.
TEST_F(DeviceTrustServiceFactoryIOSTest, UnmanagedProfileReturnsNull) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(IOSSignalsAggregatorFactory::GetInstance(),
                            base::BindOnce(&BuildMockSignalsAggregator));
  builder.AddTestingFactory(DeviceTrustServiceFactoryIOS::GetInstance(),
                            DeviceTrustServiceFactoryIOS::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> unmanaged_profile =
      std::move(builder).Build();

  policy::BrowserManagementService* management_service =
      policy::BrowserManagementServiceFactory::GetForProfile(
          unmanaged_profile.get());
  ASSERT_TRUE(management_service);
  management_service->SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::NONE);

  EXPECT_FALSE(
      DeviceTrustServiceFactoryIOS::GetForProfile(unmanaged_profile.get()));
}

// Verifies that off-the-record profiles do not receive a service instance.
TEST_F(DeviceTrustServiceFactoryIOSTest, OffTheRecordReturnsNull) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  ASSERT_TRUE(otr_profile);

  policy::BrowserManagementService* management_service =
      policy::BrowserManagementServiceFactory::GetForProfile(otr_profile);
  ASSERT_TRUE(management_service);
  management_service->SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  enterprise_connectors::DeviceTrustService* service =
      DeviceTrustServiceFactoryIOS::GetForProfile(otr_profile);
  EXPECT_FALSE(service);
}

// Verifies that testing profiles do not instantiate a service by default
// (even when managed) unless explicitly configured via `AddTestingFactory`.
TEST_F(DeviceTrustServiceFactoryIOSTest, TestingProfileReturnsNullByDefault) {
  TestProfileIOS::Builder builder;
  std::unique_ptr<TestProfileIOS> unconfigured_profile =
      std::move(builder).Build();

  policy::BrowserManagementService* management_service =
      policy::BrowserManagementServiceFactory::GetForProfile(
          unconfigured_profile.get());
  ASSERT_TRUE(management_service);

  management_service->SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
  enterprise_connectors::DeviceTrustService* service =
      DeviceTrustServiceFactoryIOS::GetForProfile(unconfigured_profile.get());
  EXPECT_FALSE(service);
}

}  // namespace
