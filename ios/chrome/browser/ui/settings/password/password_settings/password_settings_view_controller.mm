// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Sections of the password settings UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierBulkMovePasswordsToAccount,
  SectionIdentifierPasswordsInOtherApps,
  SectionIdentifierAutomaticPasskeyUpgradesSwitch,
  SectionIdentifierGooglePasswordManagerPin,
  SectionIdentifierOnDeviceEncryption,
  SectionIdentifierExportPasswordsButton,
};

// Items within the password settings UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSavePasswordsSwitch = kItemTypeEnumZero,
  ItemTypeManagedSavePasswords,
  ItemTypeBulkMovePasswordsToAccountDescription,
  ItemTypeBulkMovePasswordsToAccountButton,
  ItemTypePasswordsInOtherApps,
  ItemTypeAutomaticPasskeyUpgradesSwitch,
  ItemTypeChangeGooglePasswordManagerPinDescription,
  ItemTypeChangeGooglePasswordManagerPinButton,
  ItemTypeOnDeviceEncryptionOptInDescription,
  ItemTypeOnDeviceEncryptionOptedInDescription,
  ItemTypeOnDeviceEncryptionOptedInLearnMore,
  ItemTypeOnDeviceEncryptionSetUp,
  ItemTypeExportPasswordsButton,
};

// Indicates whether the model has not started loading, is in the process of
// loading, or has completed loading.
typedef NS_ENUM(NSInteger, ModelLoadStatus) {
  ModelNotLoaded = 0,
  ModelIsLoading,
  ModelLoadComplete,
};

bool SyncingWebauthnCredentialsEnabled() {
  return base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials);
}

}  // namespace

@interface PasswordSettingsViewController () {
  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;

  // Whether or not Chromium has been enabled as a credential provider at the
  // iOS level. This may not be known at load time; the detail text showing on
  // or off status will be omitted until this is populated.
  std::optional<bool> _passwordsInOtherAppsEnabled;
}

// State

// Tracks whether or not the model has loaded.
@property(nonatomic, assign) ModelLoadStatus modelLoadStatus;

// Whether or not the exporter should be enabled.
@property(nonatomic, assign) BOOL canExportPasswords;

// Whether or not the Password Manager is managed by enterprise policy.
@property(nonatomic, assign, getter=isManagedByPolicy) BOOL managedByPolicy;

// Indicates whether or not "Offer to Save Passwords" is set to enabled.
@property(nonatomic, assign, getter=isSavePasswordsEnabled)
    BOOL savePasswordsEnabled;

// The amount of local passwords present on device.
@property(nonatomic, assign) int localPasswordsCount;

// Inidicates whether or not the bulk move passwords to account section should
// be shown.
@property(nonatomic, assign) BOOL showBulkMovePasswordsToAccount;

// Indicates the signed in account.
@property(nonatomic, copy) NSString* signedInAccount;

// On-device encryption state according to the sync service.
@property(nonatomic, assign)
    PasswordSettingsOnDeviceEncryptionState onDeviceEncryptionState;

// UI elements

// The item related to the switch for the password manager setting.
@property(nonatomic, readonly) TableViewSwitchItem* savePasswordsItem;

// The item related to the enterprise managed save password setting.
@property(nonatomic, readonly)
    TableViewInfoButtonItem* managedSavePasswordsItem;

// The item related to the description of bulk moving passwords to the user's
// account.
@property(nonatomic, readonly)
    TableViewImageItem* bulkMovePasswordsToAccountDescriptionItem;

// The item related to the button allowing users to bulk move passwords to their
// account.
@property(nonatomic, readonly)
    TableViewTextItem* bulkMovePasswordsToAccountButtonItem;

// The item showing the current status of Passwords in Other Apps (i.e.,
// credential provider).
@property(nonatomic, readonly)
    TableViewDetailIconItem* passwordsInOtherAppsItem;

// The item related to the switch for the automatic passkey upgrades setting.
@property(nonatomic, readonly)
    TableViewSwitchItem* automaticPasskeyUpgradesSwitchItem;

// Descriptive text shown when the user has an option of changing their Google
// Password Manager PIN.
@property(nonatomic, readonly)
    TableViewImageItem* changeGooglePasswordManagerPinDescriptionItem;

