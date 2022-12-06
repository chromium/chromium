// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_service.h"

#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store_built_in_backend.h"
#import "components/password_manager/core/browser/password_store_factory_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/memory_credential_store.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::FakeAffiliationService;
using password_manager::PasswordForm;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;
using password_manager::PasswordStore;
using password_manager::LoginDatabase;

class CredentialProviderServiceTest : public PlatformTest {
 public:
  CredentialProviderServiceTest()
      : chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    [user_defaults removeObjectForKey:
                       kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
  }

  CredentialProviderServiceTest(const CredentialProviderServiceTest&) = delete;
  CredentialProviderServiceTest& operator=(
      const CredentialProviderServiceTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    password_store_ = CreatePasswordStore();
    password_store_->Init(/*prefs=*/nullptr,
                          /*affiliated_match_helper=*/nullptr);

    credential_store_ = [[MemoryCredentialStore alloc] init];

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                              ios::FaviconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        chrome_browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get()));

    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());

    testing_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);

    credential_provider_service_ = std::make_unique<CredentialProviderService>(
        &testing_pref_service_, password_store_, auth_service_,
        credential_store_, nullptr, &sync_service_, &affiliation_service_,
        IOSChromeFaviconLoaderFactory::GetForBrowserState(
            chrome_browser_state_.get()));

    // Fire sync service state changed to simulate sync setup finishing.
    sync_service_.FireStateChanged();
  }

  void TearDown() override {
    credential_provider_service_->Shutdown();
    password_store_->ShutdownOnUIThread();
    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    [user_defaults removeObjectForKey:
                       kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
    PlatformTest::TearDown();
  }

  scoped_refptr<PasswordStore> CreatePasswordStore() {
    return base::MakeRefCounted<PasswordStore>(
        std::make_unique<password_manager::PasswordStoreBuiltInBackend>(
            std::make_unique<LoginDatabase>(
                temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test")),
                password_manager::IsAccountStore(false))));
  }

 protected:
  TestingPrefServiceSimple testing_pref_service_;
  base::ScopedTempDir temp_dir_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IO_MAINLOOP};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  scoped_refptr<PasswordStore> password_store_;
  id<CredentialStore> credential_store_;
  AuthenticationService* auth_service_;
  std::unique_ptr<CredentialProviderService> credential_provider_service_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ChromeAccountManagerService* account_manager_service_;
  syncer::TestSyncService sync_service_;
  FakeAffiliationService affiliation_service_;
};

// Test that CredentialProviderService can be created.
TEST_F(CredentialProviderServiceTest, Create) {
  EXPECT_TRUE(credential_provider_service_);
}

// Test that CredentialProviderService syncs all the credentials the first time
// it runs.
TEST_F(CredentialProviderServiceTest, FirstSync) {
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
    return [user_defaults
        boolForKey:kUserDefaultsCredentialProviderFirstTimeSyncCompleted];
  }));
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, PasswordChanges) {
  EXPECT_EQ(0u, credential_store_.credentials.count);

  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";

  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  ASSERT_EQ(1u, credential_store_.credentials.count);

  NSString* keychainIdentifier =
      credential_store_.credentials.firstObject.keychainIdentifier;
  form.password_value = u"secret";

  password_store_->UpdateLogin(form);
  task_environment_.RunUntilIdle();

  // Expect that the credential in the store now has a different keychain
  // identifier.
  id<Credential> credential = credential_store_.credentials.firstObject;
  EXPECT_NSNE(keychainIdentifier, credential.keychainIdentifier);
  ASSERT_EQ(1u, credential_store_.credentials.count);

  password_store_->RemoveLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be empty.
  ASSERT_EQ(0u, credential_store_.credentials.count);
}

// Test that CredentialProviderService observes changes in the primary identity.
TEST_F(CredentialProviderServiceTest, AccountChange) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";

  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(
      auth_service_->GetPrimaryIdentity(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(credential_store_.credentials.firstObject.validationIdentifier);

  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  identity_service->AddManagedIdentities(@[ @"Name" ]);
  id<SystemIdentity> identity = account_manager_service_->GetDefaultIdentity();
  auth_service_->SignIn(identity);

  ASSERT_TRUE(auth_service_->GetPrimaryIdentity(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      auth_service_->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin));

  CoreAccountInfo account = CoreAccountInfo();
  account.email = base::SysNSStringToUTF8(identity.userEmail);
  account.gaia = base::SysNSStringToUTF8(identity.gaiaID);
  credential_provider_service_->OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent(
          signin::PrimaryAccountChangeEvent::State(
              CoreAccountInfo(), signin::ConsentLevel::kSignin),
          signin::PrimaryAccountChangeEvent::State(
              account, signin::ConsentLevel::kSync)));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return
        [auth_service_->GetPrimaryIdentity(signin::ConsentLevel::kSignin).gaiaID
            isEqualToString:credential_store_.credentials.firstObject
                                .validationIdentifier];
  }));

  auth_service_->SignOut(signin_metrics::SIGNOUT_TEST,
                         /*force_clear_browsing_data=*/false, nil);

  credential_provider_service_->OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent(
          signin::PrimaryAccountChangeEvent::State(account,
                                                   signin::ConsentLevel::kSync),
          signin::PrimaryAccountChangeEvent::State(
              CoreAccountInfo(), signin::ConsentLevel::kSignin)));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !
        [auth_service_->GetPrimaryIdentity(signin::ConsentLevel::kSignin).gaiaID
            isEqualToString:credential_store_.credentials.firstObject
                                .validationIdentifier];
  }));
}

// Test that CredentialProviderService observes changes in the password store.
TEST_F(CredentialProviderServiceTest, AndroidCredential) {
  EXPECT_EQ(0u, credential_store_.credentials.count);

  PasswordForm form;
  form.url = GURL(form.signon_realm);
  form.signon_realm = "android://hash@com.example.my.app";
  form.password_element = u"pwd";
  form.password_value = u"example";

  password_store_->AddLogin(form);
  task_environment_.RunUntilIdle();

  // Expect the store to be populated with 1 credential.
  ASSERT_EQ(1u, credential_store_.credentials.count);
}

// Test that the CredentialProviderService observes changes in the preference
// that controls password creation
TEST_F(CredentialProviderServiceTest, PasswordCreationPreference) {
  // The test is initialized with the preference as true. Make sure the
  // NSUserDefaults value is also true.
  EXPECT_TRUE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()]
      boolValue]);

  // Change the pref value to false.
  testing_pref_service_.SetBoolean(
      password_manager::prefs::kCredentialsEnableService, false);

  // Make sure the NSUserDefaults value is now false.
  EXPECT_FALSE([[app_group::GetGroupUserDefaults()
      objectForKey:
          AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled()]
      boolValue]);
}

// Tests that the CredentialProviderService has the correct stored email based
// on the password sync state.
TEST_F(CredentialProviderServiceTest, PasswordSyncStoredEmail) {
  // Start by signing in and turning sync on.
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  ios::FakeChromeIdentityService* identity_service_ =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  identity_service_->AddIdentity(identity);
  auth_service_->SignIn(identity);
  auth_service_->GrantSyncConsent(identity);
  sync_service_.FireStateChanged();

  EXPECT_NSEQ(
      identity.userEmail,
      [app_group::GetGroupUserDefaults()
          stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()]);

  // Turn off password sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kPasswords);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);

  sync_service_.FireStateChanged();

  EXPECT_FALSE([app_group::GetGroupUserDefaults()
      stringForKey:AppGroupUserDefaultsCredentialProviderUserEmail()]);
}

}  // namespace
