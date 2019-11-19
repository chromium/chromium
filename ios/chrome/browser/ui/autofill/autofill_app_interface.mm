// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kExampleUsername[] = "concrete username";
const char kExamplePassword[] = "concrete password";

// Gets the current password store.
scoped_refptr<password_manager::PasswordStore> GetPasswordStore() {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, and should not run in Incognito
  // context. Therefore IMPLICIT_ACCESS is used to let the test fail if in
  // Incognito context.
  return IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// This class is used to obtain results from the PasswordStore and hence both
// check the success of store updates and ensure that store has finished
// processing.
class TestStoreConsumer : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> obtained) override {
    obtained_ = std::move(obtained);
  }

  const std::vector<autofill::PasswordForm>& GetStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordStore()->GetAllLogins(this);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-result"
    base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout, ^bool {
          return !AreObtainedReset();
        });
#pragma clang diagnostic pop
    AppendObtainedToResults();
    return results_;
  }

 private:
  // Puts |obtained_| in a known state not corresponding to any PasswordStore
  // state.
  void ResetObtained() {
    obtained_.clear();
    obtained_.emplace_back(nullptr);
  }

  // Returns true if |obtained_| are in the reset state.
  bool AreObtainedReset() { return obtained_.size() == 1 && !obtained_[0]; }

  void AppendObtainedToResults() {
    for (const auto& source : obtained_) {
      results_.emplace_back(*source);
    }
    ResetObtained();
  }

  // Temporary cache of obtained store results.
  std::vector<std::unique_ptr<autofill::PasswordForm>> obtained_;

  // Combination of fillable and blacklisted credentials from the store.
  std::vector<autofill::PasswordForm> results_;
};

// Saves |form| to the password store and waits until the async processing is
// done.
void SaveToPasswordStore(const autofill::PasswordForm& form) {
  GetPasswordStore()->AddLogin(form);
  // When we retrieve the form from the store, |from_store| should be set.
  autofill::PasswordForm expected_form = form;
  expected_form.from_store = autofill::PasswordForm::Store::kProfileStore;
  // Check the result and ensure PasswordStore processed this.
  TestStoreConsumer consumer;
  for (const auto& result : consumer.GetStoreResults()) {
    if (result == expected_form)
      return;
  }
}

// Saves an example form in the store.
void SaveExamplePasswordForm() {
  autofill::PasswordForm example;
  example.username_value = base::ASCIIToUTF16(kExampleUsername);
  example.password_value = base::ASCIIToUTF16(kExamplePassword);
  example.origin = GURL("https://example.com/");
  example.signon_realm = example.origin.spec();
  SaveToPasswordStore(example);
}

// Saves an example form in the store for the passed URL.
void SaveLocalPasswordForm(const GURL& url) {
  autofill::PasswordForm localForm;
  localForm.username_value = base::ASCIIToUTF16(kExampleUsername);
  localForm.password_value = base::ASCIIToUTF16(kExamplePassword);
  localForm.origin = url;
  localForm.signon_realm = localForm.origin.spec();
  SaveToPasswordStore(localForm);
}

// Removes all credentials stored.
void ClearPasswordStore() {
  GetPasswordStore()->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                                 base::Closure());
  TestStoreConsumer consumer;
}

// Saves an example profile in the store.
void AddAutofillProfile(autofill::PersonalDataManager* personalDataManager) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  // If the test profile is already in the store, adding it will be a no-op.
  // In that case, early return.
  for (autofill::AutofillProfile* p : personalDataManager->GetProfiles()) {
    if (p->Compare(profile) == 0) {
      return;
    }
  }
  size_t profileCount = personalDataManager->GetProfiles().size();

  personalDataManager->AddProfile(profile);

  ConditionBlock conditionBlock = ^bool {
    return profileCount < personalDataManager->GetProfiles().size();
  };
  base::test::ios::TimeUntilCondition(
      nil, conditionBlock, false,
      base::TimeDelta::FromSeconds(base::test::ios::kWaitForActionTimeout));
}

}  // namespace

@implementation AutofillAppInterface

+ (void)clearPasswordStore {
  ClearPasswordStore();
}

+ (void)saveExamplePasswordForm {
  SaveExamplePasswordForm();
}

+ (void)savePasswordFormForURLSpec:(NSString*)URLSpec {
  SaveLocalPasswordForm(GURL(base::SysNSStringToUTF8(URLSpec)));
}

+ (NSInteger)profilesCount {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  return personalDataManager->GetProfiles().size();
}

+ (void)clearProfilesStore {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
  for (const auto* profile : personalDataManager->GetProfiles()) {
    personalDataManager->RemoveByGUID(profile->guid());
  }

  ConditionBlock conditionBlock = ^bool {
    return 0 == personalDataManager->GetProfiles().size();
  };
  base::test::ios::TimeUntilCondition(
      nil, conditionBlock, false,
      base::TimeDelta::FromSeconds(base::test::ios::kWaitForActionTimeout));
}

+ (void)saveExampleProfile {
  AddAutofillProfile([self personalDataManager]);
}

+ (NSString*)exampleProfileName {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  base::string16 name =
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                      GetApplicationContext()->GetApplicationLocale());
  return base::SysUTF16ToNSString(name);
}

+ (void)clearCreditCardStore {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  for (const auto* creditCard : personalDataManager->GetCreditCards()) {
    personalDataManager->RemoveByGUID(creditCard->guid());
  }
}

+ (NSString*)saveLocalCreditCard {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  autofill::CreditCard card = autofill::test::GetCreditCard();
  size_t card_count = personalDataManager->GetCreditCards().size();
  personalDataManager->AddCreditCard(card);
  ConditionBlock conditionBlock = ^bool {
    return card_count < personalDataManager->GetCreditCards().size();
  };
  base::test::ios::TimeUntilCondition(
      nil, conditionBlock, false,
      base::TimeDelta::FromSeconds(
          base::test::ios::kWaitForFileOperationTimeout));
  personalDataManager->NotifyPersonalDataObserver();
  return base::SysUTF16ToNSString(card.NetworkAndLastFourDigits());
}

+ (void)saveMaskedCreditCard {
  autofill::PersonalDataManager* personalDataManager =
      [self personalDataManager];
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  DCHECK(card.record_type() != autofill::CreditCard::LOCAL_CARD);

  personalDataManager->AddServerCreditCardForTest(
      std::make_unique<autofill::CreditCard>(card));
  personalDataManager->NotifyPersonalDataObserver();
}

#pragma mark - Private

// The PersonalDataManager instance for the current browser state.
+ (autofill::PersonalDataManager*)personalDataManager {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
  personalDataManager->SetSyncingForTest(true);
  return personalDataManager;
}

@end
