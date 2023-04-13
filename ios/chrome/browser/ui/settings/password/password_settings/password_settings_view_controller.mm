// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding between the "N" text and the surrounding symbol.
const CGFloat kNewFeatureIconPadding = 2.5;

// Sections of the password settings UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierPasswordsInOtherApps,
  SectionIdentifierOnDeviceEncryption,
  SectionIdentifierExportPasswordsButton,
};

// Items within the password settings UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSavePasswordsSwitch = kItemTypeEnumZero,
  ItemTypeAccountStorageSwitch,
  ItemTypeManagedSavePasswords,
  ItemTypePasswordsInOtherApps,
  ItemTypeExportPasswordsButton,
  ItemTypeOnDeviceEncryptionOptInDescription,
  ItemTypeOnDeviceEncryptionOptedInDescription,
  ItemTypeOnDeviceEncryptionOptedInLearnMore,
  ItemTypeOnDeviceEncryptionSetUp,
};

// Indicates whether the model has not started loading, is in the process of
// loading, or has completed loading.
typedef NS_ENUM(NSInteger, ModelLoadStatus) {
  ModelNotLoaded = 0,
  ModelIsLoading,
  ModelLoadComplete,
};

}  // namespace

@interface PasswordSettingsViewController () {
  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;

  // Whether or not Chromium has been enabled as a credential provider at the
  // iOS level. This may not be known at load time; the detail text showing on
  // or off status will be omitted until this is populated.
  absl::optional<bool> _passwordsInOtherAppsEnabled;
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

// Indicates the state of the account storage switch.
@property(nonatomic, assign)
    PasswordSettingsAccountStorageState accountStorageState;

// Indicates whether the account storage switch should contain an icon
// indicating a new feature. This doesn't mean the switch itself is shown.
@property(nonatomic, assign) BOOL showAccountStorageNewFeatureIcon;

// Indicates the signed in account.
@property(nonatomic, copy) NSString* signedInAccount;

// On-device encryption state according to the sync service.
@property(nonatomic, assign)
    PasswordSettingsOnDeviceEncryptionState onDeviceEncryptionState;

// UI elements

// The item related to the switch for the password manager setting.
@property(nonatomic, readonly) TableViewSwitchItem* savePasswordsItem;

// The item related to the switch for the account storage opt-in.
@property(nonatomic, readonly) TableViewSwitchItem* accountStorageItem;

// The item related to the enterprise managed save password setting.
@property(nonatomic, readonly)
    TableViewInfoButtonItem* managedSavePasswordsItem;

// The item showing the current status of Passwords in Other Apps (i.e.,
// credential provider).
@property(nonatomic, readonly)
    TableViewDetailIconItem* passwordsInOtherAppsItem;

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
@synthesize accountStorageItem = _accountStorageItem;
@synthesize managedSavePasswordsItem = _managedSavePasswordsItem;
@synthesize passwordsInOtherAppsItem = _passwordsInOtherAppsItem;
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

- (CGRect)sourceRectForPasswordExportAlerts {
  return [self.tableView
             cellForRowAtIndexPath:[self.tableViewModel
                                       indexPathForItem:_exportPasswordsItem]]
      .frame;
}

- (UIView*)sourceViewForPasswordExportAlerts {
  return self.tableView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS);
  self.tableView.accessibilityIdentifier = kPasswordsSettingsTableViewId;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  self.modelLoadStatus = ModelIsLoading;

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  [self addSavePasswordsSwitchOrManagedInfo];

  if (self.accountStorageState != PasswordSettingsAccountStorageStateNotShown) {
    [self updateAccountStorageSwitch];
  }

