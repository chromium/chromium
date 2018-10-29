// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync_settings_collection_view_controller.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#import "components/signin/ios/browser/oauth2_token_service_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_account_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/text_and_error_item.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/sync_encryption_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_encryption_passphrase_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The a11y identifier of the view controller's view.
NSString* const kSettingsSyncId = @"kSettingsSyncId";
// Notification when a switch account operation will start.
NSString* const kSwitchAccountWillStartNotification =
    @"kSwitchAccountWillStartNotification";
// Notification when a switch account operation did finish.
NSString* const kSwitchAccountDidFinishNotification =
    @"kSwitchAccountDidFinishNotification";
// Used to tag and retrieve ItemTypeSyncableDataType cell tags.
const NSInteger kTagShift = 1000;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSyncError = kSectionIdentifierEnumZero,
  SectionIdentifierEnableSync,
  SectionIdentifierSyncAccounts,
  SectionIdentifierSyncServices,
  SectionIdentifierEncryptionAndFooter,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSyncError = kItemTypeEnumZero,
  ItemTypeSyncSwitch,
  ItemTypeAccount,
  ItemTypeSyncEverything,
  ItemTypeSyncableDataType,
  ItemTypeAutofillWalletImport,
  ItemTypeEncryption,
  ItemTypeManageSyncedData,
  ItemTypeHeader,
};

}  // namespace

@interface SyncSettingsCollectionViewController ()<
    ChromeIdentityServiceObserver,
    OAuth2TokenServiceObserverBridgeDelegate,
    SettingsControllerProtocol,
    SyncObserverModelBridge> {
  ios::ChromeBrowserState* _browserState;  // Weak.
  SyncSetupService* _syncSetupService;     // Weak.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  std::unique_ptr<OAuth2TokenServiceObserverBridge> _tokenServiceObserver;
  AuthenticationFlow* _authenticationFlow;
  // Whether switching sync account is allowed on the screen.
  BOOL _allowSwitchSyncAccount;
  // Whether an authentication operation is in progress (e.g switch accounts).
  BOOL _authenticationOperationInProgress;
  // Whether Sync State changes should be currently ignored.
  BOOL _ignoreSyncStateChanges;

  // Cache for Identity items avatar images.
  ResizedAvatarCache* _avatarCache;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, CollectionViewItem*>* _identityMap;
}

// Stops observing browser state services.
- (void)stopBrowserStateServiceObservers;
// Pops the view if user is signed out and not authentication operation is in
// progress. Returns YES if the view was popped.
- (BOOL)popViewIfSignedOut;

// Returns a switch item for sync, set to on if |isOn| is YES.
- (CollectionViewItem*)syncSwitchItem:(BOOL)isOn;
// Returns an item for sync errors other than sync encryption.
- (CollectionViewItem*)syncErrorItem;
// Returns a switch item for sync everything, set to on if |isOn| is YES.
- (CollectionViewItem*)syncEverythingSwitchItem:(BOOL)isOn;
// Returns a switch item for the syncable data type |dataType|, set to on if
// |IsDataTypePreferred| for that type returns true.
- (CollectionViewItem*)switchItemForDataType:
    (SyncSetupService::SyncableDatatype)dataType;
// Returns a switch item for the Autofill wallet import setting.
- (CollectionViewItem*)switchItemForAutofillWalletImport;
// Returns an item for Encryption.
- (CollectionViewItem*)encryptionCellItem;
// Returns an item to open a link to manage the synced data.
- (CollectionViewItem*)manageSyncedDataItem;

// Action method for sync switch.
- (void)changeSyncStatusToOn:(UISwitch*)sender;
// Action method for the sync error cell.
- (void)fixSyncErrorIfPossible;
// Action method for the account cells.
- (void)startSwitchAccountForIdentity:(ChromeIdentity*)identity
                     postSignInAction:(PostSignInAction)postSigninAction;
// Callback for switch account action method.
- (void)didSwitchAccountWithSuccess:(BOOL)success;
// Action method for the Sync Everything switch.
- (void)changeSyncEverythingStatusToOn:(UISwitch*)sender;
// Action method for the data type switches.
- (void)changeDataTypeSyncStatusToOn:(UISwitch*)sender;
// Action method for the Autofill wallet import switch.
- (void)autofillWalletImportChanged:(UISwitch*)sender;
// Action method for the encryption cell.
- (void)showEncryption;

// Updates the visual status of the screen (i.e. whether cells are enabled,
// whether errors are displayed, ...).
- (void)updateCollectionView;
// Ensures the Sync error cell is shown when there is an error.
- (void)updateSyncError;
// Updates the Autofill wallet import cell (i.e. whether it is enabled and on).
- (void)updateAutofillWalletImportCell;
// Ensures the encryption cell displays an error if needed.
- (void)updateEncryptionCell;
// Updates the account item so it can reflect the latest state of the identity.
- (void)updateAccountItem:(CollectionViewAccountItem*)item
             withIdentity:(ChromeIdentity*)identity;

