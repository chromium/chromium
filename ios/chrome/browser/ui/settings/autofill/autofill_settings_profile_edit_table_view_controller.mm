// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kSymbolSize = 22;

}  // namespace

@interface AutofillSettingsProfileEditTableViewController ()

@property(nonatomic, weak)
    id<AutofillSettingsProfileEditTableViewControllerDelegate>
        delegate;

// If YES, a section is shown in the view to migrate the profile to account.
@property(nonatomic, assign) BOOL showMigrateToAccountSection;

@end

@implementation AutofillSettingsProfileEditTableViewController

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillSettingsProfileEditTableViewControllerDelegate>)
                        delegate
    shouldShowMigrateToAccountButton:(BOOL)showMigrateToAccount {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _delegate = delegate;
    _showMigrateToAccountSection = showMigrateToAccount;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_ADDRESS)];
  self.tableView.allowsSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileEditTableViewId;

  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.handler viewDidDisappear];
  [super viewDidDisappear:animated];
}

- (void)loadModel {
  [super loadModel];
  [self.handler loadModel];

  TableViewModel* model = self.tableViewModel;
  if (self.showMigrateToAccountSection &&
      ![model hasSectionForSectionIdentifier:
                  AutofillProfileDetailsSectionIdentifierMigrationToAccount]) {
    [model addSectionWithIdentifier:
               AutofillProfileDetailsSectionIdentifierMigrationToAccount];
    [model addItem:[self migrateToAccountRecommendationItem]
        toSectionWithIdentifier:
            AutofillProfileDetailsSectionIdentifierMigrationToAccount];
    [model addItem:[self migrateToAccountButtonItem]
        toSectionWithIdentifier:
            AutofillProfileDetailsSectionIdentifierMigrationToAccount];
  }

  [self.handler loadFooterForSettings];
}

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  if (itemType ==
          AutofillProfileDetailsItemTypeMigrateToAccountRecommendation ||
      itemType == AutofillProfileDetailsItemTypeMigrateToAccountButton) {
    return NO;
  }
  return [self.handler isItemAtIndexPathTextEditCell:cellPath];
}

#pragma mark - SettingsRootTableViewController

- (void)editButtonPressed {
  [super editButtonPressed];

  if (!self.tableView.editing) {
    [self.handler updateProfileData];
    [self.delegate didEditAutofillProfileFromSettings];
  }

  [self loadModel];
  [self.handler reconfigureCells];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType ==
          AutofillProfileDetailsItemTypeMigrateToAccountRecommendation ||
      itemType == AutofillProfileDetailsItemTypeMigrateToAccountButton) {
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
  }
  return [self.handler cell:cell
          forRowAtIndexPath:indexPath
           withTextDelegate:self];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType ==
          AutofillProfileDetailsItemTypeMigrateToAccountRecommendation ||
      itemType == AutofillProfileDetailsItemTypeMigrateToAccountButton) {
    return;
  }
  [self.handler didSelectRowAtIndexPath:indexPath];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.handler heightForHeaderShouldBeZeroInSection:section]) {
    return 0;
  }
  return [super tableView:tableView heightForHeaderInSection:section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.handler heightForFooterShouldBeZeroInSection:section]) {
    return 0;
  }
  return [super tableView:tableView heightForFooterInSection:section];
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // If we don't allow the edit of the cell, the selection of the cell isn't
  // forwarded.
  return YES;
}

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewCellEditingStyleNone;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

#pragma mark - Items

- (SettingsImageDetailTextItem*)migrateToAccountRecommendationItem {
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:
          AutofillProfileDetailsItemTypeMigrateToAccountRecommendation];
  // TODO(crbug.com/1407666): Replace with i18n string.
  item.detailText = @"Test Migrate To account recommendation";
  item.image = CustomSymbolWithPointSize(kCloudAndArrowUpSymbol, kSymbolSize);
  item.imageViewTintColor = [UIColor colorNamed:kBlueColor];
  return item;
}

- (TableViewTextItem*)migrateToAccountButtonItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:AutofillProfileDetailsItemTypeMigrateToAccountButton];
  // TODO(crbug.com/1407666): Replace with i18n string.
  item.text = @"Test Migrate Button";
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.enabled = !self.tableView.editing;
  return item;
}

@end
