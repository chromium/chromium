// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"

#import "base/containers/cxx20_erase_vector.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/features.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_exporter.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

using password_manager::prefs::kCredentialsEnableService;

@interface PasswordSettingsMediator () <BooleanObserver,
                                        IdentityManagerObserverBridgeDelegate,
                                        PasswordAutoFillStatusObserver,
                                        PasswordExporterDelegate,
                                        SavedPasswordsPresenterObserver,
                                        SyncObserverModelBridge> {
  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordManagerViewController.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Service which gives us a view on users' saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Allows reading and writing user preferences.
  raw_ptr<PrefService> _prefService;

  // The observable boolean that binds to the password manager setting state.
  // Saved passwords are only on if the password manager is enabled.
  PrefBackedBoolean* _passwordManagerEnabled;

  // Provides status of Chrome as iOS AutoFill credential provider (i.e.,
  // whether or not Chrome passwords can currently be used in other apps).
  PasswordAutoFillStatusManager* _passwordAutoFillStatusManager;

  // IdentityManager observer.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Service providing information about sync status.
  raw_ptr<syncer::SyncService> _syncService;

  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;

  // Flag to avoid incrementing the number of impressions of the icon more than
  // once through the lifetime of the UI.
  BOOL _accountStorageNewFeatureIconImpressionsIncremented;
}

// Helper object which maintains state about the "Export Passwords..." flow, and
// handles the actual serialization of the passwords.
@property(nonatomic, strong) PasswordExporter* passwordExporter;

// Delegate capable of showing alerts needed in the password export flow.
@property(nonatomic, weak) id<PasswordExportHandler> exportHandler;

// Whether or not there are any passwords saved.
@property(nonatomic, readwrite) BOOL hasSavedPasswords;

// Whether or not the password exporter is ready to be activated.
@property(nonatomic, readwrite) BOOL exporterIsReady;

@end

@implementation PasswordSettingsMediator

- (instancetype)
    initWithReauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
           savedPasswordsPresenter:
               (raw_ptr<password_manager::SavedPasswordsPresenter>)
                   passwordPresenter
                     exportHandler:(id<PasswordExportHandler>)exportHandler
                       prefService:(raw_ptr<PrefService>)prefService
                   identityManager:
                       (raw_ptr<signin::IdentityManager>)identityManager
                       syncService:(raw_ptr<syncer::SyncService>)syncService {
  self = [super init];
  if (self) {
    _passwordExporter =
        [[PasswordExporter alloc] initWithReauthenticationModule:reauthModule
                                                        delegate:self];
    _savedPasswordsPresenter = passwordPresenter;
    _passwordsPresenterObserver =
        std::make_unique<SavedPasswordsPresenterObserverBridge>(
            self, _savedPasswordsPresenter);
    _savedPasswordsPresenter->Init();
    _exportHandler = exportHandler;
    _prefService = prefService;
    _passwordManagerEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_prefService
                   prefName:kCredentialsEnableService];
    _passwordManagerEnabled.observer = self;
    _passwordAutoFillStatusManager =
        [PasswordAutoFillStatusManager sharedManager];
    [_passwordAutoFillStatusManager addObserver:self];
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
  }
  return self;
}

- (void)setConsumer:(id<PasswordSettingsConsumer>)consumer {
  _consumer = consumer;
  // Now that the consumer is set, ensure that the consumer starts out with the
  // correct initial value for `canExportPasswords` or else the export button
  // will not behave correctly on load.
  self.exporterIsReady = self.passwordExporter.exportState == ExportState::IDLE;
  [self savedPasswordsDidChange];

  [self.consumer setSavePasswordsEnabled:_passwordManagerEnabled.value];

  [self.consumer setSignedInAccount:base::SysUTF8ToNSString(
                                        _syncService->GetAccountInfo().email)];

  [self.consumer setAccountStorageState:[self computeAccountStorageState]];

  // < and not <= below, because the next impression must be counted.
  const int impressionCount = _prefService->GetInteger(
      password_manager::prefs::kAccountStorageNewFeatureIconImpressions);
  constexpr int maxImpressionCount = 5;
  [self.consumer
      setShowAccountStorageNewFeatureIcon:impressionCount < maxImpressionCount];

  // TODO(crbug.com/1082827): In addition to setting this value here, we should
  // observe for changes (i.e., if policy changes while the screen is open) and
  // push that to the consumer.
  [self.consumer setManagedByPolicy:_prefService->IsManagedPreference(
                                        kCredentialsEnableService)];

  [self passwordAutoFillStatusDidChange];

  [self.consumer setOnDeviceEncryptionState:[self onDeviceEncryptionState]];
}

- (void)userDidStartExportFlow {
  // Use GetSavedCredentials, rather than GetSavedPasswords, because the latter
  // can return duplicate passwords that shouldn't be included in the export.
  // However, this method also returns blocked sites ("Never save for
  // example.com"), so those must be filtered before passing to the exporter.
  std::vector<password_manager::CredentialUIEntry> passwords =
      _savedPasswordsPresenter->GetSavedCredentials();
  base::EraseIf(passwords, [](const auto& credential) {
    return credential.blocked_by_user;
  });
  [self.passwordExporter startExportFlow:passwords];
}

