// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/containers/fixed_flat_map.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_central_account_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// Ordered list of all sync switches.
static const syncer::UserSelectableType kSyncSwitchItems[] = {
    syncer::UserSelectableType::kAutofill,
    syncer::UserSelectableType::kBookmarks,
    syncer::UserSelectableType::kHistory,
    syncer::UserSelectableType::kTabs,
    syncer::UserSelectableType::kPasswords,
    syncer::UserSelectableType::kReadingList,
    syncer::UserSelectableType::kPreferences};

// Returns the configuration to be used for the accessory.
UIImageConfiguration* AccessoryConfiguration() {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolAccessoryPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
}

// Enterprise icon.
NSString* const kGoogleServicesEnterpriseImage = @"google_services_enterprise";

}  // namespace

@interface ManageSyncSettingsMediator () <BooleanObserver,
                                          IdentityManagerObserverBridgeDelegate,
                                          ChromeAccountManagerServiceObserver>

// Model item for sync everything.
@property(nonatomic, strong) TableViewItem* syncEverythingItem;
// Model item for each data types.
@property(nonatomic, strong) NSArray<TableViewItem*>* syncSwitchItems;
// Autocomplete wallet item.
@property(nonatomic, strong) TableViewItem* autocompleteWalletItem;
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

@implementation ManageSyncSettingsMediator {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Sync State changes should be currently ignored.
  BOOL _ignoreSyncStateChanges;
  // Preference value for kAutofillWalletImportEnabled.
  PrefBackedBoolean* _autocompleteWalletPreference;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Authentication service.
  AuthenticationService* _authenticationService;
  // Account manager service to retrieve Chrome identities.
  ChromeAccountManagerService* _chromeAccountManagerService;
  // Chrome account manager service observer bridge.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountAccountManagerServiceObserver;
  // Signed-in identity. Note: may be nil while signing out.
  id<SystemIdentity> _signedInIdentity;
}

- (instancetype)
      initWithSyncService:(syncer::SyncService*)syncService
          userPrefService:(PrefService*)userPrefService
          identityManager:(signin::IdentityManager*)identityManager
    authenticationService:(AuthenticationService*)authenticationService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
      initialAccountState:(SyncSettingsAccountState)initialAccountState {
  self = [super init];
  if (self) {
    DCHECK(syncService);
    CHECK(authenticationService);
    _authenticationService = authenticationService;
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
    _autocompleteWalletPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:autofill::prefs::kAutofillWalletImportEnabled];
    _autocompleteWalletPreference.observer = self;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _authenticationService = authenticationService;
    _chromeAccountManagerService = accountManagerService;
    _accountAccountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _chromeAccountManagerService);
    _signedInIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    _initialAccountState = initialAccountState;
  }
  return self;
}

- (void)disconnect {
  _authenticationService = nullptr;
  _syncObserver.reset();
  _syncService = nullptr;
  _autocompleteWalletPreference.observer = nil;
  [_autocompleteWalletPreference stop];
  _autocompleteWalletPreference = nil;
  _identityManagerObserver.reset();
  _authenticationService = nullptr;
  _chromeAccountManagerService = nullptr;
  _accountAccountManagerServiceObserver.reset();
  _signedInIdentity = nil;
}

#pragma mark - Loads sync data type section

// Loads the centered identity account section.
- (void)loadIdentityAccountSection {
  TableViewModel* model = self.consumer.tableViewModel;
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
    case SyncSettingsAccountState::kSyncing:
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
      return;
    case SyncSettingsAccountState::kSignedIn:
      [model addSectionWithIdentifier:AccountSectionIdentifier];
      CHECK(_signedInIdentity);
      TableViewCentralAccountItem* identityAccountItem =
          [[TableViewCentralAccountItem alloc]
              initWithType:IdentityAccountItemType];
      identityAccountItem.avatarImage =
          _chromeAccountManagerService->GetIdentityAvatarWithIdentity(
              _signedInIdentity, IdentityAvatarSize::ExtraLarge);
      identityAccountItem.name = _signedInIdentity.userFullName;
      identityAccountItem.email = _signedInIdentity.userEmail;
      [model addItem:identityAccountItem
          toSectionWithIdentifier:AccountSectionIdentifier];
      break;
  }
}

