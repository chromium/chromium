// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/affiliations/core/browser/fake_affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator+Testing.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

constexpr char kExampleSignonRealm[] = "https://www.example.com/";
constexpr char kExampleURL1[] = "https://www.example.com/1";
constexpr char kExampleURL2[] = "https://www.example.com/2";
constexpr char16_t kExamplePassword1[] = u"password1";
constexpr char16_t kExamplePassword2[] = u"password2";

// Creates a password form.
std::unique_ptr<PasswordForm> CreatePasswordForm(std::string url,
                                                 std::u16string password) {
  auto form = std::make_unique<PasswordForm>();
  form->username_value = u"test@gmail.com";
  form->password_value = password;
  form->url = GURL(url);
  form->signon_realm = kExampleSignonRealm;
  form->in_store = PasswordForm::Store::kProfileStore;
  return form;
}

scoped_refptr<RefcountedKeyedService> BuildPasswordStore(
    password_manager::IsAccountStore is_account_store,
    web::BrowserState* context) {
  auto store = base::MakeRefCounted<password_manager::TestPasswordStore>(
      is_account_store);
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  return store;
}

}  // namespace

// Test class that conforms to PasswordDetailsConsumer in order to test the
// consumer methods are called correctly.
@interface FakePasswordDetailsConsumer : NSObject <PasswordDetailsConsumer>

@property(nonatomic, strong) NSArray<CredentialDetails*>* credentials;

@property(nonatomic, copy) NSString* title;

@end

@implementation FakePasswordDetailsConsumer

- (void)setCredentials:(NSArray<CredentialDetails*>*)credentials
              andTitle:(NSString*)title {
  _credentials = credentials;
  _title = title;
}

- (void)setIsBlockedSite:(BOOL)isBlockedSite {
}

- (void)setUserEmail:(NSString*)userEmail {
}

- (void)setupRightShareButton:(BOOL)policyEnabled {
}

@end

