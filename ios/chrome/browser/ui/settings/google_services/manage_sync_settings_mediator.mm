// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"

#import <optional>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/containers/contains.h"
#import "base/containers/enum_set.h"
#import "base/containers/fixed_flat_map.h"
#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/local_data_description.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/cells/central_account_view.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/features.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using l10n_util::GetNSString;

namespace {

// Ordered list of all sync switches.
// This is the list of available datatypes for account state kSyncing.
static const syncer::UserSelectableType kSyncSwitchItems[] = {
    syncer::UserSelectableType::kAutofill,
    syncer::UserSelectableType::kBookmarks,
    syncer::UserSelectableType::kHistory,
    syncer::UserSelectableType::kTabs,
    syncer::UserSelectableType::kPasswords,
    syncer::UserSelectableType::kReadingList,
    syncer::UserSelectableType::kPreferences,
    syncer::UserSelectableType::kPayments};

// Ordered list of all account data type switches.
// This is the list of available datatypes for account state `kSignedIn`.
static const syncer::UserSelectableType kAccountSwitchItems[] = {
    syncer::UserSelectableType::kHistory,
    syncer::UserSelectableType::kBookmarks,
    syncer::UserSelectableType::kReadingList,
    syncer::UserSelectableType::kAutofill,
    syncer::UserSelectableType::kPasswords,
    syncer::UserSelectableType::kPayments,
    syncer::UserSelectableType::kPreferences};

// Enterprise icon.
NSString* const kGoogleServicesEnterpriseImage = @"google_services_enterprise";
constexpr CGFloat kErrorSymbolPointSize = 22.;
constexpr CGFloat kBatchUploadSymbolPointSize = 22.;

}  // namespace

@interface ManageSyncSettingsMediator () <IdentityManagerObserverBridgeDelegate,
                                          ChromeAccountManagerServiceObserver>

// Model item for sync everything.
@property(nonatomic, strong) TableViewItem* syncEverythingItem;
// Model item for each data types.
@property(nonatomic, strong) NSArray<TableViewItem*>* syncSwitchItems;
// Encryption item.
@property(nonatomic, strong) TableViewImageItem* encryptionItem;
// Sync error item.
@property(nonatomic, strong) TableViewItem* syncErrorItem;
// Batch upload item.
@property(nonatomic, strong) SettingsImageDetailTextItem* batchUploadItem;
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
  // Sync service.
  raw_ptr<syncer::SyncService> _syncService;
  // Identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Authentication service.
  raw_ptr<AuthenticationService> _authenticationService;
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _chromeAccountManagerService;
  // Chrome account manager service observer bridge.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // The pref service.
  raw_ptr<PrefService> _prefService;
  // Signed-in identity. Note: may be nil while signing out.
  id<SystemIdentity> _signedInIdentity;
}

- (instancetype)
      initWithSyncService:(syncer::SyncService*)syncService
          identityManager:(signin::IdentityManager*)identityManager
    authenticationService:(AuthenticationService*)authenticationService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
              prefService:(PrefService*)prefService
      initialAccountState:(SyncSettingsAccountState)initialAccountState {
  self = [super init];
  if (self) {
    DCHECK(syncService);
    CHECK(authenticationService);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _authenticationService = authenticationService;
    _chromeAccountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _chromeAccountManagerService);
    _signedInIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    _prefService = prefService;
    _initialAccountState = initialAccountState;
    // Register for font size change notifications
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(preferredContentSizeChanged:)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)disconnect {
  _syncObserver.reset();
  _syncService = nullptr;
  _identityManager = nullptr;
  _identityManagerObserver.reset();
  _authenticationService = nullptr;
  _chromeAccountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
  _prefService = nullptr;
  _signedInIdentity = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)autofillAlertConfirmed:(BOOL)value {
  _syncService->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, value);
}

#pragma mark - Loads sync data type section

// Loads the sync data type section.
- (void)loadSyncDataTypeSection {
  TableViewModel* model = self.consumer.tableViewModel;
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      return;
    case SyncSettingsAccountState::kSyncing:
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
      BOOL would_clear_data_on_signout =
          _authenticationService->ShouldClearDataForSignedInPeriodOnSignOut();
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
      footerItem.subtitle =
          would_clear_data_on_signout
              ? l10n_util::GetNSString(
                    IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TYPES_LIST_DESCRIPTION_FOR_MANAGED_ACCOUNT)
              : l10n_util::GetNSString(
                    IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TYPES_LIST_DESCRIPTION);
      [model setFooter:footerItem
          forSectionWithIdentifier:SyncDataTypeSectionIdentifier];
      break;
  }
  NSMutableArray* syncSwitchItems = [[NSMutableArray alloc] init];
  if (self.syncAccountState == SyncSettingsAccountState::kSignedIn) {
    for (syncer::UserSelectableType dataType : kAccountSwitchItems) {
      TableViewItem* switchItem = [self tableViewItemWithDataType:dataType];
      [syncSwitchItems addObject:switchItem];
      [model addItem:switchItem
          toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
    }
  } else {
    for (syncer::UserSelectableType dataType : kSyncSwitchItems) {
      TableViewItem* switchItem = [self tableViewItemWithDataType:dataType];
      [syncSwitchItems addObject:switchItem];
      [model addItem:switchItem
          toSectionWithIdentifier:SyncDataTypeSectionIdentifier];
    }
  }
  self.syncSwitchItems = syncSwitchItems;

  [self updateSyncItemsNotifyConsumer:NO];
}