// Loads the sync data type section.
- (void)loadSyncDataTypeSection {
  TableViewModel* model = self.consumer.tableViewModel;
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      return;
    case SyncSettingsAccountState::kSyncing:
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
      [model addSectionWithIdentifier:SyncDataTypeSectionIdentifier];
      if (self.allItemsAreSynceable) {
        SyncSwitchItem* button =
            [[SyncSwitchItem alloc] initWithType:SyncEverythingItemType];
        button.text = GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE);
        button.accessibilityIdentifier =
            kSyncEverythingItemAccessibilityIdentifier;
        self.syncEverythingItem = button;
        [self updateSyncEverythingItemNotifyConsumer:NO];
      } else {
        TableViewInfoButtonItem* button = [[TableViewInfoButtonItem alloc]
            initWithType:SyncEverythingItemType];
        button.text = GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE);
        button.statusText = GetNSString(IDS_IOS_SETTING_OFF);
        button.accessibilityIdentifier =
            kSyncEverythingItemAccessibilityIdentifier;
        self.syncEverythingItem = button;
      }
      self.syncEverythingItem.accessibilityIdentifier =
          kSyncEverythingItemAccessibilityIdentifier;
      [model addItem:self.syncEverythingItem
          toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
      break;
    case SyncSettingsAccountState::kSignedIn:
      [model addSectionWithIdentifier:SyncDataTypeSectionIdentifier];
      TableViewTextHeaderFooterItem* headerItem =
          [[TableViewTextHeaderFooterItem alloc]
              initWithType:TypesListHeaderOrFooterType];
      headerItem.text = l10n_util::GetNSString(
          IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TYPES_LIST_HEADER);
      [model setHeader:headerItem
          forSectionWithIdentifier:SyncDataTypeSectionIdentifier];

      TableViewTextHeaderFooterItem* footerItem =
          [[TableViewTextHeaderFooterItem alloc]
              initWithType:TypesListHeaderOrFooterType];
      footerItem.subtitle = l10n_util::GetNSString(
          IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TYPES_LIST_DESCRIPTION);
      [model setFooter:footerItem
          forSectionWithIdentifier:SyncDataTypeSectionIdentifier];
      break;
  }
  NSMutableArray* syncSwitchItems = [[NSMutableArray alloc] init];

  for (syncer::UserSelectableType dataType : kSyncSwitchItems) {
    TableViewItem* switchItem = [self tableViewItemWithDataType:dataType];
    [syncSwitchItems addObject:switchItem];
    [model addItem:switchItem
        toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  }
  self.syncSwitchItems = syncSwitchItems;

  if (![self isManagedSyncSettingsDataType:syncer::UserSelectableType::
                                               kAutofill]) {
    SyncSwitchItem* button =
        [[SyncSwitchItem alloc] initWithType:AutocompleteWalletItemType];
    button.text =
        GetNSString(IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
    self.autocompleteWalletItem = button;
  } else {
    TableViewInfoButtonItem* button = [[TableViewInfoButtonItem alloc]
        initWithType:AutocompleteWalletItemType];
    button.text =
        GetNSString(IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
    button.statusText = GetNSString(IDS_IOS_SETTING_OFF);
    self.autocompleteWalletItem = button;
  }
  [model addItem:self.autocompleteWalletItem
      toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
  [self updateSyncItemsNotifyConsumer:NO];
}

// Updates the sync everything item, and notify the consumer if `notifyConsumer`
// is set to YES.
- (void)updateSyncEverythingItemNotifyConsumer:(BOOL)notifyConsumer {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
    case SyncSettingsAccountState::kSignedIn:
      return;
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
    case SyncSettingsAccountState::kSyncing:
      if ([self.syncEverythingItem
              isKindOfClass:[TableViewInfoButtonItem class]]) {
        // It's possible that the sync everything pref remains true when a
        // policy change doesn't allow to sync everthing anymore. Fix that here.
        BOOL isSyncingEverything =
            self.syncSetupService->IsSyncEverythingEnabled();
        BOOL canSyncEverything = self.allItemsAreSynceable;
        if (isSyncingEverything && !canSyncEverything) {
          self.syncSetupService->SetSyncEverythingEnabled(NO);
        }
        return;
      }

      BOOL shouldSyncEverythingBeEditable = !self.disabledBecauseOfSyncError;
      BOOL shouldSyncEverythingItemBeOn =
          self.syncSetupService->IsSyncEverythingEnabled();
      SyncSwitchItem* syncEverythingItem =
          base::mac::ObjCCastStrict<SyncSwitchItem>(self.syncEverythingItem);
      BOOL needsUpdate =
          (syncEverythingItem.on != shouldSyncEverythingItemBeOn) ||
          (syncEverythingItem.enabled != shouldSyncEverythingBeEditable);
      syncEverythingItem.on = shouldSyncEverythingItemBeOn;
      syncEverythingItem.enabled = shouldSyncEverythingBeEditable;
      if (needsUpdate && notifyConsumer) {
        [self.consumer reloadItem:self.syncEverythingItem];
      }
      break;
  }
}

- (void)updateIdentityAccountSection {
  if (![self.consumer.tableViewModel
          hasItemForItemType:IdentityAccountItemType
           sectionIdentifier:AccountSectionIdentifier]) {
    return;
  }

  NSIndexPath* accountCellIndexPath = [self.consumer.tableViewModel
      indexPathForItemType:IdentityAccountItemType
         sectionIdentifier:AccountSectionIdentifier];
  TableViewCentralAccountItem* identityAccountItem =
      base::mac::ObjCCast<TableViewCentralAccountItem>(
          [self.consumer.tableViewModel itemAtIndexPath:accountCellIndexPath]);
  CHECK(identityAccountItem);
  CHECK(_signedInIdentity);
  identityAccountItem.avatarImage =
      _chromeAccountManagerService->GetIdentityAvatarWithIdentity(
          _signedInIdentity, IdentityAvatarSize::ExtraLarge);
  identityAccountItem.name = _signedInIdentity.userFullName;
  identityAccountItem.email = _signedInIdentity.userEmail;
  [self.consumer reloadItem:identityAccountItem];
}

// Updates all the items related to sync (sync data items and autocomplete
// wallet item). The consumer is notified if `notifyConsumer` is set to YES.
- (void)updateSyncItemsNotifyConsumer:(BOOL)notifyConsumer {
  [self updateSyncDataItemsNotifyConsumer:notifyConsumer];
  [self updateAutocompleteWalletItemNotifyConsumer:notifyConsumer];
}

// Updates all the sync data type items, and notify the consumer if
// `notifyConsumer` is set to YES.
- (void)updateSyncDataItemsNotifyConsumer:(BOOL)notifyConsumer {
  for (TableViewItem* item in self.syncSwitchItems) {
    if ([item isKindOfClass:[TableViewInfoButtonItem class]])
      continue;

    SyncSwitchItem* syncSwitchItem = base::mac::ObjCCast<SyncSwitchItem>(item);
    syncer::UserSelectableType dataType =
        static_cast<syncer::UserSelectableType>(syncSwitchItem.dataType);
    BOOL isDataTypeSynced =
        self.syncSetupService->IsDataTypePreferred(dataType);
    BOOL isEnabled = self.shouldSyncDataItemEnabled &&
                     ![self isManagedSyncSettingsDataType:dataType];
    BOOL needsUpdate = (syncSwitchItem.on != isDataTypeSynced) ||
                       (syncSwitchItem.isEnabled != isEnabled);
    syncSwitchItem.on = isDataTypeSynced;
    syncSwitchItem.enabled = isEnabled;
    if (needsUpdate && notifyConsumer) {
      [self.consumer reloadItem:syncSwitchItem];
    }
  }
}

// Updates the autocomplete wallet item. The consumer is notified if
// `notifyConsumer` is set to YES.
- (void)updateAutocompleteWalletItemNotifyConsumer:(BOOL)notifyConsumer {
  if ([self.autocompleteWalletItem
          isKindOfClass:[TableViewInfoButtonItem class]])
    return;

  SyncSwitchItem* syncSwitchItem =
      base::mac::ObjCCast<SyncSwitchItem>(self.autocompleteWalletItem);
  BOOL isAutofillOn = self.syncSetupService->IsDataTypePreferred(
      syncer::UserSelectableType::kAutofill);
  BOOL autocompleteWalletEnabled =
      isAutofillOn && self.shouldSyncDataItemEnabled;
  BOOL autocompleteWalletOn = _autocompleteWalletPreference.value;
  BOOL needsUpdate = (syncSwitchItem.enabled != autocompleteWalletEnabled) ||
                     (syncSwitchItem.on != autocompleteWalletOn);
  syncSwitchItem.enabled = autocompleteWalletEnabled;
  syncSwitchItem.on = autocompleteWalletOn;
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.autocompleteWalletItem];
  }
}