- (void)userDidCompleteExportFlow {
  [self.passwordExporter resetExportState];
}

- (void)userDidCancelExportFlow {
  [self.passwordExporter cancelExport];
}

- (void)disconnect {
  DCHECK(_savedPasswordsPresenter);
  DCHECK(_passwordsPresenterObserver);
  _savedPasswordsPresenter->RemoveObserver(_passwordsPresenterObserver.get());
  _passwordsPresenterObserver.reset();
  [[PasswordAutoFillStatusManager sharedManager] removeObserver:self];
  [_passwordManagerEnabled stop];
  _identityManagerObserver.reset();
  _syncObserver.reset();
}

#pragma mark - PasswordExporterDelegate

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:
                            (void (^)(NSString*, BOOL, NSArray*, NSError*))
                                completionHandler {
  [self.exportHandler showActivityViewWithActivityItems:activityItems
                                      completionHandler:completionHandler];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)errorReason {
  [self.exportHandler showExportErrorAlertWithLocalizedReason:errorReason];
}

- (void)showPreparingPasswordsAlert {
  [self.exportHandler showPreparingPasswordsAlert];
}

- (void)showSetPasscodeDialog {
  [self.exportHandler showSetPasscodeDialog];
}

- (void)updateExportPasswordsButton {
  // This is invoked by the exporter when its state changes, so we have to
  // re-read that state before pushing to the consumer.
  self.exporterIsReady = self.passwordExporter.exportState == ExportState::IDLE;
  [self pushExportStateToConsumerAndUpdate];
}

#pragma mark - PasswordSettingsDelegate

- (void)savedPasswordSwitchDidChange:(BOOL)enabled {
  _passwordManagerEnabled.value = enabled;
}

- (void)accountStorageSwitchDidChange:(BOOL)enabled {
  syncer::UserSelectableTypeSet types =
      _syncService->GetUserSettings()->GetSelectedTypes();
  if (enabled) {
    types.Put(syncer::UserSelectableType::kPasswords);
  } else {
    types.Remove(syncer::UserSelectableType::kPasswords);
  }
  _syncService->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    types);
}

- (void)accountStorageNewFeatureIconDidShow {
  if (!_accountStorageNewFeatureIconImpressionsIncremented) {
    _accountStorageNewFeatureIconImpressionsIncremented = YES;
    _prefService->SetInteger(
        password_manager::prefs::kAccountStorageNewFeatureIconImpressions,
        1 + _prefService->GetInteger(
                password_manager::prefs::
                    kAccountStorageNewFeatureIconImpressions));
  }
}

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange {
  self.hasSavedPasswords =
      !_savedPasswordsPresenter->GetSavedPasswords().empty();
  [self pushExportStateToConsumerAndUpdate];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK(observableBoolean == _passwordManagerEnabled);
  [self.consumer setSavePasswordsEnabled:observableBoolean.value];
}

#pragma mark - PasswordAutoFillStatusObserver

- (void)passwordAutoFillStatusDidChange {
  if (_passwordAutoFillStatusManager.ready) {
    [self.consumer setPasswordsInOtherAppsEnabled:_passwordAutoFillStatusManager
                                                      .autoFillEnabled];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self.consumer setOnDeviceEncryptionState:[self onDeviceEncryptionState]];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self.consumer setOnDeviceEncryptionState:[self onDeviceEncryptionState]];
  [self.consumer setSignedInAccount:base::SysUTF8ToNSString(
                                        _syncService->GetAccountInfo().email)];
  [self.consumer setAccountStorageState:[self computeAccountStorageState]];
}

#pragma mark - Private

// Returns the on-device encryption state according to the sync service.
- (PasswordSettingsOnDeviceEncryptionState)onDeviceEncryptionState {
  if (ShouldOfferTrustedVaultOptIn(_syncService)) {
    return PasswordSettingsOnDeviceEncryptionStateOfferOptIn;
  }
  if (_syncService->GetUserSettings()->GetPassphraseType() ==
      syncer::PassphraseType::kTrustedVaultPassphrase) {
    return PasswordSettingsOnDeviceEncryptionStateOptedIn;
  }
  return PasswordSettingsOnDeviceEncryptionStateNotShown;
}

// Pushes the current state of the exporter to the consumer and updates its
// export passwords button.
- (void)pushExportStateToConsumerAndUpdate {
  [self.consumer
      setCanExportPasswords:self.hasSavedPasswords && self.exporterIsReady];
  [self.consumer updateExportPasswordsButton];
}

- (PasswordSettingsAccountStorageState)computeAccountStorageState {
  if (_syncService->GetAccountInfo().IsEmpty() ||
      _syncService->IsSyncFeatureEnabled() ||
      base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return PasswordSettingsAccountStorageStateNotShown;
  }

  CHECK(base::FeatureList::IsEnabled(
      password_manager::features::kEnablePasswordsAccountStorage));

  if (_prefService->IsManagedPreference(kCredentialsEnableService) ||
      _syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kPasswords) ||
      _syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    return PasswordSettingsAccountStorageStateDisabledByPolicy;
  }

  return _syncService->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords)
             ? PasswordSettingsAccountStorageStateOptedIn
             : PasswordSettingsAccountStorageStateOptedOut;
}

@end