// Returns whether the Sync Settings screen has an Accounts section, allowing
// users to choose to which account they want to sync to.
- (BOOL)hasAccountsSection;
// Returns whether a sync error cell should be displayed.
- (BOOL)shouldDisplaySyncError;
// Returns whether the Sync settings should be disabled because of a Sync error.
- (BOOL)shouldDisableSettingsOnSyncError;
// Returns whether an error should be displayed on the encryption cell.
- (BOOL)shouldDisplayEncryptionError;
// Returns whether the existing sync error is fixable by a user action.
- (BOOL)isSyncErrorFixableByUserAction;
// Returns the ID to use to access the l10n string for the given data type.
- (int)titleIdForSyncableDataType:(SyncSetupService::SyncableDatatype)datatype;
// Returns whether the encryption item should be enabled.
- (BOOL)shouldEncryptionItemBeEnabled;
// Returns whether the sync everything item should be enabled.
- (BOOL)shouldSyncEverythingItemBeEnabled;
// Returns whether the data type items should be enabled.
- (BOOL)shouldSyncableItemsBeEnabled;
// Returns the tag for a data type switch based on its index path.
- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath;
// Returns the indexPath for a data type switch based on its tag.
- (NSIndexPath*)indexPathForTag:(NSInteger)shiftedTag;

// Whether the Autofill wallet import item should be enabled.
@property(nonatomic, readonly, getter=isAutofillWalletImportItemEnabled)
    BOOL autofillWalletImportItemEnabled;

// Whether the Autofill wallet import item should be on.
@property(nonatomic, assign, getter=isAutofillWalletImportOn)
    BOOL autofillWalletImportOn;

@end

@implementation SyncSettingsCollectionViewController

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
              allowSwitchSyncAccount:(BOOL)allowSwitchSyncAccount {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _allowSwitchSyncAccount = allowSwitchSyncAccount;
    _browserState = browserState;
    _syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(_browserState);
    self.title = l10n_util::GetNSString(IDS_IOS_SYNC_SETTING_TITLE);
    browser_sync::ProfileSyncService* syncService =
        ProfileSyncServiceFactory::GetForBrowserState(_browserState);
    _syncObserver.reset(new SyncObserverBridge(self, syncService));
    _tokenServiceObserver.reset(new OAuth2TokenServiceObserverBridge(
        ProfileOAuth2TokenServiceFactory::GetForBrowserState(_browserState),
        self));
    self.collectionViewAccessibilityIdentifier = kSettingsSyncId;
    _avatarCache = [[ResizedAvatarCache alloc] init];
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (void)stopBrowserStateServiceObservers {
  _syncObserver.reset();
  _tokenServiceObserver.reset();
  _identityServiceObserver.reset();
}

- (BOOL)popViewIfSignedOut {
  if (AuthenticationServiceFactory::GetForBrowserState(_browserState)
          ->IsAuthenticated()) {
    return NO;
  }
  if (_authenticationOperationInProgress) {
    // The signed out state might be temporary (e.g. account switch, ...).
    // Don't pop this view based on intermediary values.
    return NO;
  }
  [self.navigationController popToViewController:self animated:NO];
  [base::mac::ObjCCastStrict<SettingsNavigationController>(
      self.navigationController) popViewControllerOrCloseSettingsAnimated:NO];
  return YES;
}

#pragma mark View lifecycle

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateEncryptionCell];
}

