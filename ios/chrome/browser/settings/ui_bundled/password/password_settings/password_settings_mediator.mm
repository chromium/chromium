// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_mediator.h"

#import "base/functional/callback_helpers.h"
#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_exporter.h"
#import "ios/chrome/browser/settings/ui_bundled/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/settings/ui_bundled/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using ::password_manager::CredentialUIEntry;
using ::password_manager::prefs::kAutomaticPasskeyUpgrades;
using ::password_manager::prefs::kCredentialsEnablePasskeys;
using ::password_manager::prefs::kCredentialsEnableService;

// The user action for when the bulk move passwords to account section button is
// clicked.
constexpr const char* kBulkMovePasswordsToAccountButtonClickedUserAction =
    "Mobile.PasswordsSettings.BulkSavePasswordsToAccountButtonClicked";

// Returns true if the credential passed is a password (not a passkey) and not
// stored in the account store.
bool IsCredentialLocalPassword(const CredentialUIEntry& credential) {
  return credential.passkey_credential_id.empty() &&
         !credential.stored_in.contains(
             password_manager::PasswordForm::Store::kAccountStore);
}

}  // namespace

@interface PasswordSettingsMediator () <IdentityManagerObserverBridgeDelegate,
                                        PasswordAutoFillStatusObserver,
                                        PasswordExporterDelegate,
                                        PrefObserverDelegate,
                                        SavedPasswordsPresenterObserver,
                                        SyncObserverModelBridge>
@end

@implementation PasswordSettingsMediator {
  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordManagerViewController.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Service which gives us a view on users' saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

  // Allows reading and writing user preferences.
  raw_ptr<PrefService> _prefService;

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

  // Used to retrieve information about user's passkey security domain.
  raw_ptr<TrustedVaultClientBackend> _trustedVaultClientBackend;

  // Identity of the user. Can be nil if there is no primary account.
  id<SystemIdentity> _identity;

  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // Helper object which maintains state about the "Export Passwords..." flow,
  // and handles the actual serialization of the passwords.
  PasswordExporter* _passwordExporter;

  // Handles showing alerts for user interactions with the bulk move passwords
  // to account section in settings.
  id<BulkMoveLocalPasswordsToAccountHandler> _bulkMovePasswordsToAccountHandler;

  // Delegate capable of showing alerts needed in the password export flow.
  __weak id<PasswordExportHandler> _exportHandler;

  // Whether or not there are any passwords saved.
  BOOL _hasSavedPasswords;

  // Whether or not the password exporter is ready to be activated.
  BOOL _exporterIsReady;
}

- (instancetype)
       initWithReauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
              savedPasswordsPresenter:
                  (password_manager::SavedPasswordsPresenter*)passwordPresenter
    bulkMovePasswordsToAccountHandler:
        (id<BulkMoveLocalPasswordsToAccountHandler>)
            bulkMovePasswordsToAccountHandler
                        exportHandler:(id<PasswordExportHandler>)exportHandler
                          prefService:(PrefService*)prefService
                      identityManager:(signin::IdentityManager*)identityManager
                          syncService:(syncer::SyncService*)syncService
            trustedVaultClientBackend:
                (TrustedVaultClientBackend*)trustedVaultClientBackend
                             identity:(id<SystemIdentity>)identity {
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
    _bulkMovePasswordsToAccountHandler = bulkMovePasswordsToAccountHandler;
    _exportHandler = exportHandler;
    _prefService = prefService;
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(_prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        kAutomaticPasskeyUpgrades, _prefChangeRegistrar.get());
    _prefObserverBridge->ObserveChangesForPreference(
        kCredentialsEnablePasskeys, _prefChangeRegistrar.get());
    _prefObserverBridge->ObserveChangesForPreference(
        kCredentialsEnableService, _prefChangeRegistrar.get());

    _passwordAutoFillStatusManager =
        [PasswordAutoFillStatusManager sharedManager];
    [_passwordAutoFillStatusManager addObserver:self];
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
    _trustedVaultClientBackend = trustedVaultClientBackend;
    _identity = identity;
  }
  return self;
}