// Test fixture for testing PasswordDetailsMediator class.
class PasswordDetailsMediatorTest : public PlatformTest {
 protected:
  PasswordDetailsMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&BuildPasswordStore,
                            password_manager::IsAccountStore(false)));

    builder.AddTestingFactory(
        IOSChromeAccountPasswordStoreFactory::GetInstance(),
        base::BindRepeating(&BuildPasswordStore,
                            password_manager::IsAccountStore(true)));

    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<affiliations::FakeAffiliationService>());
        })));

    profile_ = std::move(builder).Build();

    password_check_manager_ =
        IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());

    consumer_ = [[FakePasswordDetailsConsumer alloc] init];

    AddSafePasswordForm(/*url=*/kExampleURL1, /*password=*/kExamplePassword1);

    UpdateAffiliatedGroup();

    display_name_ = [NSString
        stringWithUTF8String:affiliated_group().GetDisplayName().c_str()];

    mediator_ = [[PasswordDetailsMediator alloc]
        initWithPasswords:GetAffiliatedGroupCredentials()
              displayName:display_name()
                  profile:profile_.get()
                  context:DetailsContext::kPasswordSettings
                 delegate:nil];
    mediator_.consumer = consumer_;
  }

  void TearDown() override { [mediator() disconnect]; }

  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager() {
    return password_check_manager_.get();
  }

  PasswordDetailsMediator* mediator() { return mediator_; }

  ProfileIOS* profile() { return profile_.get(); }

  FakePasswordDetailsConsumer* consumer() { return consumer_; }

  password_manager::AffiliatedGroup affiliated_group() {
    return affiliated_group_;
  }

  NSString* display_name() { return display_name_; }

  // Returns the profile password store.
  TestPasswordStore& GetTestProfileStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  // Returns the account password store.
  TestPasswordStore& GetTestAccountStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromeAccountPasswordStoreFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  // Returns a vector of the credentials belonging to the `affiliated_group_`.
  std::vector<CredentialUIEntry> GetAffiliatedGroupCredentials() {
    UpdateAffiliatedGroup();
    std::vector<CredentialUIEntry> credentials;
    for (const auto& credential : affiliated_group_.GetCredentials()) {
      credentials.push_back(credential);
    }
    return credentials;
  }

  // Creates and adds a safe password form.
  void AddSafePasswordForm(std::string url, std::u16string password) {
    auto form = CreatePasswordForm(url, password);
    AddPasswordForm(std::move(form));
  }

  // Creates and adds an insecure password form.
  void AddInsecurePasswordForm(std::string url,
                               std::u16string password,
                               InsecureType insecure_type,
                               bool is_muted = false) {
    auto form = CreatePasswordForm(url, password);
    form->password_issues = {
        {insecure_type,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(is_muted),
             password_manager::TriggerBackendNotification(false))}};
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a safe password form to the account password store.
  void AddSafePasswordFormToAccountStore(std::string url,
                                         std::u16string password) {
    auto form = CreatePasswordForm(url, password);
    form->in_store = PasswordForm::Store::kAccountStore;
    AddPasswordForm(std::move(form), /*add_to_account_store=*/true);
  }

  // Verifies that the passwords sent by the mediator to the consumer are as
  // expected.
  void CheckConsumerPasswords(NSArray<CredentialDetails*>* expected_passwords) {
    EXPECT_EQ(consumer().credentials.count, expected_passwords.count);
    for (NSUInteger i = 0; i < consumer().credentials.count; i++) {
      CheckPasswordDetails(consumer().credentials[i], expected_passwords[i]);
    }
  }

  // Verifies that the title sent by the mediator to the consumer is as
  // expected.
  void CheckConsumerTitle() { EXPECT_NSEQ(consumer().title, display_name()); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  // Adds a password form to a test password store.
  void AddPasswordForm(std::unique_ptr<PasswordForm> form,
                       bool add_to_account_store = false) {
    if (add_to_account_store) {
      GetTestAccountStore().AddLogin(*form);
    } else {
      GetTestProfileStore().AddLogin(*form);
    }
    RunUntilIdle();
  }

  // Updates the properties of the affiliated group.
  void UpdateAffiliatedGroup() {
    affiliated_group_ = password_check_manager()
                            ->GetSavedPasswordsPresenter()
                            ->GetAffiliatedGroups()[0];
  }

  // Checks if the provided password details are equal.
  void CheckPasswordDetails(CredentialDetails* password1,
                            CredentialDetails* password2) {
    EXPECT_EQ(password1.credentialType, password2.credentialType);
    EXPECT_NSEQ(password1.signonRealm, password2.signonRealm);
    EXPECT_NSEQ(password1.origins, password2.origins);
    EXPECT_NSEQ(password1.websites, password2.websites);
    EXPECT_NSEQ(password1.username, password2.username);
    EXPECT_NSEQ(password1.federation, password2.federation);
    EXPECT_NSEQ(password1.password, password2.password);
    EXPECT_EQ(password1.note, password2.note);
    EXPECT_EQ(password1.compromised, password2.compromised);
    EXPECT_EQ(password1.muted, password2.muted);
    EXPECT_EQ(password1.changePasswordURL, password2.changePasswordURL);
    EXPECT_EQ(password1.shouldOfferToMoveToAccount,
              password2.shouldOfferToMoveToAccount);
    EXPECT_EQ(password1.context, password2.context);
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list;

  std::unique_ptr<TestProfileIOS> profile_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;
  FakePasswordDetailsConsumer* consumer_;
  PasswordDetailsMediator* mediator_;
  password_manager::AffiliatedGroup affiliated_group_;

  NSString* display_name_;
};

// Tests that the consumer is correctly notified when the saved insecure
// passwords changed.
TEST_F(PasswordDetailsMediatorTest, NotifiesConsumerOnInsecurePasswordChange) {
  // Add a compromised flag to the saved password. Adding a credential with the
  // same url and password as one that is already in the store will update the
  // one that's already in the store.
  AddInsecurePasswordForm(/*url=*/kExampleURL1, /*password*/ kExamplePassword1,
                          /*insecure_type=*/InsecureType::kLeaked);

  CredentialDetails* expected_password_details = [[CredentialDetails alloc]
      initWithCredential:GetAffiliatedGroupCredentials()[0]];
  expected_password_details.compromised = YES;

  // Verify information sent to consumer.
  CheckConsumerPasswords(/*expected_passwords=*/@[ expected_password_details ]);
  CheckConsumerTitle();
}

// Tests that `removeCredential` deletes the credential both from the mediator's
// list and that of SavedPasswordsPresenter. Also tests that the consumer is
// notified with the expected information following the password removal.
TEST_F(PasswordDetailsMediatorTest, RemoveCredential) {
  // Add a second password.
  AddSafePasswordForm(/*url=*/kExampleURL2, /*password=*/kExamplePassword2);

  // Update the mediator's credentials.
  std::vector<CredentialUIEntry> credentials = GetAffiliatedGroupCredentials();
  mediator().credentials = credentials;

  // Check that the credential with password "password2" is present in the
  // mediator's and SavedPasswordsPresenter's credentials.
  EXPECT_TRUE(std::any_of(mediator().credentials.begin(),
                          mediator().credentials.end(),
                          [](CredentialUIEntry credential) {
                            return credential.password == kExamplePassword2;
                          }));
  EXPECT_TRUE(std::any_of(credentials.begin(), credentials.end(),
                          [](CredentialUIEntry credential) {
                            return credential.password == kExamplePassword2;
                          }));

  // Remove credential with password "password2".
  CredentialDetails* credential_details =
      [[CredentialDetails alloc] initWithCredential:credentials[1]];
  [mediator() removeCredential:credential_details];
  RunUntilIdle();

  // Check that there is only one password left in the mediator's credential
  // list and that the remaining password is not with password "password2".
  ASSERT_EQ(mediator().credentials.size(), 1u);
  EXPECT_NE(mediator().credentials[0].password, kExamplePassword2);

  // Fetch the updated credentials from the SavedPasswordPresenter and verify
  // that the removed password is not in that list either.
  credentials = GetAffiliatedGroupCredentials();
  EXPECT_FALSE(std::any_of(credentials.begin(), credentials.end(),
                           [](CredentialUIEntry credential) {
                             return credential.password == kExamplePassword2;
                           }));

  CredentialDetails* remaining_password_details =
      [[CredentialDetails alloc] initWithCredential:credentials[0]];

  // Verify information sent to consumer.
  CheckConsumerPasswords(
      /*expected_passwords=*/@[ remaining_password_details ]);
  CheckConsumerTitle();
}

// Tests that `moveCredentialToAccountStore` moves the credential from the
// profile password store to the account password store. Also tests that the
// consumer is notified with the expected information after moving the password.
TEST_F(PasswordDetailsMediatorTest, MoveCredentialToAccountStore) {
  // Get the password form associated with the saved credential.
  PasswordForm expected_form =
      password_check_manager()
          ->GetSavedPasswordsPresenter()
          ->GetCorrespondingPasswordForms(mediator().credentials[0])[0];

  // Verify that the credential is in the profile password store and that the
  // account password store is empty.
  EXPECT_EQ(*mediator().credentials[0].stored_in.begin(),
            PasswordForm::Store::kProfileStore);
  EXPECT_THAT(
      GetTestProfileStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(expected_form))));
  EXPECT_THAT(GetTestAccountStore().stored_passwords(), IsEmpty());

  // Move the credential to the account password store.
  CredentialDetails* credential_details = [[CredentialDetails alloc]
      initWithCredential:GetAffiliatedGroupCredentials()[0]];
  [mediator() moveCredentialToAccountStore:credential_details];
  RunUntilIdle();

  expected_form.in_store = PasswordForm::Store::kAccountStore;

  // Verify that the credential is now stored in the account password store.
  EXPECT_EQ(*mediator().credentials[0].stored_in.begin(),
            PasswordForm::Store::kAccountStore);
  EXPECT_THAT(GetTestProfileStore().stored_passwords(), IsEmpty());
  EXPECT_THAT(
      GetTestAccountStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(expected_form))));

  // Verify information sent to consumer.
  CheckConsumerPasswords(/*expected_passwords=*/@[ credential_details ]);
  CheckConsumerTitle();
}