// A button which triggers the change Google Password Manager PIN flow.
@property(nonatomic, readonly)
    TableViewTextItem* changeGooglePasswordManagerPinItem;

// Descriptive text shown when the user has the option of enabling on-device
// encryption.
@property(nonatomic, readonly)
    TableViewImageItem* onDeviceEncryptionOptInDescriptionItem;

// Descriptive text shown when the user has already enabled on-device
// encryption.
@property(nonatomic, readonly)
    TableViewImageItem* onDeviceEncryptionOptedInDescription;

// A button giving the user more information about on-device encrpytion, shown
// when they have already enabled it.
@property(nonatomic, readonly)
    TableViewTextItem* onDeviceEncryptionOptedInLearnMore;

// A button which triggers the setup of on-device encryption.
@property(nonatomic, readonly) TableViewTextItem* setUpOnDeviceEncryptionItem;

@end

@implementation PasswordSettingsViewController

@synthesize savePasswordsItem = _savePasswordsItem;
@synthesize managedSavePasswordsItem = _managedSavePasswordsItem;
@synthesize bulkMovePasswordsToAccountDescriptionItem =
    _bulkMovePasswordsToAccountDescriptionItem;
@synthesize bulkMovePasswordsToAccountButtonItem =
    _bulkMovePasswordsToAccountButtonItem;
@synthesize passwordsInOtherAppsItem = _passwordsInOtherAppsItem;
@synthesize automaticPasskeyUpgradesSwitchItem =
    _automaticPasskeyUpgradesSwitchItem;
@synthesize changeGooglePasswordManagerPinDescriptionItem =
    _changeGooglePasswordManagerPinDescriptionItem;
@synthesize changeGooglePasswordManagerPinItem =
    _changeGooglePasswordManagerPinItem;
@synthesize onDeviceEncryptionOptInDescriptionItem =
    _onDeviceEncryptionOptInDescriptionItem;
@synthesize onDeviceEncryptionOptedInDescription =
    _onDeviceEncryptionOptedInDescription;
@synthesize onDeviceEncryptionOptedInLearnMore =
    _onDeviceEncryptionOptedInLearnMore;
@synthesize setUpOnDeviceEncryptionItem = _setUpOnDeviceEncryptionItem;

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

- (CGRect)sourceRectForBulkMovePasswordsToAccount {
  return [self.tableView
             cellForRowAtIndexPath:
                 [self.tableViewModel
                     indexPathForItem:_bulkMovePasswordsToAccountButtonItem]]
      .frame;
}

- (CGRect)sourceRectForPasswordExportAlerts {
  return [self.tableView
             cellForRowAtIndexPath:[self.tableViewModel
                                       indexPathForItem:_exportPasswordsItem]]
      .frame;
}