#pragma mark - Loads the advanced settings section

// Loads the advanced settings section.
- (void)loadAdvancedSettingsSection {
  if (self.syncAccountState == SyncSettingsAccountState::kSignedOut) {
    return;
  }
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  // EncryptionItemType.
  self.encryptionItem =
      [[TableViewImageItem alloc] initWithType:EncryptionItemType];
  self.encryptionItem.accessibilityIdentifier =
      kEncryptionAccessibilityIdentifier;
  self.encryptionItem.title = GetNSString(IDS_IOS_MANAGE_SYNC_ENCRYPTION);
  // The detail text (if any) is an error message, so color it in red.
  self.encryptionItem.detailTextColor = [UIColor colorNamed:kRedColor];
  // For kNeedsTrustedVaultKey, the disclosure indicator should not
  // be shown since the reauth dialog for the trusted vault is presented from
  // the bottom, and is not part of navigation controller.
  const syncer::SyncService::UserActionableError error =
      _syncService->GetUserActionableError();
  BOOL hasDisclosureIndicator =
      error != syncer::SyncService::UserActionableError::
                   kNeedsTrustedVaultKeyForPasswords &&
      error != syncer::SyncService::UserActionableError::
                   kNeedsTrustedVaultKeyForEverything;
  if (hasDisclosureIndicator) {
    self.encryptionItem.accessoryView = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithConfiguration(kChevronForwardSymbol,
                                                     AccessoryConfiguration())];
    self.encryptionItem.accessoryView.tintColor =
        [UIColor colorNamed:kTextQuaternaryColor];
  } else {
    self.encryptionItem.accessoryView = nil;
  }
  self.encryptionItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [self updateEncryptionItem:NO];
  [model addItem:self.encryptionItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  // GoogleActivityControlsItemType.
  TableViewImageItem* googleActivityControlsItem =
      [[TableViewImageItem alloc] initWithType:GoogleActivityControlsItemType];
  googleActivityControlsItem.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(kExternalLinkSymbol,
                                                   AccessoryConfiguration())];
  googleActivityControlsItem.accessoryView.tintColor =
      [UIColor colorNamed:kTextQuaternaryColor];
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
  dataFromChromeSyncItem.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(kExternalLinkSymbol,
                                                   AccessoryConfiguration())];
  dataFromChromeSyncItem.accessoryView.tintColor =
      [UIColor colorNamed:kTextQuaternaryColor];
  dataFromChromeSyncItem.title =
      GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_TITLE);
  dataFromChromeSyncItem.detailText =
      GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_DESCRIPTION);
  dataFromChromeSyncItem.accessibilityIdentifier =
      kDataFromChromeSyncAccessibilityIdentifier;
  dataFromChromeSyncItem.accessibilityTraits |= UIAccessibilityTraitButton;

  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedIn:
    case SyncSettingsAccountState::kSyncing:
      [model addItem:dataFromChromeSyncItem
          toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
      break;
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
      break;
    case SyncSettingsAccountState::kSignedOut:
      NOTREACHED();
  }
}

