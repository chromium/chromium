// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/ios/browser/features.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/resized_avatar_cache.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

typedef NSArray<TableViewItem*>* ItemArray;

namespace {

NSString* const kBetterSearchAndBrowsingItemAccessibilityID =
    @"betterSearchAndBrowsingItem_switch";

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  IdentitySectionIdentifier = kSectionIdentifierEnumZero,
  SyncSectionIdentifier,
  NonPersonalizedSectionIdentifier,
};

// List of items. For implementation details in
// GoogleServicesSettingsViewController, two SyncSwitchItem items should not
// share the same type. The cell UISwitch tag is used to save the item type, and
// when the user taps on the switch, this tag is used to retreive the item based
// on the type.
typedef NS_ENUM(NSInteger, ItemType) {
  // IdentitySectionIdentifier section.
  IdentityItemType = kItemTypeEnumZero,
  ManageGoogleAccountItemType,
  // SyncSectionIdentifier section.
  SignInItemType,
  RestartAuthenticationFlowErrorItemType,
  ReauthDialogAsSyncIsInAuthErrorItemType,
  ShowPassphraseDialogErrorItemType,
  SyncNeedsTrustedVaultKeyErrorItemType,
  SyncTrustedVaultRecoverabilityDegradedErrorItemType,
  SyncDisabledByAdministratorErrorItemType,
  SyncSettingsNotCofirmedErrorItemType,
  SyncChromeDataItemType,
  ManageSyncItemType,
  // NonPersonalizedSectionIdentifier section.
  AllowChromeSigninItemType,
  AutocompleteSearchesAndURLsItemType,
  AutocompleteSearchesAndURLsManagedItemType,
  SafeBrowsingItemType,
  SafeBrowsingManagedItemType,
  ImproveChromeItemType,
  ImproveChromeManagedItemType,
  BetterSearchAndBrowsingItemType,
  BetterSearchAndBrowsingManagedItemType,
  ItemTypePasswordLeakCheckSwitch,
  SignInDisabledItemType,
};

// Enterprise icon.
NSString* kGoogleServicesEnterpriseImage = @"google_services_enterprise";
// Sync error icon.
NSString* kGoogleServicesSyncErrorImage = @"google_services_sync_error";

}  // namespace

@interface GoogleServicesSettingsMediator () <
    BooleanObserver,
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Identity manager observer.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // account manager observer.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

// Returns YES if the user is authenticated.
@property(nonatomic, assign, readonly) BOOL hasPrimaryIdentity;
// Returns YES if Sync settings has been confirmed.
@property(nonatomic, assign, readonly) BOOL isSyncSettingsConfirmed;
// Returns YES if the user cannot turn on sync for enterprise policy reasons.
@property(nonatomic, assign, readonly) BOOL isSyncDisabledByAdministrator;
// Returns YES if the user is allowed to turn on sync (even if there is a sync
// error).
@property(nonatomic, assign, readonly) BOOL shouldDisplaySync;
// Sync setup service.
@property(nonatomic, assign, readonly) SyncSetupService* syncSetupService;
// ** Identity section.
// Avatar cache.
@property(nonatomic, strong) ResizedAvatarCache* resizedAvatarCache;
// Account item.
@property(nonatomic, strong) TableViewAccountItem* accountItem;
// ** Sync section.
// YES if the impression of the Signin cell has already been recorded.
@property(nonatomic, assign) BOOL hasRecordedSigninImpression;
// Sync error item (in the sync section).
@property(nonatomic, strong) TableViewItem* syncErrorItem;
// Sync your Chrome data switch item.
@property(nonatomic, strong) SyncSwitchItem* syncChromeDataSwitchItem;
// Items to open "Manage sync" settings.
@property(nonatomic, strong) TableViewImageItem* manageSyncItem;
// ** Non personalized section.
// Preference value for the "Allow Chrome Sign-in" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* allowChromeSigninPreference;
// Preference value for the "Autocomplete searches and URLs" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteSearchPreference;
// Preference value for the "Safe Browsing" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingPreference;
// Preference value for the "Help improve Chromium's features" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsagePreference;
// Preference value for the "Help improve Chromium's features" for Wifi-Only.
// TODO(crbug.com/872101): Needs to create the UI to change from Wifi-Only to
// always
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsageWifiOnlyPreference;
// Preference value for the "Make searches and browsing better" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* anonymizedDataCollectionPreference;
// The observable boolean that binds to the password leak check settings
// state.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* passwordLeakCheckEnabled;
// The item related to the switch for the automatic password leak detection
// setting.
@property(nonatomic, strong, null_resettable)
    SettingsSwitchItem* passwordLeakCheckItem;