// Updates the sync everything item, and notify the consumer if `notifyConsumer`
// is set to YES.
- (void)updateSyncEverythingItemNotifyConsumer:(BOOL)notifyConsumer {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
    case SyncSettingsAccountState::kSignedIn:
      return;
    case SyncSettingsAccountState::kSyncing:
      if ([self.syncEverythingItem
              isKindOfClass:[TableViewInfoButtonItem class]]) {
        // It's possible that the sync everything pref remains true when a
        // policy change doesn't allow to sync everthing anymore. Fix that here.
        BOOL isSyncingEverything =
            _syncService->GetUserSettings()->IsSyncEverythingEnabled();
        BOOL canSyncEverything = self.allItemsAreSynceable;
        if (isSyncingEverything && !canSyncEverything) {
          _syncService->GetUserSettings()->SetSelectedTypes(
              false, _syncService->GetUserSettings()->GetSelectedTypes());
        }
        return;
      }

      BOOL shouldSyncEverythingBeEditable = !self.disabledBecauseOfSyncError;
      BOOL shouldSyncEverythingItemBeOn =
          _syncService->GetUserSettings()->IsSyncEverythingEnabled();
      SyncSwitchItem* syncEverythingItem =
          base::apple::ObjCCastStrict<SyncSwitchItem>(self.syncEverythingItem);
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

// Returns the management state for this browser and profile.
- (ManagementState)managementState {
  return GetManagementState(_identityManager, _authenticationService,
                            _prefService);
}

// Updates the consumer when the primary account is updated.
- (void)updatePrimaryAccountDetails {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
    case SyncSettingsAccountState::kSyncing:
      return;
    case SyncSettingsAccountState::kSignedIn:
      [self.consumer
          updatePrimaryAccountWithAvatarImage:
              _chromeAccountManagerService->GetIdentityAvatarWithIdentity(
                  _signedInIdentity, IdentityAvatarSize::Large)
                                         name:_signedInIdentity.userFullName
                                        email:_signedInIdentity.userEmail
                              managementState:self.managementState];
      break;
  }
}

// Updates all the sync data type items, and notify the consumer if
// `notifyConsumer` is set to YES.
- (void)updateSyncItemsNotifyConsumer:(BOOL)notifyConsumer {
  for (TableViewItem* item in self.syncSwitchItems) {
    if ([item isKindOfClass:[TableViewInfoButtonItem class]])
      continue;

    SyncSwitchItem* syncSwitchItem =
        base::apple::ObjCCast<SyncSwitchItem>(item);
    syncer::UserSelectableType dataType =
        static_cast<syncer::UserSelectableType>(syncSwitchItem.dataType);
    BOOL isDataTypeSynced =
        _syncService->GetUserSettings()->GetSelectedTypes().Has(dataType);
    BOOL isEnabled = self.shouldSyncDataItemEnabled &&
                     ![self isManagedSyncSettingsDataType:dataType];

    if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
        dataType == syncer::UserSelectableType::kHistory) {
      // kHistory toggle represents both kHistory and kTabs in this case.
      // kHistory and kTabs should usually have the same value, but in some
      // cases they may not, e.g. if one of them is disabled by policy. In that
      // case, show the toggle as on if at least one of them is enabled. The
      // toggle should reflect the value of the non-disabled type.
      isDataTypeSynced =
          _syncService->GetUserSettings()->GetSelectedTypes().Has(
              syncer::UserSelectableType::kHistory) ||
          _syncService->GetUserSettings()->GetSelectedTypes().Has(
              syncer::UserSelectableType::kTabs);
      isEnabled = self.shouldSyncDataItemEnabled &&
                  (![self isManagedSyncSettingsDataType:
                              syncer::UserSelectableType::kHistory] ||
                   ![self isManagedSyncSettingsDataType:
                              syncer::UserSelectableType::kTabs]);
    }
    BOOL needsUpdate = (syncSwitchItem.on != isDataTypeSynced) ||
                       (syncSwitchItem.isEnabled != isEnabled);
    syncSwitchItem.on = isDataTypeSynced;
    syncSwitchItem.enabled = isEnabled;
    if (needsUpdate && notifyConsumer) {
      [self.consumer reloadItem:syncSwitchItem];
    }
  }
}

#pragma mark - Loads the advanced settings section

