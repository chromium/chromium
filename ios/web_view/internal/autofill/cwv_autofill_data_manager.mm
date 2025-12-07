// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager_observer.h"
#import "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/password_manager/core/browser/password_store/password_store_change.h"
#import "components/password_manager/core/browser/password_store/password_store_consumer.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/password_store/password_store_util.h"
#import "components/password_manager/core/browser/ui/passwords_grouper.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/public/cwv_autofill_data_manager_observer.h"
#import "ios/web_view/public/cwv_credential_provider_extension_utils.h"
#import "ui/base/resource/resource_bundle.h"
#import "url/gurl.h"

// Typedefs of |completionHandler| in |fetchProfilesWithCompletionHandler:|,
// |fetchCreditCardsWithCompletionHandler:|, and
// |fetchPasswordsWithCompletionHandler|.
typedef void (^CWVFetchProfilesCompletionHandler)(
    NSArray<CWVAutofillProfile*>* profiles);
typedef void (^CWVFetchCreditCardsCompletionHandler)(
    NSArray<CWVCreditCard*>* creditCards);
typedef void (^CWVFetchPasswordsCompletionHandler)(
    NSArray<CWVPassword*>* passwords);

namespace {
using PasswordFormList = std::vector<password_manager::PasswordForm>;
using PasswordStoreChangeMap =
    std::unordered_map<std::string,
                       password_manager::PasswordStoreChange::Type>;
using PasswordFormListCallback = base::OnceCallback<void(PasswordFormList)>;
}  // namespace

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
// Check if the password affiliation is enabled.
- (BOOL)isPasswordAffiliationEnabled;
// Matches passwords with affilation data and invokes resultCallback.
- (void)matchAffiliationsAndUpdatePasswordsWithForms:
            (PasswordFormList)passwordForms
                                      resultCallback:
                                          (PasswordFormListCallback)completion;

- (void)injectAffiliationAndBrandingInformationForForms:
            (PasswordFormList)passwordForms
                                         resultCallback:
                                             (PasswordFormListCallback)
                                                 completion;

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
    BOOL isAffiliationsEnabled = [data_manager_ isPasswordAffiliationEnabled];

    // Move forms to a regular vector.
    PasswordFormList forms;
    forms.reserve(results.size());
    for (auto& form : results) {
      forms.push_back(*form);
    }

    if (isAffiliationsEnabled) {
      auto callback = base::BindOnce(
          &WebViewPasswordStoreConsumer::OnAffiliatedPasswordsUpdated,
          weak_ptr_factory_.GetWeakPtr());
      [data_manager_
          matchAffiliationsAndUpdatePasswordsWithForms:forms
                                        resultCallback:std::move(callback)];
    } else {
      NSMutableArray<CWVPassword*>* passwords = [NSMutableArray array];
      for (auto& form : results) {
        CWVPassword* password =
            [[CWVPassword alloc] initWithPasswordForm:*form];
        [passwords addObject:password];
      }
      [data_manager_ handlePasswordStoreResults:passwords];
    }
  }

  base::WeakPtr<password_manager::PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  void OnAffiliatedPasswordsUpdated(PasswordFormList affiliated_forms) {
    NSMutableArray<CWVPassword*>* passwords = [NSMutableArray array];
    for (auto& form : affiliated_forms) {
      CWVPassword* password = [[CWVPassword alloc] initWithPasswordForm:form
                                                   isAffiliationEnabled:true];
      [passwords addObject:password];
    }
    [data_manager_ handlePasswordStoreResults:passwords];
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
    BOOL isAffiliationsEnabled = [data_manager_ isPasswordAffiliationEnabled];

    if (isAffiliationsEnabled) {
      PasswordFormList unaffiliated_forms;
      unaffiliated_forms.reserve(changes.size());
      PasswordStoreChangeMap change_map;

      for (const password_manager::PasswordStoreChange& change : changes) {
        if (change.form().blocked_by_user) {
          continue;
        }

        password_manager::PasswordForm form = change.form();
        change_map[form.keychain_identifier] = change.type();
        unaffiliated_forms.push_back(std::move(form));
      }

      auto callback = base::BindOnce(
          &WebViewPasswordStoreObserver::OnAffiliatedPasswordsUpdated,
          weak_ptr_factory_.GetWeakPtr(), std::move(change_map));
      [data_manager_
          matchAffiliationsAndUpdatePasswordsWithForms:unaffiliated_forms
                                        resultCallback:std::move(callback)];
    } else {
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
            NOTREACHED();
        }
      }
      [data_manager_ handlePasswordStoreLoginsAdded:added
                                            updated:updated
                                            removed:removed];
    }
  }
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override {
    // No op.
  }

 protected:
  void OnAffiliatedPasswordsUpdated(PasswordStoreChangeMap change_map,
                                    PasswordFormList affiliated_forms) {
    NSMutableArray* added = [NSMutableArray array];
    NSMutableArray* updated = [NSMutableArray array];
    NSMutableArray* removed = [NSMutableArray array];

    for (const auto& form : affiliated_forms) {
      CWVPassword* password = [[CWVPassword alloc] initWithPasswordForm:form
                                                   isAffiliationEnabled:true];
      auto it = change_map.find(form.keychain_identifier);
      if (it != change_map.end()) {
        switch (it->second) {
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
            NOTREACHED();
        }
      }
    }

    [data_manager_ handlePasswordStoreLoginsAdded:added
                                          updated:updated
                                          removed:removed];
  }

 private:
  __weak CWVAutofillDataManager* data_manager_;
  base::WeakPtrFactory<WebViewPasswordStoreObserver> weak_ptr_factory_{this};
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
  raw_ptr<affiliations::AffiliationService> _affiliationsService;
  std::unique_ptr<password_manager::AffiliatedMatchHelper>
      _affiliatedMatchHelper;
  BOOL _isPasswordAffiliationEnabled;
  std::unique_ptr<ios_web_view::WebViewPasswordStoreConsumer>
      _passwordStoreConsumer;
  std::unique_ptr<ios_web_view::WebViewPasswordStoreObserver>
      _passwordStoreObserver;
}

