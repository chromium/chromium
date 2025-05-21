// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/device_accounts_provider_impl.h"

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::Eq;
using testing::Property;

namespace {
class MockObserver : public DeviceAccountsProvider::Observer {
 public:
  MOCK_METHOD(void, OnAccountsOnDeviceChanged, (), (override));
  MOCK_METHOD(void,
              OnAccountOnDeviceUpdated,
              (const DeviceAccountsProvider::AccountInfo& device_account),
              (override));
};

const char* const kClientID = "ClientID";
}  // namespace

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
  provider->GetAccessToken(GaiaId("UnknownGaiaID"), kClientID, scopes,
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
  GaiaId gaia_id(fake_identity.gaiaID);
  provider->GetAccessToken(gaia_id, kClientID, scopes, std::move(callback));
  run_loop.Run();
}

// Tests the observer is invoked.
TEST_F(DeviceAccountsProviderImplTest, TestOnAccountsOnDeviceChanged) {
  DeviceAccountsProviderImpl* provider = GetDeviceAccountsProviderImpl();
  MockObserver observer;
  base::ScopedObservation<DeviceAccountsProvider, MockObserver>
      scoped_observation{&observer};
  scoped_observation.Observe(provider);

  EXPECT_CALL(observer, OnAccountsOnDeviceChanged());
  fake_system_identity_manager_->FireSystemIdentityReloaded();
}

// Tests the observer is invoked.
TEST_F(DeviceAccountsProviderImplTest, TestOnAccountOnDeviceUpdated) {
  const FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager_->AddIdentity(fake_identity);

  DeviceAccountsProviderImpl* provider = GetDeviceAccountsProviderImpl();
  MockObserver observer;
  base::ScopedObservation<DeviceAccountsProvider, MockObserver>
      scoped_observation{&observer};
  scoped_observation.Observe(provider);

  EXPECT_CALL(observer, OnAccountOnDeviceUpdated(Property(
                            &DeviceAccountsProvider::AccountInfo::GetGaiaId,
                            Eq(GaiaId(fake_identity.gaiaID)))));
  fake_system_identity_manager_->FireIdentityUpdatedNotification(fake_identity);
}