// Loads the advanced settings section.
- (void)loadAdvancedSettingsSection {
  if (self.syncAccountState == SyncSettingsAccountState::kSignedOut) {
    return;
  }
  if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
      self.isSyncDisabledByAdministrator) {
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
        initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                          kChevronForwardSymbol)];
    self.encryptionItem.accessoryView.tintColor =
        [UIColor colorNamed:kTextQuaternaryColor];
  } else {
    self.encryptionItem.accessoryView = nil;
  }
  self.encryptionItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [self updateEncryptionItem:NO];
  [model addItem:self.encryptionItem
      toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];

  if (IsLinkedServicesSettingIosEnabled()) {
    // PersonalizeGoogleServicesItemType.
    TableViewImageItem* personalizeGoogleServicesItem =
        [[TableViewImageItem alloc]
            initWithType:PersonalizeGoogleServicesItemType];
    if (self.isEEAAccount) {
      personalizeGoogleServicesItem.title = GetNSString(
          IDS_IOS_MANAGE_SYNC_PERSONALIZE_GOOGLE_SERVICES_TITLE_EEA);
      personalizeGoogleServicesItem.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                            kChevronForwardSymbol)];
    } else {
      personalizeGoogleServicesItem.title =
          GetNSString(IDS_IOS_MANAGE_SYNC_PERSONALIZE_GOOGLE_SERVICES_TITLE);
      personalizeGoogleServicesItem.accessoryView = [[UIImageView alloc]
          initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                            kExternalLinkSymbol)];
    }
    personalizeGoogleServicesItem.accessoryView.tintColor =
        [UIColor colorNamed:kTextQuaternaryColor];
    personalizeGoogleServicesItem.detailText = GetNSString(
        IDS_IOS_MANAGE_SYNC_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION);
    personalizeGoogleServicesItem.accessibilityIdentifier =
        kPersonalizeGoogleServicesIdentifier;
    personalizeGoogleServicesItem.accessibilityTraits |=
        UIAccessibilityTraitButton;
    [model addItem:personalizeGoogleServicesItem
        toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
  } else {
    // GoogleActivityControlsItemType.
    TableViewImageItem* googleActivityControlsItem = [[TableViewImageItem alloc]
        initWithType:GoogleActivityControlsItemType];
    googleActivityControlsItem.accessoryView = [[UIImageView alloc]
        initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                          kExternalLinkSymbol)];
    googleActivityControlsItem.accessoryView.tintColor =
        [UIColor colorNamed:kTextQuaternaryColor];
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
  dataFromChromeSyncItem.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                        kExternalLinkSymbol)];
  dataFromChromeSyncItem.accessoryView.tintColor =
      [UIColor colorNamed:kTextQuaternaryColor];
  dataFromChromeSyncItem.accessibilityIdentifier =
      kDataFromChromeSyncAccessibilityIdentifier;
  dataFromChromeSyncItem.accessibilityTraits |= UIAccessibilityTraitButton;

  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedIn:
      dataFromChromeSyncItem.title =
          GetNSString(IDS_IOS_MANAGE_DATA_IN_YOUR_ACCOUNT_TITLE);
      dataFromChromeSyncItem.detailText =
          GetNSString(IDS_IOS_MANAGE_DATA_IN_YOUR_ACCOUNT_DESCRIPTION);
      [model addItem:dataFromChromeSyncItem
          toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
      break;
    case SyncSettingsAccountState::kSyncing:
      dataFromChromeSyncItem.title =
          GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_TITLE);
      dataFromChromeSyncItem.detailText =
          GetNSString(IDS_IOS_MANAGE_SYNC_DATA_FROM_CHROME_SYNC_DESCRIPTION);
      [model addItem:dataFromChromeSyncItem
          toSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
      break;
    case SyncSettingsAccountState::kSignedOut:
      NOTREACHED_IN_MIGRATION();
  }
}

// Updates encryption item, and notifies the consumer if `notifyConsumer` is set
// to YES.
- (void)updateEncryptionItem:(BOOL)notifyConsumer {
  if (![self.consumer.tableViewModel
          hasSectionForSectionIdentifier:AdvancedSettingsSectionIdentifier]) {
    return;
  }
  if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
      self.isSyncDisabledByAdministrator) {
    [self.consumer.tableViewModel
        removeSectionWithIdentifier:AdvancedSettingsSectionIdentifier];
    return;
  }
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

// Creates a footer item to display below the sign out button when forced
// sign-in is enabled.
- (TableViewItem*)createForcedSigninFooterItem {
  // Add information about the forced sign-in policy below the sign-out
  // button when forced sign-in is enabled.
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:SignOutItemFooterType];
  footerItem.text = l10n_util::GetNSString(
      IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE_WITH_LEARN_MORE);
  footerItem.urls =
      @[ [[CrURL alloc] initWithGURL:GURL("chrome://management/")] ];
  return footerItem;
}

- (void)loadSignOutAndTurnOffSyncSection {
  // The SignOutAndTurnOffSyncSection only exists in
  // SyncSettingsAccountState::kSyncing state.
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      // kSignedOut is a temporary state; it only exists if the user just signed
      // out and the UI is in the process of being dismissed. In this case,
      // don't bother updating the section.
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
  CHECK_NE(NSNotFound, syncDataTypeSectionIndex);
  [model insertSectionWithIdentifier:ManageAndSignOutSectionIdentifier
                             atIndex:syncDataTypeSectionIndex + 1];
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:SignOutAndTurnOffSyncItemType];
  item.text = GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC);
  item.textColor = [UIColor colorNamed:kRedColor];
  self.signOutAndTurnOffSyncItem = item;
  [model addItem:self.signOutAndTurnOffSyncItem
      toSectionWithIdentifier:ManageAndSignOutSectionIdentifier];

  if (self.forcedSigninEnabled) {
    [model setFooter:[self createForcedSigninFooterItem]
        forSectionWithIdentifier:ManageAndSignOutSectionIdentifier];
  }
}

