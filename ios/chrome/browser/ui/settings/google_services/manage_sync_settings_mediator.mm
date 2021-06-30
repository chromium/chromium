// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// Enterprise icon.
NSString* kGoogleServicesEnterpriseImage = @"google_services_enterprise";
// Sync error icon.
NSString* kGoogleServicesSyncErrorImage = @"google_services_sync_error";
}  // namespace

@interface ManageSyncSettingsMediator () <
    BooleanObserver,
    IdentityManagerObserverBridgeDelegate> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Sync State changes should be currently ignored.
  BOOL _ignoreSyncStateChanges;
}

// Preference value for kAutofillWalletImportEnabled.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteWalletPreference;
// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;
// Model item for sync everything.
@property(nonatomic, strong) SyncSwitchItem* syncEverythingItem;
// Model item for each data types.
@property(nonatomic, strong) NSArray<SyncSwitchItem*>* syncSwitchItems;
// Autocomplete wallet item.
@property(nonatomic, strong) SyncSwitchItem* autocompleteWalletItem;
// Encryption item.
@property(nonatomic, strong) TableViewImageItem* encryptionItem;
// Sync error item.
@property(nonatomic, strong) TableViewItem* syncErrorItem;
// Sign out and turn off sync item.
@property(nonatomic, strong) TableViewItem* signOutAndTurnOffSyncItem;
// Returns YES if the sync data items should be enabled.
@property(nonatomic, assign, readonly) BOOL shouldSyncDataItemEnabled;
// Returns whether the Sync settings should be disabled because of a Sync error.
@property(nonatomic, assign, readonly) BOOL disabledBecauseOfSyncError;
// Returns YES if the user cannot turn on sync for enterprise policy reasons.
@property(nonatomic, assign, readonly) BOOL isSyncDisabledByAdministrator;

@end

@implementation ManageSyncSettingsMediator

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    userPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    DCHECK(syncService);
    self.syncService = syncService;
    _syncObserver.reset(new SyncObserverBridge(self, syncService));
    _autocompleteWalletPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:autofill::prefs::kAutofillWalletImportEnabled];
    _autocompleteWalletPreference.observer = self;
  }
  return self;
}

#pragma mark - Loads sync data type section