  [model addSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
  [model addItem:[self passwordsInOtherAppsItem]
      toSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];

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
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(savePasswordsSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeAccountStorageSwitch: {
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(accountStorageSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];

      if (!_showAccountStorageNewFeatureIcon) {
        break;
      }

      // Add new feature icon, vertically centered with the text.
      [self.delegate accountStorageNewFeatureIconDidShow];
      NSTextAttachment* iconAttachment = [[NSTextAttachment alloc] init];
      iconAttachment.image = [PasswordSettingsViewController newFeatureIcon];
      CGSize iconSize = iconAttachment.image.size;
      iconAttachment.bounds = CGRectMake(
          0, (switchCell.textLabel.font.capHeight - iconSize.height) / 2,
          iconSize.width, iconSize.height);
      NSMutableAttributedString* textAndIcon =
          [[NSMutableAttributedString alloc]
              initWithAttributedString:switchCell.textLabel.attributedText];
      [textAndIcon appendAttributedString:[[NSAttributedString alloc]
                                              initWithString:@" "]];
      [textAndIcon appendAttributedString:
                       [NSAttributedString
                           attributedStringWithAttachment:iconAttachment]];
      switchCell.textLabel.attributedText = textAndIcon;
      switchCell.accessibilityLabel = [NSString
          stringWithFormat:@"%@, %@, %@", switchCell.textLabel.text,
                           l10n_util::GetNSString(
                               IDS_IOS_NEW_FEATURE_ACCESSIBILITY_LABEL),
                           switchCell.detailTextLabel.text];
      break;
    }
    case ItemTypeManagedSavePasswords: {
      TableViewInfoButtonCell* managedCell =
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
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
      NOTREACHED();
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeExportPasswordsButton: {
      return self.canExportPasswords;
    }
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

- (TableViewSwitchItem*)accountStorageItem {
  if (_accountStorageItem) {
    return _accountStorageItem;
  }

  DCHECK_GT([self.signedInAccount length], 0u)
      << "Account storage item shouldn't be shown if there's no signed-in "
         "account";

  _accountStorageItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeAccountStorageSwitch];
  _accountStorageItem.text =
      l10n_util::GetNSString(IDS_IOS_ACCOUNT_STORAGE_OPT_IN_LABEL);
  _accountStorageItem.detailText =
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_STORAGE_OPT_IN_SUBLABEL,
                              base::SysNSStringToUTF16(self.signedInAccount));
  _accountStorageItem.accessibilityIdentifier =
      kPasswordSettingsAccountStorageSwitchTableViewId;
  return _accountStorageItem;
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
  _onDeviceEncryptionOptInDescriptionItem.accessibilityTraits |=
      UIAccessibilityTraitLink;
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
  _setUpOnDeviceEncryptionItem.accessibilityTraits |= UIAccessibilityTraitLink;
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

- (void)setAccountStorageState:(PasswordSettingsAccountStorageState)state {
  if (_accountStorageState == state) {
    return;
  }

  _accountStorageState = state;

  if (self.modelLoadStatus != ModelNotLoaded) {
    [self updateAccountStorageSwitch];
  }
}

- (void)setShowAccountStorageNewFeatureIcon:(BOOL)show {
  _showAccountStorageNewFeatureIcon = show;
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
  [self.delegate savedPasswordSwitchDidChange:switchView.on];
}

- (void)accountStorageSwitchChanged:(UISwitch*)switchView {
  base::UmaHistogramBoolean("PasswordManager.AccountStorageOptInSwitchFlipped",
                            switchView.on);
  [self.delegate accountStorageSwitchDidChange:switchView.on];
}

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  [self.presentationDelegate showManagedPrefInfoForSourceView:buttonView];
  // Disable the button when showing the bubble.
  buttonView.enabled = NO;
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

- (void)updateAccountStorageSwitch {
  const BOOL hadItem = [self.tableViewModel
      hasItemForItemType:ItemTypeAccountStorageSwitch
       sectionIdentifier:SectionIdentifierSavePasswordsSwitch];
  switch (self.accountStorageState) {
    case PasswordSettingsAccountStorageStateNotShown: {
      if (!hadItem) {
        return;
      }

      // Cache index path before removing.
      NSIndexPath* indexPath = [self.tableViewModel
          indexPathForItemType:ItemTypeAccountStorageSwitch];
      [self.tableViewModel
                 removeItemWithType:ItemTypeAccountStorageSwitch
          fromSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
      if (self.modelLoadStatus == ModelLoadComplete) {
        [self.tableView
            deleteRowsAtIndexPaths:@[ indexPath ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
      return;
    }
    case PasswordSettingsAccountStorageStateOptedIn:
    case PasswordSettingsAccountStorageStateOptedOut:
    case PasswordSettingsAccountStorageStateDisabledByPolicy: {
      if (!hadItem) {
        [self.tableViewModel addItem:self.accountStorageItem
             toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
      }

      self.accountStorageItem.on = self.accountStorageState ==
                                   PasswordSettingsAccountStorageStateOptedIn;
      self.accountStorageItem.enabled =
          self.accountStorageState !=
          PasswordSettingsAccountStorageStateDisabledByPolicy;

      if (self.modelLoadStatus != ModelLoadComplete) {
        return;
      }

      NSIndexPath* indexPath = [self.tableViewModel
          indexPathForItemType:ItemTypeAccountStorageSwitch];
      if (!hadItem) {
        [self.tableView
            insertRowsAtIndexPaths:@[ indexPath ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];

      } else {
        [self.tableView
            reloadRowsAtIndexPaths:@[ indexPath ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
      return;
    }
    default: {
      NOTREACHED();
      return;
    }
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
    [self reloadCellsForItems:@[ self.passwordsInOtherAppsItem ]
             withRowAnimation:UITableViewRowAnimationNone];
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
    NSInteger priorSectionIndex = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierPasswordsInOtherApps];
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
      NOTREACHED();
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

+ (UIImage*)newFeatureIcon {
  UIFontDescriptor* fontDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleCaption1];
  fontDescriptor = [fontDescriptor
      fontDescriptorWithDesign:UIFontDescriptorSystemDesignRounded];
  fontDescriptor = [fontDescriptor fontDescriptorByAddingAttributes:@{
    UIFontDescriptorTraitsAttribute :
        @{UIFontWeightTrait : [NSNumber numberWithFloat:UIFontWeightHeavy]}
  }];

  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont fontWithDescriptor:fontDescriptor size:0.0];
  label.text = l10n_util::GetNSString(IDS_IOS_NEW_LABEL_FEATURE_BADGE);
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  UIImageView* image = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(
                        @"seal.fill",
                        label.font.pointSize + 2 * kNewFeatureIconPadding)];
  image.tintColor = [UIColor colorNamed:kBlue600Color];
  image.translatesAutoresizingMaskIntoConstraints = NO;
  [image addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [image.centerXAnchor constraintEqualToAnchor:label.centerXAnchor],
    [image.centerYAnchor constraintEqualToAnchor:label.centerYAnchor]
  ]];
  return ImageFromView(image, [UIColor clearColor], UIEdgeInsetsZero);
}

@end