- (void)updateSignOutSection {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasSignOutSection =
      [model hasSectionForSectionIdentifier:ManageAndSignOutSectionIdentifier];

  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      break;
    case SyncSettingsAccountState::kSignedIn:
      // There should be a sign-out section. Load it if it's not there yet.
      if (!hasSignOutSection) {
        [self loadSignOutAndManageAccountsSection];
        NSUInteger sectionIndex = [model
            sectionForSectionIdentifier:ManageAndSignOutSectionIdentifier];
        [self.consumer
            insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
              rowAnimation:NO];
      }
      break;
    case SyncSettingsAccountState::kSyncing:
      // There should be a sign-out section. Load it if it's not there yet.
      if (!hasSignOutSection) {
        [self loadSignOutAndTurnOffSyncSection];
        DCHECK(self.signOutAndTurnOffSyncItem);
        NSUInteger sectionIndex = [model
            sectionForSectionIdentifier:ManageAndSignOutSectionIdentifier];
        [self.consumer
            insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
              rowAnimation:NO];
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
  // The AdvancedSettingsSectionIdentifier does not exist when sync is disabled
  // by administrator for a signed-in not syncing account.
  NSInteger previousSection =
      [model hasSectionForSectionIdentifier:AdvancedSettingsSectionIdentifier]
          ? [model
                sectionForSectionIdentifier:AdvancedSettingsSectionIdentifier]
          : [model sectionForSectionIdentifier:SyncDataTypeSectionIdentifier];
  CHECK_NE(NSNotFound, previousSection);
  [model insertSectionWithIdentifier:ManageAndSignOutSectionIdentifier
                             atIndex:previousSection + 1];

  // Creates items in the manage accounts and sign-out section.
  // Manage Google Account item.
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ManageGoogleAccountItemType];
  item.text =
      GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_GOOGLE_ACCOUNT_ITEM);
  item.textColor = [UIColor colorNamed:kBlueColor];
  [model addItem:item
      toSectionWithIdentifier:ManageAndSignOutSectionIdentifier];

  // Manage accounts on this device item.
  item = [[TableViewTextItem alloc] initWithType:ManageAccountsItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM);
  item.textColor = [UIColor colorNamed:kBlueColor];
  [model addItem:item
      toSectionWithIdentifier:ManageAndSignOutSectionIdentifier];

  // Sign out item.
  item = [[TableViewTextItem alloc] initWithType:SignOutItemType];
  item.text = GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM);
  item.textColor = [UIColor colorNamed:kBlueColor];
  [model addItem:item
      toSectionWithIdentifier:ManageAndSignOutSectionIdentifier];

  if (self.forcedSigninEnabled) {
    [model setFooter:[self createForcedSigninFooterItem]
        forSectionWithIdentifier:ManageAndSignOutSectionIdentifier];
  }
}

#pragma mark - Loads batch upload section

- (TableViewTextItem*)batchUploadButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:BatchUploadButtonItemType];
  item.text = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_BUTTON_ITEM);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityIdentifier = kBatchUploadAccessibilityIdentifier;
  return item;
}

- (NSString*)itemsToUploadRecommendationString {
  // _localPasswordsToUpload and _localItemsToUpload should be updated by
  // updateBatchUploadSectionWithNotifyConsumer before calling this method,
  // which also checks for the case of having no items to upload, thus this case
  // is not reached here.
  if (!_localPasswordsToUpload && !_localItemsToUpload) {
    NOTREACHED_IN_MIGRATION();
  }

  std::u16string userEmail =
      base::SysNSStringToUTF16(_signedInIdentity.userEmail);

  std::u16string itemsToUploadString;
  if (_localPasswordsToUpload == 0) {
    // No passwords, but there are other items to upload.
    itemsToUploadString = base::i18n::MessageFormatter::FormatWithNamedArgs(
        l10n_util::GetStringUTF16(
            IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_ITEMS_ITEM),
        "count", static_cast<int>(_localItemsToUpload), "email", userEmail);
  } else if (_localItemsToUpload > 0) {
    // Multiple passwords and items to upload.
    itemsToUploadString = base::i18n::MessageFormatter::FormatWithNamedArgs(
        l10n_util::GetStringUTF16(
            IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_PASSWORDS_AND_ITEMS_ITEM),
        "count", static_cast<int>(_localPasswordsToUpload), "email", userEmail);
  } else {
    // No items, but there are passwords to upload.
    itemsToUploadString = base::i18n::MessageFormatter::FormatWithNamedArgs(
        l10n_util::GetStringUTF16(
            IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_PASSWORDS_ITEM),
        "count", static_cast<int>(_localPasswordsToUpload), "email", userEmail);
  }
  return base::SysUTF16ToNSString(itemsToUploadString);
}

- (SettingsImageDetailTextItem*)batchUploadRecommendationItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:BatchUploadRecommendationItemType];
  item.detailText = [self itemsToUploadRecommendationString];
  item.image = CustomSymbolWithPointSize(kCloudAndArrowUpSymbol,
                                         kBatchUploadSymbolPointSize);
  item.imageViewTintColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityIdentifier =
      kBatchUploadRecommendationItemAccessibilityIdentifier;
  return item;
}

// Loads the batch upload section.
- (void)loadBatchUploadSection {
  // Drop the batch upload item from a previous loading.
  self.batchUploadItem = nil;
  // Create the batch upload section and item if needed.
  [self updateBatchUploadSectionWithNotifyConsumer:NO firstLoad:YES];
}