#pragma mark SettingsRootCollectionViewController

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // SyncError section.
  if ([self shouldDisplaySyncError]) {
    [model addSectionWithIdentifier:SectionIdentifierSyncError];
    [model addItem:[self syncErrorItem]
        toSectionWithIdentifier:SectionIdentifierSyncError];
  }

  // Sync Section.
  BOOL syncEnabled = _syncSetupService->IsSyncEnabled();
  [model addSectionWithIdentifier:SectionIdentifierEnableSync];
  [model addItem:[self syncSwitchItem:syncEnabled]
      toSectionWithIdentifier:SectionIdentifierEnableSync];

  // Sync to Section.
  if ([self hasAccountsSection]) {
    NSMutableDictionary<NSString*, CollectionViewItem*>* mutableIdentityMap =
        [[NSMutableDictionary alloc] init];
    // Accounts section. Cells enabled if sync is on.
    [model addSectionWithIdentifier:SectionIdentifierSyncAccounts];
    SettingsTextItem* syncToHeader =
        [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
    syncToHeader.text = l10n_util::GetNSString(IDS_IOS_SYNC_TO_TITLE);
    syncToHeader.textColor = [[MDCPalette greyPalette] tint500];
    [model setHeader:syncToHeader
        forSectionWithIdentifier:SectionIdentifierSyncAccounts];
    ProfileOAuth2TokenService* oauth2_service =
        ProfileOAuth2TokenServiceFactory::GetForBrowserState(_browserState);
    AccountTrackerService* accountTracker =
        ios::AccountTrackerServiceFactory::GetForBrowserState(_browserState);

    for (const std::string& account_id : oauth2_service->GetAccounts()) {
      AccountInfo account = accountTracker->GetAccountInfo(account_id);
      ChromeIdentity* identity = ios::GetChromeBrowserProvider()
                                     ->GetChromeIdentityService()
                                     ->GetIdentityWithGaiaID(account.gaia);
      CollectionViewItem* accountItem = [self accountItem:identity];
      [model addItem:accountItem
          toSectionWithIdentifier:SectionIdentifierSyncAccounts];
      [mutableIdentityMap setObject:accountItem forKey:identity.gaiaID];
    }
    _identityMap = mutableIdentityMap;
  }

  // Data Types to sync. Enabled if sync is on.
  [model addSectionWithIdentifier:SectionIdentifierSyncServices];
  SettingsTextItem* syncServicesHeader =
      [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
  syncServicesHeader.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_DATA_TYPES_TITLE);
  syncServicesHeader.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:syncServicesHeader
      forSectionWithIdentifier:SectionIdentifierSyncServices];
  BOOL syncEverythingEnabled = _syncSetupService->IsSyncingAllDataTypes();
  [model addItem:[self syncEverythingSwitchItem:syncEverythingEnabled]
      toSectionWithIdentifier:SectionIdentifierSyncServices];
  // Specific Data Types to sync. Enabled if Sync Everything is off.
  for (int i = 0; i < SyncSetupService::kNumberOfSyncableDatatypes; ++i) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(i);
    if (dataType == SyncSetupService::kSyncUserEvent) {
      // This data type should only be used with the unified consent UI.
      continue;
    }
    [model addItem:[self switchItemForDataType:dataType]
        toSectionWithIdentifier:SectionIdentifierSyncServices];
  }
  // Autofill wallet import switch.
  [model addItem:[self switchItemForAutofillWalletImport]
      toSectionWithIdentifier:SectionIdentifierSyncServices];

  // Encryption section.  Enabled if sync is on.
  [model addSectionWithIdentifier:SectionIdentifierEncryptionAndFooter];
  [model addItem:[self encryptionCellItem]
      toSectionWithIdentifier:SectionIdentifierEncryptionAndFooter];
  [model addItem:[self manageSyncedDataItem]
      toSectionWithIdentifier:SectionIdentifierEncryptionAndFooter];
}

#pragma mark - Model items

- (CollectionViewItem*)syncSwitchItem:(BOOL)isOn {
  SyncSwitchItem* syncSwitchItem = [self
      switchItemWithType:ItemTypeSyncSwitch
                   title:l10n_util::GetNSString(IDS_IOS_SYNC_SETTING_TITLE)
                subTitle:l10n_util::GetNSString(
                             IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE)];
  syncSwitchItem.on = isOn;
  return syncSwitchItem;
}

- (CollectionViewItem*)syncErrorItem {
  DCHECK([self shouldDisplaySyncError]);
  CollectionViewAccountItem* syncErrorItem =
      [[CollectionViewAccountItem alloc] initWithType:ItemTypeSyncError];
  syncErrorItem.cellStyle = CollectionViewCellStyle::kUIKit;
  syncErrorItem.text = l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  syncErrorItem.image = [UIImage imageNamed:@"settings_error"];
  syncErrorItem.detailText = GetSyncErrorMessageForBrowserState(_browserState);
  return syncErrorItem;
}

- (CollectionViewItem*)accountItem:(ChromeIdentity*)identity {
  CollectionViewAccountItem* identityAccountItem =
      [[CollectionViewAccountItem alloc] initWithType:ItemTypeAccount];
  identityAccountItem.cellStyle = CollectionViewCellStyle::kUIKit;
  [self updateAccountItem:identityAccountItem withIdentity:identity];

  identityAccountItem.enabled = _syncSetupService->IsSyncEnabled();
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(_browserState)
          ->GetAuthenticatedIdentity();
  if (identity == authenticatedIdentity) {
    identityAccountItem.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  }
  return identityAccountItem;
}

- (CollectionViewItem*)syncEverythingSwitchItem:(BOOL)isOn {
  SyncSwitchItem* syncSwitchItem = [self
      switchItemWithType:ItemTypeSyncEverything
                   title:l10n_util::GetNSString(IDS_IOS_SYNC_EVERYTHING_TITLE)
                subTitle:nil];
  syncSwitchItem.on = isOn;
  syncSwitchItem.enabled = [self shouldSyncEverythingItemBeEnabled];
  return syncSwitchItem;
}

