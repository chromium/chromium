// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/model/credential_importer.h"

#import <memory>
#import <tuple>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/webauthn/core/browser/passkey_model_change.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_passkey.h"
#import "ios/chrome/browser/credential_exchange/model/credential_exchange_password.h"
#import "ios/chrome/browser/credential_exchange/model/credential_import_manager_swift.h"
#import "ios/chrome/browser/credential_exchange/model/import_stats.h"
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
using ::password_manager::StoredCredential;
using ::password_manager::TestPasswordStore;
using ::testing::SizeIs;

NSData* ToNSData(const std::string& str) {
  return [NSData dataWithBytes:str.data() length:str.length()];
}

CredentialExchangePassword* CreateTestPassword(NSString* url) {
  return
      [[CredentialExchangePassword alloc] initWithURL:[NSURL URLWithString:url]
                                             username:@"username"
                                             password:@"password"
                                                 note:@"note"];
}

CredentialExchangePasskey* CreateTestPasskey() {
  return [[CredentialExchangePasskey alloc]
      initWithCredentialId:ToNSData("1234567890123456")
                      rpId:@"example.com"
                  userName:@"userName"
           userDisplayName:@"userDisplayName"
                    userId:ToNSData("user_id")
                privateKey:ToNSData("private_key")];
}

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    password_manager::IsAccountStore is_account_store,
    ProfileIOS* profile) {
  auto store = base::MakeRefCounted<password_manager::TestPasswordStore>(
      is_account_store);
  store->Init();
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
      const std::vector<StoredCredential>& retained_credentials) override {}

  bool WaitForLoginsChanged() { return future_.Wait(); }

 private:
  base::test::TestFuture<void> future_;
};

class FakePasskeyModelObserver : public webauthn::PasskeyModel::Observer {
 public:
  explicit FakePasskeyModelObserver(webauthn::PasskeyModel* model)
      : model_(model) {
    model_->AddObserver(this);
  }

  ~FakePasskeyModelObserver() override { model_->RemoveObserver(this); }

  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override {
    future_.SetValue();
  }

  void OnPasskeyModelShuttingDown() override {}
  void OnPasskeyModelIsReady(bool is_ready) override {}

  bool WaitForPasskeysChanged() { return future_.Wait(); }

 private:
  raw_ptr<webauthn::PasskeyModel> model_;
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

TEST_F(CredentialImporterTest, ImportsValidPasskey) {
  [importer_ onCredentialsTranslatedWithPasswords:@[]
                                         passkeys:@[ CreateTestPasskey() ]
                              exporterDisplayName:@""
                                            stats:[[ImportStats alloc] init]];

  FakePasskeyModelObserver observer(passkey_model_.get());

  [importer_
      startImportingCredentialsWithTrustedVaultKeys:{std::vector<uint8_t>(16,
                                                                          0)}];

  ASSERT_TRUE(observer.WaitForPasskeysChanged());
  ASSERT_FALSE(passkey_model_->IsEmpty());
  auto passkeys = passkey_model_->GetPasskeys(
      webauthn::PasskeyModel::AnyRp(),
      webauthn::PasskeyModel::ShadowedCredentials::kExclude);
  ASSERT_THAT(passkeys, SizeIs(1));
  EXPECT_EQ(passkeys[0].rp_id(), "example.com");
  EXPECT_EQ(passkeys[0].user_name(), "userName");
  EXPECT_EQ(passkeys[0].user_display_name(), "userDisplayName");
}

TEST_F(CredentialImporterTest, DoesNotImportInvalidPassword) {
  NSString* tooLongPassword = [@"" stringByPaddingToLength:1001
                                                withString:@"a"
                                           startingAtIndex:0];
  CredentialExchangePassword* invalidPassword =
      [[CredentialExchangePassword alloc]
          initWithURL:[NSURL URLWithString:@"https://invalid.com"]
             username:@"invalid_username"
             password:tooLongPassword
                 note:@"invalid_note"];

  [importer_ onCredentialsTranslatedWithPasswords:@[
    invalidPassword, CreateTestPassword(@"https://example.com")
  ]
                                         passkeys:@[]
                              exporterDisplayName:@""
                                            stats:[[ImportStats alloc] init]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithTrustedVaultKeys:{}];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetAllLoginsSync(&GetProfileStore()).empty());
  auto forms_map = GetAllLoginsSync(&GetAccountStore());
  ASSERT_THAT(forms_map, SizeIs(1));
  std::vector<PasswordForm> forms = forms_map.begin()->second;
  EXPECT_EQ(forms[0].url.spec(), "https://example.com/");
  EXPECT_EQ(forms[0].username_value, u"username");
  EXPECT_EQ(forms[0].password_value, u"password");
  EXPECT_EQ(forms[0].GetNoteWithEmptyUniqueDisplayName(), u"note");
  EXPECT_EQ(forms[0].in_store, PasswordForm::Store::kAccountStore);
}