- (UIView*)sourceViewForAlerts {
  return self.tableView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS);
  self.tableView.accessibilityIdentifier = kPasswordsSettingsTableViewId;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  self.modelLoadStatus = ModelIsLoading;

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  [self addSavePasswordsSwitchOrManagedInfo];

  [model addSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
  [model addItem:[self passwordsInOtherAppsItem]
      toSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];

  if (SyncingWebauthnCredentialsEnabled()) {
    // TODO(crbug.com/358343061): Add item for the policy enforced toggle.
    [model addSectionWithIdentifier:
               SectionIdentifierAutomaticPasskeyUpgradesSwitch];
    [model addItem:[self automaticPasskeyUpgradesSwitchItem]
        toSectionWithIdentifier:
            SectionIdentifierAutomaticPasskeyUpgradesSwitch];

    // TODO(crbug.com/358342483): Add this section only if the device is
    // bootstrapped for using passkeys.
    [model addSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
    [model addItem:[self changeGooglePasswordManagerPinDescriptionItem]
        toSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
    [model addItem:[self changeGooglePasswordManagerPinItem]
        toSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
  }

  if (self.onDeviceEncryptionState !=
      PasswordSettingsOnDeviceEncryptionStateNotShown) {
    [self updateOnDeviceEncryptionSectionWithOldState:
              PasswordSettingsOnDeviceEncryptionStateNotShown];
  }

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  _exportPasswordsItem = [self makeExportPasswordsItem];
  [self updateExportPasswordsButton];
  [model addItem:_exportPasswordsItem
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];

  if (self.showBulkMovePasswordsToAccount) {
    [self updateBulkMovePasswordsToAccountSection];
  }

  self.modelLoadStatus = ModelLoadComplete;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeSavePasswordsSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(savePasswordsSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeManagedSavePasswords: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeAutomaticPasskeyUpgradesSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(automaticPasskeyUpgradesSwitchChanged)
          forControlEvents:(UIControlEvents)UIControlEventValueChanged];
    }
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePasswordsInOtherApps: {
      [self.presentationDelegate showPasswordsInOtherAppsScreen];
      break;
    }
    case ItemTypeBulkMovePasswordsToAccountButton: {
      if (self.showBulkMovePasswordsToAccount) {
        [self.delegate bulkMovePasswordsToAccountButtonClicked];
      }
      break;
    }
    case ItemTypeExportPasswordsButton: {
      if (self.canExportPasswords) {
        [self.presentationDelegate startExportFlow];
      }
      break;
    }
    case ItemTypeOnDeviceEncryptionSetUp: {
      [self.presentationDelegate showOnDeviceEncryptionSetUp];
      break;
    }
    case ItemTypeOnDeviceEncryptionOptedInLearnMore: {
      [self.presentationDelegate showOnDeviceEncryptionHelp];
      break;
    }
    case ItemTypeOnDeviceEncryptionOptedInDescription:
    case ItemTypeOnDeviceEncryptionOptInDescription:
    case ItemTypeSavePasswordsSwitch:
    case ItemTypeManagedSavePasswords: {
      DUMP_WILL_BE_NOTREACHED();
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeExportPasswordsButton:
      return self.canExportPasswords;
    case ItemTypeSavePasswordsSwitch:
      return NO;
  }
  return YES;
}

#pragma mark - UI item factories

// Creates the switch allowing users to enable/disable the saving of passwords.
- (TableViewSwitchItem*)savePasswordsItem {
  if (_savePasswordsItem) {
    return _savePasswordsItem;
  }

  _savePasswordsItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSavePasswordsSwitch];
  _savePasswordsItem.text =
      l10n_util::GetNSString(IDS_IOS_OFFER_TO_SAVE_PASSWORDS);
  _savePasswordsItem.accessibilityIdentifier =
      kPasswordSettingsSavePasswordSwitchTableViewId;
  [self updateSavePasswordsSwitch];
  return _savePasswordsItem;
}

// Creates the row which replaces `savePasswordsItem` when this preference is
// being managed by enterprise policy.
- (TableViewInfoButtonItem*)managedSavePasswordsItem {
  if (_managedSavePasswordsItem) {
    return _managedSavePasswordsItem;
  }

  _managedSavePasswordsItem = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeManagedSavePasswords];
  _managedSavePasswordsItem.text =
      l10n_util::GetNSString(IDS_IOS_OFFER_TO_SAVE_PASSWORDS);
  _managedSavePasswordsItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  _managedSavePasswordsItem.accessibilityIdentifier =
      kPasswordSettingsManagedSavePasswordSwitchTableViewId;
  [self updateManagedSavePasswordsItem];
  return _managedSavePasswordsItem;
}

// Creates and returns the move passwords to account description item.
- (TableViewImageItem*)bulkMovePasswordsToAccountDescriptionItem {
  if (_bulkMovePasswordsToAccountDescriptionItem) {
    return _bulkMovePasswordsToAccountDescriptionItem;
  }

  _bulkMovePasswordsToAccountDescriptionItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeBulkMovePasswordsToAccountDescription];
  _bulkMovePasswordsToAccountDescriptionItem.title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_TITLE);
  // TODO(crbug.com/40283775): Without setting the table view image item to
  // enabled, the accessibility voiceover reads out dimmed.
  _bulkMovePasswordsToAccountDescriptionItem.enabled = YES;
  _bulkMovePasswordsToAccountDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountDescriptionTableViewId;
  _bulkMovePasswordsToAccountDescriptionItem.accessibilityTraits =
      UIAccessibilityTraitHeader;

  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_DESCRIPTION);
  std::u16string result = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "COUNT", self.localPasswordsCount, "EMAIL",
      base::SysNSStringToUTF16(self.signedInAccount));

  _bulkMovePasswordsToAccountDescriptionItem.detailText =
      base::SysUTF16ToNSString(result);

  return _bulkMovePasswordsToAccountDescriptionItem;
}

