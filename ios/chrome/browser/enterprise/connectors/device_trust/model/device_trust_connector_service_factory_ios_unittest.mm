// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"

#import <set>
#import <utility>

#import "base/test/task_environment.h"
#import "base/values.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/prefs.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using DTCPolicyLevel = enterprise_connectors::DTCPolicyLevel;
using DeviceTrustConnectorService =
    enterprise_connectors::DeviceTrustConnectorService;

}  // namespace

class DeviceTrustConnectorServiceFactoryIOSTest : public PlatformTest {
 protected:
  DeviceTrustConnectorServiceFactoryIOSTest() {
    profile_ = TestProfileIOS::Builder().Build();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Verifies that `DeviceTrustConnectorServiceFactoryIOS` creates a valid service
// instance for a regular profile.
TEST_F(DeviceTrustConnectorServiceFactoryIOSTest, CreateConnectorService) {
  DeviceTrustConnectorService* service =
      DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile_.get());
  EXPECT_NE(service, nullptr);
  EXPECT_FALSE(service->IsConnectorEnabled());
}

// Verifies that `DeviceTrustConnectorServiceFactoryIOS` returns null in
// Incognito.
TEST_F(DeviceTrustConnectorServiceFactoryIOSTest,
       OffTheRecordReturnsNullForConnector) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  DeviceTrustConnectorService* service =
      DeviceTrustConnectorServiceFactoryIOS::GetForProfile(otr_profile);
  EXPECT_EQ(service, nullptr);
}

// Verifies that setting the allowlist policy pref enables the connector and
// watches the specified URLs.
TEST_F(DeviceTrustConnectorServiceFactoryIOSTest,
       ManagedAllowlistEnablesConnector) {
  DeviceTrustConnectorService* service =
      DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile_.get());
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(service->IsConnectorEnabled());

  base::ListValue urls;
  urls.Append("https://example.com");
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_connectors::kUserContextAwareAccessSignalsAllowlistPref,
      std::move(urls));

  EXPECT_TRUE(service->IsConnectorEnabled());
  EXPECT_EQ(service->Watches(GURL("https://example.com")),
            std::set<DTCPolicyLevel>({DTCPolicyLevel::kUser}));
  EXPECT_TRUE(service->Watches(GURL("https://other.com")).empty());
}
