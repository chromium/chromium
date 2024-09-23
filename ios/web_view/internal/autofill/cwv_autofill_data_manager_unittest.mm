// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/public/cwv_autofill_data_manager_observer.h"
#import "ios/web_view/public/cwv_credential_provider_extension_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {
NSString* const kNewName = @"John Doe";
}  // namespace

class CWVAutofillDataManagerTest : public PlatformTest {
 protected:
  CWVAutofillDataManagerTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

    personal_data_manager_ =
        std::make_unique<autofill::TestPersonalDataManager>();

    // Set to stub out behavior inside PersonalDataManager.
    personal_data_manager_->test_address_data_manager()
        .SetAutofillProfileEnabled(true);
    personal_data_manager_->test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);
    personal_data_manager_->test_payments_data_manager()
        .SetAutofillWalletImportEnabled(true);

    password_store_ = new password_manager::TestPasswordStore(
        password_manager::IsAccountStore(true));
    password_store_->Init(/*prefs=*/nullptr,
                          /*affiliated_match_helper=*/nullptr);

    autofill_data_manager_ = [[CWVAutofillDataManager alloc]
        initWithPersonalDataManager:personal_data_manager_.get()
                      passwordStore:password_store_.get()];
  }

  // Fetches profiles from |autofill_data_manager_| and returns them in
  // |completion_handler|. Returns true if fetch was successful.
  [[nodiscard]] bool FetchProfiles(
      void (^completion_handler)(NSArray<CWVAutofillProfile*>* profiles)) {
    __block BOOL fetch_completion_was_called = NO;
    [autofill_data_manager_ fetchProfilesWithCompletionHandler:^(
                                NSArray<CWVAutofillProfile*>* profiles) {
      fetch_completion_was_called = YES;
      completion_handler(profiles);
    }];
    return WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return fetch_completion_was_called;
    });
  }

  // Fetches credit cards from |autofill_data_manager_| and returns them in
  // |completion_handler|. Returns true if fetch was successful.
  [[nodiscard]] bool FetchCreditCards(
      void (^completion_handler)(NSArray<CWVCreditCard*>* credit_cards)) {
    __block BOOL fetch_completion_was_called = NO;
    [autofill_data_manager_ fetchCreditCardsWithCompletionHandler:^(
                                NSArray<CWVCreditCard*>* credit_cards) {
      fetch_completion_was_called = YES;
      completion_handler(credit_cards);
    }];
    return WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return fetch_completion_was_called;
    });
  }

  // Create a test password form for testing.
  password_manager::PasswordForm GetTestPassword() {
    password_manager::PasswordForm password_form;
    password_form.url = GURL("http://www.example.com/accounts/LoginAuth");
    password_form.action = GURL("http://www.example.com/accounts/Login");
    password_form.username_element = base::SysNSStringToUTF16(@"Email");
    password_form.username_value = base::SysNSStringToUTF16(@"test@egmail.com");
    password_form.password_element = base::SysNSStringToUTF16(@"Passwd");
    password_form.password_value = base::SysNSStringToUTF16(@"test");
    password_form.submit_element = base::SysNSStringToUTF16(@"signIn");
    password_form.signon_realm = "http://www.example.com/";
    password_form.scheme = password_manager::PasswordForm::Scheme::kHtml;
    password_form.blocked_by_user = false;
    return password_form;
  }

  // Fetches passwords from |autofill_data_manager_| and returns them.
  [[nodiscard]] NSArray<CWVPassword*>* FetchPasswords() {
    __block NSArray<CWVPassword*>* fetched_passwords = nil;
    [autofill_data_manager_ fetchPasswordsWithCompletionHandler:^(
                                NSArray<CWVPassword*>* passwords) {
      fetched_passwords = passwords;
    }];
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
      base::RunLoop().RunUntilIdle();
      return fetched_passwords != nil;
    }));
    return fetched_passwords;
  }

  ~CWVAutofillDataManagerTest() override {
    password_store_->ShutdownOnUIThread();
    ui::ResourceBundle::CleanupSharedInstance();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  scoped_refptr<password_manager::TestPasswordStore> password_store_;
  CWVAutofillDataManager* autofill_data_manager_;
};

// Tests CWVAutofillDataManager properly invokes did change callback.
TEST_F(CWVAutofillDataManagerTest, DidChangeCallback) {
  // OCMock objects are often autoreleased, but it must be destroyed before this
  // test exits to avoid holding on to |autofill_data_manager_|.
  @autoreleasepool {
    id observer = OCMProtocolMock(@protocol(CWVAutofillDataManagerObserver));

    [autofill_data_manager_ addObserver:observer];
    [[observer expect] autofillDataManagerDataDidChange:autofill_data_manager_];
    personal_data_manager_->address_data_manager().AddProfile(
        autofill::test::GetFullProfile());
    [observer verify];

    [autofill_data_manager_ removeObserver:observer];
    [[observer reject] autofillDataManagerDataDidChange:autofill_data_manager_];
    personal_data_manager_->address_data_manager().AddProfile(
        autofill::test::GetFullProfile2());
    [observer verify];
  }
}