// Creates and returns the move passwords to account button.
- (TableViewTextItem*)bulkMovePasswordsToAccountButtonItem {
  if (_bulkMovePasswordsToAccountButtonItem) {
    return _bulkMovePasswordsToAccountButtonItem;
  }

  _bulkMovePasswordsToAccountButtonItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeBulkMovePasswordsToAccountButton];
  _bulkMovePasswordsToAccountButtonItem.text = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_BUTTON,
      self.localPasswordsCount);
  _bulkMovePasswordsToAccountButtonItem.textColor =
      [UIColor colorNamed:kBlueColor];
  _bulkMovePasswordsToAccountButtonItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  _bulkMovePasswordsToAccountButtonItem.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId;
  return _bulkMovePasswordsToAccountButtonItem;
}

- (TableViewDetailIconItem*)passwordsInOtherAppsItem {
  if (_passwordsInOtherAppsItem) {
    return _passwordsInOtherAppsItem;
  }

  _passwordsInOtherAppsItem = [[TableViewDetailIconItem alloc]
      initWithType:ItemTypePasswordsInOtherApps];
  _passwordsInOtherAppsItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS);
  _passwordsInOtherAppsItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _passwordsInOtherAppsItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _passwordsInOtherAppsItem.accessibilityIdentifier =
      kPasswordSettingsPasswordsInOtherAppsRowId;
  [self updatePasswordsInOtherAppsItem];
  return _passwordsInOtherAppsItem;
}

- (TableViewSwitchItem*)automaticPasskeyUpgradesSwitchItem {
  _automaticPasskeyUpgradesSwitchItem = [[TableViewSwitchItem alloc]
      initWithType:ItemTypeAutomaticPasskeyUpgradesSwitch];
  _automaticPasskeyUpgradesSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES);
  _automaticPasskeyUpgradesSwitchItem.detailText =
      l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES_SUBTITLE);
  return _automaticPasskeyUpgradesSwitchItem;
}

- (TableViewImageItem*)changeGooglePasswordManagerPinDescriptionItem {
  _changeGooglePasswordManagerPinDescriptionItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeChangeGooglePasswordManagerPinDescription];
  _changeGooglePasswordManagerPinDescriptionItem.title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_TITLE);
  _changeGooglePasswordManagerPinDescriptionItem.detailText =
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_DESCRIPTION);
  return _changeGooglePasswordManagerPinDescriptionItem;
}

- (TableViewTextItem*)changeGooglePasswordManagerPinItem {
  _changeGooglePasswordManagerPinItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeChangeGooglePasswordManagerPinButton];
  _changeGooglePasswordManagerPinItem.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_CHANGE_PIN);
  _changeGooglePasswordManagerPinItem.textColor =
      [UIColor colorNamed:kBlueColor];
  _changeGooglePasswordManagerPinItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  return _changeGooglePasswordManagerPinItem;
}

- (TableViewImageItem*)onDeviceEncryptionOptInDescriptionItem {
  if (_onDeviceEncryptionOptInDescriptionItem) {
    return _onDeviceEncryptionOptInDescriptionItem;
  }

  _onDeviceEncryptionOptInDescriptionItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeOnDeviceEncryptionOptInDescription];
  _onDeviceEncryptionOptInDescriptionItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION);
  _onDeviceEncryptionOptInDescriptionItem.detailText = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPT_IN);
  _onDeviceEncryptionOptInDescriptionItem.enabled = NO;
  _onDeviceEncryptionOptInDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionOptInId;
  return _onDeviceEncryptionOptInDescriptionItem;
}