// Loads the sync data type section.
- (void)loadSyncDataTypeSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  self.syncEverythingItem =
      [[SyncSwitchItem alloc] initWithType:SyncEverythingItemType];
  self.syncEverythingItem.text = GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE);
  [self updateSyncEverythingItemNotifyConsumer:NO];
  [model addItem:self.syncEverythingItem
      toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  self.syncSwitchItems = @[
    [self switchItemWithDataType:SyncSetupService::kSyncAutofill],
    [self switchItemWithDataType:SyncSetupService::kSyncBookmarks],
    [self switchItemWithDataType:SyncSetupService::kSyncOmniboxHistory],
    [self switchItemWithDataType:SyncSetupService::kSyncOpenTabs],
    [self switchItemWithDataType:SyncSetupService::kSyncPasswords],
    [self switchItemWithDataType:SyncSetupService::kSyncReadingList],
    [self switchItemWithDataType:SyncSetupService::kSyncPreferences]
  ];
  for (SyncSwitchItem* switchItem in self.syncSwitchItems) {
    [model addItem:switchItem
        toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  }
  self.autocompleteWalletItem =
      [[SyncSwitchItem alloc] initWithType:AutocompleteWalletItemType];
  self.autocompleteWalletItem.text =
      GetNSString(IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
  [model addItem:self.autocompleteWalletItem
      toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  [self updateSyncItemsNotifyConsumer:NO];
}

// Updates the sync everything item, and notify the consumer if |notifyConsumer|
// is set to YES.
- (void)updateSyncEverythingItemNotifyConsumer:(BOOL)notifyConsumer {
  BOOL shouldSyncEverythingBeEditable =
      (self.syncSetupService->CanSyncFeatureStart() ||
       base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) &&
      (!self.disabledBecauseOfSyncError || self.syncSettingsNotConfirmed);
  BOOL shouldSyncEverythingItemBeOn =
      self.syncSetupService->CanSyncFeatureStart() &&
      self.syncSetupService->IsSyncingAllDataTypes();
  BOOL needsUpdate =
      (self.syncEverythingItem.on != shouldSyncEverythingItemBeOn) ||
      (self.syncEverythingItem.enabled != shouldSyncEverythingBeEditable);
  self.syncEverythingItem.on = shouldSyncEverythingItemBeOn;
  self.syncEverythingItem.enabled = shouldSyncEverythingBeEditable;
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.syncEverythingItem];
  }
}

// Updates all the items related to sync (sync data items and autocomplete
// wallet item). The consumer is notified if |notifyConsumer| is set to YES.
- (void)updateSyncItemsNotifyConsumer:(BOOL)notifyConsumer {
  [self updateSyncDataItemsNotifyConsumer:notifyConsumer];
  [self updateAutocompleteWalletItemNotifyConsumer:notifyConsumer];
}

// Updates all the sync data type items, and notify the consumer if
// |notifyConsumer| is set to YES.
- (void)updateSyncDataItemsNotifyConsumer:(BOOL)notifyConsumer {
  for (SyncSwitchItem* syncSwitchItem in self.syncSwitchItems) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(
            syncSwitchItem.dataType);
    syncer::ModelType modelType = self.syncSetupService->GetModelType(dataType);
    BOOL isDataTypeSynced;
    if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
      isDataTypeSynced = self.syncSetupService->IsSyncRequested() &&
                         self.syncSetupService->IsDataTypePreferred(modelType);
    } else {
      isDataTypeSynced = self.syncSetupService->IsDataTypePreferred(modelType);
    }
    BOOL needsUpdate =
        (syncSwitchItem.on != isDataTypeSynced) ||
        (syncSwitchItem.isEnabled != self.shouldSyncDataItemEnabled);
    syncSwitchItem.on = isDataTypeSynced;
    syncSwitchItem.enabled = self.shouldSyncDataItemEnabled;
    if (needsUpdate && notifyConsumer) {
      [self.consumer reloadItem:syncSwitchItem];
    }
  }
}

// Updates the autocomplete wallet item. The consumer is notified if
// |notifyConsumer| is set to YES.
- (void)updateAutocompleteWalletItemNotifyConsumer:(BOOL)notifyConsumer {
  syncer::ModelType autofillModelType =
      self.syncSetupService->GetModelType(SyncSetupService::kSyncAutofill);
  BOOL isAutofillOn;
  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    isAutofillOn =
        self.syncSetupService->IsSyncRequested() &&
        self.syncSetupService->IsDataTypePreferred(autofillModelType);
  } else {
    isAutofillOn =
        self.syncSetupService->IsDataTypePreferred(autofillModelType);
  }
  BOOL autocompleteWalletEnabled =
      isAutofillOn && self.shouldSyncDataItemEnabled;
  BOOL autocompleteWalletOn;
  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    autocompleteWalletOn = self.syncSetupService->IsSyncRequested() &&
                           self.autocompleteWalletPreference.value;
  } else {
    autocompleteWalletOn = self.autocompleteWalletPreference.value;
  }
  BOOL needsUpdate =
      (self.autocompleteWalletItem.enabled != autocompleteWalletEnabled) ||
      (self.autocompleteWalletItem.on != autocompleteWalletOn);
  self.autocompleteWalletItem.enabled = autocompleteWalletEnabled;
  self.autocompleteWalletItem.on = autocompleteWalletOn;
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.autocompleteWalletItem];
  }
}

#pragma mark - Loads the advanced settings section