// Tests CWVAutofillDataManager properly returns profiles.
TEST_F(CWVAutofillDataManagerTest, ReturnProfile) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  personal_data_manager_->address_data_manager().AddProfile(profile);

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    EXPECT_EQ(1ul, profiles.count);

    CWVAutofillProfile* cwv_profile = profiles.firstObject;
    EXPECT_EQ(profile, *cwv_profile.internalProfile);
  }));
}

// Tests CWVAutofillDataManager properly deletes profiles.
TEST_F(CWVAutofillDataManagerTest, DeleteProfile) {
  personal_data_manager_->address_data_manager().AddProfile(
      autofill::test::GetFullProfile());

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    for (CWVAutofillProfile* cwv_profile in profiles) {
      [autofill_data_manager_ deleteProfile:cwv_profile];
    }
  }));
  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    EXPECT_EQ(0ul, profiles.count);
  }));
}

// Tests CWVAutofillDataManager properly updates profiles.
TEST_F(CWVAutofillDataManagerTest, UpdateProfile) {
  personal_data_manager_->address_data_manager().AddProfile(
      autofill::test::GetFullProfile());

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    CWVAutofillProfile* cwv_profile = profiles.firstObject;
    cwv_profile.name = kNewName;
    [autofill_data_manager_ updateProfile:cwv_profile];
  }));

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    EXPECT_NSEQ(kNewName, profiles.firstObject.name);
  }));
}

// Tests CWVAutofillDataManager properly returns credit cards.
TEST_F(CWVAutofillDataManagerTest, ReturnCreditCard) {
  autofill::CreditCard credit_card = autofill::test::GetCreditCard();
  personal_data_manager_->payments_data_manager().AddCreditCard(credit_card);

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    EXPECT_EQ(1ul, credit_cards.count);

    CWVCreditCard* cwv_credit_card = credit_cards.firstObject;
    EXPECT_EQ(credit_card, *cwv_credit_card.internalCard);
  }));
}

// Tests CWVAutofillDataManager properly returns passwords.
TEST_F(CWVAutofillDataManagerTest, ReturnPassword) {
  password_manager::PasswordForm test_password = GetTestPassword();
  password_store_->AddLogin(test_password);
  NSArray<CWVPassword*>* fetched_passwords = FetchPasswords();
  EXPECT_EQ(1ul, fetched_passwords.count);
  EXPECT_THAT(test_password, password_manager::MatchesFormExceptStore(
                                 *[fetched_passwords[0] internalPasswordForm]));
}

// Tests CWVAutofillDataManager no ops when nil is passed to updatePassword.
TEST_F(CWVAutofillDataManagerTest, UpdatePasswordNilArguments) {
  password_store_->AddLogin(GetTestPassword());

  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  CWVPassword* old_password = passwords.firstObject;

  [autofill_data_manager_ updatePassword:old_password
                             newUsername:nil
                             newPassword:nil];

  passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  CWVPassword* new_password = passwords.firstObject;
  EXPECT_NSEQ(old_password.username, new_password.username);
  EXPECT_NSEQ(old_password.password, new_password.password);
}

// Tests CWVAutofillDataManager properly updates just the username.
TEST_F(CWVAutofillDataManagerTest, UpdateUsernameOnly) {
  password_store_->AddLogin(GetTestPassword());

  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  CWVPassword* password = passwords.firstObject;
  NSString* old_password_value = password.password;
  EXPECT_NSNE(@"new-username", password.username);

  [autofill_data_manager_ updatePassword:password
                             newUsername:@"new-username"
                             newPassword:nil];
  EXPECT_NSEQ(@"new-username", password.username);
  EXPECT_NSEQ(old_password_value, password.password);

  passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  password = passwords.firstObject;
  EXPECT_NSEQ(@"new-username", password.username);
  EXPECT_NSEQ(old_password_value, password.password);
}

// Tests CWVAutofillDataManager properly updates just the password.
TEST_F(CWVAutofillDataManagerTest, UpdatePasswordOnly) {
  password_store_->AddLogin(GetTestPassword());

  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  CWVPassword* password = passwords.firstObject;
  NSString* old_username_value = password.username;
  EXPECT_NSNE(@"new-password", password.password);

  [autofill_data_manager_ updatePassword:password
                             newUsername:nil
                             newPassword:@"new-password"];
  EXPECT_NSEQ(old_username_value, password.username);
  EXPECT_NSEQ(@"new-password", password.password);

  passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  password = passwords.firstObject;
  EXPECT_NSEQ(old_username_value, password.username);
  EXPECT_NSEQ(@"new-password", password.password);
}