- (void)setConsumer:(id<PasswordSettingsConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }

  // Now that the consumer is set, ensure that the consumer starts out with the
  // correct initial value for `canExportPasswords` or else the export button
  // will not behave correctly on load.
  _exporterIsReady = _passwordExporter.exportState == ExportState::IDLE;
  [self savedPasswordsDidChange];

  [self.consumer setSavingPasswordsEnabled:_prefService->GetBoolean(
                                               kCredentialsEnableService)
                           managedByPolicy:_prefService->IsManagedPreference(
                                               kCredentialsEnableService)];

  [self.consumer setUserEmail:base::SysUTF8ToNSString(
                                  _syncService->GetAccountInfo().email)];

  [self.consumer
      setAutomaticPasskeyUpgradesEnabled:_prefService->GetBoolean(
                                             kAutomaticPasskeyUpgrades)];

  [self.consumer setSavingPasskeysEnabled:_prefService->GetBoolean(
                                              kCredentialsEnablePasskeys)];

  [self passwordAutoFillStatusDidChange];

  [self.consumer setOnDeviceEncryptionState:[self onDeviceEncryptionState]];

  [self updateShowBulkMovePasswordsToAccount];

  [self checkUserCanChangeGPMPin];
}

- (void)userDidStartBulkMoveLocalPasswordsToAccountFlow {
  int localPasswordsCount = [self computeLocalPasswordsCount];

  _syncService->TriggerLocalDataMigration(
      syncer::DataTypeSet{syncer::DataType::PASSWORDS});

  // TODO(crbug.com/40281800): Remove this histogram enumeration when using
  // `MoveCredentialsToAccount`.
  base::UmaHistogramEnumeration(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2",
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredForMultiplePasswordsInSettings);

  base::UmaHistogramCounts100(
      "IOS.PasswordManager.BulkSavePasswordsInAccountCount",
      localPasswordsCount);

  [self showMovedToAccountSnackbarWithPasswordCount:localPasswordsCount];
}

- (void)userDidStartDeleteFlow {
  _savedPasswordsPresenter->DeleteAllData(base::DoNothing());
}

- (void)userDidStartExportFlow:(UIWindow*)window {
  std::vector<CredentialUIEntry> passwords =
      _savedPasswordsPresenter->GetSavedPasswords();
  [_passwordExporter startExportFlow:passwords];
}

- (void)userDidCompleteExportFlow {
  [_passwordExporter resetExportState];
}

- (void)exportFlowCanceled {
  [_passwordExporter cancelExport];
}

- (void)disconnect {
  DCHECK(_savedPasswordsPresenter);
  DCHECK(_passwordsPresenterObserver);
  _savedPasswordsPresenter->RemoveObserver(_passwordsPresenterObserver.get());
  _passwordsPresenterObserver.reset();
  [[PasswordAutoFillStatusManager sharedManager] removeObserver:self];
  _prefObserverBridge.reset();
  _prefChangeRegistrar.reset();
  _identityManagerObserver.reset();
  _syncObserver.reset();
  _savedPasswordsPresenter = nullptr;
  _prefService = nullptr;
  _syncService = nullptr;
  _trustedVaultClientBackend = nullptr;
}

- (CredentialCounts)passwordAndPasskeyCounts {
  int passwordsCount = 0;
  int passkeysCount = 0;
  for (CredentialUIEntry entry :
       _savedPasswordsPresenter->GetSavedCredentials()) {
    if (entry.blocked_by_user) {
      continue;
    }
    if (entry.passkey_credential_id.empty()) {
      passwordsCount++;
    } else {
      passkeysCount++;
    }
  }
  struct CredentialCounts credentialCounts;
  credentialCounts.passwordCounts = passwordsCount;
  credentialCounts.passkeyCounts = passkeysCount;

  return credentialCounts;
}

#pragma mark - PasswordExporterDelegate

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:
                            (void (^)(NSString*, BOOL, NSArray*, NSError*))
                                completionHandler {
  [_exportHandler showActivityViewWithActivityItems:activityItems
                                  completionHandler:completionHandler];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)errorReason {
  [_exportHandler showExportErrorAlertWithLocalizedReason:errorReason];
}

