// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_ios.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#import "components/enterprise/client_certificates/core/features.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace policy {

class ChromeBrowserCloudManagementControllerIOSTest : public PlatformTest {
 protected:
  ChromeBrowserCloudManagementControllerIOSTest() = default;
  ~ChromeBrowserCloudManagementControllerIOSTest() override = default;

  void SetUp() override {
    TestingApplicationContext::GetGlobal()->SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    TestingApplicationContext::GetGlobal()->GetBrowserPolicyConnector()->Init(
        TestingApplicationContext::GetGlobal()->GetLocalState(),
        TestingApplicationContext::GetGlobal()->GetSharedURLLoaderFactory());
  }

  void TearDown() override {
    TestingApplicationContext::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
    TestingApplicationContext::GetGlobal()
        ->GetBrowserPolicyConnector()
        ->Shutdown();
  }

 private:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
};

// Tests that when the client certificate provisioning feature is disabled on
// iOS, the service creator returns a null pointer.
TEST_F(ChromeBrowserCloudManagementControllerIOSTest,
       CreateCertificateProvisioningService_FeatureDisabled) {
  ChromeBrowserCloudManagementControllerIOS delegate;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      client_certificates::features::kEnableClientCertificateProvisioningOnIOS);
  auto service = delegate.CreateCertificateProvisioningService();
  EXPECT_EQ(service, nullptr);
}

// Tests that when the client certificate provisioning feature is enabled on
// iOS, the service creator correctly returns a non-null instance of the
// certificate provisioning service.
TEST_F(ChromeBrowserCloudManagementControllerIOSTest,
       CreateCertificateProvisioningService_FeatureEnabled) {
  ChromeBrowserCloudManagementControllerIOS delegate;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      client_certificates::features::kEnableClientCertificateProvisioningOnIOS);
  auto service = delegate.CreateCertificateProvisioningService();
  EXPECT_TRUE(service);
}

}  // namespace policy