// Fetches the local data descriptions from the sync server, and calls
// `-[ManageSyncSettingsMediator localDataDescriptionsFetchedWithDescription:]`
// to process those description.
- (void)fetchLocalDataDescriptionsForBatchUploadWithFirstLoad:(BOOL)firstLoad {
  if (self.syncAccountState != SyncSettingsAccountState::kSignedIn) {
    return;
  }

  // Types that are disabled by policy will be ignored.
  syncer::DataTypeSet requestedTypes;
  for (syncer::UserSelectableType userSelectableType : kAccountSwitchItems) {
    if (![self isManagedSyncSettingsDataType:userSelectableType]) {
      requestedTypes.Put(
          syncer::UserSelectableTypeToCanonicalDataType(userSelectableType));
    }
  }

  __weak __typeof__(self) weakSelf = self;
  _syncService->GetLocalDataDescriptions(
      requestedTypes,
      base::BindOnce(^(std::map<syncer::DataType, syncer::LocalDataDescription>
                           description) {
        [weakSelf localDataDescriptionsFetchedWithDescription:description
                                                    firstLoad:firstLoad];
      }));
}

// Saves the local data description, and update the batch upload section.
- (void)localDataDescriptionsFetchedWithDescription:
            (std::map<syncer::DataType, syncer::LocalDataDescription>)
                description
                                          firstLoad:(BOOL)firstLoad {
  self.localPasswordsToUpload = 0;
  self.localItemsToUpload = 0;

  for (auto& type : description) {
    if (type.first == syncer::PASSWORDS) {
      self.localPasswordsToUpload = type.second.item_count;
      continue;
    } else {
      self.localItemsToUpload += type.second.item_count;
    }
  }
  [self updateBatchUploadSectionWithNotifyConsumer:YES firstLoad:firstLoad];
}

// Deletes the batch upload section and notifies the consumer about model
// changes.
- (void)removeBatchUploadSection {
  TableViewModel* model = self.consumer.tableViewModel;
  if (![model hasSectionForSectionIdentifier:BatchUploadSectionIdentifier]) {
    return;
  }
  NSInteger sectionIndex =
      [model sectionForSectionIdentifier:BatchUploadSectionIdentifier];
  [model removeSectionWithIdentifier:BatchUploadSectionIdentifier];
  self.batchUploadItem = nil;

  // Remove the batch upload section from the table view model.
  NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
  [self.consumer deleteSections:indexSet rowAnimation:YES];
}

