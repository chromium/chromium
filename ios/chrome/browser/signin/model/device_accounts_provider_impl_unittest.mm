// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/device_accounts_provider_impl.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
const char* const kClientID = "ClientID";
}

class DeviceAccountsProviderImplTest : public PlatformTest {
 public:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    device_accounts_provider_impl_ =
        std::make_unique<DeviceAccountsProviderImpl>(account_manager_service);
  }

  DeviceAccountsProviderImpl* GetDeviceAccountsProviderImpl() {
    return device_accounts_provider_impl_.get();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<DeviceAccountsProviderImpl> device_accounts_provider_impl_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
};

// Tests the returned error when the identity is unknown, while fetching
// an access token.
TEST_F(DeviceAccountsProviderImplTest, TestFetchWithUnknownIdentity) {
  DeviceAccountsProviderImpl* provider = GetDeviceAccountsProviderImpl();
  base::RunLoop run_loop;
  std::set<std::string> scopes;
  DeviceAccountsProvider::AccessTokenCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         DeviceAccountsProvider::AccessTokenResult result) {
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(),
                  kAuthenticationErrorCategoryUnknownIdentityErrors);
        run_loop->Quit();
      },
      &run_loop);
  provider->GetAccessToken("UnknownGaiaID", kClientID, scopes,
                           std::move(callback));
  run_loop.Run();
}

// Tests no error returned, while fetching an access token.
TEST_F(DeviceAccountsProviderImplTest, TestFetchWithFakeIdentity) {
  const FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager_->AddIdentity(fake_identity);
  DeviceAccountsProviderImpl* provider = GetDeviceAccountsProviderImpl();
  base::RunLoop run_loop;
  std::set<std::string> scopes;
  DeviceAccountsProvider::AccessTokenCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         DeviceAccountsProvider::AccessTokenResult result) {
        EXPECT_TRUE(result.has_value());
        run_loop->Quit();
      },
      &run_loop);
  std::string gaia_id = base::SysNSStringToUTF8(fake_identity.gaiaID);
  provider->GetAccessToken(gaia_id, kClientID, scopes, std::move(callback));
  run_loop.Run();
}