// Tests that `moveCredentialToAccountStoreWithConflict` moves the credential
// from the the profile password store to the account password store and keep
// the most recent version of the credential when there is a conflict. Also
// tests that the consumer is notified with the expected information after
// moving the password.
TEST_F(PasswordDetailsMediatorTest, MoveCredentialToAccountStoreWithConflict) {
  // Move the saved credential to the account password store.
  AddSafePasswordFormToAccountStore(/*url=*/kExampleURL1,
                                    /*password=*/kExamplePassword1);

  // Add a second credential that is saved in the profile password store. Use
  // same url and username, but different password as the other credential so
  // there is a conflict when moving the second one to the account store.
  AddSafePasswordForm(/*url=*/kExampleURL1, /*password=*/kExamplePassword2);

  // Update the mediator's credentials.
  std::vector<CredentialUIEntry> credentials = GetAffiliatedGroupCredentials();
  mediator().credentials = credentials;

  PasswordForm account_store_form =
      password_check_manager()
          ->GetSavedPasswordsPresenter()
          ->GetCorrespondingPasswordForms(mediator().credentials[0])[0];

  PasswordForm profile_store_form =
      password_check_manager()
          ->GetSavedPasswordsPresenter()
          ->GetCorrespondingPasswordForms(mediator().credentials[1])[0];

  // Check that the mediator's credentials are associated with the expected
  // store.
  EXPECT_EQ(mediator().credentials.size(), 2u);
  EXPECT_EQ(*mediator().credentials[0].stored_in.begin(),
            PasswordForm::Store::kAccountStore);
  EXPECT_EQ(*mediator().credentials[1].stored_in.begin(),
            PasswordForm::Store::kProfileStore);

  // Check that the both stores both contain the expected password.
  EXPECT_THAT(
      GetTestAccountStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(account_store_form))));
  EXPECT_THAT(
      GetTestProfileStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(profile_store_form))));

  // Move the profile credential to the account password store.
  CredentialDetails* credential_details =
      [[CredentialDetails alloc] initWithCredential:mediator().credentials[1]];
  [mediator() moveCredentialToAccountStoreWithConflict:credential_details];
  RunUntilIdle();

  // The profile credential should now be in the account store.
  PasswordForm expected_form = profile_store_form;
  expected_form.in_store = PasswordForm::Store::kAccountStore;

  // Check that the mediator only has one credential left, that it is saved in
  // the account store and and its password is "password2".
  EXPECT_EQ(mediator().credentials.size(), 1u);
  EXPECT_EQ(*mediator().credentials[0].stored_in.begin(),
            PasswordForm::Store::kAccountStore);
  EXPECT_EQ(mediator().credentials[0].password, kExamplePassword2);

  // Check that the profile password store is now empty and that the account
  // store only has the updated version (i.e., version with password
  // "password2") of the credential previously saved.
  EXPECT_THAT(GetTestProfileStore().stored_passwords(), IsEmpty());
  EXPECT_THAT(
      GetTestAccountStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(expected_form))));

  // Verify information sent to consumer.
  CheckConsumerPasswords(/*expected_passwords=*/@[ credential_details ]);
  CheckConsumerTitle();
}