// Loads the advanced settings section.
- (void)loadAdvancedSettingsSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  // EncryptionItemType.
  self.encryptionItem =
      [[TableViewImageItem alloc] initWithType:EncryptionItemType];
  self.encryptionItem.title = GetNSString(IDS_IOS_MANAGE_SYNC_ENCRYPTION);
  // The detail text (if any) is an error message, so color it in red.
  self.encryptionItem.detailTextColor = [UIColor colorNamed:kRedColor];
  // For kSyncServiceNeedsTrustedVaultKey, the disclosure indicator should not
  // be shown since the reauth dialog for the trusted vault is presented from
  // the bottom, and is not part of navigation controller.
  BOOL hasDisclosureIndicator =
      self.syncSetupService->GetSyncServiceState() !=
      SyncSetupService::kSyncServiceNeedsTrustedVaultKey;
  self.encryptionItem.accessoryType =
      hasDisclosureIndicator ? UITableViewCellAccessoryDisclosureIndicator
                             : UITableViewCellAccessoryNone;
  [self updateEncryptionItem:NO];
  [model addItem:self.encryptionItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  // GoogleActivityControlsItemType.
  if (signin::IsSSOEditingEnabled()) {
    TableViewImageItem* googleActivityControlsItem = [[TableViewImageItem alloc]
        initWithType:GoogleActivityControlsItemType];
    googleActivityControlsItem.title =
        GetNSString(IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_TITLE);
    googleActivityControlsItem.detailText =
        GetNSString(IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_DESCRIPTION);
    googleActivityControlsItem.accessibilityTraits |=
        UIAccessibilityTraitButton;
    [model addItem:googleActivityControlsItem
        toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  }

  // AdvancedSettingsSectionIdentifier.
  TableViewImageItem* dataFromChromeSyncItem =
      [[TableViewImageItem alloc] initWithType:DataFromChromeSync];
  dataFromChromeSyncItem.title =
      GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_TITLE);
  dataFromChromeSyncItem.detailText =
      GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_DESCRIPTION);
  dataFromChromeSyncItem.accessibilityIdentifier =
      kDataFromChromeSyncAccessibilityIdentifier;
  dataFromChromeSyncItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:dataFromChromeSyncItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
}

// Updates encryption item, and notifies the consumer if |notifyConsumer| is set
// to YES.
- (void)updateEncryptionItem:(BOOL)notifyConsumer {
  BOOL needsUpdate =
      self.shouldEncryptionItemBeEnabled &&
      (self.encryptionItem.enabled != self.shouldEncryptionItemBeEnabled);
  if (self.shouldEncryptionItemBeEnabled &&
      self.syncSetupService->GetSyncServiceState() ==
          SyncSetupService::kSyncServiceNeedsPassphrase) {
    needsUpdate = needsUpdate || self.encryptionItem.image == nil;
    self.encryptionItem.image =
        [UIImage imageNamed:kGoogleServicesSyncErrorImage];
    self.encryptionItem.detailText = GetNSString(
        IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENTER_PASSPHRASE_TO_START_SYNC);
  } else if (self.shouldEncryptionItemBeEnabled &&
             self.syncSetupService->GetSyncServiceState() ==
                 SyncSetupService::kSyncServiceNeedsTrustedVaultKey) {
    needsUpdate = needsUpdate || self.encryptionItem.image == nil;
    self.encryptionItem.image =
        [UIImage imageNamed:kGoogleServicesSyncErrorImage];
    self.encryptionItem.detailText =
        GetNSString(self.syncSetupService->IsEncryptEverythingEnabled()
                        ? IDS_IOS_SYNC_ERROR_DESCRIPTION
                        : IDS_IOS_SYNC_PASSWORDS_ERROR_DESCRIPTION);
  } else {
    needsUpdate = needsUpdate || self.encryptionItem.image != nil;
    self.encryptionItem.image = nil;
    self.encryptionItem.detailText = nil;
  }
  self.encryptionItem.enabled = self.shouldEncryptionItemBeEnabled;
  if (self.shouldEncryptionItemBeEnabled) {
    self.encryptionItem.textColor = nil;
  } else {
    self.encryptionItem.textColor = UIColor.cr_secondaryLabelColor;
  }
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.encryptionItem];
  }
}

#pragma mark - Loads sign out section

