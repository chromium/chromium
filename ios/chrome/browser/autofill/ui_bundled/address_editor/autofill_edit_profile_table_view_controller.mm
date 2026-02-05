// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Deafult height of the header/footer, used to speed the constraints.
const CGFloat kDefaultHeaderFooterHeight = 10;
}  // namespace

@interface AutofillEditProfileTableViewController () {
  // Delegate for this view controller.
  __weak id<AutofillEditProfileTableViewControllerDelegate> _delegate;

  // Denotes the mode of the address save for the edit profile bottom sheet.
  AutofillSaveProfilePromptMode _editSheetMode;
}

@end

@implementation AutofillEditProfileTableViewController

#pragma mark - Initialization

- (instancetype)initWithDelegate:
                    (id<AutofillEditProfileTableViewControllerDelegate>)delegate
                   editSheetMode:(AutofillSaveProfilePromptMode)editSheetMode {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _delegate = delegate;
    _editSheetMode = editSheetMode;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];
  cancelButton.accessibilityIdentifier = kEditProfileBottomSheetCancelButton;

  self.navigationItem.leftBarButtonItem = cancelButton;

  self.navigationController.navigationBar.prefersLargeTitles = NO;
  switch (_editSheetMode) {
    case AutofillSaveProfilePromptMode::kNewProfile:
      [self setTitle:l10n_util::GetNSString(
                         IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE)];
      break;
    case AutofillSaveProfilePromptMode::kUpdateProfile:
      [self setTitle:l10n_util::GetNSString(
                         IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE)];
      break;
    case AutofillSaveProfilePromptMode::kMigrateProfile:
      [self
          setTitle:
              l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE)];
      break;
  }

  self.tableView.allowsSelectionDuringEditing = YES;
  self.view.accessibilityIdentifier = kEditProfileBottomSheetViewIdentfier;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  [self.handler
      setMigrationPrompt:(_editSheetMode ==
                          AutofillSaveProfilePromptMode::kMigrateProfile)];
  [self.handler loadModel];
  [self.handler
      loadMessageAndButtonForModalIfSaveOrUpdate:
          (_editSheetMode == AutofillSaveProfilePromptMode::kUpdateProfile)];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  return [self.handler cell:cell forRowAtIndexPath:indexPath];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.handler didSelectRowAtIndexPath:indexPath];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  [self.handler configureView:view forFooterInSection:section];
  return view;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.tableViewModel headerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }

  return kDefaultHeaderFooterHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.handler heightForFooterShouldBeZeroInSection:section]) {
    return 0;
  }

  if ([self.tableViewModel footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }

  return kDefaultHeaderFooterHeight;
}

#pragma mark - Actions

- (void)handleCancelButton {
  CHECK(_delegate);
  [_delegate didCancelBottomSheetView];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return NO;
}

@end
