// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"

#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/public/cwv_autofill_data_manager_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Typedefs of |completionHandler| in |fetchProfilesWithCompletionHandler:|,
// |fetchCreditCardsWithCompletionHandler:|, and
// |fetchPasswordsWithCompletionHandler|.
typedef void (^CWVFetchProfilesCompletionHandler)(
    NSArray<CWVAutofillProfile*>* profiles);
typedef void (^CWVFetchCreditCardsCompletionHandler)(
    NSArray<CWVCreditCard*>* creditCards);
typedef void (^CWVFetchPasswordsCompletionHandler)(
    NSArray<CWVPassword*>* passwords);

@interface CWVAutofillDataManager ()

// Called when WebViewPasswordStoreConsumer's |OnGetPasswordStoreResults| is
// invoked.
- (void)handlePasswordStoreResults:(NSArray<CWVPassword*>*)passwords;
// Called when WebViewPersonalDataManagerObserverBridge's
// |OnPersonalDataChanged| is invoked.
- (void)personalDataDidChange;
// Collects and converts autofill::AutofillProfiles stored internally in
// |_personalDataManager| to CWVAutofillProfiles.
- (NSArray<CWVAutofillProfile*>*)profiles;
// Collects and converts autofill::CreditCards stored internally in
// |_personalDataManager| to CWVCreditCards.
- (NSArray<CWVCreditCard*>*)creditCards;

@end

namespace ios_web_view {
// C++ to ObjC bridge for PersonalDataManagerObserver.
class WebViewPersonalDataManagerObserverBridge
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit WebViewPersonalDataManagerObserverBridge(
      CWVAutofillDataManager* data_manager)
      : data_manager_(data_manager) {}
  ~WebViewPersonalDataManagerObserverBridge() override = default;

  // autofill::PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override {
    [data_manager_ personalDataDidChange];
  }

  void OnInsufficientFormData() override {
    // Nop.
  }

 private:
  __weak CWVAutofillDataManager* data_manager_;
};

// C++ to ObjC bridge for PasswordStoreConsumer.
class WebViewPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  explicit WebViewPasswordStoreConsumer(CWVAutofillDataManager* data_manager)
      : data_manager_(data_manager) {}
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override {
    NSMutableArray<CWVPassword*>* passwords = [NSMutableArray array];
    for (auto& form : results) {
      CWVPassword* password = [[CWVPassword alloc] initWithPasswordForm:*form];
      [passwords addObject:password];
    }
    [data_manager_ handlePasswordStoreResults:passwords];
  }

 private:
  __weak CWVAutofillDataManager* data_manager_;
};

}  // namespace ios_web_view

@implementation CWVAutofillDataManager {
  autofill::PersonalDataManager* _personalDataManager;
  std::unique_ptr<ios_web_view::WebViewPersonalDataManagerObserverBridge>
      _personalDataManagerObserverBridge;
  // These completion handlers are stored so they can be called later on when
  // |_personalDataManager| completes its initial loading.
  NSMutableArray<CWVFetchProfilesCompletionHandler>*
      _fetchProfilesCompletionHandlers;
  NSMutableArray<CWVFetchCreditCardsCompletionHandler>*
      _fetchCreditCardsCompletionHandlers;
  NSMutableArray<CWVFetchPasswordsCompletionHandler>*
      _fetchPasswordsCompletionHandlers;
  // Holds weak observers.
  NSHashTable<id<CWVAutofillDataManagerObserver>>* _observers;

  password_manager::PasswordStore* _passwordStore;
  std::unique_ptr<ios_web_view::WebViewPasswordStoreConsumer>
      _passwordStoreConsumer;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                              passwordStore:(password_manager::PasswordStore*)
                                                passwordStore {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    _passwordStore = passwordStore;
    _personalDataManagerObserverBridge = std::make_unique<
        ios_web_view::WebViewPersonalDataManagerObserverBridge>(self);
    _personalDataManager->AddObserver(_personalDataManagerObserverBridge.get());
    _fetchProfilesCompletionHandlers = [NSMutableArray array];
    _fetchCreditCardsCompletionHandlers = [NSMutableArray array];
    _fetchPasswordsCompletionHandlers = [NSMutableArray array];
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)dealloc {
  _personalDataManager->RemoveObserver(
      _personalDataManagerObserverBridge.get());
}

#pragma mark - Public Methods

- (void)addObserver:(__weak id<CWVAutofillDataManagerObserver>)observer {
  [_observers addObject:observer];
}

- (void)removeObserver:(__weak id<CWVAutofillDataManagerObserver>)observer {
  [_observers removeObject:observer];
}

- (void)fetchProfilesWithCompletionHandler:
    (void (^)(NSArray<CWVAutofillProfile*>* profiles))completionHandler {
  // If data is already loaded, return the existing data asynchronously to match
  // client expectation. Otherwise, save the |completionHandler| and wait for
  // |personalDataDidChange| to be invoked.
  if (_personalDataManager->IsDataLoaded()) {
    NSArray<CWVAutofillProfile*>* profiles = [self profiles];
    base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                     completionHandler(profiles);
                   }));
  } else {
    [_fetchProfilesCompletionHandlers addObject:completionHandler];
  }
}