// Updates encryption item, and notifies the consumer if `notifyConsumer` is set
// to YES.
- (void)updateEncryptionItem:(BOOL)notifyConsumer {
  BOOL needsUpdate =
      self.shouldEncryptionItemBeEnabled &&
      (self.encryptionItem.enabled != self.shouldEncryptionItemBeEnabled);
  self.encryptionItem.enabled = self.shouldEncryptionItemBeEnabled;
  if (self.shouldEncryptionItemBeEnabled) {
    self.encryptionItem.textColor = nil;
  } else {
    self.encryptionItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  if (needsUpdate && notifyConsumer) {
    [self.consumer reloadItem:self.encryptionItem];
  }
}

#pragma mark - Loads sign out section

- (void)loadSignOutAndTurnOffSyncSection {
  // The SignOutAndTurnOffSyncSection only exists in
  // SyncSettingsAccountState::kSyncing state.
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      // kSignedOut is a temporary state; it only exists if the user just signed
      // out and the UI is in the process of being dismissed. In this case,
      // don't bother updating the section.
      return;
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
      CHECK(!self.signOutAndTurnOffSyncItem);
      return;
    case SyncSettingsAccountState::kSignedIn:
      // For kSignedIn, loadSignOutAndManageAccountsSection will load the
      // corresponding section.
      return;
    case SyncSettingsAccountState::kSyncing:
      break;
  }
  // Creates the sign-out item and its section.
  TableViewModel* model = self.consumer.tableViewModel;
  NSInteger syncDataTypeSectionIndex =
      [model sectionForSectionIdentifier:SyncDataTypeSectionIdentifier];
  DCHECK_NE(NSNotFound, syncDataTypeSectionIndex);
  [model insertSectionWithIdentifier:SignOutSectionIdentifier
                             atIndex:syncDataTypeSectionIndex + 1];
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:SignOutAndTurnOffSyncItemType];
  item.text = GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC);
  item.textColor = [UIColor colorNamed:kRedColor];
  self.signOutAndTurnOffSyncItem = item;
  [model addItem:self.signOutAndTurnOffSyncItem
      toSectionWithIdentifier:SignOutSectionIdentifier];

  if (self.forcedSigninEnabled) {
    // Add information about the forced sign-in policy below the sign-out
    // button when forced sign-in is enabled.
    TableViewLinkHeaderFooterItem* footerItem =
        [[TableViewLinkHeaderFooterItem alloc]
            initWithType:SignOutItemFooterType];
    footerItem.text = l10n_util::GetNSString(
        IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE);
    footerItem.urls =
        @[ [[CrURL alloc] initWithGURL:GURL("chrome://management/")] ];
    [model setFooter:footerItem
        forSectionWithIdentifier:SignOutSectionIdentifier];
  }
}

