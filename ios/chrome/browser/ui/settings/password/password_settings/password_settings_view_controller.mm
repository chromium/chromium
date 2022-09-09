// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
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
  SectionIdentifierExportPasswordsButton = kSectionIdentifierEnumZero,
};

// Items within the password settings UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeExportPasswordsButton = kItemTypeEnumZero,
};

}  // namespace

@interface PasswordSettingsViewController () {
  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;
}

// Whether or not the exporter should be enabled.
@property(nonatomic) BOOL canExportPasswords;

@end

@implementation PasswordSettingsViewController

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
  [self loadModel];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self.presentationDelegate passwordSettingsViewControllerDidDismiss];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  _exportPasswordsItem = [self makeExportPasswordsItem];
  [self updateExportPasswordsButton];
  [model addItem:_exportPasswordsItem
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
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

- (void)updateExportPasswordsButton {
  // This can be invoked before the item is ready when passwords are received
  // too early.
  if (!_exportPasswordsItem) {
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

@end