// All the items for the non-personalized section.
@property(nonatomic, strong, readonly) ItemArray nonPersonalizedItems;

// User pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Local pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* localPrefService;

// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation GoogleServicesSettingsMediator

@synthesize nonPersonalizedItems = _nonPersonalizedItems;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                       localPrefService:(PrefService*)localPrefService
                       syncSetupService:(SyncSetupService*)syncSetupService
                  accountManagerService:
                      (ChromeAccountManagerService*)accountManagerService
                                   mode:(GoogleServicesSettingsMode)mode {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    DCHECK(syncSetupService);
    _mode = mode;
    _syncSetupService = syncSetupService;
    _userPrefService = userPrefService;
    _localPrefService = localPrefService;
    _allowChromeSigninPreference =
        [[PrefBackedBoolean alloc] initWithPrefService:userPrefService
                                              prefName:prefs::kSigninAllowed];
    _allowChromeSigninPreference.observer = self;
    _autocompleteSearchPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSearchSuggestEnabled];
    _autocompleteSearchPreference.observer = self;
    _safeBrowsingPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnabled];
    _safeBrowsingPreference.observer = self;
    _sendDataUsagePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:metrics::prefs::kMetricsReportingEnabled];
    _sendDataUsagePreference.observer = self;
    _passwordLeakCheckEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:password_manager::prefs::
                                kPasswordLeakDetectionEnabled];
    _passwordLeakCheckEnabled.observer = self;
    _anonymizedDataCollectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:unified_consent::prefs::
                                kUrlKeyedAnonymizedDataCollectionEnabled];
    _anonymizedDataCollectionPreference.observer = self;
    _resizedAvatarCache = [[ResizedAvatarCache alloc] initWithDefaultTableView];
    _accountManagerService = accountManagerService;
  }
  return self;
}

#pragma mark - Loads identity section

// Loads the identity section.
- (void)loadIdentitySection {
  self.accountItem = nil;
  if (!self.hasPrimaryIdentity)
    return;
  [self createIdentitySection];
  [self configureIdentityAccountItem];
}

// Creates the identity section.
- (void)createIdentitySection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model insertSectionWithIdentifier:IdentitySectionIdentifier atIndex:0];
  DCHECK(!self.accountItem);
  self.accountItem =
      [[TableViewAccountItem alloc] initWithType:IdentityItemType];
  self.accountItem.accessibilityIdentifier =
      kAccountListItemAccessibilityIdentifier;
  if (self.mode == GoogleServicesSettingsModeAdvancedSigninSettings) {
    self.accountItem.mode = TableViewAccountModeNonTappable;
  } else {
    self.accountItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
  }
  [model addItem:self.accountItem
      toSectionWithIdentifier:IdentitySectionIdentifier];
  TableViewImageItem* manageGoogleAccount =
      [[TableViewImageItem alloc] initWithType:ManageGoogleAccountItemType];
  manageGoogleAccount.title =
      GetNSString(IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE);
  [model addItem:manageGoogleAccount
      toSectionWithIdentifier:IdentitySectionIdentifier];
}

// Creates, removes or updates the identity section as needed. And notifies the
// consumer.
- (void)updateIdentitySectionAndNotifyConsumer {
  // Do not display the identity section in Google Services for MICE.
  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    return;
  }
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasIdentitySection =
      [model hasSectionForSectionIdentifier:IdentitySectionIdentifier];
  if (!self.hasPrimaryIdentity) {
    if (!hasIdentitySection) {
      DCHECK(!self.accountItem);
      return;
    }
    self.accountItem = nil;
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:IdentitySectionIdentifier];
    [model removeSectionWithIdentifier:IdentitySectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer deleteSections:indexSet];
    return;
  }
  if (!hasIdentitySection) {
    [self createIdentitySection];
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:IdentitySectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer insertSections:indexSet];
  }
  [self configureIdentityAccountItem];
  [self.consumer reloadItem:self.accountItem];
}

// Configures the identity account item.
- (void)configureIdentityAccountItem {
  DCHECK(self.accountItem);
  ChromeIdentity* identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  DCHECK(identity);
  self.accountItem.image =
      [self.resizedAvatarCache resizedAvatarForIdentity:identity];
  self.accountItem.text = identity.userFullName;
  if (self.mode == GoogleServicesSettingsModeAdvancedSigninSettings ||
      self.isSyncSettingsConfirmed) {
    self.accountItem.detailText = GetNSString(IDS_IOS_SYNC_SETUP_IN_PROGRESS);
  } else {
    self.accountItem.detailText = identity.userEmail;
  }
}