- (void)updateSignOutSection {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasSignOutSection =
      [model hasSectionForSectionIdentifier:SignOutSectionIdentifier];

  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      break;
    case SyncSettingsAccountState::kSignedIn:
      // There should be a sign-out section. Load it if it's not there yet.
      if (!hasSignOutSection) {
        [self loadSignOutAndManageAccountsSection];
        NSUInteger sectionIndex =
            [model sectionForSectionIdentifier:SignOutSectionIdentifier];
        [self.consumer
            insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
      }
      break;
    case SyncSettingsAccountState::kSyncing:
      // There should be a sign-out section. Load it if it's not there yet.
      if (!hasSignOutSection) {
        [self loadSignOutAndTurnOffSyncSection];
        DCHECK(self.signOutAndTurnOffSyncItem);
        NSUInteger sectionIndex =
            [model sectionForSectionIdentifier:SignOutSectionIdentifier];
        [self.consumer
            insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
      }
      break;
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
      // There shouldn't be a sign-out section. Remove it if it's there.
      if (hasSignOutSection) {
        NSUInteger sectionIndex =
            [model sectionForSectionIdentifier:SignOutSectionIdentifier];
        [model removeSectionWithIdentifier:SignOutSectionIdentifier];
        self.signOutAndTurnOffSyncItem = nil;
        [self.consumer
            deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
      }
      break;
  }
}

- (void)loadSignOutAndManageAccountsSection {
  if (self.syncAccountState != SyncSettingsAccountState::kSignedIn) {
    return;
  }

  // Creates the manage accounts and sign-out section.
  TableViewModel* model = self.consumer.tableViewModel;
  NSInteger advancedSettingsSectionIndex =
      [model sectionForSectionIdentifier:AdvancedSettingsSectionIdentifier];
  DCHECK_NE(NSNotFound, advancedSettingsSectionIndex);
  [model insertSectionWithIdentifier:SignOutSectionIdentifier
                             atIndex:advancedSettingsSectionIndex + 1];

  // Creates items in the manage accounts and sign-out section.
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:SignOutItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM);
  item.textColor = [UIColor colorNamed:kBlueColor];
  [model addItem:item toSectionWithIdentifier:SignOutSectionIdentifier];
}

#pragma mark - Private

// Creates a SyncSwitchItem or TableViewInfoButtonItem instance if the item is
// managed.
- (TableViewItem*)tableViewItemWithDataType:
    (syncer::UserSelectableType)dataType {
  NSInteger itemType = 0;
  int textStringID = 0;
  NSString* accessibilityIdentifier = nil;
  switch (dataType) {
    case syncer::UserSelectableType::kBookmarks:
      itemType = BookmarksDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_BOOKMARKS;
      accessibilityIdentifier = kSyncBookmarksIdentifier;
      break;
    case syncer::UserSelectableType::kHistory:
      itemType = HistoryDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TYPED_URLS;
      accessibilityIdentifier = kSyncOmniboxHistoryIdentifier;
      break;
    case syncer::UserSelectableType::kPasswords:
      itemType = PasswordsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PASSWORDS;
      accessibilityIdentifier = kSyncPasswordsIdentifier;
      break;
    case syncer::UserSelectableType::kTabs:
      itemType = OpenTabsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_TABS;
      accessibilityIdentifier = kSyncOpenTabsIdentifier;
      break;
    case syncer::UserSelectableType::kAutofill:
      itemType = AutofillDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_AUTOFILL;
      accessibilityIdentifier = kSyncAutofillIdentifier;
      break;
    case syncer::UserSelectableType::kPreferences:
      itemType = SettingsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PREFERENCES;
      accessibilityIdentifier = kSyncPreferencesIdentifier;
      break;
    case syncer::UserSelectableType::kReadingList:
      itemType = ReadingListDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_READING_LIST;
      accessibilityIdentifier = kSyncReadingListIdentifier;
      break;
    case syncer::UserSelectableType::kThemes:
    case syncer::UserSelectableType::kExtensions:
    case syncer::UserSelectableType::kApps:
    case syncer::UserSelectableType::kSavedTabGroups:
      NOTREACHED();
      break;
  }
  DCHECK_NE(itemType, 0);
  DCHECK_NE(textStringID, 0);
  DCHECK(accessibilityIdentifier);
  if (![self isManagedSyncSettingsDataType:dataType]) {
    SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
    switchItem.text = GetNSString(textStringID);
    switchItem.dataType = static_cast<NSInteger>(dataType);
    switchItem.accessibilityIdentifier = accessibilityIdentifier;
    return switchItem;
  } else {
    TableViewInfoButtonItem* button =
        [[TableViewInfoButtonItem alloc] initWithType:itemType];
    button.text = GetNSString(textStringID);
    button.statusText = GetNSString(IDS_IOS_SETTING_OFF);
    button.accessibilityIdentifier = accessibilityIdentifier;
    return button;
  }
}

#pragma mark - Properties

- (BOOL)disabledBecauseOfSyncError {
  switch (_syncService->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kGenericUnrecoverableError:
      return YES;
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNone:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return NO;
  }
  NOTREACHED();
}

- (BOOL)shouldSyncDataItemEnabled {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      return NO;
    case SyncSettingsAccountState::kSignedIn:
      return !self.disabledBecauseOfSyncError;
    case SyncSettingsAccountState::kSyncing:
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
      return (!self.syncSetupService->IsSyncEverythingEnabled() ||
              !self.allItemsAreSynceable) &&
             !self.disabledBecauseOfSyncError;
  }
}

