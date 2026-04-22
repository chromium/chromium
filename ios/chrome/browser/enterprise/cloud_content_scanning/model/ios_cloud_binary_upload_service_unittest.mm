// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#import "components/policy/core/common/management/management_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/signin/model/signin_client_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

// Mock implementation of BinaryUploadRequest for testing.
class MockBinaryUploadRequest : public BinaryUploadRequest {
 public:
  MockBinaryUploadRequest()
      : BinaryUploadRequest(
            base::DoNothing(),
            CloudOrLocalAnalysisSettings(),
            base::BindRepeating(
                []() -> policy::BrowserPolicyConnector* { return nullptr; })) {}
  MOCK_METHOD1(GetRequestData, void(DataCallback));
};

// Mock implementation of BrowserManagementService to control management
// authority in tests.
class MockBrowserManagementService : public policy::BrowserManagementService {
 public:
  explicit MockBrowserManagementService(ProfileIOS* profile)
      : policy::BrowserManagementService(profile) {}
};

std::unique_ptr<KeyedService> CreateMockBrowserManagementService(
    ProfileIOS* profile) {
  return std::make_unique<MockBrowserManagementService>(profile);
}

}  // namespace

// Test fixture for IOSCloudBinaryUploadService.
class IOSCloudBinaryUploadServiceTest : public PlatformTest {
 public:
  IOSCloudBinaryUploadServiceTest() {
    TestProfileIOS::Builder builder;
    // Install mock management service and identity manager.
    builder.AddTestingFactory(
        policy::BrowserManagementServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockBrowserManagementService));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    // Provide a policy connector to handle affiliation checks.
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnector>());
    profile_ = std::move(builder).Build();

    service_ = std::make_unique<IOSCloudBinaryUploadService>(profile_.get());
  }

  // Returns the management service for the test profile.
  policy::BrowserManagementService* management_service() {
    return policy::BrowserManagementServiceFactory::GetForProfile(
        profile_.get());
  }

  // Returns the service as a Delegate to access private/protected methods
  // through the interface.
  CloudBinaryUploadServiceBase::Delegate* delegate() { return service_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<IOSCloudBinaryUploadService> service_;
};

// Tests that IsEnhancedProtection correctly reflects the preference state.
TEST_F(IOSCloudBinaryUploadServiceTest, IsEnhancedProtection) {
  EXPECT_FALSE(delegate()->IsEnhancedProtection());
}

// Tests that IsAdvancedProtection returns false (not supported on iOS).
TEST_F(IOSCloudBinaryUploadServiceTest, IsAdvancedProtection) {
  EXPECT_FALSE(delegate()->IsAdvancedProtection());
}

// Tests that the BrowserPolicyConnectorGetter returns the global connector.
TEST_F(IOSCloudBinaryUploadServiceTest, BrowserPolicyConnectorGetter) {
  auto getter = delegate()->BrowserPolicyConnectorGetter();
  EXPECT_EQ(getter.Run(), GetApplicationContext()->GetBrowserPolicyConnector());
}

// Tests that access tokens are fetched for unmanaged devices.
TEST_F(IOSCloudBinaryUploadServiceTest, MaybeGetAccessToken_Unmanaged) {
  management_service()->SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::NONE);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);
  signin::SetAutomaticIssueOfAccessTokens(identity_manager, true);

  MockBinaryUploadRequest request;
  std::string token;
  bool called = false;

  base::RunLoop run_loop;
  delegate()->MaybeGetAccessToken(
      &request,
      base::BindLambdaForTesting([&](const std::string& fetched_token) {
        token = fetched_token;
        called = true;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_EQ(token, "access_token");
}

// Tests that access tokens are NOT fetched for managed devices when the request
// is not per-profile.
TEST_F(IOSCloudBinaryUploadServiceTest,
       MaybeGetAccessToken_Managed_NoPerProfile) {
  management_service()->SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  MockBinaryUploadRequest request;
  request.set_per_profile_request(false);
  std::string token = "initial";
  bool called = false;

  delegate()->MaybeGetAccessToken(
      &request,
      base::BindLambdaForTesting([&](const std::string& fetched_token) {
        token = fetched_token;
        called = true;
      }));

  EXPECT_TRUE(called);
  EXPECT_EQ(token, "");
}

// Tests that access tokens ARE fetched for managed devices when the request
// is explicitly per-profile.
TEST_F(IOSCloudBinaryUploadServiceTest,
       MaybeGetAccessToken_Managed_PerProfile) {
  management_service()->SetManagementAuthoritiesForTesting(
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@example.com",
                                      signin::ConsentLevel::kSignin);
  signin::SetAutomaticIssueOfAccessTokens(identity_manager, true);

  MockBinaryUploadRequest request;
  request.set_per_profile_request(true);
  std::string token;
  bool called = false;

  base::RunLoop run_loop;
  delegate()->MaybeGetAccessToken(
      &request,
      base::BindLambdaForTesting([&](const std::string& fetched_token) {
        token = fetched_token;
        called = true;
        run_loop.Quit();
      }));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_EQ(token, "access_token");
}

}  // namespace enterprise_connectors
