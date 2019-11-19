// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

NSString* const kDataFromChromeSyncAccessibilityIdentifier =
    @"DataFromChromeSyncAccessibilityIdentifier";

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  // Section for all the sync settings.
  SyncDataTypeSectionIdentifier = kSectionIdentifierEnumZero,
  // Advanced settings.
  AdvancedSettingsSectionIdentifier,
};

// List of items. For implementation details in
// ManageSyncSettingsTableViewController, two SyncSwitchItem items should not
// share the same type. The cell UISwitch tag is used to save the item type, and
// when the user taps on the switch, this tag is used to retreive the item based
// on the type.
typedef NS_ENUM(NSInteger, ItemType) {
  // SyncDataTypeSectionIdentifier section.
  // Sync everything item.
  SyncEverythingItemType = kItemTypeEnumZero,
  // kSyncAutofill.
  AutofillDataTypeItemType,
  // kSyncBookmarks.
  BookmarksDataTypeItemType,
  // kSyncOmniboxHistory.
  HistoryDataTypeItemType,
  // kSyncOpenTabs.
  OpenTabsDataTypeItemType,
  // kSyncPasswords
  PasswordsDataTypeItemType,
  // kSyncReadingList.
  ReadingListDataTypeItemType,
  // kSyncPreferences.
  SettingsDataTypeItemType,
  // Item for kAutofillWalletImportEnabled.
  AutocompleteWalletItemType,
  // AdvancedSettingsSectionIdentifier section.
  // Encryption item.
  EncryptionItemType,
  // Google activity controls item.
  GoogleActivityControlsItemType,
  // Data from Chrome sync.
  DataFromChromeSync,
};

NSString* kGoogleServicesSyncErrorImage = @"google_services_sync_error";

}  // namespace

@interface ManageSyncSettingsMediator () <BooleanObserver,
                                          SyncObserverModelBridge> {
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
// Returns YES if the sync data items should be enabled.
@property(nonatomic, assign, readonly) BOOL shouldSyncDataItemEnabled;
// Returns whether the Sync settings should be disabled because of a Sync error.
@property(nonatomic, assign, readonly) BOOL disabledBecauseOfSyncError;

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
      self.syncSetupService->IsSyncEnabled() &&
      !self.disabledBecauseOfSyncError;
  BOOL shouldSyncEverythingItemBeOn =
      self.syncSetupService->IsSyncEnabled() &&
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
    BOOL isDataTypeSynced =
        self.syncSetupService->IsDataTypePreferred(modelType);
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
  BOOL isAutofillOn =
      self.syncSetupService->IsDataTypePreferred(autofillModelType);
  BOOL autocompleteWalletEnabled =
      isAutofillOn && self.shouldSyncDataItemEnabled;
  BOOL autocompleteWalletOn = self.autocompleteWalletPreference.value;
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
  self.encryptionItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:self.encryptionItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  [self updateEncryptionItem:NO];

  // GoogleActivityControlsItemType.
  TableViewImageItem* googleActivityControlsItem =
      [[TableViewImageItem alloc] initWithType:GoogleActivityControlsItemType];
  googleActivityControlsItem.title =
      GetNSString(IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_TITLE);
  googleActivityControlsItem.detailText =
      GetNSString(IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_DESCRIPTION);
  googleActivityControlsItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:googleActivityControlsItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

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
    [self.consumer reloadItem:self.self.encryptionItem];
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

- (BOOL)disabledBecauseOfSyncError {
  SyncSetupService::SyncServiceState state =
      self.syncSetupService->GetSyncServiceState();
  return state != SyncSetupService::kNoSyncServiceError &&
         state != SyncSetupService::kSyncServiceNeedsPassphrase;
}

- (BOOL)shouldSyncDataItemEnabled {
  return (!self.syncSetupService->IsSyncingAllDataTypes() &&
          self.syncSetupService->IsSyncEnabled() &&
          !self.disabledBecauseOfSyncError);
}

- (BOOL)shouldEncryptionItemBeEnabled {
  return self.syncService->IsEngineInitialized() &&
         self.syncSetupService->IsSyncEnabled() &&
         !self.disabledBecauseOfSyncError;
}

#pragma mark - ManageSyncSettingsTableViewControllerModelDelegate

- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadSyncDataTypeSection];
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
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
  [self updateEncryptionItem:YES];
}

#pragma mark - ManageSyncSettingsServiceDelegate

- (void)toggleSwitchItem:(SyncSwitchItem*)switchItem withValue:(BOOL)value {
  {
    // The notifications should be ignored to get smooth switch animations.
    // Notifications are sent by SyncObserverModelBridge while changing
    // settings.
    base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
    switchItem.on = value;
    ItemType itemType = static_cast<ItemType>(switchItem.type);
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
        SyncSetupService::SyncableDatatype dataType =
            static_cast<SyncSetupService::SyncableDatatype>(
                switchItem.dataType);
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
      case EncryptionItemType:
      case GoogleActivityControlsItemType:
      case DataFromChromeSync:
        NOTREACHED();
        break;
    }
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
}

- (void)didSelectItem:(TableViewItem*)item {
  ItemType itemType = static_cast<ItemType>(item.type);
  switch (itemType) {
    case EncryptionItemType:
      [self.commandHandler openPassphraseDialog];
      break;
    case GoogleActivityControlsItemType:
      [self.commandHandler openWebAppActivityDialog];
      break;
    case DataFromChromeSync:
      [self.commandHandler openDataFromChromeSyncWebPage];
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
      // Nothing to do.
      break;
  }
}

@end