- (void)updateProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->UpdateProfile(*profile.internalProfile);
}

- (void)deleteProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->RemoveByGUID(profile.internalProfile->guid());
}

- (void)fetchCreditCardsWithCompletionHandler:
    (void (^)(NSArray<CWVCreditCard*>* creditCards))completionHandler {
  // If data is already loaded, return the existing data asynchronously to match
  // client expectation. Otherwise, save the |completionHandler| and wait for
  // |personalDataDidChange| to be invoked.
  if (_personalDataManager->IsDataLoaded()) {
    NSArray<CWVCreditCard*>* creditCards = [self creditCards];
    base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                     completionHandler(creditCards);
                   }));
  } else {
    [_fetchCreditCardsCompletionHandlers addObject:completionHandler];
  }
}

- (void)updateCreditCard:(CWVCreditCard*)creditCard {
  DCHECK(!creditCard.fromGooglePay);
  _personalDataManager->UpdateCreditCard(*creditCard.internalCard);
}

- (void)deleteCreditCard:(CWVCreditCard*)creditCard {
  DCHECK(!creditCard.fromGooglePay);
  _personalDataManager->RemoveByGUID(creditCard.internalCard->guid());
}

- (void)fetchPasswordsWithCompletionHandler:
    (void (^)(NSArray<CWVPassword*>* passwords))completionHandler {
  [_fetchPasswordsCompletionHandlers addObject:completionHandler];

  // Fetch is already pending.
  if (_passwordStoreConsumer) {
    return;
  }

  _passwordStoreConsumer.reset(
      new ios_web_view::WebViewPasswordStoreConsumer(self));
  _passwordStore->GetAllLogins(_passwordStoreConsumer.get());
}

- (void)deletePassword:(CWVPassword*)password {
  _passwordStore->RemoveLogin(*[password internalPasswordForm]);
}

#pragma mark - Private Methods

- (void)handlePasswordStoreResults:(NSArray<CWVPassword*>*)passwords {
  for (CWVFetchPasswordsCompletionHandler completionHandler in
           _fetchPasswordsCompletionHandlers) {
    completionHandler(passwords);
  }
  [_fetchPasswordsCompletionHandlers removeAllObjects];
  _passwordStoreConsumer.reset();
}

- (void)personalDataDidChange {
  // Invoke completionHandlers if they are still outstanding.
  if (_personalDataManager->IsDataLoaded()) {
    if (_fetchProfilesCompletionHandlers.count > 0) {
      NSArray<CWVAutofillProfile*>* profiles = [self profiles];
      for (CWVFetchProfilesCompletionHandler completionHandler in
               _fetchProfilesCompletionHandlers) {
        completionHandler(profiles);
      }
      [_fetchProfilesCompletionHandlers removeAllObjects];
    }
    if (_fetchCreditCardsCompletionHandlers.count > 0) {
      NSArray<CWVCreditCard*>* creditCards = [self creditCards];
      for (CWVFetchCreditCardsCompletionHandler completionHandler in
               _fetchCreditCardsCompletionHandlers) {
        completionHandler(creditCards);
      }
      [_fetchCreditCardsCompletionHandlers removeAllObjects];
    }
  }
  for (id<CWVAutofillDataManagerObserver> observer in _observers) {
    [observer autofillDataManagerDataDidChange:self];
  }
}

- (NSArray<CWVAutofillProfile*>*)profiles {
  NSMutableArray* profiles = [NSMutableArray array];
  for (autofill::AutofillProfile* internalProfile :
       _personalDataManager->GetProfiles()) {
    CWVAutofillProfile* profile =
        [[CWVAutofillProfile alloc] initWithProfile:*internalProfile];
    [profiles addObject:profile];
  }
  return [profiles copy];
}

- (NSArray<CWVCreditCard*>*)creditCards {
  NSMutableArray* creditCards = [NSMutableArray array];
  for (autofill::CreditCard* internalCard :
       _personalDataManager->GetCreditCards()) {
    CWVCreditCard* creditCard =
        [[CWVCreditCard alloc] initWithCreditCard:*internalCard];
    [creditCards addObject:creditCard];
  }
  return [creditCards copy];
}

@end
