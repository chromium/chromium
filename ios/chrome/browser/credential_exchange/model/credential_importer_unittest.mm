// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"

#import <memory>
#import <tuple>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

using ::password_manager::PasswordForm;
using ::password_manager::PasswordStoreInterface;
using ::password_manager::SavedPasswordsPresenter;
using ::password_manager::TestPasswordStore;
using ::testing::SizeIs;

CredentialExchangePassword* CreateTestPassword(NSString* url) {
  return
      [[CredentialExchangePassword alloc] initWithURL:[NSURL URLWithString:url]
                                             username:@"username"
                                             password:@"password"
                                                 note:@"note"];
}

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    password_manager::IsAccountStore is_account_store,
    ProfileIOS* profile) {
  auto store = base::MakeRefCounted<password_manager::TestPasswordStore>(
      is_account_store);
  store->Init(/*affiliated_match_helper=*/nullptr);
  return store;
}

class FakePasswordStoreObserver
    : public password_manager::PasswordStoreInterface::Observer {
 public:
  void OnLoginsChanged(
      PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override {
    future_.SetValue();
  }

  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override {}

  bool WaitForLoginsChanged() { return future_.Wait(); }

 private:
  base::test::TestFuture<void> future_;
};

class CredentialImporterTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(&BuildPasswordStore,
                       password_manager::IsAccountStore(false)));

    builder.AddTestingFactory(
        IOSChromeAccountPasswordStoreFactory::GetInstance(),
        base::BindOnce(&BuildPasswordStore,
                       password_manager::IsAccountStore(true)));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
          return std::make_unique<affiliations::FakeAffiliationService>();
        }));
    profile_ = std::move(builder).Build();

    account_store_ = base::WrapRefCounted(static_cast<TestPasswordStore*>(
        IOSChromeAccountPasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get()));
    profile_store_ = base::WrapRefCounted(static_cast<TestPasswordStore*>(
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get()));
    presenter_ = std::make_unique<SavedPasswordsPresenter>(
        IOSChromeAffiliationServiceFactory::GetForProfile(profile_.get()),
        profile_store_, account_store_);
    presenter_->Init();

    passkey_model_ = std::make_unique<webauthn::TestPasskeyModel>();
    importer_delegate_ = OCMProtocolMock(@protocol(CredentialImporterDelegate));

    importer_ =
        [[CredentialImporter alloc] initWithDelegate:importer_delegate_
                             savedPasswordsPresenter:presenter_.get()
                                        passkeyModel:passkey_model_.get()];
  }

  TestPasswordStore& GetAccountStore() { return *account_store_.get(); }
  TestPasswordStore& GetProfileStore() { return *profile_store_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<TestPasswordStore> account_store_;
  scoped_refptr<TestPasswordStore> profile_store_;
  std::unique_ptr<SavedPasswordsPresenter> presenter_;
  std::unique_ptr<webauthn::TestPasskeyModel> passkey_model_;
  id importer_delegate_;
  CredentialImporter* importer_;
};

// TODO(crbug.com/458733320): Add tests for passkeys and invalid credentials.
TEST_F(CredentialImporterTest, ImportsValidPassword) {
  [importer_ onCredentialsTranslatedWithPasswords:@[ CreateTestPassword(
                                                      @"https://example.com") ]
                                         passkeys:@[]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithSecurityDomainSecrets:nil];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetProfileStore().IsEmpty());
  ASSERT_THAT(GetAccountStore().stored_passwords(), SizeIs(1));
  std::vector<PasswordForm> forms =
      GetAccountStore().stored_passwords().begin()->second;
  ASSERT_THAT(forms, SizeIs(1));
  EXPECT_EQ(forms[0].url.spec(), "https://example.com/");
  EXPECT_EQ(forms[0].username_value, u"username");
  EXPECT_EQ(forms[0].password_value, u"password");
  EXPECT_EQ(forms[0].GetNoteWithEmptyUniqueDisplayName(), u"note");
  EXPECT_EQ(forms[0].in_store, PasswordForm::Store::kAccountStore);
}

TEST_F(CredentialImporterTest, ImportsPasswordWithoutHttpsScheme) {
  [importer_ onCredentialsTranslatedWithPasswords:@[ CreateTestPassword(
                                                      @"example.com") ]
                                         passkeys:@[]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithSecurityDomainSecrets:nil];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetProfileStore().IsEmpty());
  ASSERT_THAT(GetAccountStore().stored_passwords(), SizeIs(1));
  std::vector<PasswordForm> forms =
      GetAccountStore().stored_passwords().begin()->second;
  ASSERT_THAT(forms, SizeIs(1));
  EXPECT_EQ(forms[0].url.spec(), "https://example.com/");
  EXPECT_EQ(forms[0].username_value, u"username");
  EXPECT_EQ(forms[0].password_value, u"password");
  EXPECT_EQ(forms[0].GetNoteWithEmptyUniqueDisplayName(), u"note");
  EXPECT_EQ(forms[0].in_store, PasswordForm::Store::kAccountStore);
}

TEST_F(CredentialImporterTest, DoesNotImportPasswordWithoutUrl) {
  CredentialExchangePassword* passwordWithoutUrl =
      [[CredentialExchangePassword alloc] initWithURL:nil
                                             username:@"username"
                                             password:@"password"
                                                 note:@"note"];
  [importer_ onCredentialsTranslatedWithPasswords:@[
    passwordWithoutUrl, CreateTestPassword(@"example.com")
  ]
                                         passkeys:@[]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithSecurityDomainSecrets:nil];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetProfileStore().IsEmpty());
  ASSERT_THAT(GetAccountStore().stored_passwords(), SizeIs(1));
  std::vector<PasswordForm> forms =
      GetAccountStore().stored_passwords().begin()->second;
  ASSERT_THAT(forms, SizeIs(1));
  EXPECT_EQ(forms[0].url.spec(), "https://example.com/");
  EXPECT_EQ(forms[0].username_value, u"username");
  EXPECT_EQ(forms[0].password_value, u"password");
  EXPECT_EQ(forms[0].GetNoteWithEmptyUniqueDisplayName(), u"note");
  EXPECT_EQ(forms[0].in_store, PasswordForm::Store::kAccountStore);
}

}  // namespace
