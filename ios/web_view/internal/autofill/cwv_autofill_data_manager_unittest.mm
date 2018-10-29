// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
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
    personal_data_manager_->SetAutofillEnabled(true);
    personal_data_manager_->SetAutofillProfileEnabled(true);
    personal_data_manager_->SetAutofillCreditCardEnabled(true);
    personal_data_manager_->SetAutofillWalletImportEnabled(true);

    autofill_data_manager_ = [[CWVAutofillDataManager alloc]
        initWithPersonalDataManager:personal_data_manager_.get()];
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

  ~CWVAutofillDataManagerTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

  web::TestWebThreadBundle web_thread_bundle_;
  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
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

// Tests CWVAutofillDataManager properly deletes all local data.
TEST_F(CWVAutofillDataManagerTest, ClearAllLocalData) {
  personal_data_manager_->AddCreditCard(autofill::test::GetCreditCard());
  personal_data_manager_->AddCreditCard(autofill::test::GetCreditCard2());
  personal_data_manager_->AddServerCreditCard(
      autofill::test::GetMaskedServerCard());
  personal_data_manager_->AddProfile(autofill::test::GetFullProfile());
  personal_data_manager_->AddProfile(autofill::test::GetFullProfile2());

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    EXPECT_EQ(3ul, credit_cards.count);
  }));

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    EXPECT_EQ(2ul, profiles.count);
  }));

  [autofill_data_manager_ clearAllLocalData];

  EXPECT_TRUE(FetchCreditCards(^(NSArray<CWVCreditCard*>* credit_cards) {
    EXPECT_EQ(1ul, credit_cards.count);
    EXPECT_TRUE(credit_cards.firstObject.fromGooglePay);
  }));

  EXPECT_TRUE(FetchProfiles(^(NSArray<CWVAutofillProfile*>* profiles) {
    EXPECT_EQ(0ul, profiles.count);
  }));
}

}  // namespace ios_web_view