- (instancetype)
     initWithPersonalDataManager:
         (autofill::PersonalDataManager*)personalDataManager
                   passwordStore:
                       (password_manager::PasswordStoreInterface*)passwordStore
             affiliationsService:
                 (affiliations::AffiliationService*)affiliationsService
    isPasswordAffiliationEnabled:(BOOL)isPasswordAffiliationEnabled {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    _passwordStore = passwordStore;
    _affiliationsService = affiliationsService;
    _affiliatedMatchHelper =
        std::make_unique<password_manager::AffiliatedMatchHelper>(
            affiliationsService);
    _isPasswordAffiliationEnabled = isPasswordAffiliationEnabled;
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
  _personalDataManager->address_data_manager().RemoveProfile(
      profile.internalProfile->guid());
}

- (UIImage*)fetchIconForCreditCard:(CWVCreditCard*)creditCard {
  // Check if custom card art is available.
  GURL cardArtURL = _personalDataManager->payments_data_manager().GetCardArtURL(
      *creditCard.internalCard);
  if (!cardArtURL.is_empty() && cardArtURL.is_valid()) {
    if (const gfx::Image* image =
            _personalDataManager->payments_data_manager()
                .GetCachedCardArtImageForUrl(cardArtURL)) {
      return image->ToUIImage();
    }
  }

  // Otherwise, try to get the default card icon
  autofill::Suggestion::Icon icon =
      creditCard.internalCard->CardIconForAutofillSuggestion();
  return icon == autofill::Suggestion::Icon::kNoIcon
             ? nil
             : ui::ResourceBundle::GetSharedInstance()
                   .GetNativeImageNamed(
                       autofill::CreditCard::IconResourceId(icon))
                   .ToUIImage();
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
           newPassword:(nullable NSString*)newPassword
             timestamp:(NSDate*)timestamp {
  password_manager::PasswordForm* passwordForm =
      [password internalPasswordForm];
  passwordForm->date_password_modified = base::Time::FromNSDate(timestamp);

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
                             site:(NSString*)site
                        timestamp:(NSDate*)timestamp {
  password_manager::PasswordForm form;

  DCHECK_GT(username.length, 0ul);
  DCHECK_GT(password.length, 0ul);
  GURL url(base::SysNSStringToUTF8(site));
  DCHECK(url.is_valid());

  form.url = password_manager_util::StripAuthAndParams(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = base::SysNSStringToUTF16(username);
  form.password_value = base::SysNSStringToUTF16(password);
  form.date_created = base::Time::FromNSDate(timestamp);

  _passwordStore->AddLogin(form);
}

- (void)addNewPasswordForUsername:(NSString*)username
                serviceIdentifier:(NSString*)serviceIdentifier
               keychainIdentifier:(NSString*)keychainIdentifier
                        timestamp:(NSDate*)timestamp {
  password_manager::PasswordForm form;

  GURL url(base::SysNSStringToUTF8(serviceIdentifier));
  DCHECK(url.is_valid());

  form.url = password_manager_util::StripAuthAndParams(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.username_value = base::SysNSStringToUTF16(username);
  form.keychain_identifier = base::SysNSStringToUTF8(keychainIdentifier);
  form.date_created = base::Time::FromNSDate(timestamp);

  _passwordStore->AddLogin(form);
}

#pragma mark - Private Methods

- (BOOL)isPasswordAffiliationEnabled {
  return _isPasswordAffiliationEnabled;
}

- (void)injectAffiliationAndBrandingInformationForForms:
            (PasswordFormList)passwordForms
                                         resultCallback:
                                             (PasswordFormListCallback)
                                                 completion {
  _affiliatedMatchHelper->InjectAffiliationAndBrandingInformation(
      passwordForms,
      base::BindOnce(&password_manager::GetLoginsOrEmptyListOnFailure)
          .Then(std::move(completion)));
}

- (void)matchAffiliationsAndUpdatePasswordsWithForms:
            (PasswordFormList)passwordForms
                                      resultCallback:
                                          (PasswordFormListCallback)completion {
  // Convert forms to Facets to fetch affilation data.
  std::vector<affiliations::FacetURI> facets;
  facets.reserve(passwordForms.size());
  for (const auto& form : passwordForms) {
    facets.emplace_back(affiliations::FacetURI::FromPotentiallyInvalidSpec(
        GetFacetRepresentation(form)));
  }

  __weak __typeof(self) weakSelf = self;
  auto updateAffiliationsCallback = base::BindOnce(
      ^(PasswordFormListCallback innerCompletion) {
        [weakSelf
            injectAffiliationAndBrandingInformationForForms:passwordForms
                                             resultCallback:
                                                 std::move(innerCompletion)];
      },
      std::move(completion));

  _affiliationsService->UpdateAffiliationsAndBranding(
      facets, std::move(updateAffiliationsCallback));
}

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
  for (const autofill::CreditCard* internalCard :
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