- (TableViewItem*)allowChromeSigninItem {
  if (signin::IsSigninAllowedByPolicy(self.userPrefService)) {
    return
        [self switchItemWithItemType:AllowChromeSigninItemType
                        textStringID:
                            IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT
                      detailStringID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL
                            dataType:0];
  }
  // Disables "Allow Chrome Sign-in" switch with a disclosure that the
  // setting has been disabled by the organization.
  return [self
      TableViewInfoButtonItemType:AllowChromeSigninItemType
                     textStringID:
                         IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT
                   detailStringID:
                       IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL
                           status:NO];
}

#pragma mark - Loads sync section

// Loads the sync section.
- (void)loadSyncSection {
  self.syncErrorItem = nil;
  self.syncChromeDataSwitchItem = nil;
  self.manageSyncItem = nil;
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:SyncSectionIdentifier];
  [self updateSyncSection:NO];
}

// Updates the sync section. If |notifyConsumer| is YES, the consumer is
// notified about model changes.
- (void)updateSyncSection:(BOOL)notifyConsumer {
  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    // Chrome adds the sync section within "Manage Your Settings" for the MICE
    // experiment.
    return;
  }
  BOOL needsAccountSigninItemUpdate = [self updateAccountSignInItem];
  BOOL needsSyncErrorItemsUpdate = [self updateSyncErrorItems];
  BOOL needsSyncChromeDataItemUpdate = [self updateSyncChromeDataItem];
  BOOL needsManageSyncItemUpdate = [self updateManageSyncItem];
  if (notifyConsumer &&
      (needsAccountSigninItemUpdate || needsSyncErrorItemsUpdate ||
       needsSyncChromeDataItemUpdate || needsManageSyncItemUpdate)) {
    TableViewModel* model = self.consumer.tableViewModel;
    NSUInteger sectionIndex =
        [model sectionForSectionIdentifier:SyncSectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer reloadSections:indexSet];
  }
}

// Adds, removes and updates the account sign-in item in the model as needed.
// Returns YES if the consumer should be notified.
- (BOOL)updateAccountSignInItem {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasAccountSignInItem = [model hasItemForItemType:SignInItemType
                                      sectionIdentifier:SyncSectionIdentifier];

  if (self.hasPrimaryIdentity) {
    self.hasRecordedSigninImpression = NO;
    if (!hasAccountSignInItem)
      return NO;
    [model removeItemWithType:SignInItemType
        fromSectionWithIdentifier:SyncSectionIdentifier];
    return YES;
  }

  if (hasAccountSignInItem)
    return NO;

  if (!signin::IsSigninAllowed(self.userPrefService)) {
    // Sign-in is disabled by policy.
    TableViewInfoButtonItem* signinDisabledItem =
        [[TableViewInfoButtonItem alloc] initWithType:SignInDisabledItemType];
    signinDisabledItem.text =
        l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE);
    signinDisabledItem.detailText =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SIGNIN_DISABLED);
    signinDisabledItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
    signinDisabledItem.image = CircularImageFromImage(
        ios::provider::GetSigninDefaultAvatar(), kAccountProfilePhotoDimension);
    signinDisabledItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    signinDisabledItem.tintColor = [UIColor colorNamed:kGrey300Color];
    [model addItem:signinDisabledItem
        toSectionWithIdentifier:SyncSectionIdentifier];
    return YES;
  }

  AccountSignInItem* accountSignInItem =
      [[AccountSignInItem alloc] initWithType:SignInItemType];
  accountSignInItem.detailText =
      GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_SIGN_IN_DETAIL_TEXT);
  [model addItem:accountSignInItem
      toSectionWithIdentifier:SyncSectionIdentifier];

  if (!self.hasRecordedSigninImpression) {
    // Once the Settings are open, this button impression will at most be
    // recorded once per dialog displayed and per sign-in.
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS);
    self.hasRecordedSigninImpression = YES;
  }
  return YES;
}

