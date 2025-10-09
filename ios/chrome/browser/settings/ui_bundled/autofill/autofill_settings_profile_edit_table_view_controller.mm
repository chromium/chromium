// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_profile_edit_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

const CGFloat kSymbolSize = 22;

}  // namespace

@interface AutofillSettingsProfileEditTableViewController ()

// Stores the signed in user email, or the empty string if the user is not
// signed-in.
@property(nonatomic, readonly) NSString* userEmail;

@end

@implementation AutofillSettingsProfileEditTableViewController {
  __weak id<AutofillSettingsProfileEditTableViewControllerDelegate> _delegate;

  // If YES, a section is shown in the view to migrate the profile to account.
  BOOL _showMigrateToAccountSection;

  // If YES, denotes that the view shown is to edit the incomplete profiles so
  // that it can migrated to account.
  BOOL _editIncompleteProfileForAccountView;

  // If `YES`, denotes that the migration to account was clicked.
  BOOL _migrationToAccountSectionWasClicked;

  // If `YES`, the button is shown as a cell item in a section.
  BOOL _showEditButtonAsCell;
}

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
    _editIncompleteProfileForAccountView = NO;
    _migrationToAccountSectionWasClicked = NO;
    switch ([_delegate accountRecordType]) {
      case autofill::AutofillProfile::RecordType::kAccountHome:
      case autofill::AutofillProfile::RecordType::kAccountWork:
      case autofill::AutofillProfile::RecordType::kAccountNameEmail:
        _showEditButtonAsCell = YES;
        break;
      case autofill::AutofillProfile::RecordType::kAccount:
      case autofill::AutofillProfile::RecordType::kLocalOrSyncable:
        _showEditButtonAsCell = NO;
        break;
    }
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

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  SettingsNavigationController* navigationController =
      base::apple::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  if (!navigationController) {
    return;
  }

  // Add a "Done" button to the navigation bar if this view controller is the
  // first in the navigation stack. This "Done" button's purpose being to
  // dismiss the presented view.
  if (navigationController.viewControllers.count > 0 &&
      navigationController.viewControllers.firstObject == self) {
    UIBarButtonItem* doneButton = [navigationController doneButton];

    // If not in edit mode, set the newly created "Done" button as the left bar
    // button item. Otherwise, don't override the "Cancel" button that's shown
    // when in edit mode.
    if (!self.tableView.editing) {
      self.navigationItem.leftBarButtonItem = doneButton;
    }

    // Set `customLeftBarButtonItem` with the "Done" button, so that it'll be
    // used as the left bar button item when exiting edit mode.
    self.customLeftBarButtonItem = doneButton;
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.handler viewDidDisappear];
  [super viewDidDisappear:animated];
}

- (void)loadModel {
  [super loadModel];
  [self.handler loadModel];

  TableViewModel* model = self.tableViewModel;
  if (_showMigrateToAccountSection) {
    [model addSectionWithIdentifier:
               AutofillProfileDetailsSectionIdentifierMigrationButton];
    [model addItem:[self migrateToAccountRecommendationItem]
        toSectionWithIdentifier:
            AutofillProfileDetailsSectionIdentifierMigrationButton];
    [model addItem:[self migrateToAccountButtonItem]
        toSectionWithIdentifier:
            AutofillProfileDetailsSectionIdentifierMigrationButton];
  }

  if (_showEditButtonAsCell) {
    [model
        addSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierEdit];
    [model addItem:[self editCellButtonItem]
        toSectionWithIdentifier:AutofillProfileDetailsSectionIdentifierEdit];
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
    if (_editIncompleteProfileForAccountView) {
      [_delegate didTapMigrateToAccountButton];
      [self showPostMigrationToast];
      [self.handler setMoveToAccountFromSettings:NO];
      _editIncompleteProfileForAccountView = NO;
    } else {
      [_delegate didEditAutofillProfileFromSettings];
    }
  }
  [self reloadData];
}

- (BOOL)shouldShowEditButton {
  return !_showEditButtonAsCell;
}

- (BOOL)editButtonEnabled {
  return !_migrationToAccountSectionWasClicked ||
         _showMigrateToAccountSection || [_delegate isMinimumAddress];
}

- (BOOL)showCancelDuringEditing {
  SettingsNavigationController* navigationController =
      base::apple::ObjCCast<SettingsNavigationController>(
          self.navigationController);

  return navigationController &&
         navigationController.viewControllers.count > 0 &&
         navigationController.viewControllers.firstObject == self;
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
  if (itemType == AutofillProfileDetailsItemTypeEdit) {
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
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
    if ([_delegate isMinimumAddress]) {
      [_delegate didTapMigrateToAccountButton];
      __weak __typeof(self) weakSelf = self;
      void (^completion)(BOOL) = ^(BOOL) {
        [weakSelf showPostMigrationToast];
      };
      [self removeMigrateButton:completion];
    } else {
      // Show the profile in the edit mode.
      _editIncompleteProfileForAccountView = YES;
      [self removeMigrateButton:nil];
      [self editButtonPressed];
      [self.handler setMoveToAccountFromSettings:YES];
    }
    return;
  }
  if (itemType == AutofillProfileDetailsItemTypeEdit) {
    std::string URL;
    switch ([_delegate accountRecordType]) {
      case autofill::AutofillProfile::RecordType::kAccountHome:
        URL = kGoogleMyAccountHomeAddressURL;
        break;
      case autofill::AutofillProfile::RecordType::kAccountWork:
        URL = kGoogleMyAccountWorkAddressURL;
        break;
      case autofill::AutofillProfile::RecordType::kAccountNameEmail:
        URL = kGoogleAccountNameEmailAddressEditURL;
        break;
      case autofill::AutofillProfile::RecordType::kAccount:
      case autofill::AutofillProfile::RecordType::kLocalOrSyncable:
        NOTREACHED();
    }

    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:GURL(URL)];
    [self.applicationHandler closePresentedViewsAndOpenURL:command];
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
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  return itemType != AutofillProfileDetailsItemTypeEdit;
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
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_BUTTON_ITEM);
  item.textColor = self.tableView.editing
                       ? [UIColor colorNamed:kTextSecondaryColor]
                       : [UIColor colorNamed:kBlueColor];
  item.enabled = !self.tableView.editing;
  item.accessibilityIdentifier = kAutofillAddressMigrateToAccountButtonId;
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

- (TableViewDetailTextItem*)editCellButtonItem {
  TableViewDetailTextItem* item = [[TableViewDetailTextItem alloc]
      initWithType:AutofillProfileDetailsItemTypeEdit];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_EDIT_ADDRESS_CELL);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityIdentifier = kAutofillEditButtonCellId;
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessorySymbol = TableViewDetailTextCellAccessorySymbolExternalLink;
  return item;
}

#pragma mark - Private

// Removes the migrate button section from the view.
- (void)removeMigrateButton:(void (^)(BOOL finished))onCompletion {
  [self
      performBatchTableViewUpdates:^{
        [self removeSectionWithIdentifier:
                  AutofillProfileDetailsSectionIdentifierMigrationButton
                         withRowAnimation:UITableViewRowAnimationFade];
      }
                        completion:onCompletion];
  _showMigrateToAccountSection = NO;
  _migrationToAccountSectionWasClicked = YES;
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