// Updates the batch upload section according to data already fetched.
// `notifyConsummer` if YES, call the consumer to update the table view.
// `firstLoad` if YES, load the section without animations.
- (void)updateBatchUploadSectionWithNotifyConsumer:(BOOL)notifyConsummer
                                         firstLoad:(BOOL)firstLoad {
  // Batch upload option is not shown if sync is disabled by policy, if the
  // account is in a persistent error state that requires a user action, or if
  // there is no local data to offer the batch upload.
  if (self.syncErrorItem || self.isSyncDisabledByAdministrator ||
      (!_localPasswordsToUpload && !_localItemsToUpload)) {
    [self removeBatchUploadSection];
    return;
  }

  TableViewModel* model = self.consumer.tableViewModel;
  DCHECK(![model hasSectionForSectionIdentifier:SyncErrorsSectionIdentifier]);

  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
    case SyncSettingsAccountState::kSyncing:
      return;
    case SyncSettingsAccountState::kSignedIn:
      break;
  }

  NSInteger batchUploadSectionIndex = 0;

  BOOL batchUploadSectionAlreadyExists = self.batchUploadItem;
  if (!batchUploadSectionAlreadyExists) {
    // Creates the batch upload section.
    [model insertSectionWithIdentifier:BatchUploadSectionIdentifier
                               atIndex:batchUploadSectionIndex];
    self.batchUploadItem = [self batchUploadRecommendationItem];
    [model addItem:self.batchUploadItem
        toSectionWithIdentifier:BatchUploadSectionIdentifier];
    [model addItem:[self batchUploadButtonItem]
        toSectionWithIdentifier:BatchUploadSectionIdentifier];
  } else {
    // The section already exists, update it.
    self.batchUploadItem.detailText = [self itemsToUploadRecommendationString];
    [self.consumer reloadItem:self.batchUploadItem];
  }

  if (!notifyConsummer) {
    return;
  }
  NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:batchUploadSectionIndex];
  if (batchUploadSectionAlreadyExists) {
    // The section should be updated if it already exists.
    [self.consumer reloadSections:indexSet];
  } else {
    // The animation is not needed if this is a first time load of the card.
    [self.consumer insertSections:indexSet rowAnimation:!firstLoad];
  }
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
      textStringID =
          self.syncAccountState == SyncSettingsAccountState::kSignedIn
              ? IDS_SYNC_DATATYPE_HISTORY_AND_TABS
              : IDS_SYNC_DATATYPE_TYPED_URLS;
      accessibilityIdentifier =
          self.syncAccountState == SyncSettingsAccountState::kSignedIn
              ? kSyncHistoryAndTabsIdentifier
              : kSyncOmniboxHistoryIdentifier;
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
    case syncer::UserSelectableType::kPayments:
      itemType = PaymentsDataTypeItemType;
      textStringID = IDS_SYNC_DATATYPE_PAYMENTS;
      accessibilityIdentifier = kSyncPaymentsIdentifier;
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
    case syncer::UserSelectableType::kSharedTabGroupData:
    case syncer::UserSelectableType::kProductComparison:
    case syncer::UserSelectableType::kCookies:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DCHECK_NE(itemType, 0);
  DCHECK_NE(textStringID, 0);
  DCHECK(accessibilityIdentifier);

  BOOL isToggleEnabled = ![self isManagedSyncSettingsDataType:dataType];
  if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
      dataType == syncer::UserSelectableType::kHistory) {
    // kHistory toggle represents both kHistory and kTabs in this case.
    // kHistory and kTabs should usually have the same value, but in some
    // cases they may not, e.g. if one of them is disabled by policy. In that
    // case, show the toggle as on if at least one of them is enabled. The
    // toggle should reflect the value of the non-disabled type.
    isToggleEnabled =
        ![self isManagedSyncSettingsDataType:syncer::UserSelectableType::
                                                 kHistory] ||
        ![self isManagedSyncSettingsDataType:syncer::UserSelectableType::kTabs];
  }
  if (isToggleEnabled) {
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

// Updates the consumer when the content size is updated.
- (void)preferredContentSizeChanged:(NSNotification*)notification {
  [self updatePrimaryAccountDetails];
}

#pragma mark - Properties

- (BOOL)disabledBecauseOfSyncError {
  return _syncService->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
}

- (BOOL)shouldSyncDataItemEnabled {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedOut:
      return NO;
    case SyncSettingsAccountState::kSignedIn:
      return !self.disabledBecauseOfSyncError;
    case SyncSettingsAccountState::kSyncing:
      return (!_syncService->GetUserSettings()->IsSyncEverythingEnabled() ||
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
                 kNeedsTrustedVaultKeyForEverything &&
         _syncService->GetUserSettings()->IsCustomPassphraseAllowed();
}

- (NSString*)overrideViewControllerTitle {
  switch (self.syncAccountState) {
    case SyncSettingsAccountState::kSignedIn:
      return l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TITLE);
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
  if (!_authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // If the user signed out from this view or a child controller the view is
    // closing and should not re-load the model.
    return;
  }
  [self loadSyncErrorsSection];
  [self loadBatchUploadSection];
  [self loadSyncDataTypeSection];
  [self loadSignOutAndTurnOffSyncSection];
  [self loadAdvancedSettingsSection];
  [self loadSignOutAndManageAccountsSection];
  [self fetchLocalDataDescriptionsForBatchUploadWithFirstLoad:YES];
  // Loading the header asks the consumer to reload the data, so it should be
  // done after all sections are initially loaded.
  [self updatePrimaryAccountDetails];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_ignoreSyncStateChanges) {
    // The UI should not updated so the switch animations can run smoothly.
    return;
  }
  [self updateSyncErrorsSection:YES];
  [self updateBatchUploadSectionWithNotifyConsumer:YES firstLoad:NO];
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
  [self updateEncryptionItem:YES];
  [self updateSignOutSection];
  [self fetchLocalDataDescriptionsForBatchUploadWithFirstLoad:NO];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      _signedInIdentity = _authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
      [self updatePrimaryAccountDetails];
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
    [self updatePrimaryAccountDetails];
    [self updateSyncItemsNotifyConsumer:YES];
    [self updateSyncErrorsSection:YES];
    [self updateBatchUploadSectionWithNotifyConsumer:YES firstLoad:NO];
    [self updateEncryptionItem:YES];
    [self fetchLocalDataDescriptionsForBatchUploadWithFirstLoad:NO];
  }
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark - ManageSyncSettingsServiceDelegate

- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  {
    SyncSwitchItem* syncSwitchItem =
        base::apple::ObjCCast<SyncSwitchItem>(item);
    syncSwitchItem.on = value;
    if (value &&
        static_cast<syncer::UserSelectableType>(syncSwitchItem.dataType) ==
            syncer::UserSelectableType::kAutofill &&
        _syncService->GetUserSettings()->IsUsingExplicitPassphrase()) {
      [self.commandHandler showAdressesNotEncryptedDialog];
      return;
    }

    // The notifications should be ignored to get smooth switch animations.
    // Notifications are sent by SyncObserverModelBridge while changing
    // settings.
    base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
    SyncSettingsItemType itemType =
        static_cast<SyncSettingsItemType>(item.type);
    switch (itemType) {
      case SyncEverythingItemType:
        if ([self.syncEverythingItem
                isKindOfClass:[TableViewInfoButtonItem class]]) {
          return;
        }

        _syncService->GetUserSettings()->SetSelectedTypes(
            value, _syncService->GetUserSettings()->GetSelectedTypes());
        break;
      case HistoryDataTypeItemType: {
        DCHECK(syncSwitchItem);
        // Update History Sync decline prefs.
        value ? history_sync::ResetDeclinePrefs(_prefService)
              : history_sync::RecordDeclinePrefs(_prefService);
        // Don't try to toggle the managed item.
        if (![self isManagedSyncSettingsDataType:syncer::UserSelectableType::
                                                     kHistory]) {
          _syncService->GetUserSettings()->SetSelectedType(
              syncer::UserSelectableType::kHistory, value);
        }
        // In kSignedIn case, the kTabs toggle does not exist. Instead it's
        // controlled by the history toggle.
        if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
            ![self isManagedSyncSettingsDataType:syncer::UserSelectableType::
                                                     kTabs]) {
          _syncService->GetUserSettings()->SetSelectedType(
              syncer::UserSelectableType::kTabs, value);
        }
        break;
      }
      case PaymentsDataTypeItemType:
      case AutofillDataTypeItemType:
      case BookmarksDataTypeItemType:
      case OpenTabsDataTypeItemType:
      case PasswordsDataTypeItemType:
      case ReadingListDataTypeItemType:
      case SettingsDataTypeItemType: {
        // Don't try to toggle if item is managed.
        DCHECK(syncSwitchItem);
        syncer::UserSelectableType dataType =
            static_cast<syncer::UserSelectableType>(syncSwitchItem.dataType);
        if ([self isManagedSyncSettingsDataType:dataType]) {
          break;
        }

        _syncService->GetUserSettings()->SetSelectedType(dataType, value);
        break;
      }
      case SignOutAndTurnOffSyncItemType:
      case ManageGoogleAccountItemType:
      case ManageAccountsItemType:
      case SignOutItemType:
      case EncryptionItemType:
      case GoogleActivityControlsItemType:
      case DataFromChromeSync:
      case PersonalizeGoogleServicesItemType:
      case PrimaryAccountReauthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
      case SyncNeedsTrustedVaultKeyErrorItemType:
      case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      case SyncDisabledByAdministratorErrorItemType:
      case SignOutItemFooterType:
      case TypesListHeaderOrFooterType:
      case AccountErrorMessageItemType:
      case BatchUploadButtonItemType:
      case BatchUploadRecommendationItemType:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  [self updateSyncEverythingItemNotifyConsumer:YES];
  [self updateSyncItemsNotifyConsumer:YES];
  // Switching toggles might affect the batch upload recommendation.
  [self fetchLocalDataDescriptionsForBatchUploadWithFirstLoad:NO];
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
      [self.syncErrorHandler openPassphraseDialogWithModalPresentation:NO];
      break;
    }
    case GoogleActivityControlsItemType:
      [self.commandHandler openWebAppActivityDialog];
      break;
    case DataFromChromeSync:
      [self.commandHandler openDataFromChromeSyncWebPage];
      break;
    case PersonalizeGoogleServicesItemType:
      if (self.isEEAAccount) {
        [self.commandHandler openPersonalizeGoogleServices];
      } else {
        [self.commandHandler openWebAppActivityDialog];
      }
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
      [self.syncErrorHandler openPassphraseDialogWithModalPresentation:YES];
      break;
    case SyncNeedsTrustedVaultKeyErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauthForFetchKeys];
      break;
    case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      [self.syncErrorHandler openTrustedVaultReauthForDegradedRecoverability];
      break;
    case SignOutAndTurnOffSyncItemType:
    case SignOutItemType:
      [self.commandHandler signOutFromTargetRect:cellRect];
      break;
    case ManageGoogleAccountItemType:
      [self.commandHandler showManageYourGoogleAccount];
      break;
    case ManageAccountsItemType:
      [self.commandHandler showAccountsPage];
      break;
    case BatchUploadButtonItemType:
      [self.commandHandler openBulkUpload];
      break;
    case SyncEverythingItemType:
    case AutofillDataTypeItemType:
    case BookmarksDataTypeItemType:
    case HistoryDataTypeItemType:
    case OpenTabsDataTypeItemType:
    case PasswordsDataTypeItemType:
    case ReadingListDataTypeItemType:
    case SettingsDataTypeItemType:
    case PaymentsDataTypeItemType:
    case SyncDisabledByAdministratorErrorItemType:
    case SignOutItemFooterType:
    case TypesListHeaderOrFooterType:
    case AccountErrorMessageItemType:
    case BatchUploadRecommendationItemType:
      // Nothing to do.
      break;
  }
}