- (void)loadSignOutSection {
  // The sign-out section will only apply to kMobileIdentityConsistency.
  if (!base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    return;
  }

  // Creates the sign-out item and its section.
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SignOutSectionIdentifier];
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:SignOutItemType];
  item.text = GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC);
  item.textColor = [UIColor colorNamed:kRedColor];
  self.signOutAndTurnOffSyncItem = item;

  // The user must be signed-in and syncing.
  if (!self.shouldDisplaySignoutSection) {
    return;
  }
  [model addItem:self.signOutAndTurnOffSyncItem
      toSectionWithIdentifier:SignOutSectionIdentifier];
}

- (void)updateSignOutSection {
  // The sign-out section will only apply to kMobileIdentityConsistency.
  if (!base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    return;
  }

  BOOL hasModelUpdate = NO;
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasSignOutItem = [model hasItem:self.signOutAndTurnOffSyncItem];
  if (!hasSignOutItem && self.shouldDisplaySignoutSection) {
    DCHECK(self.signOutAndTurnOffSyncItem);
    [model addItem:self.signOutAndTurnOffSyncItem
        toSectionWithIdentifier:SignOutSectionIdentifier];
    hasModelUpdate = YES;
  } else if (hasSignOutItem && !self.shouldDisplaySignoutSection) {
    [model removeItemWithType:SignOutItemType
        fromSectionWithIdentifier:SignOutSectionIdentifier];
    hasModelUpdate = YES;
  }

  if (hasModelUpdate) {
    NSUInteger sectionIndex =
        [model sectionForSectionIdentifier:SignOutSectionIdentifier];
    [self.consumer reloadSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
  }
}

#pragma mark - Private

// Creates a SyncSwitchItem instance.
- (SyncSwitchItem*)switchItemWithDataType:
    (SyncSetupService::SyncableDatatype)dataType {
  NSInteger itemType = 0;
  int textStringID = 0;
  switch (dataType) {
    case SyncSetupService::kSyncBookmarks:
      itemType = BookmarksDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_BOOKMARKS;
      break;
    case SyncSetupService::kSyncOmniboxHistory:
      itemType = HistoryDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TYPED_URLS;
      break;
    case SyncSetupService::kSyncPasswords:
      itemType = PasswordsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PASSWORDS;
      break;
    case SyncSetupService::kSyncOpenTabs:
      itemType = OpenTabsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TABS;
      break;
    case SyncSetupService::kSyncAutofill:
      itemType = AutofillDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_AUTOFILL;
      break;
    case SyncSetupService::kSyncPreferences:
      itemType = SettingsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PREFERENCES;
      break;
    case SyncSetupService::kSyncReadingList:
      itemType = ReadingListDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_READING_LIST;
      break;
    case SyncSetupService::kNumberOfSyncableDatatypes:
      NOTREACHED();
      break;
  }
  DCHECK_NE(itemType, 0);
  DCHECK_NE(textStringID, 0);
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  switchItem.dataType = dataType;
  return switchItem;
}

#pragma mark - Properties

- (BOOL)syncSettingsNotConfirmed {
  SyncSetupService::SyncServiceState state =
      self.syncSetupService->GetSyncServiceState();
  return state == SyncSetupService::kSyncSettingsNotConfirmed;
}

- (BOOL)disabledBecauseOfSyncError {
  switch (self.syncSetupService->GetSyncServiceState()) {
    case SyncSetupService::kSyncServiceUnrecoverableError:
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
    case SyncSetupService::kSyncSettingsNotConfirmed:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return YES;
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceNeedsPassphrase:
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      return NO;
  }
  NOTREACHED();
}

- (BOOL)shouldSyncDataItemEnabled {
  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    return (!self.syncSetupService->IsSyncingAllDataTypes() ||
            !self.syncSetupService->IsSyncRequested()) &&
           (!self.disabledBecauseOfSyncError || self.syncSettingsNotConfirmed);
  }
  return (!self.syncSetupService->IsSyncingAllDataTypes() &&
          self.syncSetupService->CanSyncFeatureStart() &&
          (!self.disabledBecauseOfSyncError || self.syncSettingsNotConfirmed));
}

- (BOOL)shouldEncryptionItemBeEnabled {
  return self.syncService->IsEngineInitialized() &&
         self.syncSetupService->CanSyncFeatureStart() &&
         !self.disabledBecauseOfSyncError;
}

