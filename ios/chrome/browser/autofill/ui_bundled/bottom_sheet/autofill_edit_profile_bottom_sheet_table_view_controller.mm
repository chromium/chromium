// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomExpandedDetentIdentifier = @"customExpandedDetent";

// Deafult height of the header/footer, used to speed the constraints.
const CGFloat kDefaultHeaderFooterHeight = 10;
}  // namespace

@interface AutofillEditProfileBottomSheetTableViewController () <
    UITextFieldDelegate> {
  // Delegate for this view controller.
  __weak id<AutofillEditProfileBottomSheetTableViewControllerDelegate>
      _delegate;

  // Denotes the mode of the address save for the edit profile bottom sheet.
  AutofillSaveProfilePromptMode _editSheetMode;
}

@end

@implementation AutofillEditProfileBottomSheetTableViewController

#pragma mark - Initialization

- (instancetype)
    initWithDelegate:
        (id<AutofillEditProfileBottomSheetTableViewControllerDelegate>)delegate
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

  [self setUpBottomSheetPresentationController];
  [self setUpBottomSheetDetents];

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

- (void)expandBottomSheet {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  // Expand to large detent.
  [presentationController animateChanges:^{
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }];
}

- (void)setUpBottomSheetPresentationController {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
}

- (void)setUpBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  presentationController.selectedDetentIdentifier =
      UISheetPresentationControllerDetentIdentifierLarge;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  return [self.handler cell:cell
          forRowAtIndexPath:indexPath
           withTextDelegate:self];
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