// Creates an item to display and handle the sync error for syncing users.
// `itemType` should only be one of those types:
//   + PrimaryAccountReauthErrorItemType
//   + ShowPassphraseDialogErrorItemType
//   + SyncNeedsTrustedVaultKeyErrorItemType
//   + SyncTrustedVaultRecoverabilityDegradedErrorItemType
- (TableViewItem*)createSyncErrorIconItemWithItemType:(NSInteger)itemType {
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
      if (!_syncService->GetUserSettings()->IsEncryptEverythingEnabled()) {
        syncErrorItem.text = GetNSString(IDS_IOS_SYNC_PASSWORDS_ERROR_TITLE);
      }
      break;
    case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      syncErrorItem.detailText = GetNSString(
          _syncService->GetUserSettings()->IsEncryptEverythingEnabled()
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

// Creates a message item to display the sync error description for signed in
// not syncing users.
- (TableViewItem*)createSyncErrorMessageItem:(int)messageID {
  CHECK(self.syncAccountState == SyncSettingsAccountState::kSignedIn);
  SettingsImageDetailTextItem* syncErrorItem =
      [[SettingsImageDetailTextItem alloc]
          initWithType:AccountErrorMessageItemType];
  syncErrorItem.detailText = l10n_util::GetNSString(messageID);
  syncErrorItem.image =
      DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kErrorSymbolPointSize);
  syncErrorItem.imageViewTintColor = [UIColor colorNamed:kRed500Color];
  return syncErrorItem;
}

// Creates an error action button item to handle the indicated sync error type
// for signed in not syncing users.
- (TableViewItem*)createSyncErrorButtonItemWithItemType:(NSInteger)itemType
                                          buttonLabelID:(int)buttonLabelID {
  CHECK((itemType == PrimaryAccountReauthErrorItemType) ||
        (itemType == ShowPassphraseDialogErrorItemType) ||
        (itemType == SyncNeedsTrustedVaultKeyErrorItemType) ||
        (itemType == SyncTrustedVaultRecoverabilityDegradedErrorItemType))
      << "itemType: " << itemType;
  CHECK(self.syncAccountState == SyncSettingsAccountState::kSignedIn);
  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:itemType];
  item.text = l10n_util::GetNSString(buttonLabelID);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits = UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSyncErrorButtonIdentifier;
  return item;
}

