// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/passphrase_enums.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_exporter.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using password_manager::CredentialUIEntry;
using password_manager::prefs::kCredentialsEnableService;

namespace {

// The user action for when the bulk move passwords to account section button is
// clicked.
constexpr const char* kBulkMovePasswordsToAccountButtonClickedUserAction =
    "Mobile.PasswordsSettings.BulkSavePasswordsToAccountButtonClicked";

// Returns true if the credential passed is not stored in the account store.
bool IsCredentialNotInAccountStore(const CredentialUIEntry& credential) {
  return !credential.stored_in.contains(
      password_manager::PasswordForm::Store::kAccountStore);
}

}  // namespace

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
}

// Helper object which maintains state about the "Export Passwords..." flow, and
// handles the actual serialization of the passwords.
@property(nonatomic, strong) PasswordExporter* passwordExporter;

@property(nonatomic, strong) id<BulkMoveLocalPasswordsToAccountHandler>
    bulkMovePasswordsToAccountHandler;

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
                  (password_manager::SavedPasswordsPresenter*)passwordPresenter
    bulkMovePasswordsToAccountHandler:
        (id<BulkMoveLocalPasswordsToAccountHandler>)
            bulkMovePasswordsToAccountHandler
                        exportHandler:(id<PasswordExportHandler>)exportHandler
                          prefService:(PrefService*)prefService
                      identityManager:(signin::IdentityManager*)identityManager
                          syncService:(syncer::SyncService*)syncService {
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

  // TODO(crbug.com/40131118): In addition to setting this value here, we should
  // observe for changes (i.e., if policy changes while the screen is open) and
  // push that to the consumer.
  [self.consumer setManagedByPolicy:_prefService->IsManagedPreference(
                                        kCredentialsEnableService)];

  [self passwordAutoFillStatusDidChange];

  [self.consumer setOnDeviceEncryptionState:[self onDeviceEncryptionState]];

  [self updateShowBulkMovePasswordsToAccount];
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

- (void)userDidStartExportFlow {
  // Use GetSavedCredentials, rather than GetSavedPasswords, because the latter
  // can return duplicate passwords that shouldn't be included in the export.
  // However, this method also returns blocked sites ("Never save for
  // example.com"), so those must be filtered before passing to the exporter.
  std::vector<CredentialUIEntry> passwords =
      _savedPasswordsPresenter->GetSavedCredentials();
  std::erase_if(passwords, [](const auto& credential) {
    return credential.blocked_by_user;
  });
  [self.passwordExporter startExportFlow:passwords];
}

- (void)userDidCompleteExportFlow {
  [self.passwordExporter resetExportState];
}

- (void)exportFlowCanceled {
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

- (void)showSetPasscodeForPasswordExportDialog {
  [self.exportHandler showSetPasscodeForPasswordExportDialog];
}

- (void)updateExportPasswordsButton {
  // This is invoked by the exporter when its state changes, so we have to
  // re-read that state before pushing to the consumer.
  self.exporterIsReady = self.passwordExporter.exportState == ExportState::IDLE;
  [self pushExportStateToConsumerAndUpdate];
}

#pragma mark - PasswordSettingsDelegate

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
  [self.bulkMovePasswordsToAccountHandler
      showConfirmationDialogWithAlertTitle:alertTitle
                          alertDescription:alertDescription];
}

- (void)savedPasswordSwitchDidChange:(BOOL)enabled {
  _passwordManagerEnabled.value = enabled;
}

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange {
  self.hasSavedPasswords =
      !_savedPasswordsPresenter->GetSavedPasswords().empty();
  [self pushExportStateToConsumerAndUpdate];
  [self updateShowBulkMovePasswordsToAccount];
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
  [self updateShowBulkMovePasswordsToAccount];
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

// Computes the amount of local passwords and passes that on to the consumer.
- (void)updateShowBulkMovePasswordsToAccount {
  [self.consumer setLocalPasswordsCount:[self computeLocalPasswordsCount]
                    withUserEligibility:password_manager::features_util::
                                            IsOptedInForAccountStorage(
                                                _prefService, _syncService)];
}

// Returns the amount of local passwords.
- (int)computeLocalPasswordsCount {
  std::vector<password_manager::AffiliatedGroup> affiliatedGroups =
      _savedPasswordsPresenter->GetAffiliatedGroups();

  // Count passwords that don't appear in the account store.
  int passwordsCount = 0;
  for (password_manager::AffiliatedGroup group : affiliatedGroups) {
    passwordsCount += base::ranges::count_if(group.GetCredentials().begin(),
                                             group.GetCredentials().end(),
                                             IsCredentialNotInAccountStore);
  }

  return passwordsCount;
}

// Returns the list of distinct domains present in the local passwords. If they
// are in different affiliated groups, they are presumed to be distinct.
- (NSMutableArray<NSString*>*)computeDistinctDomainsFromLocalPasswords {
  std::vector<password_manager::AffiliatedGroup> affiliatedGroups =
      _savedPasswordsPresenter->GetAffiliatedGroups();

  // Add distinct domains for which there exists a password that doesn't appear
  // in the account store.
  NSMutableArray<NSString*>* distinctDomains = [NSMutableArray array];

  for (const password_manager::AffiliatedGroup& group : affiliatedGroups) {
    auto credential = base::ranges::find_if(group.GetCredentials().begin(),
                                            group.GetCredentials().end(),
                                            IsCredentialNotInAccountStore);

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
  [self.bulkMovePasswordsToAccountHandler
      showMovedToAccountSnackbarWithPasswordCount:count
                                        userEmail:_syncService->GetAccountInfo()
                                                      .email];
}

@end