- (BOOL)shouldDisplaySignoutSection {
  return self.syncSetupService->IsFirstSetupComplete() &&
         (self.syncSetupService->CanSyncFeatureStart() ||
          base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency));
}

#pragma mark - ManageSyncSettingsTableViewControllerModelDelegate

- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadSyncErrorsSection];
  [self loadSyncDataTypeSection];
  [self loadSignOutSection];
  [self loadAdvancedSettingsSection];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateAutocompleteWalletItemNotifyConsumer:YES];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_ignoreSyncStateChanges) {
    // The UI should not updated so the switch animations can run smoothly.
    return;
  }
  [self updateSyncErrorsSection:YES];
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
  [self updateEncryptionItem:YES];
  [self updateSignOutSection];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateSyncErrorsSection:YES];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - ManageSyncSettingsServiceDelegate

- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  {
    // The notifications should be ignored to get smooth switch animations.
    // Notifications are sent by SyncObserverModelBridge while changing
    // settings.
    base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
    SyncSwitchItem* syncSwitchItem = base::mac::ObjCCast<SyncSwitchItem>(item);
    syncSwitchItem.on = value;
    SyncSettingsItemType itemType =
        static_cast<SyncSettingsItemType>(item.type);
    switch (itemType) {
      case SyncEverythingItemType:
        self.syncSetupService->SetSyncingAllDataTypes(value);
        if (value) {
          // When sync everything is turned on, the autocomplete wallet
          // should be turned on. This code can be removed once
          // crbug.com/937234 is fixed.
          self.autocompleteWalletPreference.value = true;
        }
        break;
      case AutofillDataTypeItemType:
      case BookmarksDataTypeItemType:
      case HistoryDataTypeItemType:
      case OpenTabsDataTypeItemType:
      case PasswordsDataTypeItemType:
      case ReadingListDataTypeItemType:
      case SettingsDataTypeItemType: {
        DCHECK(syncSwitchItem);
        SyncSetupService::SyncableDatatype dataType =
            static_cast<SyncSetupService::SyncableDatatype>(
                syncSwitchItem.dataType);
        syncer::ModelType modelType =
            self.syncSetupService->GetModelType(dataType);
        self.syncSetupService->SetDataTypeEnabled(modelType, value);
        if (dataType == SyncSetupService::kSyncAutofill) {
          // When the auto fill data type is updated, the autocomplete wallet
          // should be updated too. Autocomplete wallet should not be enabled
          // when auto fill data type disabled. This behaviour not be
          // implemented in the UI code. This code can be removed once
          // crbug.com/937234 is fixed.
          self.autocompleteWalletPreference.value = value;
        }
        break;
      }
      case AutocompleteWalletItemType:
        self.autocompleteWalletPreference.value = value;
        break;
      case SignOutItemType:
      case EncryptionItemType:
      case GoogleActivityControlsItemType:
      case DataFromChromeSync:
      case RestartAuthenticationFlowErrorItemType:
      case ReauthDialogAsSyncIsInAuthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
      case SyncNeedsTrustedVaultKeyErrorItemType:
      case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      case SyncDisabledByAdministratorErrorItemType:
        NOTREACHED();
        break;
    }
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
}

