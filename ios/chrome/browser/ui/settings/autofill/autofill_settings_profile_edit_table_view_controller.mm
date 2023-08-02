// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kSymbolSize = 22;

}  // namespace

@interface AutofillSettingsProfileEditTableViewController ()

@property(nonatomic, weak)
    id<AutofillSettingsProfileEditTableViewControllerDelegate>
        delegate;

// If YES, a section is shown in the view to migrate the profile to account.
@property(nonatomic, assign) BOOL showMigrateToAccountSection;

// Stores the signed in user email, or the empty string if the user is not
// signed-in.
@property(nonatomic, readonly) NSString* userEmail;

@end

@implementation AutofillSettingsProfileEditTableViewController

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillSettingsProfileEditTableViewControllerDelegate>)
                        delegate
    shouldShowMigrateToAccountButton:(BOOL)showMigrateToAccount
                           userEmail:(NSString*)userEmail {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _delegate = delegate;
    _showMigrateToAccountSection = showMigrateToAccount;
    _userEmail = userEmail;
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
  if (self.showMigrateToAccountSection) {
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
    // It can happen that the profile does not satisfy minimum requirements for
    // the migration so we don't show the migration button.
    if (self.showMigrateToAccountSection && ![self.delegate isMinimumAddress]) {
      [self removeMigrateButtonSection:nil];
    }
  }

  [self loadModel];
  [self.handler reconfigureCells];
  if (self.showMigrateToAccountSection) {
    [self
        reconfigureCellsForItems:
            [self.tableViewModel
                itemsInSectionWithIdentifier:
                    AutofillProfileDetailsSectionIdentifierMigrationToAccount]];
  }
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
      AutofillProfileDetailsItemTypeMigrateToAccountRecommendation) {
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }
  if (itemType == AutofillProfileDetailsItemTypeMigrateToAccountButton) {
    [self.delegate didTapMigrateToAccountButton];
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
    __weak __typeof(self) weakSelf = self;
    void (^completion)(BOOL) = ^(BOOL) {
      [weakSelf showPostMigrationToast];
    };
    [self removeMigrateButtonSection:completion];
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
  CHECK(_userEmail.length)
      << "User must be signed-in to migrate an address to the "
         "account;";
  SettingsImageDetailTextItem* item = [[SettingsImageDetailTextItem alloc]
      initWithType:
          AutofillProfileDetailsItemTypeMigrateToAccountRecommendation];
  item.detailText = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_MIGRATE_ADDRESS_TO_ACCOUNT_BUTTON_DESCRIPTION,
      base::SysNSStringToUTF16(self.userEmail));
  item.image = CustomSymbolWithPointSize(kCloudAndArrowUpSymbol, kSymbolSize);
  item.imageViewTintColor = [UIColor colorNamed:kBlueColor];
  return item;
}

- (TableViewTextItem*)migrateToAccountButtonItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:AutofillProfileDetailsItemTypeMigrateToAccountButton];
  item.text = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_AUTOFILL_MIGRATE_ADDRESS_TO_ACCOUNT_BUTTON_TITLE);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.enabled = !self.tableView.editing;
  item.accessibilityIdentifier = kAutofillAddressMigrateToAccountButtonId;
  return item;
}

#pragma mark - Private

// Removes the migrate button section from the view.
- (void)removeMigrateButtonSection:(void (^)(BOOL finished))onCompletion {
  [self
      performBatchTableViewUpdates:^{
        [self removeSectionWithIdentifier:
                  AutofillProfileDetailsSectionIdentifierMigrationToAccount
                         withRowAnimation:UITableViewRowAnimationFade];
      }
                        completion:onCompletion];
  self.showMigrateToAccountSection = NO;
}

// Removes the given section if it exists.
- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier
                   withRowAnimation:(UITableViewRowAnimation)animation {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    [model removeSectionWithIdentifier:sectionIdentifier];
    [self.tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
                  withRowAnimation:animation];
  }
}

- (void)showPostMigrationToast {
  CHECK(self.snackbarCommandsHandler);
  CHECK(_userEmail.length)
      << "User must be signed-in to migrate an address to the "
         "account;";
  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_MIGRATE_ADDRESS_TO_ACCOUNT_CONFIRMATION_TEXT,
      base::SysNSStringToUTF16(self.userEmail));
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [self.snackbarCommandsHandler showSnackbarWithMessage:message
                                             buttonText:nil
                                          messageAction:nil
                                       completionAction:nil];
}

@end