// Tests that `moveCredentialToAccountStoreWithConflict` moves the credential
// from the the profile password store to the account password store and keep
// the most recent version of the credential when there is a conflict.
TEST_F(PasswordDetailsMediatorTest,
       MoveCredentialToAccountStoreWithConflictKeepMostRecent) {
  // Add a second credential that is saved in the profile password store. Use
  // same url and username, but different password as the other credential so
  // there is a conflict when moving the second one to the account store.
  AddSafePasswordForm(/*url=*/kExampleURL1, /*password=*/kExamplePassword1);

  // Add saved credential to the account password store.
  AddSafePasswordFormToAccountStore(/*url=*/kExampleURL1,
                                    /*password=*/kExamplePassword2);

  // Update the mediator's credentials.
  std::vector<CredentialUIEntry> credentials = GetAffiliatedGroupCredentials();
  // Update the last used time of the credentials.
  credentials[0].last_used_time = base::Time::Now();
  credentials[1].last_used_time = base::Time::Now() + base::Hours(1);
  mediator().credentials = credentials;

  PasswordForm profile_store_form =
      password_check_manager()
          ->GetSavedPasswordsPresenter()
          ->GetCorrespondingPasswordForms(mediator().credentials[0])[0];

  PasswordForm account_store_form =
      password_check_manager()
          ->GetSavedPasswordsPresenter()
          ->GetCorrespondingPasswordForms(mediator().credentials[1])[0];

  // Check that the mediator's credentials are associated with the expected
  // store.
  EXPECT_EQ(mediator().credentials.size(), 2u);
  EXPECT_EQ(*mediator().credentials[0].stored_in.begin(),
            PasswordForm::Store::kProfileStore);
  EXPECT_EQ(*mediator().credentials[1].stored_in.begin(),
            PasswordForm::Store::kAccountStore);

  // Check that the both stores both contain the expected password.
  EXPECT_THAT(
      GetTestProfileStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(profile_store_form))));
  EXPECT_THAT(
      GetTestAccountStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(account_store_form))));

  // Move the profile credential to the account password store to resolve the
  // conflict.
  [mediator() moveCredentialToAccountStoreWithConflict:
                  [[CredentialDetails alloc]
                      initWithCredential:mediator().credentials[0]]];
  RunUntilIdle();

  // The account credential only should be in the account store.
  PasswordForm expected_form = account_store_form;
  expected_form.in_store = PasswordForm::Store::kAccountStore;

  // Check that the mediator only has one credential left, that it is saved in
  // the account store and and its password is "password2" that is the most
  // recent.
  EXPECT_EQ(mediator().credentials.size(), 1u);
  EXPECT_EQ(*mediator().credentials[0].stored_in.begin(),
            PasswordForm::Store::kAccountStore);
  EXPECT_EQ(mediator().credentials[0].password, kExamplePassword2);

  // Check that the profile password store is now empty and that the account
  // store only has the updated version (i.e., version with password
  // "password2") of the credential previously saved.
  EXPECT_THAT(GetTestProfileStore().stored_passwords(), IsEmpty());
  EXPECT_THAT(
      GetTestAccountStore().stored_passwords(),
      ElementsAre(Pair(kExampleSignonRealm, ElementsAre(expected_form))));

  // Verify information sent to consumer.
  CheckConsumerPasswords(/*expected_passwords=*/@[ [[CredentialDetails alloc]
      initWithCredential:mediator().credentials[0]] ]);
  CheckConsumerTitle();
}