// Only requires Sync-the-feature to not be disabled because of a sync error and
// to not need a trusted vault key.
- (BOOL)shouldEncryptionItemBeEnabled {
  return !self.disabledBecauseOfSyncError &&
         _syncService->GetUserActionableError() !=
             syncer::SyncService::UserActionableError::
                 kNeedsTrustedVaultKeyForPasswords &&
         _syncService->GetUserActionableError() !=
             syncer::SyncService::UserActionableError::
                 kNeedsTrustedVaultKeyForEverything;
}

- (NSString*)overrideViewControllerTitle {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedIn:
      return l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TITLE);
    case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
    case SyncSettingsAccountState::kSyncing:
    case SyncSettingsAccountState::kSignedOut:
      return nil;
  }
}

- (SyncSettingsAccountState)syncAccountState {
  // As the manage sync settings mediator is running, the sync account state
  // does not change except only when the user signs out of their account.

  if (_syncService->GetAccountInfo().IsEmpty()) {
    return SyncSettingsAccountState::kSignedOut;
  }
  return _initialAccountState;
}

#pragma mark - ManageSyncSettingsTableViewControllerModelDelegate

- (void)manageSyncSettingsTableViewControllerLoadModel:
    (id<ManageSyncSettingsConsumer>)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadIdentityAccountSection];
  [self loadSyncErrorsSection];
  [self loadSyncDataTypeSection];
  [self loadSignOutAndTurnOffSyncSection];
  [self loadAdvancedSettingsSection];
  [self loadSignOutAndManageAccountsSection];
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
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      _signedInIdentity = _authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
      [self updateIdentityAccountSection];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Temporary state, we can ignore this event, until the UI is signed out.
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([_signedInIdentity isEqual:identity]) {
    [self updateIdentityAccountSection];
    [self updateSyncItemsNotifyConsumer:YES];
    [self updateSyncErrorsSection:YES];
    [self updateEncryptionItem:YES];
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
        if ([self.syncEverythingItem
                isKindOfClass:[TableViewInfoButtonItem class]])
          return;

        self.syncSetupService->SetSyncEverythingEnabled(value);
        if (value) {
          // When sync everything is turned on, the autocomplete wallet
          // should be turned on. This code can be removed once
          // crbug.com/937234 is fixed.
          _autocompleteWalletPreference.value = true;
        }
        break;
      case AutofillDataTypeItemType:
      case BookmarksDataTypeItemType:
      case HistoryDataTypeItemType:
      case OpenTabsDataTypeItemType:
      case PasswordsDataTypeItemType:
      case ReadingListDataTypeItemType:
      case SettingsDataTypeItemType: {
        // Don't try to toggle if item is managed.
        DCHECK(syncSwitchItem);
        syncer::UserSelectableType dataType =
            static_cast<syncer::UserSelectableType>(syncSwitchItem.dataType);
        if ([self isManagedSyncSettingsDataType:dataType])
          break;

        switch (self.syncAccountState) {
          case SyncSettingsAccountState::kSignedIn:
            _syncService->GetUserSettings()->SetSelectedType(dataType, value);
            break;
          case SyncSettingsAccountState::kSyncing:
          case SyncSettingsAccountState::kAdvancedInitialSyncSetup:
            self.syncSetupService->SetDataTypeEnabled(dataType, value);
            break;
          case SyncSettingsAccountState::kSignedOut:
            NOTREACHED();
            break;
        }
        if (dataType == syncer::UserSelectableType::kAutofill) {
          // When the auto fill data type is updated, the autocomplete wallet
          // should be updated too. Autocomplete wallet should not be enabled
          // when auto fill data type disabled. This behaviour not be
          // implemented in the UI code. This code can be removed once
          // crbug.com/937234 is fixed.
          _autocompleteWalletPreference.value = value;
        }
        break;
      }
      case AutocompleteWalletItemType:
        if ([self isManagedSyncSettingsDataType:syncer::UserSelectableType::
                                                    kAutofill]) {
          break;
        }
        _autocompleteWalletPreference.value = value;
        break;
      case SignOutAndTurnOffSyncItemType:
      case SignOutItemType:
      case EncryptionItemType:
      case GoogleActivityControlsItemType:
      case DataFromChromeSync:
      case PrimaryAccountReauthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
      case SyncNeedsTrustedVaultKeyErrorItemType:
      case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      case SyncDisabledByAdministratorErrorItemType:
      case SignOutItemFooterType:
      case TypesListHeaderOrFooterType:
      case IdentityAccountItemType:
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
    case EncryptionItemType: {
      const syncer::SyncService::UserActionableError error =
          _syncService->GetUserActionableError();
      if (error == syncer::SyncService::UserActionableError::
                       kNeedsTrustedVaultKeyForPasswords ||
          error == syncer::SyncService::UserActionableError::
                       kNeedsTrustedVaultKeyForEverything) {
        [self.syncErrorHandler openTrustedVaultReauthForFetchKeys];
        break;
      }
      [self.syncErrorHandler openPassphraseDialog];
      break;
    }
    case GoogleActivityControlsItemType:
      [self.commandHandler openWebAppActivityDialog];
      break;
    case DataFromChromeSync:
      [self.commandHandler openDataFromChromeSyncWebPage];
      break;
    case PrimaryAccountReauthErrorItemType: {
      id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
      if (_authenticationService->HasCachedMDMErrorForIdentity(identity)) {
        [self.syncErrorHandler openMDMErrodDialogWithSystemIdentity:identity];
      } else {
        [self.syncErrorHandler openPrimaryAccountReauthDialog];
      }
      break;
    }
    case ShowPassphraseDialogErrorItemType:
      [self.syncErrorHandler openPassphraseDialog];
      break;
    case SyncNeedsTrustedVaultKeyErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauthForFetchKeys];
      break;
    case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauthForDegradedRecoverability];
      break;
    case SignOutAndTurnOffSyncItemType:
      [self.commandHandler showTurnOffSyncOptionsFromTargetRect:cellRect];
      break;
    case SignOutItemType:
      [self.commandHandler signOut];
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
    case SignOutItemFooterType:
    case TypesListHeaderOrFooterType:
    case IdentityAccountItemType:
      // Nothing to do.
      break;
  }
}