- (void)showPreparingPasswordsAlert {
  [_exportHandler showPreparingPasswordsAlert];
}

- (void)showSetPasscodeForPasswordExportDialog {
  [_exportHandler showSetPasscodeForPasswordExportDialog];
}

- (void)updateExportPasswordsButton {
  // This is invoked by the exporter when its state changes, so we have to
  // re-read that state before pushing to the consumer.
  _exporterIsReady = _passwordExporter.exportState == ExportState::IDLE;
  [self pushExportStateToConsumer];
}

#pragma mark - PasswordSettingsDelegate

- (void)savedPasswordSwitchDidChange:(BOOL)enabled {
  _prefService->SetBoolean(kCredentialsEnableService, enabled);
}

- (void)passwordAutoFillWasTurnedOn {
  if (_passwordAutoFillStatusManager.ready) {
    [_passwordAutoFillStatusManager checkAndUpdatePasswordAutoFillStatus];
  }
}

- (void)bulkMovePasswordsToAccountButtonClicked {
  base::RecordAction(base::UserMetricsAction(
      kBulkMovePasswordsToAccountButtonClickedUserAction));

  // Create the confirmation dialog title.
  NSString* alertTitle = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_ALERT_TITLE,
      [self computeLocalPasswordsCount]);

  // Create the confirmation dialog description.
  NSMutableArray<NSString*>* distinctDomains =
      [self computeDistinctDomainsFromLocalPasswords];

  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_ALERT_DESCRIPTION);
  std::u16string result = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "COUNT", (int)[distinctDomains count], "DOMAIN_ONE",
      [distinctDomains count] >= 1
          ? base::SysNSStringToUTF16(distinctDomains[0])
          : base::SysNSStringToUTF16(@""),
      "DOMAIN_TWO",
      [distinctDomains count] >= 2
          ? base::SysNSStringToUTF16(distinctDomains[1])
          : base::SysNSStringToUTF16(@""),
      "OTHER_DOMAINS_COUNT", (int)([distinctDomains count] - 2), "EMAIL",
      _syncService->GetAccountInfo().email);

  NSString* alertDescription = base::SysUTF16ToNSString(result);

  // Create and show the confirmation dialog.
  [_bulkMovePasswordsToAccountHandler
      showConfirmationDialogWithAlertTitle:alertTitle
                          alertDescription:alertDescription];
}

- (void)automaticPasskeyUpgradesSwitchDidChange:(BOOL)enabled {
  _prefService->SetBoolean(kAutomaticPasskeyUpgrades, enabled);
}

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange {
  _hasSavedPasswords = !_savedPasswordsPresenter->GetSavedPasswords().empty();
  [self pushDeleteStateToConsumer];
  [self pushExportStateToConsumer];
  [self updateShowBulkMovePasswordsToAccount];
}

#pragma mark - PrefObserverDelegate

// Called when the value of one of the prefs changes.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  CHECK(preferenceName == kAutomaticPasskeyUpgrades ||
        preferenceName == kCredentialsEnablePasskeys ||
        preferenceName == kCredentialsEnableService)
      << "Unsupported preference: " << preferenceName;

  [self.consumer
      setAutomaticPasskeyUpgradesEnabled:_prefService->GetBoolean(
                                             kAutomaticPasskeyUpgrades)];

  if (preferenceName == kAutomaticPasskeyUpgrades) {
    [self.consumer
        setAutomaticPasskeyUpgradesEnabled:_prefService->GetBoolean(
                                               kAutomaticPasskeyUpgrades)];
  } else if (preferenceName == kCredentialsEnablePasskeys) {
    [self.consumer setSavingPasskeysEnabled:_prefService->GetBoolean(
                                                kCredentialsEnablePasskeys)];
  } else {
    [self.consumer setSavingPasswordsEnabled:_prefService->GetBoolean(
                                                 kCredentialsEnableService)
                             managedByPolicy:_prefService->IsManagedPreference(
                                                 kCredentialsEnableService)];
  }
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
  [self.consumer setUserEmail:base::SysUTF8ToNSString(
                                  _syncService->GetAccountInfo().email)];
  [self updateShowBulkMovePasswordsToAccount];
  // TODO(crbug.com/430876032): Update GPM Pin section properly.
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