// Tests that a password edit is reflected in both the mediator's and
// SavedPasswordsPresenter's credentials. Also tests that the consumer is
// notified with the expected information following the edit.
TEST_F(PasswordDetailsMediatorTest, EditCredential) {
  CredentialDetails* old_password_details =
      [[CredentialDetails alloc] initWithCredential:mediator().credentials[0]];
  CredentialDetails* new_password_details =
      [[CredentialDetails alloc] initWithCredential:mediator().credentials[0]];
  new_password_details.password = @"new password";

  // Edit the credential. Change password from "password1" to "new password".
  [mediator() passwordDetailsViewController:nullptr
                   didEditCredentialDetails:new_password_details
                            withOldUsername:old_password_details.username
                         oldUserDisplayName:old_password_details.userDisplayName
                                oldPassword:old_password_details.password
                                    oldNote:old_password_details.note];
  RunUntilIdle();

  // Check that the credential found in the mediator now has password "new
  // password".
  EXPECT_EQ(mediator().credentials[0].password, u"new password");

  // Fetch the updated credentials from the SavedPasswordPresenter and check
  // that the SavedPasswordCredential has a credential with password "new
  // password" and none with password "password1".
  std::vector<CredentialUIEntry> credentials = GetAffiliatedGroupCredentials();
  EXPECT_TRUE(std::any_of(credentials.begin(), credentials.end(),
                          [](CredentialUIEntry credential) {
                            return credential.password == u"new password";
                          }));
  EXPECT_FALSE(std::any_of(credentials.begin(), credentials.end(),
                           [](CredentialUIEntry credential) {
                             return credential.password == kExamplePassword1;
                           }));

  // Verify information sent to consumer.
  CheckConsumerPasswords(/*expected_passwords=*/@[ new_password_details ]);
  CheckConsumerTitle();
}

