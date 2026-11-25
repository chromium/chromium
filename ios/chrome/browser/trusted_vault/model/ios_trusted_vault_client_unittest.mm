// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_client.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/test_future.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/trusted_vault/trusted_vault_key_and_version.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "google_apis/gaia/core_account_id.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/trusted_vault/trusted_vault_client_backend.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::StrictMock;

constexpr trusted_vault::SecurityDomainId kSecurityDomainId =
    trusted_vault::SecurityDomainId::kPasskeys;

CoreAccountInfo AccountInfoForIdentity(id<SystemIdentity> identity) {
  CoreAccountInfo account_info;
  account_info.gaia = identity.gaiaId;
  account_info.email = base::SysNSStringToUTF8(identity.userEmail);
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  return account_info;
}

class MockTrustedVaultClientBackend : public TrustedVaultClientBackend {
 public:
  MockTrustedVaultClientBackend() = default;
  ~MockTrustedVaultClientBackend() override = default;

  MOCK_METHOD(void,
              FetchKeys,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               KeysWithVersionsFetchedCallback completion),
              (override));
  MOCK_METHOD(void,
              MarkLocalKeysAsStale,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               base::OnceClosure completion),
              (override));
  MOCK_METHOD(void,
              GetDegradedRecoverabilityStatus,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               base::OnceCallback<void(bool)> completion),
              (override));
  MOCK_METHOD(CancelDialogCallback,
              Reauthentication,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               trusted_vault::TrustedVaultUserActionTriggerForUMA trigger,
               UIViewController* presenting_view_controller,
               CompletionBlock completion),
              (override));
  MOCK_METHOD(CancelDialogCallback,
              FixDegradedRecoverability,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               UIViewController* presenting_view_controller,
               CompletionBlock completion),
              (override));
  MOCK_METHOD(void,
              ClearLocalData,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               base::OnceCallback<void(bool)> completion),
              (override));
  MOCK_METHOD(void,
              GetPublicKeyForIdentity,
              (id<SystemIdentity> identity, GetPublicKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateGPMPinForAccount,
              (id<SystemIdentity> identity,
               trusted_vault::SecurityDomainId security_domain_id,
               UINavigationController* navigation_controller,
               UIView* branded_navigation_item_title_view,
               UpdateGPMPinCompletionCallback completion),
              (override));
};

class IOSTrustedVaultClientTest : public PlatformTest {
 public:
  IOSTrustedVaultClientTest() {
    TestProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile);
    system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    client_ = std::make_unique<IOSTrustedVaultClient>(
        account_manager_service_, /*identity_manager=*/nullptr, &backend_,
        /*shared_url_loader_factory=*/nullptr, kSecurityDomainId);
  }

  ~IOSTrustedVaultClientTest() override = default;

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_ = nullptr;
  raw_ptr<FakeSystemIdentityManager> system_identity_manager_ = nullptr;
  StrictMock<MockTrustedVaultClientBackend> backend_;
  // Declared after `profile_manager_` and `backend_` so it is destroyed before
  // the services it holds `raw_ptr`s to.
  std::unique_ptr<IOSTrustedVaultClient> client_;
};

TEST_F(IOSTrustedVaultClientTest, FetchKeysWithMissingIdentity) {
  FakeSystemIdentity* unknown_identity = [FakeSystemIdentity fakeIdentity1];
  EXPECT_CALL(backend_, FetchKeys).Times(0);

  TestFuture<const std::vector<trusted_vault::TrustedVaultKeyAndVersion>&>
      future;
  client_->FetchKeys(AccountInfoForIdentity(unknown_identity),
                     future.GetCallback());
  // `callback` should be posted asynchronously rather than run re-entrantly.
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(),
            std::vector<trusted_vault::TrustedVaultKeyAndVersion>());
}

TEST_F(IOSTrustedVaultClientTest, FetchKeysForwardsKnownIdentity) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  system_identity_manager_->AddIdentity(identity);

  const trusted_vault::TrustedVaultKeyAndVersion kExpectedKey({1, 2, 3},
                                                              /*version=*/1);
  EXPECT_CALL(backend_, FetchKeys(identity, kSecurityDomainId, _))
      .WillOnce(RunOnceCallback<2>(
          std::vector<trusted_vault::TrustedVaultKeyAndVersion>{kExpectedKey}));

  TestFuture<const std::vector<trusted_vault::TrustedVaultKeyAndVersion>&>
      future;
  client_->FetchKeys(AccountInfoForIdentity(identity), future.GetCallback());
  EXPECT_THAT(future.Get(), ElementsAre(kExpectedKey));
}

TEST_F(IOSTrustedVaultClientTest, MarkLocalKeysAsStaleWithMissingIdentity) {
  FakeSystemIdentity* unknown_identity = [FakeSystemIdentity fakeIdentity1];
  EXPECT_CALL(backend_, MarkLocalKeysAsStale).Times(0);

  TestFuture<bool> future;
  client_->MarkLocalKeysAsStale(AccountInfoForIdentity(unknown_identity),
                                future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

TEST_F(IOSTrustedVaultClientTest, MarkLocalKeysAsStaleForwardsKnownIdentity) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  system_identity_manager_->AddIdentity(identity);

  EXPECT_CALL(backend_, MarkLocalKeysAsStale(identity, kSecurityDomainId, _))
      .WillOnce(RunOnceClosure<2>());

  TestFuture<bool> future;
  client_->MarkLocalKeysAsStale(AccountInfoForIdentity(identity),
                                future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(IOSTrustedVaultClientTest,
       GetIsRecoverabilityDegradedWithMissingIdentity) {
  FakeSystemIdentity* unknown_identity = [FakeSystemIdentity fakeIdentity1];
  EXPECT_CALL(backend_, GetDegradedRecoverabilityStatus).Times(0);

  TestFuture<bool> future;
  client_->GetIsRecoverabilityDegraded(AccountInfoForIdentity(unknown_identity),
                                       future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(future.Get());
}

TEST_F(IOSTrustedVaultClientTest,
       GetIsRecoverabilityDegradedForwardsKnownIdentity) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  system_identity_manager_->AddIdentity(identity);

  EXPECT_CALL(backend_,
              GetDegradedRecoverabilityStatus(identity, kSecurityDomainId, _))
      .WillOnce(RunOnceCallback<2>(true));

  TestFuture<bool> future;
  client_->GetIsRecoverabilityDegraded(AccountInfoForIdentity(identity),
                                       future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(IOSTrustedVaultClientTest, ClearLocalDataForAccountWithMissingIdentity) {
  FakeSystemIdentity* unknown_identity = [FakeSystemIdentity fakeIdentity1];
  EXPECT_CALL(backend_, ClearLocalData).Times(0);

  client_->ClearLocalDataForAccount(AccountInfoForIdentity(unknown_identity));
}

TEST_F(IOSTrustedVaultClientTest,
       ClearLocalDataForAccountForwardsKnownIdentity) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  system_identity_manager_->AddIdentity(identity);

  EXPECT_CALL(backend_, ClearLocalData(identity, kSecurityDomainId, _));

  client_->ClearLocalDataForAccount(AccountInfoForIdentity(identity));
}

}  // namespace