TEST_F(CredentialImporterTest, ImportsValidPassword) {
  [importer_ onCredentialsTranslatedWithPasswords:@[ CreateTestPassword(
                                                      @"https://example.com") ]
                                         passkeys:@[]
                              exporterDisplayName:@""
                                            stats:[[ImportStats alloc] init]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithTrustedVaultKeys:{}];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetAllLoginsSync(&GetProfileStore()).empty());
  auto forms_map = GetAllLoginsSync(&GetAccountStore());
  ASSERT_THAT(forms_map, SizeIs(1));
  std::vector<PasswordForm> forms = forms_map.begin()->second;
  EXPECT_EQ(forms[0].url.spec(), "https://example.com/");
  EXPECT_EQ(forms[0].username_value, u"username");
  EXPECT_EQ(forms[0].password_value, u"password");
  EXPECT_EQ(forms[0].GetNoteWithEmptyUniqueDisplayName(), u"note");
  EXPECT_EQ(forms[0].in_store, PasswordForm::Store::kAccountStore);
}

TEST_F(CredentialImporterTest, ImportsPasswordWithoutHttpsScheme) {
  [importer_ onCredentialsTranslatedWithPasswords:@[ CreateTestPassword(
                                                      @"example.com") ]
                                         passkeys:@[]
                              exporterDisplayName:@""
                                            stats:[[ImportStats alloc] init]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithTrustedVaultKeys:{}];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetAllLoginsSync(&GetProfileStore()).empty());
  auto forms_map = GetAllLoginsSync(&GetAccountStore());
  ASSERT_THAT(forms_map, SizeIs(1));
  std::vector<PasswordForm> forms = forms_map.begin()->second;
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
                                         passkeys:@[]
                              exporterDisplayName:@""
                                            stats:[[ImportStats alloc] init]];

  FakePasswordStoreObserver observer;
  GetAccountStore().AddObserver(&observer);

  [importer_ startImportingCredentialsWithTrustedVaultKeys:{}];

  ASSERT_TRUE(observer.WaitForLoginsChanged());
  GetAccountStore().RemoveObserver(&observer);

  EXPECT_TRUE(GetAllLoginsSync(&GetProfileStore()).empty());
  auto forms_map = GetAllLoginsSync(&GetAccountStore());
  ASSERT_THAT(forms_map, SizeIs(1));
  std::vector<PasswordForm> forms = forms_map.begin()->second;
  EXPECT_EQ(forms[0].url.spec(), "https://example.com/");
  EXPECT_EQ(forms[0].username_value, u"username");
  EXPECT_EQ(forms[0].password_value, u"password");
  EXPECT_EQ(forms[0].GetNoteWithEmptyUniqueDisplayName(), u"note");
  EXPECT_EQ(forms[0].in_store, PasswordForm::Store::kAccountStore);
}

TEST_F(CredentialImporterTest, PropagatesImportError) {
  [[importer_delegate_ expect] onImportError];

  [importer_ onImportError];

  [importer_delegate_ verify];
}

TEST_F(CredentialImporterTest, RecordsCredentialsReceivedMetrics) {
  base::HistogramTester histogram_tester;

  ImportStats* stats = [[ImportStats alloc] init];
  stats.addressCount = 1;
  stats.apiKeyCount = 2;
  stats.basicAuthenticationCount = 3;
  stats.creditCardCount = 4;
  stats.customFieldsCount = 5;
  stats.driversLicenseCount = 6;
  stats.generatedPasswordCount = 7;
  stats.identityDocumentCount = 8;
  stats.itemReferenceCount = 9;
  stats.noteCount = 10;
  stats.noteForPasswordCount = 11;
  stats.passkeyCount = 12;
  stats.passportCount = 13;
  stats.personNameCount = 14;
  stats.sshKeyCount = 15;
  stats.totpCount = 16;
  stats.wifiCount = 17;

  [importer_ onCredentialsTranslatedWithPasswords:@[]
                                         passkeys:@[]
                              exporterDisplayName:@""
                                            stats:stats];

  const std::map<std::string, int> expected_counts = {
      {"Address", 1},
      {"APIKey", 2},
      {"BasicAuthentication", 3},
      {"CreditCard", 4},
      {"CustomFields", 5},
      {"DriversLicense", 6},
      {"GeneratedPassword", 7},
      {"IdentityDocument", 8},
      {"ItemReference", 9},
      {"Note", 10},
      {"NoteForPassword", 11},
      {"Passkey", 12},
      {"Passport", 13},
      {"PersonName", 14},
      {"SSHKey", 15},
      {"TOTP", 16},
      {"WiFi", 17}};
  for (const auto& [suffix, count] : expected_counts) {
    histogram_tester.ExpectUniqueSample(
        "IOS.CredentialExchange.CredentialsReceivedCount." + suffix, count, 1);
  }
}

}  // namespace