- (TableViewImageItem*)onDeviceEncryptionOptedInDescription {
  if (_onDeviceEncryptionOptedInDescription) {
    return _onDeviceEncryptionOptedInDescription;
  }

  _onDeviceEncryptionOptedInDescription = [[TableViewImageItem alloc]
      initWithType:ItemTypeOnDeviceEncryptionOptedInDescription];
  _onDeviceEncryptionOptedInDescription.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION);
  _onDeviceEncryptionOptedInDescription.detailText = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_LEARN_MORE);
  _onDeviceEncryptionOptedInDescription.enabled = NO;
  _onDeviceEncryptionOptedInDescription.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionOptedInTextId;
  return _onDeviceEncryptionOptedInDescription;
}

- (TableViewTextItem*)onDeviceEncryptionOptedInLearnMore {
  if (_onDeviceEncryptionOptedInLearnMore) {
    return _onDeviceEncryptionOptedInLearnMore;
  }

  _onDeviceEncryptionOptedInLearnMore = [[TableViewTextItem alloc]
      initWithType:ItemTypeOnDeviceEncryptionOptedInLearnMore];
  _onDeviceEncryptionOptedInLearnMore.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPTED_IN_LEARN_MORE);
  _onDeviceEncryptionOptedInLearnMore.textColor =
      [UIColor colorNamed:kBlueColor];
  _onDeviceEncryptionOptedInLearnMore.accessibilityTraits =
      UIAccessibilityTraitButton;
  _onDeviceEncryptionOptedInLearnMore.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionLearnMoreId;
  return _onDeviceEncryptionOptedInLearnMore;
}

- (TableViewTextItem*)setUpOnDeviceEncryptionItem {
  if (_setUpOnDeviceEncryptionItem) {
    return _setUpOnDeviceEncryptionItem;
  }

  _setUpOnDeviceEncryptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOnDeviceEncryptionSetUp];
  _setUpOnDeviceEncryptionItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_SET_UP);
  _setUpOnDeviceEncryptionItem.textColor = [UIColor colorNamed:kBlueColor];
  _setUpOnDeviceEncryptionItem.accessibilityTraits = UIAccessibilityTraitButton;
  _setUpOnDeviceEncryptionItem.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionSetUpId;
  return _setUpOnDeviceEncryptionItem;
}

// Creates the "Export Passwords..." button. Coloring and enabled/disabled state
// are handled by `updateExportPasswordsButton`, which should be called as soon
// as the mediator has provided the necessary state.
- (TableViewTextItem*)makeExportPasswordsItem {
  TableViewTextItem* exportPasswordsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeExportPasswordsButton];
  exportPasswordsItem.text = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS);
  exportPasswordsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return exportPasswordsItem;
}

#pragma mark - PasswordSettingsConsumer

// The `setCanExportPasswords` method required for the PasswordSettingsConsumer
// protocol is provided by property synthesis.

- (void)setManagedByPolicy:(BOOL)managedByPolicy {
  if (_managedByPolicy == managedByPolicy) {
    return;
  }

  _managedByPolicy = managedByPolicy;

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  [self.tableViewModel deleteAllItemsFromSectionWithIdentifier:
                           SectionIdentifierSavePasswordsSwitch];
  [self addSavePasswordsSwitchOrManagedInfo];

  NSIndexSet* indexSet = [[NSIndexSet alloc]
      initWithIndex:[self.tableViewModel
                        sectionForSectionIdentifier:
                            SectionIdentifierSavePasswordsSwitch]];

  [self.tableView reloadSections:indexSet
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)setSavePasswordsEnabled:(BOOL)enabled {
  if (_savePasswordsEnabled == enabled) {
    return;
  }

  _savePasswordsEnabled = enabled;

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  if (self.isManagedByPolicy) {
    [self updateManagedSavePasswordsItem];
  } else {
    [self updateSavePasswordsSwitch];
  }
}

- (void)setLocalPasswordsCount:(int)count
           withUserEligibility:(BOOL)eligibility {
  BOOL showSection = count > 0 && eligibility;

  if (_localPasswordsCount == count &&
      _showBulkMovePasswordsToAccount == showSection) {
    return;
  }

  _localPasswordsCount = count;
  _showBulkMovePasswordsToAccount = showSection;
  [self updateBulkMovePasswordsToAccountSection];
}

- (void)setPasswordsInOtherAppsEnabled:(BOOL)enabled {
  if (_passwordsInOtherAppsEnabled.has_value() &&
      _passwordsInOtherAppsEnabled.value() == enabled) {
    return;
  }

  _passwordsInOtherAppsEnabled = enabled;

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  [self updatePasswordsInOtherAppsItem];
}

- (void)setOnDeviceEncryptionState:
    (PasswordSettingsOnDeviceEncryptionState)onDeviceEncryptionState {
  PasswordSettingsOnDeviceEncryptionState oldState = _onDeviceEncryptionState;
  if (oldState == onDeviceEncryptionState) {
    return;
  }
  _onDeviceEncryptionState = onDeviceEncryptionState;

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  [self updateOnDeviceEncryptionSectionWithOldState:oldState];
}

- (void)updateExportPasswordsButton {
  // This can be invoked before the item is ready when passwords are received
  // too early.
  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }
  if (self.canExportPasswords) {
    _exportPasswordsItem.textColor = [UIColor colorNamed:kBlueColor];
    _exportPasswordsItem.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    // Disable, rather than remove, because the button will go back and forth
    // between enabled/disabled status as the flow progresses.
    _exportPasswordsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _exportPasswordsItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ _exportPasswordsItem ]];
}