// Adds, removes and updates the sync error item in the model as needed. Returns
// YES if the consumer should be notified.
- (BOOL)updateSyncErrorItems {
  TableViewModel* model = self.consumer.tableViewModel;
  BOOL hasError = NO;
  ItemType type;

  if (self.isSyncDisabledByAdministrator) {
    type = SyncDisabledByAdministratorErrorItemType;
    hasError = YES;
  } else if (self.hasPrimaryIdentity &&
             self.syncSetupService->CanSyncFeatureStart()) {
    switch (self.syncSetupService->GetSyncServiceState()) {
      case SyncSetupService::kSyncServiceUnrecoverableError:
        type = RestartAuthenticationFlowErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceSignInNeedsUpdate:
        type = ReauthDialogAsSyncIsInAuthErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceNeedsPassphrase:
        type = ShowPassphraseDialogErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
        type = SyncNeedsTrustedVaultKeyErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
        type = SyncTrustedVaultRecoverabilityDegradedErrorItemType;
        hasError = YES;
        break;
      case SyncSetupService::kSyncSettingsNotConfirmed:
        if (self.mode == GoogleServicesSettingsModeSettings) {
          type = SyncSettingsNotCofirmedErrorItemType;
          hasError = YES;
        }
        break;
      case SyncSetupService::kNoSyncServiceError:
      case SyncSetupService::kSyncServiceCouldNotConnect:
      case SyncSetupService::kSyncServiceServiceUnavailable:
        break;
    }
  }

  if ((!hasError && !self.syncErrorItem) ||
      (hasError && self.syncErrorItem && type == self.syncErrorItem.type)) {
    // Nothing to update.
    return NO;
  }

  if (self.syncErrorItem) {
    // Remove the previous sync error item, since it is either the wrong error
    // (if hasError is YES), or there is no error anymore.
    [model removeItemWithType:self.syncErrorItem.type
        fromSectionWithIdentifier:SyncSectionIdentifier];
    self.syncErrorItem = nil;
    if (!hasError)
      return YES;
  }
  // Add the sync error item and its section.
  if (type == SyncDisabledByAdministratorErrorItemType) {
    self.syncErrorItem = [self createSyncDisabledByAdministratorErrorItem];
  } else {
    self.syncErrorItem = [self createSyncErrorItemWithItemType:type];
  }
  [model insertItem:self.syncErrorItem
      inSectionWithIdentifier:SyncSectionIdentifier
                      atIndex:0];
  return YES;
}

// Reloads the manage sync item, and returns YES if the section should be
// reloaded.
- (BOOL)updateManageSyncItem {
  TableViewModel* model = self.consumer.tableViewModel;
  if (self.shouldDisplaySync) {
    BOOL needsUpdate = NO;
    if (!self.manageSyncItem) {
      self.manageSyncItem =
          [[TableViewImageItem alloc] initWithType:ManageSyncItemType];
      self.manageSyncItem.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
      self.manageSyncItem.title =
          GetNSString(IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE);
      self.manageSyncItem.accessibilityIdentifier =
          kManageSyncCellAccessibilityIdentifier;
      [model addItem:self.manageSyncItem
          toSectionWithIdentifier:SyncSectionIdentifier];
      needsUpdate = YES;
    }
    needsUpdate =
        needsUpdate || self.manageSyncItem.enabled != self.isSyncEnabled;
    self.manageSyncItem.enabled = self.isSyncEnabled;
    self.manageSyncItem.textColor =
        self.manageSyncItem.enabled ? nil : UIColor.cr_secondaryLabelColor;
    return needsUpdate;
  }
  if (!self.manageSyncItem)
    return NO;
  [model removeItemWithType:ManageSyncItemType
      fromSectionWithIdentifier:SyncSectionIdentifier];
  self.manageSyncItem = nil;
  return YES;
}

// Updates the sync Chrome data item, and returns YES if the item has been
// updated.
- (BOOL)updateSyncChromeDataItem {
  TableViewModel* model = self.consumer.tableViewModel;
  if (self.shouldDisplaySync) {
    BOOL needsUpdate = NO;
    if (!self.syncChromeDataSwitchItem) {
      self.syncChromeDataSwitchItem =
          [self switchItemWithItemType:SyncChromeDataItemType
                          textStringID:
                              IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_CHROME_DATA
                        detailStringID:0
                              dataType:0];
      [model addItem:self.syncChromeDataSwitchItem
          toSectionWithIdentifier:SyncSectionIdentifier];
      needsUpdate = YES;
    }
    needsUpdate =
        needsUpdate || self.isSyncEnabled != self.syncChromeDataSwitchItem.on;
    self.syncChromeDataSwitchItem.on = self.isSyncEnabled;
    return needsUpdate;
  }
  if (!self.syncChromeDataSwitchItem)
    return NO;
  self.syncChromeDataSwitchItem = nil;
  [model removeItemWithType:SyncChromeDataItemType
      fromSectionWithIdentifier:SyncSectionIdentifier];
  return YES;
}

#pragma mark - Load non personalized section

