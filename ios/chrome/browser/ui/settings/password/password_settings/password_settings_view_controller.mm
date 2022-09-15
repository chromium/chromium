// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sections of the password settings UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierExportPasswordsButton,
};

// Items within the password settings UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSavePasswordsSwitch = kItemTypeEnumZero,
  ItemTypeManagedSavePasswords,
  ItemTypeExportPasswordsButton,
};

}  // namespace

@interface PasswordSettingsViewController () {
  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;

  // Whether or not the model has been loaded.
  BOOL _isModelLoaded;
}

// State

// Whether or not the exporter should be enabled.
@property(nonatomic, assign) BOOL canExportPasswords;

// Whether or not the Password Manager is managed by enterprise policy.
@property(nonatomic, assign, getter=isManagedByPolicy) BOOL managedByPolicy;

// Indicates whether or not "Offer to Save Passwords" is set to enabled.
@property(nonatomic, assign, getter=isSavePasswordsEnabled)
    BOOL savePasswordsEnabled;

// UI elements

// The item related to the switch for the password manager setting.
@property(nonatomic, readonly) TableViewSwitchItem* savePasswordsItem;

// The item related to the enterprise managed save password setting.
@property(nonatomic, readonly)
    TableViewInfoButtonItem* managedSavePasswordsItem;

@end

@implementation PasswordSettingsViewController

@synthesize savePasswordsItem = _savePasswordsItem;
@synthesize managedSavePasswordsItem = _managedSavePasswordsItem;

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

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  [self addSavePasswordsSwitchOrManagedInfo];

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  _exportPasswordsItem = [self makeExportPasswordsItem];
  [self updateExportPasswordsButton];
  [model addItem:_exportPasswordsItem
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];

  _isModelLoaded = YES;
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
    case ItemTypeExportPasswordsButton: {
      if (self.canExportPasswords) {
        [self.presentationDelegate startExportFlow];
      }
      break;
    }
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

  if (!_isModelLoaded) {
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

  if (!_isModelLoaded) {
    return;
  }

  if (self.isManagedByPolicy) {
    [self updateManagedSavePasswordsItem];
  } else {
    [self updateSavePasswordsSwitch];
  }
}

- (void)updateExportPasswordsButton {
  // This can be invoked before the item is ready when passwords are received
  // too early.
  if (!_isModelLoaded) {
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
  [self reconfigureCellsForItems:@[ self.savePasswordsItem ]];
}

@end