#pragma mark - Actions

- (void)savePasswordsSwitchChanged:(UISwitch*)switchView {
  base::UmaHistogramBoolean(
      "PasswordManager.Settings.ToggleOfferToSavePasswords", switchView.on);
  [self.delegate savedPasswordSwitchDidChange:switchView.on];
}

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  [self.presentationDelegate showManagedPrefInfoForSourceView:buttonView];
  // Disable the button when showing the bubble.
  buttonView.enabled = NO;
}

// Called when the user changes the state of the automatic passkey upgrades
// switch.
- (void)automaticPasskeyUpgradesSwitchChanged {
  // TODO(crbug.com/358343061): Handle changing the switch value.
}

#pragma mark - Private

// Adds the appropriate content to the Save Passwords Switch section depending
// on whether or not the pref is managed.
- (void)addSavePasswordsSwitchOrManagedInfo {
  TableViewItem* item = self.isManagedByPolicy ? self.managedSavePasswordsItem
                                               : self.savePasswordsItem;
  [self.tableViewModel addItem:item
       toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
}

// Updates the appearance of the Managed Save Passwords item to reflect the
// current state of `isSavePasswordEnabled`.
- (void)updateManagedSavePasswordsItem {
  self.managedSavePasswordsItem.statusText =
      self.isSavePasswordsEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  [self reconfigureCellsForItems:@[ self.managedSavePasswordsItem ]];
}

// Updates the appearance of the Save Passwords switch to reflect the current
// state of `isSavePasswordEnabled`.
- (void)updateSavePasswordsSwitch {
  self.savePasswordsItem.on = self.isSavePasswordsEnabled;

  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }
  [self reconfigureCellsForItems:@[ self.savePasswordsItem ]];
}