- (CollectionViewItem*)switchItemForDataType:
    (SyncSetupService::SyncableDatatype)dataType {
  syncer::ModelType modelType = _syncSetupService->GetModelType(dataType);
  BOOL isOn = _syncSetupService->IsDataTypePreferred(modelType);

  SyncSwitchItem* syncDataTypeItem =
      [self switchItemWithType:ItemTypeSyncableDataType
                         title:l10n_util::GetNSString(
                                   [self titleIdForSyncableDataType:dataType])
                      subTitle:nil];
  syncDataTypeItem.dataType = dataType;
  syncDataTypeItem.on = isOn;
  syncDataTypeItem.enabled = [self shouldSyncableItemsBeEnabled];
  return syncDataTypeItem;
}

- (CollectionViewItem*)switchItemForAutofillWalletImport {
  NSString* title = l10n_util::GetNSString(
      IDS_AUTOFILL_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL);
  SyncSwitchItem* autofillWalletImportItem =
      [self switchItemWithType:ItemTypeAutofillWalletImport
                         title:title
                      subTitle:nil];
  autofillWalletImportItem.on = [self isAutofillWalletImportOn];
  autofillWalletImportItem.enabled = [self isAutofillWalletImportItemEnabled];
  return autofillWalletImportItem;
}

- (CollectionViewItem*)encryptionCellItem {
  TextAndErrorItem* encryptionCellItem =
      [[TextAndErrorItem alloc] initWithType:ItemTypeEncryption];
  encryptionCellItem.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE);
  encryptionCellItem.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  encryptionCellItem.shouldDisplayError = [self shouldDisplayEncryptionError];
  encryptionCellItem.enabled = [self shouldEncryptionItemBeEnabled];
  return encryptionCellItem;
}

- (CollectionViewItem*)manageSyncedDataItem {
  SettingsTextItem* manageSyncedDataItem =
      [[SettingsTextItem alloc] initWithType:ItemTypeManageSyncedData];
  manageSyncedDataItem.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_RESET_GOOGLE_DASHBOARD_NO_LINK);
  manageSyncedDataItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return manageSyncedDataItem;
}

#pragma mark Item Constructors

- (SyncSwitchItem*)switchItemWithType:(NSInteger)type
                                title:(NSString*)title
                             subTitle:(NSString*)detailText {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.detailText = detailText;
  return switchItem;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeSyncError: {
      CollectionViewAccountCell* accountCell =
          base::mac::ObjCCastStrict<CollectionViewAccountCell>(cell);
      accountCell.textLabel.textColor = [[MDCPalette redPalette] tint700];
      accountCell.detailTextLabel.numberOfLines = 1;
      break;
    }
    case ItemTypeSyncSwitch: {
      SyncSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SyncSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(changeSyncStatusToOn:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncEverything: {
      SyncSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SyncSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(changeSyncEverythingStatusToOn:)
          forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeSyncableDataType: {
      SyncSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SyncSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(changeDataTypeSyncStatusToOn:)
                      forControlEvents:UIControlEventValueChanged];
      switchCell.switchView.tag = [self tagForIndexPath:indexPath];
      break;
    }
    case ItemTypeAutofillWalletImport: {
      SyncSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SyncSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(autofillWalletImportChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    default:
      break;
  }
  return cell;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSyncError:
      [self fixSyncErrorIfPossible];
      break;
    case ItemTypeAccount: {
      CollectionViewAccountItem* accountItem =
          base::mac::ObjCCastStrict<CollectionViewAccountItem>(
              [self.collectionViewModel itemAtIndexPath:indexPath]);
      if (!accountItem.accessoryType) {
        [self startSwitchAccountForIdentity:accountItem.chromeIdentity
                           postSignInAction:POST_SIGNIN_ACTION_NONE];
      }
      break;
    }
    case ItemTypeEncryption:
      [self showEncryption];
      break;
    case ItemTypeManageSyncedData: {
      GURL learnMoreUrl = google_util::AppendGoogleLocaleParam(
          GURL(kSyncGoogleDashboardURL),
          GetApplicationContext()->GetApplicationLocale());
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithURLFromChrome:learnMoreUrl];
      [self.dispatcher closeSettingsUIAndOpenURL:command];
      break;
    }
    default:
      break;
  }
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeAccount:
      return MDCCellDefaultTwoLineHeight;
    case ItemTypeSyncSwitch:
    case ItemTypeAutofillWalletImport:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeSyncError:
      return MDCCellDefaultOneLineWithAvatarHeight;
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeSyncSwitch:
    case ItemTypeSyncEverything:
    case ItemTypeSyncableDataType:
      return YES;
    default:
      return NO;
  }
}