// Pushes the current state of the credential deletion button to the consumer.
- (void)pushDeleteStateToConsumer {
  if (_savedPasswordsPresenter) {
    [self.consumer
        setCanDeleteAllCredentials:!_savedPasswordsPresenter
                                        ->GetSavedCredentials()
                                        .empty() ||
                                   !_savedPasswordsPresenter->GetBlockedSites()
                                        .empty()];
  }
}

// Pushes the current state of the exporter to the consumer.
- (void)pushExportStateToConsumer {
  [self.consumer setCanExportPasswords:_hasSavedPasswords && _exporterIsReady];
}

// Computes the amount of local passwords and passes that on to the consumer.
- (void)updateShowBulkMovePasswordsToAccount {
  [self.consumer setCanBulkMove:password_manager::features_util::
                                    IsAccountStorageEnabled(_syncService)
            localPasswordsCount:[self computeLocalPasswordsCount]];
}

// Returns the amount of local passwords.
- (int)computeLocalPasswordsCount {
  int passwordsCount = 0;
  for (password_manager::AffiliatedGroup group :
       _savedPasswordsPresenter->GetAffiliatedGroups()) {
    passwordsCount += std::ranges::count_if(group.GetCredentials().begin(),
                                            group.GetCredentials().end(),
                                            IsCredentialLocalPassword);
  }
  return passwordsCount;
}

// Returns the list of distinct domains present in the local passwords. If they
// are in different affiliated groups, they are presumed to be distinct.
- (NSMutableArray<NSString*>*)computeDistinctDomainsFromLocalPasswords {
  // Add distinct domains for which there exists a password that doesn't appear
  // in the account store.
  NSMutableArray<NSString*>* distinctDomains = [NSMutableArray array];

  for (const password_manager::AffiliatedGroup& group :
       _savedPasswordsPresenter->GetAffiliatedGroups()) {
    auto credential = std::ranges::find_if(group.GetCredentials().begin(),
                                           group.GetCredentials().end(),
                                           IsCredentialLocalPassword);

    // If a credential exists in this group that is in the profile store, append
    // the group's display name to the distinct domains.
    if (credential != group.GetCredentials().end()) {
      [distinctDomains
          addObject:[NSString
                        stringWithUTF8String:group.GetDisplayName().c_str()]];
    }
  }

  return distinctDomains;
}

// Shows the snackbar indicating to the user that their local passwords have
// been saved to their account.
- (void)showMovedToAccountSnackbarWithPasswordCount:(int)count {
  [_bulkMovePasswordsToAccountHandler
      showMovedToAccountSnackbarWithPasswordCount:count
                                        userEmail:_syncService->GetAccountInfo()
                                                      .email];
}

// Checks whether the account is recoverable in the passkey security domain
// (this means that the user has a GPM Pin created). If yes, proceeds to check
// whether the device was bootstrapped to use passkeys.
- (void)checkUserCanChangeGPMPin {
  __weak __typeof(self) weakSelf = self;
  _trustedVaultClientBackend->GetDegradedRecoverabilityStatus(
      _identity, trusted_vault::SecurityDomainId::kPasskeys,
      base::BindOnce(^(BOOL is_degraded) {
        if (!is_degraded) {
          [weakSelf checkDeviceBootstrappedForPasskeys];
        }
      }));
}

// Checks whether the device can fetch shared keys for passkey security domain.
// If yes, notifies the consumer that the change GPM Pin button should be
// visible. This should be called from `checkUserCanChangeGPMPin`.
- (void)checkDeviceBootstrappedForPasskeys {
  __weak id<PasswordSettingsConsumer> weakConsumer = self.consumer;
  _trustedVaultClientBackend->FetchKeys(
      _identity, trusted_vault::SecurityDomainId::kPasskeys,
      base::BindOnce(^(const std::vector<std::vector<uint8_t>>& keys) {
        [weakConsumer setCanChangeGPMPin:!keys.empty()];
      }));
}

@end
