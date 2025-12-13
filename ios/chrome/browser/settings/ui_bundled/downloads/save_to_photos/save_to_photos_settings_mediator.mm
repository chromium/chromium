// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_confirmation_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_selection_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"

@interface SaveToPhotosSettingsMediator () <
    IdentityManagerObserverBridgeDelegate,
    PrefObserverDelegate>

@end

@implementation SaveToPhotosSettingsMediator {
  // Account manager service.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;

  // PrefService with registrar and observer.
  raw_ptr<PrefService> _prefService;
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  std::unique_ptr<PrefObserverBridge> _prefServiceObserver;

  // IdentityManager with observer.
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // raw_ptr to the PhotosService object.
  raw_ptr<PhotosService> _photosService;
}

#pragma mark - Initialization

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  prefService:(PrefService*)prefService
                              identityManager:
                                  (signin::IdentityManager*)identityManager
                                photosService:(PhotosService*)photosService {
  self = [super init];
  if (self) {
    CHECK(accountManagerService);
    CHECK(prefService);
    CHECK(identityManager);

    _accountManagerService = accountManagerService;

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

    _photosService = photosService;
  }
  return self;
}

#pragma mark - Setters

- (void)setAccountConfirmationConsumer:
    (id<SaveToPhotosSettingsAccountConfirmationConsumer>)
        accountConfirmationConsumer {
  _accountConfirmationConsumer = accountConfirmationConsumer;
  [self displayOrHideSaveToPhotosSettingsUI];
  [self updateConsumers];
}

- (void)setAccountSelectionConsumer:
    (id<SaveToPhotosSettingsAccountSelectionConsumer>)accountSelectionConsumer {
  _accountSelectionConsumer = accountSelectionConsumer;
  [self updateConsumers];
}

#pragma mark - Public

- (void)disconnect {
  _accountManagerService = nullptr;
  _prefServiceObserver.reset();
  _prefService = nullptr;
  _prefChangeRegistrar.reset();
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

#pragma mark - SaveToPhotosSettingsMutator

- (void)setSelectedIdentityGaiaID:(const GaiaId*)gaiaID {
  CHECK(!gaiaID->empty());
  _prefService->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                          gaiaID->ToString());
}

- (void)setAskWhichAccountToUseEveryTime:(BOOL)askEveryTime {
  _prefService->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                           !askEveryTime);
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  [self updateConsumers];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self displayOrHideSaveToPhotosSettingsUI];
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    return;
  }
  [self updateConsumers];
}

- (void)onAccountsOnDeviceChanged {
  [self updateConsumers];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  [self updateConsumers];
}

#pragma mark - Private

// The Save to Photos settings UI will be displayed or removed depending on
// whether the application's current state is configured to support Save to
// Photos functionality.
- (void)displayOrHideSaveToPhotosSettingsUI {
  if (!_photosService || !_photosService->IsSupported()) {
    [_accountConfirmationConsumer hideSaveToPhotosSettingsUI];
    return;
  }

  [_accountConfirmationConsumer displaySaveToPhotosSettingsUI];
}

// Update consumers with information from `_prefService` and
// `_accountManagerService`.
- (void)updateConsumers {
  const GaiaId savedGaiaID(
      _prefService->GetString(prefs::kIosSaveToPhotosDefaultGaiaId));
  id<SystemIdentity> savedIdentity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(savedGaiaID);

  // Get signed-in identity.
  const CoreAccountInfo primaryAccountInfo =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  id<SystemIdentity> primaryAccount =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(
          primaryAccountInfo.gaia);

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
      _prefService->GetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker);
  [self.accountConfirmationConsumer
      setIdentityButtonAvatar:GetApplicationContext()
                                  ->GetIdentityAvatarProvider()
                                  ->GetIdentityAvatar(
                                      selectedIdentity,
                                      IdentityAvatarSize::TableViewIcon)
                         name:selectedIdentity.userFullName
                        email:selectedIdentity.userEmail
                       gaiaID:selectedIdentity.gaiaId
         askEveryTimeSwitchOn:askEveryTimeSwitchOn];

  // Update secondary consumer with the list of accounts on the device and which
  // one is currently saved as default to save images to Google Photos, if any.
  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(_identityManager, _accountManagerService);
  NSMutableArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*
      identityItemConfigurators = [[NSMutableArray alloc] init];
  for (id<SystemIdentity> systemIdentity in identitiesOnDevice) {
    AccountPickerSelectionScreenIdentityItemConfigurator* configurator =
        [[AccountPickerSelectionScreenIdentityItemConfigurator alloc] init];
    configurator.gaiaID = systemIdentity.gaiaId;
    configurator.name = systemIdentity.userFullName;
    configurator.email = systemIdentity.userEmail;
    configurator.avatar =
        GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
            systemIdentity, IdentityAvatarSize::TableViewIcon);
    configurator.selected = systemIdentity.gaiaId == selectedIdentity.gaiaId;
    [identityItemConfigurators addObject:configurator];
  }
  [self.accountSelectionConsumer
      populateAccountsOnDevice:identityItemConfigurators];
}

@end
