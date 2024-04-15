// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/autofill_edit_profile_bottom_sheet_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
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

}  // namespace

@interface AutofillEditProfileBottomSheetTableViewController () <
    UITextFieldDelegate>

// TODO(crbug.com/1482269): Update via the consumer protocol.
// Yes, if the edit is done for updating the profile.
@property(nonatomic, assign) BOOL isEditForUpdate;

// TODO(crbug.com/1482269): Update via the consumer protocol.
// Yes, if the edit is shown for the migration prompt.
@property(nonatomic, assign) BOOL migrationPrompt;

@end

@implementation AutofillEditProfileBottomSheetTableViewController

#pragma mark - Initialization

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setUpBottomSheetPresentationController];
  [self setUpBottomSheetDetents];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.estimatedRowHeight = 56;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(handleCancelButton)];

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;
  if (self.migrationPrompt) {
    [self setTitle:
              l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_ADDRESS_MIGRATION_TO_ACCOUNT_PROMPT_TITLE)];
  } else {
    [self setTitle:l10n_util::GetNSString(
                       self.isEditForUpdate
                           ? IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE
                           : IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE)];
  }

  self.tableView.allowsSelectionDuringEditing = YES;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  [self.handler setMigrationPrompt:self.migrationPrompt];
  [self.handler loadModel];
  [self.handler
      loadMessageAndButtonForModalIfSaveOrUpdate:self.isEditForUpdate];
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

#pragma mark - Actions

- (void)handleCancelButton {
  // TODO(crbug.com/1482269): Implement.
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return NO;
}

@end