// Loads NonPersonalizedSectionIdentifier section.
- (void)loadNonPersonalizedSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  for (TableViewItem* item in self.nonPersonalizedItems) {
    [model addItem:item
        toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  }
  [self updateNonPersonalizedSectionWithNotification:NO];
}

// Updates the non-personalized section according to the user consent. If
// |notifyConsumer| is YES, the consumer is notified about model changes.
- (void)updateNonPersonalizedSectionWithNotification:(BOOL)notifyConsumer {
  for (TableViewItem* item in self.nonPersonalizedItems) {
    ItemType type = static_cast<ItemType>(item.type);
    switch (type) {
      case AllowChromeSigninItemType: {
        SyncSwitchItem* signinDisabledItem =
            base::mac::ObjCCast<SyncSwitchItem>(item);
        if (signin::IsSigninAllowedByPolicy(self.userPrefService)) {
          signinDisabledItem.on = self.allowChromeSigninPreference.value;
        } else {
          signinDisabledItem.on = NO;
          signinDisabledItem.enabled = NO;
        }
        break;
      }
      case AutocompleteSearchesAndURLsItemType:
        base::mac::ObjCCast<SyncSwitchItem>(item).on =
            self.autocompleteSearchPreference.value;
        break;
      case AutocompleteSearchesAndURLsManagedItemType:
        base::mac::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.autocompleteSearchPreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case SafeBrowsingItemType:
        base::mac::ObjCCast<SyncSwitchItem>(item).on =
            self.safeBrowsingPreference.value;
        break;
      case SafeBrowsingManagedItemType:
        base::mac::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.safeBrowsingPreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case ImproveChromeItemType:
        base::mac::ObjCCast<SyncSwitchItem>(item).on =
            self.sendDataUsagePreference.value;
        break;
      case ImproveChromeManagedItemType:
        base::mac::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.sendDataUsagePreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case BetterSearchAndBrowsingItemType:
        base::mac::ObjCCast<SyncSwitchItem>(item).on =
            self.anonymizedDataCollectionPreference.value;
        break;
      case BetterSearchAndBrowsingManagedItemType:
        base::mac::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.anonymizedDataCollectionPreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case ItemTypePasswordLeakCheckSwitch:
        [self updateLeakCheckItem];
        break;
      case IdentityItemType:
      case ManageGoogleAccountItemType:
      case SignInItemType:
      case RestartAuthenticationFlowErrorItemType:
      case ReauthDialogAsSyncIsInAuthErrorItemType:
      case ShowPassphraseDialogErrorItemType:
      case SyncNeedsTrustedVaultKeyErrorItemType:
      case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
      case SyncDisabledByAdministratorErrorItemType:
      case SyncSettingsNotCofirmedErrorItemType:
      case SyncChromeDataItemType:
      case ManageSyncItemType:
      case SignInDisabledItemType:
        NOTREACHED();
        break;
    }
  }
  if (notifyConsumer) {
    TableViewModel* model = self.consumer.tableViewModel;
    NSUInteger sectionIndex =
        [model sectionForSectionIdentifier:NonPersonalizedSectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer reloadSections:indexSet];
  }
}

#pragma mark - Properties