- (void)updateBulkMovePasswordsToAccountSection {
  BOOL sectionExists =
      [self.tableViewModel hasSectionForSectionIdentifier:
                               SectionIdentifierBulkMovePasswordsToAccount];

  // Remove the section if it exists and we shouldn't show it.
  if (!_showBulkMovePasswordsToAccount && sectionExists) {
    NSInteger section =
        [self.tableViewModel sectionForSectionIdentifier:
                                 SectionIdentifierBulkMovePasswordsToAccount];
    [self.tableViewModel removeSectionWithIdentifier:
                             SectionIdentifierBulkMovePasswordsToAccount];
    if (self.modelLoadStatus == ModelLoadComplete) {
      [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    return;
  }

  if (!_showBulkMovePasswordsToAccount) {
    return;
  }

  // Prepare the section in the model, either by clearing or adding it.
  if (sectionExists) {
    [self.tableViewModel deleteAllItemsFromSectionWithIdentifier:
                             SectionIdentifierBulkMovePasswordsToAccount];
  } else {
    // Find the section that's supposed to be before Bulk Move Passwords to
    // Account, and insert after that.
    NSInteger bulkMovePasswordsToAccountSectionIndex =
        [self.tableViewModel
            sectionForSectionIdentifier:SectionIdentifierSavePasswordsSwitch] +
        1;
    [self.tableViewModel
        insertSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount
                            atIndex:bulkMovePasswordsToAccountSectionIndex];

    // Record histogram only if the section doesn't already exist but is about
    // to be shown.
    base::UmaHistogramEnumeration(
        "PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered",
        password_manager::metrics_util::MoveToAccountStoreTrigger::
            kExplicitlyTriggeredForMultiplePasswordsInSettings);
  }

  // Add the description and button items to the bulk move passwords to account
  // section.
  [self.tableViewModel addItem:self.bulkMovePasswordsToAccountDescriptionItem
       toSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount];
  [self.tableViewModel addItem:self.bulkMovePasswordsToAccountButtonItem
       toSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount];

  NSIndexSet* indexSet = [NSIndexSet
      indexSetWithIndex:[self.tableViewModel
                            sectionForSectionIdentifier:
                                SectionIdentifierBulkMovePasswordsToAccount]];

  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }

  // Reload the section if it exists, otherwise insert it if it does not.
  if (sectionExists) {
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    [self.tableView insertSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

// Updates the appearance of the Passwords In Other Apps item to reflect the
// current state of `_passwordsInOtherAppsEnabled`.
- (void)updatePasswordsInOtherAppsItem {
  if (_passwordsInOtherAppsEnabled.has_value()) {
    self.passwordsInOtherAppsItem.detailText =
        _passwordsInOtherAppsEnabled.value()
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

    if (self.modelLoadStatus != ModelLoadComplete) {
      return;
    }
    [self reconfigureCellsForItems:@[ self.passwordsInOtherAppsItem ]];
  }
}

// Updates the UI to present the correct elements for the user's current
// on-device encryption state. `oldState` indicates the currently-displayed UI
// at the time of invocation and is used to determine if we need to add a new
// section or clear (and possibly reload) an existing one.
- (void)updateOnDeviceEncryptionSectionWithOldState:
    (PasswordSettingsOnDeviceEncryptionState)oldState {
  // Easy case: the section just needs to be removed.
  if (self.onDeviceEncryptionState ==
          PasswordSettingsOnDeviceEncryptionStateNotShown &&
      [self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]) {
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption];
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
    if (self.modelLoadStatus == ModelLoadComplete) {
      [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                    withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    return;
  }

  // Prepare the section in the model, either by clearing or adding it.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]) {
    [self.tableViewModel deleteAllItemsFromSectionWithIdentifier:
                             SectionIdentifierOnDeviceEncryption];
  } else {
    // Find the section that's supposed to be before On-Device Encryption, and
    // insert after that.
    NSInteger priorSectionIndex =
        [self.tableViewModel sectionForSectionIdentifier:
                                 SyncingWebauthnCredentialsEnabled()
                                     ? SectionIdentifierGooglePasswordManagerPin
                                     : SectionIdentifierPasswordsInOtherApps];
    NSInteger onDeviceEncryptionSectionIndex = priorSectionIndex + 1;
    [self.tableViewModel
        insertSectionWithIdentifier:SectionIdentifierOnDeviceEncryption
                            atIndex:onDeviceEncryptionSectionIndex];
  }

  // Actually populate the section.
  switch (self.onDeviceEncryptionState) {
    case PasswordSettingsOnDeviceEncryptionStateOptedIn: {
      [self.tableViewModel addItem:self.onDeviceEncryptionOptedInDescription
           toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      [self.tableViewModel addItem:self.onDeviceEncryptionOptedInLearnMore
           toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      break;
    }
    case PasswordSettingsOnDeviceEncryptionStateOfferOptIn: {
      [self.tableViewModel addItem:self.onDeviceEncryptionOptInDescriptionItem
           toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      [self.tableViewModel addItem:self.setUpOnDeviceEncryptionItem
           toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      break;
    }
    default: {
      // If the state is PasswordSettingsOnDeviceEncryptionStateNotShown, then
      // we shouldn't be trying to populate this section. If it's some other
      // value, then this switch needs to be updated.
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }

  // If the model hasn't finished loading, there's no need to update the table
  // view.
  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }

  NSIndexSet* indexSet = [NSIndexSet
      indexSetWithIndex:
          [self.tableViewModel
              sectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]];

  if (oldState == PasswordSettingsOnDeviceEncryptionStateNotShown) {
    [self.tableView insertSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

@end
