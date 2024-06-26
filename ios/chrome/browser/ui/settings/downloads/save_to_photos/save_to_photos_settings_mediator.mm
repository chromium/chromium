// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_confirmation_consumer.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_consumer.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mediator_delegate.h"

@interface SaveToPhotosSettingsMediator () <
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate,
    PrefObserverDelegate>

@end

@implementation SaveToPhotosSettingsMediator {
  // Account manager service with observer.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;

  // PrefService with registrar and observer.
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  std::unique_ptr<PrefObserverBridge> _prefServiceObserver;

  // IdentityManager with observer.
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}

#pragma mark - Initialization

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  prefService:(PrefService*)prefService
                              identityManager:
                                  (signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    CHECK(accountManagerService);
    CHECK(prefService);
    CHECK(identityManager);

    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);

    _prefService = prefService;
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(_prefService);
    _prefServiceObserver = std::make_unique<PrefObserverBridge>(self);
    _prefServiceObserver->ObserveChangesForPreference(
        prefs::kIosSaveToPhotosDefaultGaiaId, _prefChangeRegistrar.get());
    _prefServiceObserver->ObserveChangesForPreference(
        prefs::kIosSaveToPhotosSkipAccountPicker, _prefChangeRegistrar.get());

    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
  }
  return self;
}

#pragma mark - Setters

- (void)setAccountConfirmationConsumer:
    (id<SaveToPhotosSettingsAccountConfirmationConsumer>)
        accountConfirmationConsumer {
  _accountConfirmationConsumer = accountConfirmationConsumer;
  [self updateConsumers];
}

- (void)setAccountSelectionConsumer:
    (id<SaveToPhotosSettingsAccountSelectionConsumer>)accountSelectionConsumer {
  _accountSelectionConsumer = accountSelectionConsumer;
  [self updateConsumers];
}

#pragma mark - Public

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  _accountManagerService = nullptr;
  _prefServiceObserver.reset();
  _prefService = nullptr;
  _prefChangeRegistrar.reset();
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

#pragma mark - SaveToPhotosSettingsMutator

- (void)setSelectedIdentityGaiaID:(NSString*)gaiaID {
  CHECK(gaiaID);
  _prefService->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                          base::SysNSStringToUTF8(gaiaID));
}

- (void)setAskWhichAccountToUseEveryTime:(BOOL)askEveryTime {
  _prefService->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                           !askEveryTime);
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self updateConsumers];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self updateConsumers];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  [self updateConsumers];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    [self.delegate hideSaveToPhotosSettings];
    return;
  }
  [self updateConsumers];
}

#pragma mark - Private

// Update consumers with information from `_prefService` and
// `_accountManagerService`.
- (void)updateConsumers {
  const std::string savedGaiaID =
      _prefService->GetString(prefs::kIosSaveToPhotosDefaultGaiaId);
  id<SystemIdentity> savedIdentity =
      _accountManagerService->GetIdentityWithGaiaID(savedGaiaID);

  // Get signed-in identity.
  const CoreAccountInfo primaryAccountInfo =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  id<SystemIdentity> primaryAccount =
      _accountManagerService->GetIdentityWithGaiaID(primaryAccountInfo.gaia);

  // Update primary consumer with the currently selected Save to Photos account,
  // if any.
  id<SystemIdentity> selectedIdentity =
      savedIdentity ? savedIdentity : primaryAccount;
  if (!selectedIdentity) {
    // If `selectedIdentity` is nil then there is no identity on the device so
    // Save to Photos settings should be hidden.
    [self.delegate hideSaveToPhotosSettings];
    return;
  }

  BOOL askEveryTimeSwitchOn =
      !_prefService->GetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker);
  if (IsSaveToPhotosAccountPickerImprovementEnabled()) {
    askEveryTimeSwitchOn = !askEveryTimeSwitchOn;
  }
  [self.accountConfirmationConsumer
      setIdentityButtonAvatar:_accountManagerService
                                  ->GetIdentityAvatarWithIdentity(
                                      selectedIdentity,
                                      IdentityAvatarSize::TableViewIcon)
                         name:selectedIdentity.userFullName
                        email:selectedIdentity.userEmail
                       gaiaID:selectedIdentity.gaiaID
         askEveryTimeSwitchOn:askEveryTimeSwitchOn];

  // Update secondary consumer with the list of accounts on the device and which
  // one is currently saved as default to save images to Google Photos, if any.
  NSArray<id<SystemIdentity>>* systemIdentities =
      _accountManagerService->GetAllIdentities();
  NSMutableArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*
      identityItemConfigurators = [[NSMutableArray alloc] init];
  for (id<SystemIdentity> systemIdentity in systemIdentities) {
    AccountPickerSelectionScreenIdentityItemConfigurator* configurator =
        [[AccountPickerSelectionScreenIdentityItemConfigurator alloc] init];
    configurator.gaiaID = systemIdentity.gaiaID;
    configurator.name = systemIdentity.userFullName;
    configurator.email = systemIdentity.userEmail;
    configurator.avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
        systemIdentity, IdentityAvatarSize::TableViewIcon);
    configurator.selected =
        [systemIdentity.gaiaID isEqual:selectedIdentity.gaiaID];
    [identityItemConfigurators addObject:configurator];
  }
  [self.accountSelectionConsumer
      populateAccountsOnDevice:identityItemConfigurators];
}

@end