- (BOOL)hasPrimaryIdentity {
  return self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

- (BOOL)isSyncSettingsConfirmed {
  return self.syncSetupService->GetSyncServiceState() ==
         SyncSetupService::kSyncSettingsNotConfirmed;
}

- (BOOL)isSyncDisabledByAdministrator {
  return self.syncService->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

- (BOOL)isSyncEnabled {
  // Sync is not active when |syncSetupService->IsFirstSetupComplete()| is
  // false. Show sync being turned off in the UI in this cases.
  return self.syncSetupService->CanSyncFeatureStart() &&
         (self.syncSetupService->IsFirstSetupComplete() ||
          self.mode == GoogleServicesSettingsModeAdvancedSigninSettings);
}

- (BOOL)shouldDisplaySync {
  return self.hasPrimaryIdentity && !self.isSyncDisabledByAdministrator;
}

- (ItemArray)nonPersonalizedItems {
  if (!_nonPersonalizedItems) {
    NSMutableArray* items = [NSMutableArray array];
    if (signin::IsMobileIdentityConsistencyEnabled()) {
      TableViewItem* allowSigninItem = [self allowChromeSigninItem];
      allowSigninItem.accessibilityIdentifier =
          kAllowSigninItemAccessibilityIdentifier;
      [items addObject:allowSigninItem];
    }
    if (base::FeatureList::IsEnabled(kEnableIOSManagedSettingsUI) &&
        self.userPrefService->IsManagedPreference(
            prefs::kSearchSuggestEnabled)) {
      TableViewInfoButtonItem* autocompleteItem = [self
          TableViewInfoButtonItemType:AutocompleteSearchesAndURLsManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL
                               status:self.autocompleteSearchPreference.value];
      [items addObject:autocompleteItem];
    } else {
      SyncSwitchItem* autocompleteItem = [self
          switchItemWithItemType:AutocompleteSearchesAndURLsItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL
                        dataType:0];
      [items addObject:autocompleteItem];
    }
    if (base::FeatureList::IsEnabled(kEnableIOSManagedSettingsUI) &&
        self.userPrefService->IsManagedPreference(
            prefs::kSafeBrowsingEnabled)) {
      TableViewInfoButtonItem* safeBrowsingManagedItem = [self
          TableViewInfoButtonItemType:AutocompleteSearchesAndURLsManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_SAFE_BROWSING_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_SAFE_BROWSING_DETAIL
                               status:self.safeBrowsingPreference.value];
      [items addObject:safeBrowsingManagedItem];
    } else {
      SyncSwitchItem* safeBrowsingItem = [self
          switchItemWithItemType:SafeBrowsingItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_SAFE_BROWSING_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_SAFE_BROWSING_DETAIL
                        dataType:0];
      safeBrowsingItem.accessibilityIdentifier =
          kSafeBrowsingItemAccessibilityIdentifier;
      [items addObject:safeBrowsingItem];
    }
    [items addObject:self.passwordLeakCheckItem];
    if (base::FeatureList::IsEnabled(kEnableIOSManagedSettingsUI) &&
        self.localPrefService->IsManagedPreference(
            metrics::prefs::kMetricsReportingEnabled)) {
      TableViewInfoButtonItem* improveChromeItem = [self
          TableViewInfoButtonItemType:ImproveChromeManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL
                               status:self.sendDataUsagePreference];
      [items addObject:improveChromeItem];
    } else {
      SyncSwitchItem* improveChromeItem = [self
          switchItemWithItemType:ImproveChromeItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL
                        dataType:0];
      [items addObject:improveChromeItem];
    }
    if (base::FeatureList::IsEnabled(kEnableIOSManagedSettingsUI) &&
        self.userPrefService->IsManagedPreference(
            unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)) {
      TableViewInfoButtonItem* betterSearchAndBrowsingItem = [self
          TableViewInfoButtonItemType:BetterSearchAndBrowsingManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL
                               status:self.anonymizedDataCollectionPreference];
      betterSearchAndBrowsingItem.accessibilityIdentifier =
          kBetterSearchAndBrowsingItemAccessibilityID;
      [items addObject:betterSearchAndBrowsingItem];
    } else {
      SyncSwitchItem* betterSearchAndBrowsingItem = [self
          switchItemWithItemType:BetterSearchAndBrowsingItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL
                        dataType:0];
      betterSearchAndBrowsingItem.accessibilityIdentifier =
          kBetterSearchAndBrowsingItemAccessibilityID;
      [items addObject:betterSearchAndBrowsingItem];
    }

    _nonPersonalizedItems = items;
  }
  return _nonPersonalizedItems;
}

- (SettingsSwitchItem*)passwordLeakCheckItem {
  if (!_passwordLeakCheckItem) {
    SettingsSwitchItem* passwordLeakCheckItem = [[SettingsSwitchItem alloc]
        initWithType:ItemTypePasswordLeakCheckSwitch];
    passwordLeakCheckItem.text =
        l10n_util::GetNSString(IDS_IOS_LEAK_CHECK_SWITCH);
    passwordLeakCheckItem.on = [self passwordLeakCheckItemOnState];
    passwordLeakCheckItem.accessibilityIdentifier =
        kPasswordLeakCheckItemAccessibilityIdentifier;
    passwordLeakCheckItem.enabled = self.hasPrimaryIdentity;
    _passwordLeakCheckItem = passwordLeakCheckItem;
  }
  return _passwordLeakCheckItem;
}

#pragma mark - Private

// Creates a SyncSwitchItem instance.
- (SyncSwitchItem*)switchItemWithItemType:(NSInteger)itemType
                             textStringID:(int)textStringID
                           detailStringID:(int)detailStringID
                                 dataType:(NSInteger)dataType {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  if (detailStringID)
    switchItem.detailText = GetNSString(detailStringID);
  switchItem.dataType = dataType;
  return switchItem;
}

// Create a TableViewInfoButtonItem instance.
- (TableViewInfoButtonItem*)TableViewInfoButtonItemType:(NSInteger)itemType
                                           textStringID:(int)textStringID
                                         detailStringID:(int)detailStringID
                                                 status:(BOOL)status {
  TableViewInfoButtonItem* managedItem =
      [[TableViewInfoButtonItem alloc] initWithType:itemType];
  managedItem.text = GetNSString(textStringID);
  managedItem.detailText = GetNSString(detailStringID);
  managedItem.statusText = status ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  if (!status) {
    managedItem.tintColor = [UIColor colorNamed:kGrey300Color];
    managedItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  managedItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  return managedItem;
}

// Creates an item to display the sync error. |itemType| should only be one of
// those types:
//   + RestartAuthenticationFlowErrorItemType
//   + ReauthDialogAsSyncIsInAuthErrorItemType
//   + ShowPassphraseDialogErrorItemType
//   + SyncNeedsTrustedVaultKeyErrorItemType
//   + SyncTrustedVaultRecoverabilityDegradedErrorItemType
//   + SyncSettingsNotCofirmedErrorItemType
- (TableViewItem*)createSyncErrorItemWithItemType:(NSInteger)itemType {
  DCHECK(itemType == RestartAuthenticationFlowErrorItemType ||
         itemType == ReauthDialogAsSyncIsInAuthErrorItemType ||
         itemType == ShowPassphraseDialogErrorItemType ||
         itemType == SyncNeedsTrustedVaultKeyErrorItemType ||
         itemType == SyncTrustedVaultRecoverabilityDegradedErrorItemType ||
         itemType == SyncSettingsNotCofirmedErrorItemType);
  SettingsImageDetailTextItem* syncErrorItem =
      [[SettingsImageDetailTextItem alloc] initWithType:itemType];
  syncErrorItem.text = GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncSetupService(self.syncSetupService);

  switch (itemType) {
    case SyncSettingsNotCofirmedErrorItemType:
      // Special case for the sync error title.
      syncErrorItem.text = GetNSString(IDS_IOS_SYNC_SETUP_NOT_CONFIRMED_TITLE);
      break;
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

  syncErrorItem.image = [UIImage imageNamed:kGoogleServicesSyncErrorImage];
  return syncErrorItem;
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

// Returns a boolean indicating if the switch should appear as "On" or "Off"
// based on the sync preference and the sign in status.
- (BOOL)passwordLeakCheckItemOnState {
  return self.safeBrowsingPreference.value &&
         self.passwordLeakCheckEnabled.value && self.hasPrimaryIdentity;
}

// Updates the detail text and on state of the leak check item based on the
// state.
- (void)updateLeakCheckItem {
  self.passwordLeakCheckItem.enabled =
      self.hasPrimaryIdentity && self.safeBrowsingPreference.value;
  self.passwordLeakCheckItem.on = [self passwordLeakCheckItemOnState];

  if (!self.hasPrimaryIdentity && self.passwordLeakCheckEnabled.value) {
    // If the user is signed out and the sync preference is enabled, this
    // informs that it will be turned on on sign in.
    self.passwordLeakCheckItem.detailText =
        l10n_util::GetNSString(IDS_IOS_LEAK_CHECK_SIGNED_OUT_ENABLED_DESC);
    return;
  }
  self.passwordLeakCheckItem.detailText = nil;
}

// Updates leak item and asks the consumer to reload it.
- (void)updateLeakCheckItemAndReload {
  [self updateLeakCheckItem];
  [self.consumer reloadItem:self.passwordLeakCheckItem];
}

#pragma mark - GoogleServicesSettingsViewControllerModelDelegate

- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.consumer, controller);
  // For the MICE experiment Chrome will display the Sync section within "Manage
  // Sync Settings".
  if (!base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    [self loadIdentitySection];
    [self loadSyncSection];
  }
  [self loadNonPersonalizedSection];
  _identityManagerObserverBridge.reset(
      new signin::IdentityManagerObserverBridge(self.identityManager, self));
  DCHECK(self.syncService);
  _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
  _accountManagerServiceObserver.reset(
      new ChromeAccountManagerServiceObserverBridge(
          self, self.accountManagerService));
}

#pragma mark - GoogleServicesSettingsServiceDelegate

- (void)toggleSwitchItem:(TableViewItem*)item
               withValue:(BOOL)value
              targetRect:(CGRect)targetRect {
  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCast<SyncSwitchItem>(item);
  syncSwitchItem.on = value;
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case AllowChromeSigninItemType: {
      if (self.hasPrimaryIdentity) {
        __weak GoogleServicesSettingsMediator* weakSelf = self;
        [self.commandHandler
            showSignOutFromTargetRect:targetRect
                           completion:^(BOOL success) {
                             weakSelf.allowChromeSigninPreference.value =
                                 success ? value : !value;
                             [weakSelf
                                 updateNonPersonalizedSectionWithNotification:
                                     YES];
                           }];
      } else {
        self.allowChromeSigninPreference.value = value;
      }
      break;
    }
    case AutocompleteSearchesAndURLsItemType:
      self.autocompleteSearchPreference.value = value;
      break;
    case SafeBrowsingItemType:
      self.safeBrowsingPreference.value = value;
      [self updateLeakCheckItemAndReload];
      break;
    case ImproveChromeItemType:
      self.sendDataUsagePreference.value = value;
      // Don't set value if sendDataUsageWifiOnlyPreference has not been
      // allocated.
      if (value && self.sendDataUsageWifiOnlyPreference) {
        // Should be wifi only, until https://crbug.com/872101 is fixed.
        self.sendDataUsageWifiOnlyPreference.value = YES;
      }
      break;
    case BetterSearchAndBrowsingItemType:
      self.anonymizedDataCollectionPreference.value = value;
      break;
    case SyncChromeDataItemType:
      self.syncSetupService->SetSyncEnabled(value);
      if (self.mode == GoogleServicesSettingsModeSettings &&
          !self.syncSetupService->IsFirstSetupComplete()) {
        // FirstSetupComplete flag needs to be turned on when the user enables
        // sync for the first time. This flag should not be turned on in
        // GoogleServicesSettingsModeAdvancedSigninSettings. In that mode,
        // this flag should be turned on only when the user clicks the confirm
        // button.
        CHECK(value);
        self.syncSetupService->PrepareForFirstSyncSetup();
        self.syncSetupService->SetFirstSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
      }
      break;
    case ItemTypePasswordLeakCheckSwitch:
      // Update the pref.
      self.passwordLeakCheckEnabled.value = value;
      // Update the item.
      [self updateLeakCheckItem];
      break;
    case AutocompleteSearchesAndURLsManagedItemType:
    case IdentityItemType:
    case ManageGoogleAccountItemType:
    case SignInItemType:
    case RestartAuthenticationFlowErrorItemType:
    case ReauthDialogAsSyncIsInAuthErrorItemType:
    case SafeBrowsingManagedItemType:
    case BetterSearchAndBrowsingManagedItemType:
    case ShowPassphraseDialogErrorItemType:
    case SyncNeedsTrustedVaultKeyErrorItemType:
    case SyncTrustedVaultRecoverabilityDegradedErrorItemType:
    case SyncDisabledByAdministratorErrorItemType:
    case SyncSettingsNotCofirmedErrorItemType:
    case ManageSyncItemType:
    case SignInDisabledItemType:
    case ImproveChromeManagedItemType:
      NOTREACHED();
      break;
  }
}

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case IdentityItemType:
      [self.commandHandler openAccountSettings];
      break;
    case ManageGoogleAccountItemType:
      [self.commandHandler openManageGoogleAccount];
      break;
    case SignInItemType:
      [self.commandHandler showSignIn];
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
    case ManageSyncItemType:
      [self.commandHandler openManageSyncSettings];
      break;
    case SignInDisabledItemType:
    case SyncDisabledByAdministratorErrorItemType:
    case SyncSettingsNotCofirmedErrorItemType:
    case AutocompleteSearchesAndURLsItemType:
    case AutocompleteSearchesAndURLsManagedItemType:
    case AllowChromeSigninItemType:
    case SafeBrowsingItemType:
    case SafeBrowsingManagedItemType:
    case ItemTypePasswordLeakCheckSwitch:
    case ImproveChromeItemType:
    case ImproveChromeManagedItemType:
    case BetterSearchAndBrowsingItemType:
    case BetterSearchAndBrowsingManagedItemType:
    case SyncChromeDataItemType:
      break;
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateSyncSection:YES];
  // It is possible for |onSyncStateChanged| to be called before
  // |onPrimaryAccountCleared|, when the primary account is removed.
  if (self.hasPrimaryIdentity && self.accountItem) {
    [self configureIdentityAccountItem];
    [self.consumer reloadItem:self.accountItem];
  }
}
#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateSyncSection:YES];
      [self updateIdentitySectionAndNotifyConsumer];
      [self updateLeakCheckItemAndReload];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNonPersonalizedSectionWithNotification:YES];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityChanged:(ChromeIdentity*)identity {
  [self updateIdentitySectionAndNotifyConsumer];
  [self updateLeakCheckItemAndReload];
}

@end
