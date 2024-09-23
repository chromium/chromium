// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#import "base/location.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/address_data_manager.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store/password_store_change.h"
#import "components/password_manager/core/browser/password_store/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/public/cwv_autofill_data_manager_observer.h"
#import "ios/web_view/public/cwv_credential_provider_extension_utils.h"
#include "url/gurl.h"

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

// Called when WebViewPasswordStoreObserver's |OnLoginsChanged| is called.
- (void)handlePasswordStoreLoginsAdded:(NSArray<CWVPassword*>*)added
                               updated:(NSArray<CWVPassword*>*)updated
                               removed:(NSArray<CWVPassword*>*)removed;
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
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override {
    NSMutableArray<CWVPassword*>* passwords = [NSMutableArray array];
    for (auto& form : results) {
      CWVPassword* password = [[CWVPassword alloc] initWithPasswordForm:*form];
      [passwords addObject:password];
    }
    [data_manager_ handlePasswordStoreResults:passwords];
  }

  base::WeakPtr<password_manager::PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  __weak CWVAutofillDataManager* data_manager_;
  base::WeakPtrFactory<WebViewPasswordStoreConsumer> weak_ptr_factory_{this};
};

// C++ to ObjC bridge for PasswordStoreInterface::Observer.
class WebViewPasswordStoreObserver
    : public password_manager::PasswordStoreInterface::Observer {
 public:
  explicit WebViewPasswordStoreObserver(CWVAutofillDataManager* data_manager)
      : data_manager_(data_manager) {}
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override {
    NSMutableArray* added = [NSMutableArray array];
    NSMutableArray* updated = [NSMutableArray array];
    NSMutableArray* removed = [NSMutableArray array];
    for (const password_manager::PasswordStoreChange& change : changes) {
      if (change.form().blocked_by_user) {
        continue;
      }
      CWVPassword* password =
          [[CWVPassword alloc] initWithPasswordForm:change.form()];
      switch (change.type()) {
        case password_manager::PasswordStoreChange::ADD:
          [added addObject:password];
          break;
        case password_manager::PasswordStoreChange::UPDATE:
          [updated addObject:password];
          break;
        case password_manager::PasswordStoreChange::REMOVE:
          [removed addObject:password];
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
    [data_manager_ handlePasswordStoreLoginsAdded:added
                                          updated:updated
                                          removed:removed];
  }
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override {
    // No op.
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

  password_manager::PasswordStoreInterface* _passwordStore;
  std::unique_ptr<ios_web_view::WebViewPasswordStoreConsumer>
      _passwordStoreConsumer;
  std::unique_ptr<ios_web_view::WebViewPasswordStoreObserver>
      _passwordStoreObserver;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                              passwordStore:
                                  (password_manager::PasswordStoreInterface*)
                                      passwordStore {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    _passwordStore = passwordStore;
    _passwordStoreObserver =
        std::make_unique<ios_web_view::WebViewPasswordStoreObserver>(self);
    _passwordStore->AddObserver(_passwordStoreObserver.get());
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
  _passwordStore->RemoveObserver(_passwordStoreObserver.get());
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
    web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                               completionHandler(profiles);
                                             }));
  } else {
    [_fetchProfilesCompletionHandlers addObject:completionHandler];
  }
}

- (void)updateProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->address_data_manager().UpdateProfile(
      *profile.internalProfile);
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
    web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                               completionHandler(creditCards);
                                             }));
  } else {
    [_fetchCreditCardsCompletionHandlers addObject:completionHandler];
  }
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
  _passwordStore->GetAllLogins(_passwordStoreConsumer->GetWeakPtr());
}

- (void)updatePassword:(CWVPassword*)password
           newUsername:(nullable NSString*)newUsername
           newPassword:(nullable NSString*)newPassword {
  password_manager::PasswordForm* passwordForm =
      [password internalPasswordForm];

  // Only change the password if it actually changed and not empty.
  if (newPassword && newPassword.length > 0 &&
      ![newPassword isEqualToString:password.password]) {
    passwordForm->password_value = base::SysNSStringToUTF16(newPassword);
  }

  // Because a password's primary key depends on its username, changing the
  // username requires that |UpdateLoginWithPrimaryKey| is called instead.
  if (newUsername && newUsername.length > 0 &&
      ![newUsername isEqualToString:password.username]) {
    // Make a local copy of the old password before updating it.
    auto oldPasswordForm = *passwordForm;
    passwordForm->username_value = base::SysNSStringToUTF16(newUsername);
    auto newPasswordForm = *passwordForm;
    _passwordStore->UpdateLoginWithPrimaryKey(newPasswordForm, oldPasswordForm);
  } else {
    _passwordStore->UpdateLogin(*passwordForm);
  }
}

- (void)deletePassword:(CWVPassword*)password {
  _passwordStore->RemoveLogin(FROM_HERE, *[password internalPasswordForm]);
}

- (void)addNewPasswordForUsername:(NSString*)username
                         password:(NSString*)password
                             site:(NSString*)site {
  password_manager::PasswordForm form;

  DCHECK_GT(username.length, 0ul);
  DCHECK_GT(password.length, 0ul);
  GURL url(base::SysNSStringToUTF8(site));
  DCHECK(url.is_valid());

  form.url = password_manager_util::StripAuthAndParams(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = base::SysNSStringToUTF16(username);
  form.password_value = base::SysNSStringToUTF16(password);

  _passwordStore->AddLogin(form);
}

- (void)addNewPasswordForUsername:(NSString*)username
                serviceIdentifier:(NSString*)serviceIdentifier
               keychainIdentifier:(NSString*)keychainIdentifier {
  password_manager::PasswordForm form;

  GURL url(base::SysNSStringToUTF8(serviceIdentifier));
  DCHECK(url.is_valid());

  form.url = password_manager_util::StripAuthAndParams(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = base::SysNSStringToUTF16(username);
  form.keychain_identifier = base::SysNSStringToUTF8(keychainIdentifier);

  _passwordStore->AddLogin(form);
}

#pragma mark - Private Methods

- (void)handlePasswordStoreLoginsAdded:(NSArray<CWVPassword*>*)added
                               updated:(NSArray<CWVPassword*>*)updated
                               removed:(NSArray<CWVPassword*>*)removed {
  for (id<CWVAutofillDataManagerObserver> observer in _observers) {
    [observer autofillDataManager:self
        didChangePasswordsByAdding:added
                          updating:updated
                          removing:removed];
  }
}

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
  for (const autofill::AutofillProfile* internalProfile :
       _personalDataManager->address_data_manager().GetProfiles()) {
    CWVAutofillProfile* profile =
        [[CWVAutofillProfile alloc] initWithProfile:*internalProfile];
    [profiles addObject:profile];
  }
  return [profiles copy];
}

- (NSArray<CWVCreditCard*>*)creditCards {
  NSMutableArray* creditCards = [NSMutableArray array];
  for (autofill::CreditCard* internalCard :
       _personalDataManager->payments_data_manager().GetCreditCards()) {
    CWVCreditCard* creditCard =
        [[CWVCreditCard alloc] initWithCreditCard:*internalCard];
    [creditCards addObject:creditCard];
  }
  return [creditCards copy];
}

- (void)shutDown {
  _personalDataManager->RemoveObserver(
      _personalDataManagerObserverBridge.get());
  _passwordStore->RemoveObserver(_passwordStoreObserver.get());
}

@end