// Tests CWVAutofillDataManager properly updates both the username and password.
TEST_F(CWVAutofillDataManagerTest, UpdateUsernameAndPassword) {
  password_store_->AddLogin(GetTestPassword());

  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  CWVPassword* password = passwords.firstObject;
  EXPECT_NSNE(@"new-username", password.username);
  EXPECT_NSNE(@"new-password", password.password);

  [autofill_data_manager_ updatePassword:password
                             newUsername:@"new-username"
                             newPassword:@"new-password"];
  EXPECT_NSEQ(@"new-username", password.username);
  EXPECT_NSEQ(@"new-password", password.password);

  passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  password = passwords.firstObject;
  EXPECT_NSEQ(@"new-username", password.username);
  EXPECT_NSEQ(@"new-password", password.password);
}

// Tests CWVAutofillDataManager properly deletes passwords.
TEST_F(CWVAutofillDataManagerTest, DeletePassword) {
  password_store_->AddLogin(GetTestPassword());
  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  [autofill_data_manager_ deletePassword:passwords[0]];
  passwords = FetchPasswords();
  EXPECT_EQ(0ul, passwords.count);
}

// Tests CWVAutofillDataManager properly adds new passwords.
TEST_F(CWVAutofillDataManagerTest, AddNewPassword) {
  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(0ul, passwords.count);

  [autofill_data_manager_
      addNewPasswordForUsername:@"new-username"
                       password:@"new-password"
                           site:@"https://www.chromium.org/"];
  passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);

  CWVPassword* password = passwords.firstObject;
  EXPECT_NSEQ(@"new-username", password.username);
  EXPECT_NSEQ(@"new-password", password.password);
  EXPECT_NSEQ(@"https://www.chromium.org/", password.site);
}

// Tests CWVAutofillDataManager properly handles conflicts when adding passwords
// whose primary key already exists.
TEST_F(CWVAutofillDataManagerTest, AddNewPasswordWithConflictingPrimaryKey) {
  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(0ul, passwords.count);

  [autofill_data_manager_
      addNewPasswordForUsername:@"some-username"
                       password:@"some-password"
                           site:@"https://www.chromium.org/"];
  [autofill_data_manager_
      addNewPasswordForUsername:@"some-username"
                       password:@"different-password"
                           site:@"https://www.chromium.org/"];
  passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);

  CWVPassword* password = passwords.firstObject;
  EXPECT_NSEQ(@"some-username", password.username);
  EXPECT_NSEQ(@"different-password", password.password);
  EXPECT_NSEQ(@"https://www.chromium.org/", password.site);
}

// Tests CWVAutofillDataManager invokes password did change callback.
TEST_F(CWVAutofillDataManagerTest, PasswordsDidChangeCallback) {
  // OCMock objects are often autoreleased, but it must be destroyed before this
  // test exits to avoid holding on to |autofill_data_manager_|.
  @autoreleasepool {
    id observer = OCMProtocolMock(@protocol(CWVAutofillDataManagerObserver));
    [autofill_data_manager_ addObserver:observer];

    password_manager::PasswordForm test_password = GetTestPassword();
    [[observer expect]
               autofillDataManager:autofill_data_manager_
        didChangePasswordsByAdding:[OCMArg checkWithBlock:^BOOL(
                                               NSArray<CWVPassword*>* added) {
          EXPECT_EQ(1U, added.count);
          CWVPassword* added_password = added.firstObject;
          return *[added_password internalPasswordForm] == test_password;
        }]
                          updating:@[]
                          removing:@[]];

    // AddLogin is async, so the run loop needs to run until idle so the
    // callback will be invoked.
    password_store_->AddLogin(test_password);
    base::RunLoop().RunUntilIdle();

    [observer verify];
  }
}

// Tests CWVAutofillDataManager can add a new password created from the
// credential provider extension.
TEST_F(CWVAutofillDataManagerTest,
       AddNewPasswordFromCredentialProviderExtension) {
  NSString* keychain_identifier = @"keychain-identifier";
  [CWVCredentialProviderExtensionUtils
      storePasswordForKeychainIdentifier:keychain_identifier
                                password:@"testpassword"];
  [autofill_data_manager_ addNewPasswordForUsername:@"testusername"
                                  serviceIdentifier:@"https://www.chromium.org/"
                                 keychainIdentifier:keychain_identifier];

  NSArray<CWVPassword*>* passwords = FetchPasswords();
  ASSERT_EQ(1ul, passwords.count);
  CWVPassword* password = passwords.firstObject;
  EXPECT_NSEQ(@"testusername", password.username);

  // The following expectation fails because the TestPasswordStore does not
  // use the LoginDatabase underneath. A LoginDatabase will properly decrypt
  // the password from the keychain identifier and fill it out.
  // EXPECT_NSEQ(@"testpassword", password.password);

  EXPECT_NSEQ(@"https://www.chromium.org/", password.site);
  EXPECT_NSEQ(keychain_identifier, password.keychainIdentifier);
}

}  // namespace ios_web_view