// Tests that `didConfirmWarningDismissalForPassword` mutes the credential in
// the SavedPasswordsPresenter's credential list. Also tests that the consumer
// is notified with the expected information.
TEST_F(PasswordDetailsMediatorTest, DidConfirmWarningDismissalForPassword) {
  // Add an unmuted compromsied password to the store.
  AddInsecurePasswordForm(/*url=*/kExampleURL2,
                          /*password*/ kExamplePassword2,
                          /*insecure_type=*/InsecureType::kLeaked);

  // Update the mediator's credentials.
  std::vector<CredentialUIEntry> credentials = GetAffiliatedGroupCredentials();
  mediator().credentials = credentials;

  EXPECT_FALSE(credentials[0].IsMuted());
  EXPECT_FALSE(credentials[1].IsMuted());

  CredentialDetails* password_details1 =
      [[CredentialDetails alloc] initWithCredential:credentials[0]];
  CredentialDetails* password_details2 =
      [[CredentialDetails alloc] initWithCredential:credentials[1]];

  // Mute the credential.
  [mediator() didConfirmWarningDismissalForPassword:password_details2];
  RunUntilIdle();

  // Fetch the updated credentials from the SavedPasswordPresenter.
  credentials = GetAffiliatedGroupCredentials();

  // Verify that only the second credential is now muted.
  EXPECT_FALSE(credentials[0].IsMuted());
  EXPECT_TRUE(credentials[1].IsMuted());

  // Muting a password in the kPasswordSettings context removes the
  // `compromised` flag of the CredentialDetails object.
  password_details2.compromised = NO;

  // Verify information sent to consumer.
  CheckConsumerPasswords(
      /*expected_passwords=*/@[ password_details1, password_details2 ]);
  CheckConsumerTitle();
}

// Tests that `restoreWarningForCurrentPassword` unmutes the credential in the
// SavedPasswordsPresenter's credential list. Also tests that the consumer is
// notified with the expected information.
TEST_F(PasswordDetailsMediatorTest, RestoreWarningForCurrentPassword) {
  // Warning restauration is only available in the `kDismissedWarnings`
  // context.
  mediator().context = DetailsContext::kDismissedWarnings;

  // Add the muted and compromised flags to the saved password.
  AddInsecurePasswordForm(/*url=*/kExampleURL1,
                          /*password*/ kExamplePassword1,
                          /*insecure_type=*/InsecureType::kLeaked,
                          /*is_muted=*/true);

  // Update the mediator's credentials.
  std::vector<CredentialUIEntry> credentials = GetAffiliatedGroupCredentials();
  mediator().credentials = credentials;

  EXPECT_TRUE(credentials[0].IsMuted());

  // Restore the warning
  [mediator() restoreWarningForCurrentPassword];
  RunUntilIdle();

  // Fetch the updated credentials from the SavedPasswordPresenter.
  credentials = GetAffiliatedGroupCredentials();

  EXPECT_FALSE(credentials[0].IsMuted());

  // The credential list should now be empty since restoring a compromised
  // warning removes the credential from the list.
  EXPECT_EQ(consumer().credentials.count, 0u);

  // Verify information sent to consumer.
  CheckConsumerPasswords(/*expected_passwords=*/@[]);
  CheckConsumerTitle();
}