#pragma mark - Actions

- (void)changeSyncStatusToOn:(UISwitch*)sender {
  if (self.navigationController.topViewController != self) {
    // Workaround for timing issue when taping on a switch and the error or
    // encryption cells. See crbug.com/647678.
    return;
  }

  BOOL isOn = sender.isOn;
  BOOL wasOn = _syncSetupService->IsSyncEnabled();
  if (wasOn == isOn)
    return;

  base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
  _syncSetupService->SetSyncEnabled(isOn);

  BOOL isNowOn = _syncSetupService->IsSyncEnabled();
  if (isNowOn == wasOn) {
    DLOG(WARNING) << "Call to SetSyncEnabled(" << (isOn ? "YES" : "NO")
                  << ") failed.";
    // This shouldn't happen, but in case there was an underlying sync problem,
    // make sure the UI reflects sync's reality.
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeSyncSwitch
           sectionIdentifier:SectionIdentifierEnableSync];

    SyncSwitchItem* item = base::mac::ObjCCastStrict<SyncSwitchItem>(
        [self.collectionViewModel itemAtIndexPath:indexPath]);
    item.on = isNowOn;
  }
  [self updateCollectionView];
}

- (void)fixSyncErrorIfPossible {
  if (![self isSyncErrorFixableByUserAction] || ![self shouldDisplaySyncError])
    return;

  // Unrecoverable errors are special-cased to only do the signing out and back
  // in from the Sync settings screen (where user interaction can safely be
  // prevented).
  if (_syncSetupService->GetSyncServiceState() ==
      SyncSetupService::kSyncServiceUnrecoverableError) {
    ChromeIdentity* authenticatedIdentity =
        AuthenticationServiceFactory::GetForBrowserState(_browserState)
            ->GetAuthenticatedIdentity();
    [self startSwitchAccountForIdentity:authenticatedIdentity
                       postSignInAction:POST_SIGNIN_ACTION_START_SYNC];
    return;
  }

  SyncSetupService::SyncServiceState syncState =
      GetSyncStateForBrowserState(_browserState);
  if (ShouldShowSyncSignin(syncState)) {
    [self.dispatcher
                showSignin:[[ShowSigninCommand alloc]
                               initWithOperation:
                                   AUTHENTICATION_OPERATION_REAUTHENTICATE
                                     accessPoint:signin_metrics::AccessPoint::
                                                     ACCESS_POINT_UNKNOWN]
        baseViewController:self];
  } else if (ShouldShowSyncSettings(syncState)) {
    [self.dispatcher showSyncSettingsFromViewController:self];
  } else if (ShouldShowSyncPassphraseSettings(syncState)) {
    [self.dispatcher showSyncPassphraseSettingsFromViewController:self];
  }
}

- (void)startSwitchAccountForIdentity:(ChromeIdentity*)identity
                     postSignInAction:(PostSignInAction)postSignInAction {
  if (!_syncSetupService->IsSyncEnabled())
    return;

  _authenticationOperationInProgress = YES;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSwitchAccountWillStartNotification
                    object:self];
  [self preventUserInteraction];
  DCHECK(!_authenticationFlow);
  _authenticationFlow = [[AuthenticationFlow alloc]
          initWithBrowserState:_browserState
                      identity:identity
               shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
              postSignInAction:postSignInAction
      presentingViewController:self];
  _authenticationFlow.dispatcher = self.dispatcher;

  __weak SyncSettingsCollectionViewController* weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf didSwitchAccountWithSuccess:success];
  }];
}

- (void)didSwitchAccountWithSuccess:(BOOL)success {
  _authenticationFlow = nil;
  [self allowUserInteraction];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSwitchAccountDidFinishNotification
                    object:self];
  _authenticationOperationInProgress = NO;
  if (![self popViewIfSignedOut]) {
    // Only reload the view if it wasn't popped.
    [self reloadData];
  }
}

- (void)changeSyncEverythingStatusToOn:(UISwitch*)sender {
  if (!_syncSetupService->IsSyncEnabled() ||
      [self shouldDisableSettingsOnSyncError])
    return;
  BOOL isOn = sender.isOn;
  BOOL wasOn = _syncSetupService->IsSyncingAllDataTypes();
  if (wasOn == isOn)
    return;

  base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
  _syncSetupService->SetSyncingAllDataTypes(isOn);

  // Base the UI on the actual sync value, not the toggle.
  BOOL isNowOn = _syncSetupService->IsSyncingAllDataTypes();
  if (isNowOn == wasOn) {
    DLOG(WARNING) << "Call to SetSyncingAllDataTypes(" << (isOn ? "YES" : "NO")
                  << ") failed";
    // No change - there was a sync-level problem that didn't allow the change.
    // This really shouldn't happen, but just in case, make sure the UI reflects
    // sync's reality.
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeSyncEverything
           sectionIdentifier:SectionIdentifierSyncServices];
    SyncSwitchItem* item = base::mac::ObjCCastStrict<SyncSwitchItem>(
        [self.collectionViewModel itemAtIndexPath:indexPath]);
    item.on = isNowOn;
  }
  [self updateCollectionView];
}

