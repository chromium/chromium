// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_content.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_mutator.h"
#import "ios/chrome/browser/ui/autofill/cells/card_unmask_header_item.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemTypeTextField = kItemTypeEnumZero,
};

}  // namespace

@interface OtpInputDialogViewController () <UITableViewDelegate,
                                            UITextFieldDelegate> {
}

@end

@implementation OtpInputDialogViewController {
  OtpInputDialogContent* _content;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  BOOL _contentSet;
  NSString* _inputValue;
}

- (instancetype)init {
  return [super initWithStyle:UITableViewStyleInsetGrouped];
}

#pragma mark - ChromeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_AUTOFILL_CARD_UNMASK_PROMPT_NAVIGATION_TITLE_VERIFICATION);
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton)];
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithTitle:_content.confirmButtonLabel
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapConfirmButton)];
  // Enable the confirm button only after a valid OTP has been entered.
  self.navigationItem.rightBarButtonItem.enabled = NO;
  self.tableView.allowsSelection = NO;
  [self loadModel];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  CardUnmaskHeaderView* view =
      DequeueTableViewHeaderFooter<CardUnmaskHeaderView>(self.tableView);
  view.titleLabel.text = _content.windowTitle;
  return view;
}

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setContent:(OtpInputDialogContent*)content {
  // Content should not be updated once initialized.
  CHECK(!_contentSet);
  _content = content;
  _contentSet = YES;
}

- (void)setConfirmButtonEnabled:(BOOL)enabled {
  self.navigationItem.rightBarButtonItem.enabled = enabled;
}

- (void)showPendingState {
  // TODO(crbug.com/303715678): Handle pending state (after the confirm button
  // is clicked).
}

- (void)showInvalidState:(NSString*)invalidLabelText {
  // TODO(crbug.com/303715678): Handle error state (after the confirm button is
  // clicked and server returns a result).
}

#pragma mark - Private

// Helper function to load the model to the data source.
- (void)loadModel {
  CHECK(_contentSet);
  RegisterTableViewHeaderFooter<CardUnmaskHeaderView>(self.tableView);
  RegisterTableViewCell<TableViewTextEditCell>(self.tableView);
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierContent) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemTypeTextField) ]
             intoSectionWithIdentifier:@(SectionIdentifierContent)];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the appropriate cell for the table view.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  TableViewTextEditCell* cell =
      DequeueTableViewCell<TableViewTextEditCell>(self.tableView);
  [cell setIdentifyingIcon:nil];
  [cell setIcon:TableViewTextEditItemIconTypeEdit];
  cell.textField.placeholder = _content.textFieldPlaceholder;
  [cell.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.keyboardType = UIKeyboardTypeNumberPad;
  cell.textField.returnKeyType = UIReturnKeyDone;
  cell.textField.textAlignment = NSTextAlignmentLeft;
  return cell;
}

- (void)textFieldDidChange:(UITextField*)textField {
  _inputValue = textField.text;
  [self didChangeOtpInputText];
}

// Invoked when the confirm button in the navigation bar is tapped by the user.
// This means a valid OTP value is typed in.
- (void)didTapConfirmButton {
  [_mutator didTapConfirmButton:_inputValue];
}

// Invoked when the cancel button in the navigation bar is tapped by the user.
- (void)didTapCancelButton {
  [_mutator didTapCancelButton];
}

// Notify the model controller when the OTP input value changes.
- (void)didChangeOtpInputText {
  [_mutator onOtpInputChanges:_inputValue];
}

@end