// Creates an item to display the sync error. `itemType` should only be one of
// those types:
//   + PrimaryAccountReauthErrorItemType
//   + ShowPassphraseDialogErrorItemType
//   + SyncNeedsTrustedVaultKeyErrorItemType
//   + SyncTrustedVaultRecoverabilityDegradedErrorItemType
- (TableViewItem*)createSyncErrorItemWithItemType:(NSInteger)itemType {
  DCHECK((itemType == PrimaryAccountReauthErrorItemType) ||
         (itemType == ShowPassphraseDialogErrorItemType) ||
         (itemType == SyncNeedsTrustedVaultKeyErrorItemType) ||
         (itemType == SyncTrustedVaultRecoverabilityDegradedErrorItemType))
      << "itemType: " << itemType;
  TableViewDetailIconItem* syncErrorItem =
      [[TableViewDetailIconItem alloc] initWithType:itemType];
  syncErrorItem.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  syncErrorItem.text = GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncService(_syncService);
  syncErrorItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
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

      // Also override the title to be more accurate.
      syncErrorItem.text = GetNSString(IDS_SYNC_NEEDS_VERIFICATION_TITLE);
      break;
  }
  syncErrorItem.iconImage = DefaultSettingsRootSymbol(kSyncErrorSymbol);
  syncErrorItem.iconBackgroundColor = [UIColor colorNamed:kRed500Color];
  syncErrorItem.iconTintColor = UIColor.whiteColor;
  syncErrorItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  return syncErrorItem;
}