- (void)changeDataTypeSyncStatusToOn:(UISwitch*)sender {
  if (!_syncSetupService->IsSyncEnabled() ||
      _syncSetupService->IsSyncingAllDataTypes() ||
      [self shouldDisableSettingsOnSyncError])
    return;

  BOOL isOn = sender.isOn;

  SyncSwitchItem* syncSwitchItem =
      base::mac::ObjCCastStrict<SyncSwitchItem>([self.collectionViewModel
          itemAtIndexPath:[self indexPathForTag:sender.tag]]);
  SyncSetupService::SyncableDatatype dataType =
      (SyncSetupService::SyncableDatatype)syncSwitchItem.dataType;
  syncer::ModelType modelType = _syncSetupService->GetModelType(dataType);

  base::AutoReset<BOOL> autoReset(&_ignoreSyncStateChanges, YES);
  _syncSetupService->SetDataTypeEnabled(modelType, isOn);

  // Set value of Autofill wallet import accordingly if Autofill Sync changed.
  if (dataType == SyncSetupService::kSyncAutofill) {
    [self setAutofillWalletImportOn:isOn];
    [self updateCollectionView];
  }
}

- (void)autofillWalletImportChanged:(UISwitch*)sender {
  if (![self isAutofillWalletImportItemEnabled])
    return;

  [self setAutofillWalletImportOn:sender.isOn];
}

- (void)showEncryption {
  browser_sync::ProfileSyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  if (!syncService->IsEngineInitialized() ||
      !_syncSetupService->IsSyncEnabled() ||
      [self shouldDisableSettingsOnSyncError])
    return;

  SettingsRootCollectionViewController* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (syncService->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseCollectionViewController alloc]
        initWithBrowserState:_browserState];
  } else {
    controllerToPush = [[SyncEncryptionCollectionViewController alloc]
        initWithBrowserState:_browserState];
  }
  controllerToPush.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

#pragma mark Updates

- (void)updateCollectionView {
  __weak SyncSettingsCollectionViewController* weakSelf = self;
  [self.collectionView performBatchUpdates:^{
    [weakSelf updateCollectionViewInternal];
  }
                                completion:nil];
}

- (void)updateCollectionViewInternal {
  NSIndexPath* indexPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeSyncSwitch
         sectionIdentifier:SectionIdentifierEnableSync];

  SyncSwitchItem* syncItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);
  syncItem.on = _syncSetupService->IsSyncEnabled();
  [self reconfigureCellsForItems:@[ syncItem ]];

  // Update Sync Accounts section.
  if ([self hasAccountsSection]) {
    NSInteger section = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierSyncAccounts];
    NSInteger itemsCount =
        [self.collectionViewModel numberOfItemsInSection:section];
    NSMutableArray* accountsToReconfigure = [[NSMutableArray alloc] init];
    for (NSInteger item = 0; item < itemsCount; ++item) {
      NSIndexPath* indexPath = [self.collectionViewModel
          indexPathForItemType:ItemTypeAccount
             sectionIdentifier:SectionIdentifierSyncAccounts
                       atIndex:item];
      CollectionViewAccountItem* accountItem =
          base::mac::ObjCCastStrict<CollectionViewAccountItem>(
              [self.collectionViewModel itemAtIndexPath:indexPath]);
      accountItem.enabled = _syncSetupService->IsSyncEnabled();
      [accountsToReconfigure addObject:accountItem];
    }
    [self reconfigureCellsForItems:accountsToReconfigure];
  }

  // Update Sync Services section.
  indexPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeSyncEverything
         sectionIdentifier:SectionIdentifierSyncServices];
  SyncSwitchItem* syncEverythingItem =
      base::mac::ObjCCastStrict<SyncSwitchItem>(
          [self.collectionViewModel itemAtIndexPath:indexPath]);
  syncEverythingItem.on = _syncSetupService->IsSyncingAllDataTypes();
  syncEverythingItem.enabled = [self shouldSyncEverythingItemBeEnabled];
  [self reconfigureCellsForItems:@[ syncEverythingItem ]];

  // Syncable data types cells
  NSMutableArray* switchsToReconfigure = [[NSMutableArray alloc] init];
  for (NSInteger index = 0;
       index < SyncSetupService::kNumberOfSyncableDatatypes; ++index) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(index);
    if (dataType == SyncSetupService::kSyncUserEvent) {
      // This data type should only be used with the unified consent UI.
      continue;
    }
    NSIndexPath* indexPath = [self.collectionViewModel
        indexPathForItemType:ItemTypeSyncableDataType
           sectionIdentifier:SectionIdentifierSyncServices
                     atIndex:index];
    SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
        [self.collectionViewModel itemAtIndexPath:indexPath]);
    DCHECK_EQ(index, syncSwitchItem.dataType);
    syncer::ModelType modelType = _syncSetupService->GetModelType(dataType);
    syncSwitchItem.on = _syncSetupService->IsDataTypePreferred(modelType);
    syncSwitchItem.enabled = [self shouldSyncableItemsBeEnabled];
    [switchsToReconfigure addObject:syncSwitchItem];
  }
  [self reconfigureCellsForItems:switchsToReconfigure];

  // Update Autofill wallet import cell.
  [self updateAutofillWalletImportCell];

  // Update Encryption cell.
  [self updateEncryptionCell];

  // Add/Remove the Sync Error. This is the only update that can change index
  // paths. It is done last because self.collectionViewModel isn't aware of
  // the performBatchUpdates:completion: order of update/remove/delete.
  [self updateSyncError];
}