- (void)didSelectItem:(TableViewItem*)item cellRect:(CGRect)cellRect {
  SyncSettingsItemType itemType = static_cast<SyncSettingsItemType>(item.type);
  switch (itemType) {
    case EncryptionItemType:
      if (self.syncSetupService->GetSyncServiceState() ==
          SyncSetupService::kSyncServiceNeedsTrustedVaultKey) {
        [self.syncErrorHandler openTrustedVaultReauthForFetchKeys];
        break;
      }
      [self.syncErrorHandler openPassphraseDialog];
      break;
    case GoogleActivityControlsItemType:
      [self.commandHandler openWebAppActivityDialog];
      break;
    case DataFromChromeSync:
      [self.commandHandler openDataFromChromeSyncWebPage];
      break;
    case RestartAuthenticationFlowErrorItemType:
      [self.syncErrorHandler restartAuthenticationFlow];
      break;
    case ReauthDialogAsSyncIsInAuthErrorItemType:
      [self.syncErrorHandler openReauthDialogAsSyncIsInAuthError];
      break;
    case ShowPassphraseDialogErrorItemType:
      [self.syncErrorHandler openPassphraseDialog];
      break;
    case SyncNeedsTrustedVaultKeyErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauthForFetchKeys];
      break;
    case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauthForDegradedRecoverability];
      break;
    case SignOutItemType:
      [self.commandHandler showTurnOffSyncOptionsFromTargetRect:cellRect];
      break;
    case SyncEverythingItemType:
    case AutofillDataTypeItemType:
    case BookmarksDataTypeItemType:
    case HistoryDataTypeItemType:
    case OpenTabsDataTypeItemType:
    case PasswordsDataTypeItemType:
    case ReadingListDataTypeItemType:
    case SettingsDataTypeItemType:
    case AutocompleteWalletItemType:
    case SyncDisabledByAdministratorErrorItemType:
      // Nothing to do.
      break;
  }
}

// Creates an item to display the sync error. |itemType| should only be one of
// those types:
//   + RestartAuthenticationFlowErrorItemType
//   + ReauthDialogAsSyncIsInAuthErrorItemType
//   + ShowPassphraseDialogErrorItemType
//   + SyncNeedsTrustedVaultKeyErrorItemType
//   + SyncTrustedVaultRecoverabilityDegradedErrorItemType
- (TableViewItem*)createSyncErrorItemWithItemType:(NSInteger)itemType {
  DCHECK(itemType == RestartAuthenticationFlowErrorItemType ||
         itemType == ReauthDialogAsSyncIsInAuthErrorItemType ||
         itemType == ShowPassphraseDialogErrorItemType ||
         itemType == SyncNeedsTrustedVaultKeyErrorItemType ||
         itemType == SyncTrustedVaultRecoverabilityDegradedErrorItemType);
  SettingsImageDetailTextItem* syncErrorItem =
      [[SettingsImageDetailTextItem alloc] initWithType:itemType];
  syncErrorItem.text = GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncSetupService(self.syncSetupService);
  switch (itemType) {
    case ShowPassphraseDialogErrorItemType:
      // Special case only for the sync passphrase error message. The regular
      // error message should be still be displayed in the first settings
      // screen.
      syncErrorItem.detailText = GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENTER_PASSPHRASE_TO_START_SYNC);
      break;
    case SyncNeedsTrustedVaultKeyErrorItemType:
      syncErrorItem.detailText =
          GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_ENCRYPTION_FIX_NOW);

      // Also override the title to be more accurate, if only passwords are
      // being encrypted.
      if (!self.syncSetupService->IsEncryptEverythingEnabled()) {
        syncErrorItem.text = GetNSString(IDS_IOS_SYNC_PASSWORDS_ERROR_TITLE);
      }
      break;
    case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      syncErrorItem.detailText = GetNSString(
          self.syncSetupService->IsEncryptEverythingEnabled()
              ? IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_FIX_RECOVERABILITY_DEGRADED_FOR_EVERYTHING
              : IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_FIX_RECOVERABILITY_DEGRADED_FOR_PASSWORDS);

      // Also override the title to be more accurate, if only passwords are
      // being encrypted.
      if (!self.syncSetupService->IsEncryptEverythingEnabled()) {
        syncErrorItem.text = GetNSString(IDS_IOS_SYNC_PASSWORDS_ERROR_TITLE);
      }
      break;
  }
  syncErrorItem.image = [UIImage imageNamed:kGoogleServicesSyncErrorImage];
  return syncErrorItem;
}

// Loads the sync errors section.
- (void)loadSyncErrorsSection {
  // The |self.consumer.tableViewModel| will be reset prior to this method.
  // Ignore any previous value the |self.syncErrorItem| may have contained.
  self.syncErrorItem = nil;
  [self updateSyncErrorsSection:NO];
}