// Deletes the error section. If `notifyConsumer` is YES, the consumer is
// notified about model changes.
- (void)removeSyncErrorsSection:(BOOL)notifyConsumer {
  TableViewModel* model = self.consumer.tableViewModel;
  if (![model hasSectionForSectionIdentifier:SyncErrorsSectionIdentifier]) {
    return;
  }
  NSInteger sectionIndex =
      [model sectionForSectionIdentifier:SyncErrorsSectionIdentifier];
  [model removeSectionWithIdentifier:SyncErrorsSectionIdentifier];
  self.syncErrorItem = nil;

  // Remove the sync error section from the table view model.
  if (notifyConsumer) {
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer deleteSections:indexSet rowAnimation:NO];
  }
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
  std::optional<SyncSettingsItemType> type = [self syncErrorItemType];
  if (![self needsErrorSectionUpdate:type]) {
    return;
  }

  TableViewModel* model = self.consumer.tableViewModel;
  // There is no sync error now, but there previously was an error.
  if (!type.has_value()) {
    [self removeSyncErrorsSection:notifyConsumer];
    return;
  }

  // There is an error now and there might be a previous error.
  BOOL errorSectionAlreadyExists = self.syncErrorItem;

  if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
      errorSectionAlreadyExists) {
    // As the previous error might not have a message item in case it is
    // SyncDisabledByAdministratorError, clear the whole section instead of
    // updating it's items.
    errorSectionAlreadyExists = NO;
    [self removeSyncErrorsSection:notifyConsumer];
  }

  if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
      GetAccountErrorUIInfo(_syncService) == nil) {
    // In some transient states like in SyncService::TransportState::PAUSED,
    // GetAccountErrorUIInfo returns nil and thus will not be able to fetch the
    // current error data. In this case, do not update/add the error item.
    return;
  }

  // Create the new sync error item.
  DCHECK(type.has_value());
  if (type.value() == SyncDisabledByAdministratorErrorItemType) {
    self.syncErrorItem = [self createSyncDisabledByAdministratorErrorItem];
  } else if (self.syncAccountState == SyncSettingsAccountState::kSignedIn) {
    // For signed in not syncing users, the sync error item will be displayed as
    // a button.
    self.syncErrorItem =
        [self createSyncErrorButtonItemWithItemType:type.value()
                                      buttonLabelID:GetAccountErrorUIInfo(
                                                        _syncService)
                                                        .buttonLabelID];
  } else {
    // For syncing users, the sync error item will be displayed as
    // an icon with descriptive text.
    self.syncErrorItem =
        [self createSyncErrorIconItemWithItemType:type.value()];
  }

  NSInteger syncErrorSectionIndex = 0;
  if (!errorSectionAlreadyExists) {
    if (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
        type.value() != SyncDisabledByAdministratorErrorItemType) {
      [model insertSectionWithIdentifier:SyncErrorsSectionIdentifier
                                 atIndex:syncErrorSectionIndex];
      // For signed in not syncing users, the sync error item will be preceded
      // by a descriptive message item.
      [model addItem:[self createSyncErrorMessageItem:GetAccountErrorUIInfo(
                                                          _syncService)
                                                          .messageID]
          toSectionWithIdentifier:SyncErrorsSectionIdentifier];
      [model addItem:self.syncErrorItem
          toSectionWithIdentifier:SyncErrorsSectionIdentifier];
    } else if (self.syncAccountState != SyncSettingsAccountState::kSignedIn) {
      [model insertSectionWithIdentifier:SyncErrorsSectionIdentifier
                                 atIndex:syncErrorSectionIndex];
      [model addItem:self.syncErrorItem
          toSectionWithIdentifier:SyncErrorsSectionIdentifier];
    }
  }

  if (notifyConsumer) {
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:syncErrorSectionIndex];
    if (errorSectionAlreadyExists) {
      [self.consumer reloadSections:indexSet];
    } else {
      [self.consumer insertSections:indexSet rowAnimation:NO];
    }
  }
}

// Returns the sync error item type or std::nullopt if the item
// is not an actionable error.
- (std::optional<SyncSettingsItemType>)syncErrorItemType {
  if (self.isSyncDisabledByAdministrator) {
    return SyncDisabledByAdministratorErrorItemType;
  }
  switch (_syncService->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return PrimaryAccountReauthErrorItemType;
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return ShowPassphraseDialogErrorItemType;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return SyncNeedsTrustedVaultKeyErrorItemType;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return SyncTrustedVaultRecoverabilityDegradedErrorItemType;
    case syncer::SyncService::UserActionableError::kNone:
      return std::nullopt;
  }
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

// Returns whether the error state has changed since the last update.
- (BOOL)needsErrorSectionUpdate:(std::optional<SyncSettingsItemType>)type {
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
  return _syncService->GetUserSettings()->IsTypeManagedByPolicy(type) ||
         (self.syncAccountState == SyncSettingsAccountState::kSignedIn &&
          self.isSyncDisabledByAdministrator);
}

#pragma mark - Properties

- (BOOL)isSyncDisabledByAdministrator {
  return _syncService->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

// Returns NO if any syncable item is managed, YES otherwise.
- (BOOL)allItemsAreSynceable {
  // This method in not be called in the kSignedIn state, as there is no sync
  // everything item.
  CHECK(self.syncAccountState != SyncSettingsAccountState::kSignedIn);
  for (const auto& type : kSyncSwitchItems) {
    if ([self isManagedSyncSettingsDataType:type]) {
      return NO;
    }
  }
  return YES;
}

@end