- (void)updateSyncError {
  BOOL shouldDisplayError = [self shouldDisplaySyncError];
  BOOL isDisplayingError =
      [self.collectionViewModel hasItemForItemType:ItemTypeSyncError
                                 sectionIdentifier:SectionIdentifierSyncError];
  if (shouldDisplayError && !isDisplayingError) {
    [self.collectionViewModel
        insertSectionWithIdentifier:SectionIdentifierSyncError
                            atIndex:0];
    [self.collectionViewModel addItem:[self syncErrorItem]
              toSectionWithIdentifier:SectionIdentifierSyncError];
    NSInteger section = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierSyncError];
    [self.collectionView insertSections:[NSIndexSet indexSetWithIndex:section]];
  } else if (!shouldDisplayError && isDisplayingError) {
    NSInteger section = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierSyncError];
    [self.collectionViewModel
        removeSectionWithIdentifier:SectionIdentifierSyncError];
    [self.collectionView deleteSections:[NSIndexSet indexSetWithIndex:section]];
  }
}

- (void)updateAutofillWalletImportCell {
  // Force turn on Autofill wallet import if syncing everthing.
  BOOL isSyncingEverything = _syncSetupService->IsSyncingAllDataTypes();
  if (isSyncingEverything) {
    [self setAutofillWalletImportOn:isSyncingEverything];
  }

  NSIndexPath* indexPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeAutofillWalletImport
         sectionIdentifier:SectionIdentifierSyncServices];
  SyncSwitchItem* syncSwitchItem = base::mac::ObjCCastStrict<SyncSwitchItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);
  syncSwitchItem.on = [self isAutofillWalletImportOn];
  syncSwitchItem.enabled = [self isAutofillWalletImportItemEnabled];
  [self reconfigureCellsForItems:@[ syncSwitchItem ]];
}

- (void)updateEncryptionCell {
  BOOL shouldDisplayEncryptionError = [self shouldDisplayEncryptionError];
  NSIndexPath* indexPath = [self.collectionViewModel
      indexPathForItemType:ItemTypeEncryption
         sectionIdentifier:SectionIdentifierEncryptionAndFooter];
  TextAndErrorItem* item = base::mac::ObjCCastStrict<TextAndErrorItem>(
      [self.collectionViewModel itemAtIndexPath:indexPath]);
  item.shouldDisplayError = shouldDisplayEncryptionError;
  item.enabled = [self shouldEncryptionItemBeEnabled];
  [self reconfigureCellsForItems:@[ item ]];
}

- (void)updateAccountItem:(CollectionViewAccountItem*)item
             withIdentity:(ChromeIdentity*)identity {
  item.image = [_avatarCache resizedAvatarForIdentity:identity];
  item.text = identity.userEmail;
  item.chromeIdentity = identity;
}

#pragma mark Helpers

- (BOOL)hasAccountsSection {
  OAuth2TokenService* tokenService =
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(_browserState);
  return _allowSwitchSyncAccount && tokenService->GetAccounts().size() > 1;
}

- (BOOL)shouldDisplaySyncError {
  SyncSetupService::SyncServiceState state =
      _syncSetupService->GetSyncServiceState();
  return state != SyncSetupService::kNoSyncServiceError;
}

- (BOOL)shouldDisableSettingsOnSyncError {
  SyncSetupService::SyncServiceState state =
      _syncSetupService->GetSyncServiceState();
  return state != SyncSetupService::kNoSyncServiceError &&
         state != SyncSetupService::kSyncServiceNeedsPassphrase;
}