// Loads the sync errors section.
- (void)loadSyncErrorsSection {
  // The `self.consumer.tableViewModel` will be reset prior to this method.
  // Ignore any previous value the `self.syncErrorItem` may have contained.
  self.syncErrorItem = nil;
  [self updateSyncErrorsSection:NO];
}

// Updates the sync errors section. If `notifyConsumer` is YES, the consumer is
// notified about model changes.
- (void)updateSyncErrorsSection:(BOOL)notifyConsumer {
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
  switch (_syncService->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return absl::make_optional<SyncSettingsItemType>(
          PrimaryAccountReauthErrorItemType);
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return absl::make_optional<SyncSettingsItemType>(
          ShowPassphraseDialogErrorItemType);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return absl::make_optional<SyncSettingsItemType>(
          SyncNeedsTrustedVaultKeyErrorItemType);
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return absl::make_optional<SyncSettingsItemType>(
          SyncTrustedVaultRecoverabilityDegradedErrorItemType);
    case syncer::SyncService::UserActionableError::kGenericUnrecoverableError:
    case syncer::SyncService::UserActionableError::kNone:
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
  item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return item;
}

// Returns YES if the given type is managed by policies (i.e. is not syncable)
- (BOOL)isManagedSyncSettingsDataType:(syncer::UserSelectableType)type {
  return IsManagedSyncDataType(_syncService, type);
}

#pragma mark - Properties

- (BOOL)isSyncDisabledByAdministrator {
  return _syncService->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

// Returns NO if any syncable item is managed, YES otherwise.
- (BOOL)allItemsAreSynceable {
  for (const auto& type : kSyncSwitchItems) {
    if ([self isManagedSyncSettingsDataType:type]) {
      return NO;
    }
  }
  return YES;
}

@end
