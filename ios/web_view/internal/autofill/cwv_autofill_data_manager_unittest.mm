// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/public/cwv_autofill_data_manager_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    personal_data_manager_->SetAutofillProfileEnabled(true);
    personal_data_manager_->SetAutofillCreditCardEnabled(true);
    personal_data_manager_->SetAutofillWalletImportEnabled(true);

    password_store_ = new password_manager::TestPasswordStore();
    password_store_->Init(base::RepeatingCallback<void(syncer::ModelType)>(),
                          nullptr);

    autofill_data_manager_ = [[CWVAutofillDataManager alloc]
        initWithPersonalDataManager:personal_data_manager_.get()
                      passwordStore:password_store_.get()];
  }

  // Fetches profiles from |autofill_data_manager_| and returns them in
  // |completion_handler|. Returns true if fetch was successful.
  bool FetchProfiles(void (^completion_handler)(
      NSArray<CWVAutofillProfile*>* profiles)) WARN_UNUSED_RESULT {
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
  bool FetchCreditCards(void (^completion_handler)(
      NSArray<CWVCreditCard*>* credit_cards)) WARN_UNUSED_RESULT {
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
  autofill::PasswordForm GetTestPassword() {
    autofill::PasswordForm password_form;
    password_form.origin = GURL("http://www.example.com/accounts/LoginAuth");
    password_form.action = GURL("http://www.example.com/accounts/Login");
    password_form.username_element = base::SysNSStringToUTF16(@"Email");
    password_form.username_value = base::SysNSStringToUTF16(@"test@egmail.com");
    password_form.password_element = base::SysNSStringToUTF16(@"Passwd");
    password_form.password_value = base::SysNSStringToUTF16(@"test");
    password_form.submit_element = base::SysNSStringToUTF16(@"signIn");
    password_form.signon_realm = "http://www.example.com/";
    password_form.preferred = false;
    password_form.scheme = autofill::PasswordForm::Scheme::kHtml;
    password_form.blacklisted_by_user = false;
    return password_form;
  }

  // Fetches passwords from |autofill_data_manager_| and returns them.
  NSArray<CWVPassword*>* FetchPasswords() WARN_UNUSED_RESULT {
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
    personal_data_manager_->AddProfile(autofill::test::GetFullProfile());
    [observer verify];

    [autofill_data_manager_ removeObserver:observer];
    [[observer reject] autofillDataManagerDataDidChange:autofill_data_manager_];
    personal_data_manager_->AddProfile(autofill::test::GetFullProfile2());
    [observer verify];
  }
}

// Tests CWVAutofillDataManager properly returns profiles.
TEST_F(CWVAutofillDataManagerTest, ReturnProfile) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  personal_data_manager_->AddProfile(profile);

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    EXPECT_EQ(1ul, profiles.count);

    CWVAutofillProfile* cwv_profile = profiles.firstObject;
    EXPECT_EQ(profile, *cwv_profile.internalProfile);
  }));
}

// Tests CWVAutofillDataManager properly deletes profiles.
TEST_F(CWVAutofillDataManagerTest, DeleteProfile) {
  personal_data_manager_->AddProfile(autofill::test::GetFullProfile());

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
  personal_data_manager_->AddProfile(autofill::test::GetFullProfile());

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
  personal_data_manager_->AddCreditCard(credit_card);

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    EXPECT_EQ(1ul, credit_cards.count);

    CWVCreditCard* cwv_credit_card = credit_cards.firstObject;
    EXPECT_EQ(credit_card, *cwv_credit_card.internalCard);
  }));
}

// Tests CWVAutofillDataManager properly deletes credit cards.
TEST_F(CWVAutofillDataManagerTest, DeleteCreditCard) {
  personal_data_manager_->AddCreditCard(autofill::test::GetCreditCard());

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    for (CWVCreditCard* cwv_credit_card in credit_cards) {
      [autofill_data_manager_ deleteCreditCard:cwv_credit_card];
    }
  }));
  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    EXPECT_EQ(0ul, credit_cards.count);
  }));
}

// Tests CWVAutofillDataManager properly updates credit cards.
TEST_F(CWVAutofillDataManagerTest, UpdateCreditCard) {
  personal_data_manager_->AddCreditCard(autofill::test::GetCreditCard());

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    CWVCreditCard* cwv_credit_card = credit_cards.firstObject;
    cwv_credit_card.cardHolderFullName = kNewName;
    [autofill_data_manager_ updateCreditCard:cwv_credit_card];
  }));

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    EXPECT_NSEQ(kNewName, credit_cards.firstObject.cardHolderFullName);
  }));
}

// Tests CWVAutofillDataManager properly returns passwords.
TEST_F(CWVAutofillDataManagerTest, ReturnPassword) {
  autofill::PasswordForm test_password = GetTestPassword();
  password_store_->AddLogin(test_password);
  NSArray<CWVPassword*>* fetched_passwords = FetchPasswords();
  EXPECT_EQ(1ul, fetched_passwords.count);
  EXPECT_EQ(test_password, *[fetched_passwords[0] internalPasswordForm]);
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

}  // namespace ios_web_view