- (BOOL)shouldDisplayEncryptionError {
  return _syncSetupService->GetSyncServiceState() ==
         SyncSetupService::kSyncServiceNeedsPassphrase;
}

- (BOOL)isSyncErrorFixableByUserAction {
  SyncSetupService::SyncServiceState state =
      _syncSetupService->GetSyncServiceState();
  return state == SyncSetupService::kSyncServiceNeedsPassphrase ||
         state == SyncSetupService::kSyncServiceSignInNeedsUpdate ||
         state == SyncSetupService::kSyncServiceUnrecoverableError;
}

- (int)titleIdForSyncableDataType:(SyncSetupService::SyncableDatatype)datatype {
  switch (datatype) {
    case SyncSetupService::kSyncBookmarks:
      return IDS_SYNC_DATATYPE_BOOKMARKS;
    case SyncSetupService::kSyncOmniboxHistory:
      return IDS_SYNC_DATATYPE_TYPED_URLS;
    case SyncSetupService::kSyncPasswords:
      return IDS_SYNC_DATATYPE_PASSWORDS;
    case SyncSetupService::kSyncOpenTabs:
      return IDS_SYNC_DATATYPE_TABS;
    case SyncSetupService::kSyncAutofill:
      return IDS_SYNC_DATATYPE_AUTOFILL;
    case SyncSetupService::kSyncPreferences:
      return IDS_SYNC_DATATYPE_PREFERENCES;
    case SyncSetupService::kSyncReadingList:
      return IDS_SYNC_DATATYPE_READING_LIST;
    case SyncSetupService::kSyncUserEvent:
    // Not supported for the code before the unified consent.
    case SyncSetupService::kNumberOfSyncableDatatypes:
      NOTREACHED();
  }
  return 0;
}

- (BOOL)shouldEncryptionItemBeEnabled {
  browser_sync::ProfileSyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  return (syncService->IsEngineInitialized() &&
          _syncSetupService->IsSyncEnabled() &&
          ![self shouldDisableSettingsOnSyncError]);
}

- (BOOL)shouldSyncEverythingItemBeEnabled {
  return (_syncSetupService->IsSyncEnabled() &&
          ![self shouldDisableSettingsOnSyncError]);
}

- (BOOL)shouldSyncableItemsBeEnabled {
  return (!_syncSetupService->IsSyncingAllDataTypes() &&
          _syncSetupService->IsSyncEnabled() &&
          ![self shouldDisableSettingsOnSyncError]);
}

- (BOOL)isAutofillWalletImportItemEnabled {
  syncer::ModelType autofillModelType =
      _syncSetupService->GetModelType(SyncSetupService::kSyncAutofill);
  BOOL isAutofillOn = _syncSetupService->IsDataTypePreferred(autofillModelType);
  return isAutofillOn && [self shouldSyncableItemsBeEnabled];
}

- (BOOL)isAutofillWalletImportOn {
  return autofill::prefs::IsPaymentsIntegrationEnabled(
      _browserState->GetPrefs());
}

- (void)setAutofillWalletImportOn:(BOOL)on {
  autofill::prefs::SetPaymentsIntegrationEnabled(_browserState->GetPrefs(), on);
}

- (NSInteger)tagForIndexPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath.section ==
         [self.collectionViewModel
             sectionForSectionIdentifier:SectionIdentifierSyncServices]);
  NSInteger index =
      [self.collectionViewModel indexInItemTypeForIndexPath:indexPath];
  return index + kTagShift;
}

- (NSIndexPath*)indexPathForTag:(NSInteger)shiftedTag {
  NSInteger unshiftedTag = shiftedTag - kTagShift;
  return [self.collectionViewModel
      indexPathForItemType:ItemTypeSyncableDataType
         sectionIdentifier:SectionIdentifierSyncServices
                   atIndex:unshiftedTag];
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_ignoreSyncStateChanges || _authenticationOperationInProgress) {
    return;
  }
  [self updateCollectionView];
}

#pragma mark OAuth2TokenServiceObserverBridgeDelegate

- (void)onEndBatchChanges {
  if (_authenticationOperationInProgress) {
    return;
  }
  if (![self popViewIfSignedOut]) {
    // Only reload the view if it wasn't popped.
    [self reloadData];
  }
}

#pragma mark SettingsControllerProtocol callbacks

- (void)settingsWillBeDismissed {
  [self stopBrowserStateServiceObservers];
  [_authenticationFlow cancelAndDismiss];
}

#pragma mark - ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
  CollectionViewAccountItem* item =
      base::mac::ObjCCastStrict<CollectionViewAccountItem>(
          [_identityMap objectForKey:identity.gaiaID]);
  if (!item) {
    // Ignoring unknown identity.
    return;
  }
  [self updateAccountItem:item withIdentity:identity];
  [self reconfigureCellsForItems:@[ item ]];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