// Updates the sync errors section. If |notifyConsumer| is YES, the consumer is
// notified about model changes.
- (void)updateSyncErrorsSection:(BOOL)notifyConsumer {
  if (!base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency) ||
      !self.syncSetupService->HasFinishedInitialSetup()) {
    return;
  }

  // Checks if the sync setup service state has changed from the saved state in
  // the table view model.
  absl::optional<SyncSettingsItemType> type = [self syncErrorItemType];
  if (![self needsSyncSetupServiceStateUpdate:type]) {
    return;
  }

  TableViewModel* model = self.consumer.tableViewModel;
  // There is no error in sync setup service, but there previously was an error.
  if (!type.has_value()) {
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:SyncErrorsSectionIdentifier];
    [model removeSectionWithIdentifier:SyncErrorsSectionIdentifier];
    self.syncErrorItem = nil;

    // Remove the sync error section from the table view model.
    if (notifyConsumer) {
      NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
      [self.consumer deleteSections:indexSet];
    }
    return;
  }

  // There is an error in the sync setup service with no previous error.
  BOOL hasPreviousError = self.syncErrorItem;

  // Create the new sync error item.
  DCHECK(type.has_value());
  if (type.value() == SyncDisabledByAdministratorErrorItemType) {
    self.syncErrorItem = [self createSyncDisabledByAdministratorErrorItem];
  } else {
    self.syncErrorItem = [self createSyncErrorItemWithItemType:type.value()];
  }

  if (!hasPreviousError) {
    [model insertSectionWithIdentifier:SyncErrorsSectionIdentifier atIndex:0];
    [model addItem:self.syncErrorItem
        toSectionWithIdentifier:SyncErrorsSectionIdentifier];
  }

  if (notifyConsumer) {
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:0];
    if (hasPreviousError) {
      [self.consumer reloadSections:indexSet];
    } else {
      [self.consumer insertSections:indexSet];
    }
  }
}

// Returns the sync error item type or absl::nullopt if the item
// is not an error.
- (absl::optional<SyncSettingsItemType>)syncErrorItemType {
  if (self.isSyncDisabledByAdministrator) {
    return absl::make_optional<SyncSettingsItemType>(
        SyncDisabledByAdministratorErrorItemType);
  }
  switch (self.syncSetupService->GetSyncServiceState()) {
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return absl::make_optional<SyncSettingsItemType>(
          RestartAuthenticationFlowErrorItemType);
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return absl::make_optional<SyncSettingsItemType>(
          ReauthDialogAsSyncIsInAuthErrorItemType);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return absl::make_optional<SyncSettingsItemType>(
          ShowPassphraseDialogErrorItemType);
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
      return absl::make_optional<SyncSettingsItemType>(
          SyncNeedsTrustedVaultKeyErrorItemType);
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      return absl::make_optional<SyncSettingsItemType>(
          SyncTrustedVaultRecoverabilityDegradedErrorItemType);
    case SyncSetupService::kSyncSettingsNotConfirmed:
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return absl::nullopt;
  }
  NOTREACHED();
  return absl::nullopt;
}

// Returns whether the sync setup service state has changed since the last
// update.
- (BOOL)needsSyncSetupServiceStateUpdate:
    (absl::optional<SyncSettingsItemType>)type {
  BOOL hasError = type.has_value();
  return (hasError && !self.syncErrorItem) ||
         (!hasError && self.syncErrorItem) ||
         (hasError && self.syncErrorItem &&
          type.value() != self.syncErrorItem.type);
}

// Returns an item to show to the user the sync cannot be turned on for an
// enterprise policy reason.
- (TableViewItem*)createSyncDisabledByAdministratorErrorItem {
  TableViewImageItem* item = [[TableViewImageItem alloc]
      initWithType:SyncDisabledByAdministratorErrorItemType];
  item.image = [UIImage imageNamed:kGoogleServicesEnterpriseImage];
  item.title = GetNSString(
      IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_DISABLBED_BY_ADMINISTRATOR_TITLE);
  item.enabled = NO;
  item.textColor = UIColor.cr_secondaryLabelColor;
  return item;
}

#pragma mark - Properties

- (BOOL)isSyncDisabledByAdministrator {
  return self.syncService->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

@end
