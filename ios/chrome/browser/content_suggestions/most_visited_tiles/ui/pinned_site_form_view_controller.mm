// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/pinned_site_form_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/metrics.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/pinned_site_action.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_pinned_site_mutator.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

/// The identifier for the only section in the table.
const NSInteger kSection = 0;

/// Identifier for items.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemTypeName = 0,
  ItemTypeURL,
};

/// Helper method to detect whether `input` is non-empty.
BOOL IsInputValid(NSString* input) {
  return [[input stringByTrimmingCharactersInSet:[NSCharacterSet
                                                     whitespaceCharacterSet]]
             length] > 0;
}

/// Error message that should be displayed for each possible results when the
/// user applies the changes.
NSString* GetErrorMessage(PinnedSiteMutationResult result) {
  int message_id;
  switch (result) {
    case PinnedSiteMutationResult::kSuccess:
      return nil;
    case PinnedSiteMutationResult::kURLExisted:
      message_id = IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL_EXISTS;
      break;
    case PinnedSiteMutationResult::kURLInvalid:
      message_id =
          IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL_VALIDATION_FAILED;
      break;
  }
  return l10n_util::GetNSString(message_id);
}

}  // namespace

@interface PinnedSiteFormViewController ()

@end

@implementation PinnedSiteFormViewController {
  /// Action performed on the form.
  PinnedSiteAction _action;
  /// Data source for this table.
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  /// Original input values.
  NSString* _originalName;
  NSString* _originalURL;
  /// Current input values.
  NSString* _name;
  NSString* _URL;
  /// Currently displaying error message. Should be updated using
  /// `-setErrorMessage:` method.
  NSString* _errorMessage;
  /// Whether any error has been encountered before form dismissal.
  BOOL _hasFailedOnce;
  /// If `YES`, the form is ready to be edited.
  BOOL _canBeginEditing;
}

- (instancetype)initWithAction:(PinnedSiteAction)action
                       forItem:(MostVisitedItem*)item {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _action = action;
    if (_action == PinnedSiteAction::kModify) {
      CHECK(item);
      _originalName = item.title;
      _originalURL = base::SysUTF8ToNSString(item.URL.spec());
    }
    _name = _originalName;
    _URL = _originalURL;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  int titleId;
  int doneButtonTextId;
  switch (_action) {
    case PinnedSiteAction::kCreate:
      titleId = IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ADD_PINNED_SITE_TITLE;
      doneButtonTextId = IDS_ADD;
      break;
    case PinnedSiteAction::kModify:
      titleId = IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_EDIT_PINNED_SITE_TITLE;
      doneButtonTextId = IDS_SAVE;
      break;
  }
  self.navigationItem.title = l10n_util::GetNSString(titleId);
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(onCancel)];
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(doneButtonTextId)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(onApplyButtonTap)];
  [self loadModel];
  [self updateApplyButtonState];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordPinnedSiteFormUserAction(
      _action, _hasFailedOnce
                   ? MostVisitedPinSiteFormUserAction::kDismissAfterFailure
                   : MostVisitedPinSiteFormUserAction::kDismissImmediately);
}

#pragma mark - UIResponder

/// To always be able to register key commands via `keyCommands`, the VC must be
/// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self onCancel];
}

#pragma mark - Private

/// Creates the data source and apply initial snapshot.
- (void)loadModel {
  /// Create data source.
  RegisterTableViewCell<TableViewTextEditCell>(self.tableView);
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSourceCellProvider cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath,
      NSNumber* itemIdentifier) {
    CHECK_EQ(tableView, weakSelf.tableView);
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:itemIdentifier];
  };
  _dataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];
  /// Initialize table.
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(kSection) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemTypeName), @(ItemTypeURL) ]
             intoSectionWithIdentifier:@(kSection)];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Cell provider for the data source.
- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(NSNumber*)identifier {
  TableViewTextEditCell* cell =
      DequeueTableViewCell<TableViewTextEditCell>(self.tableView);
  cell.textField.enabled = YES;
  [cell.textField removeTarget:self
                        action:nil
              forControlEvents:UIControlEventEditingChanged];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.identifyingIconButton.hidden = YES;
  cell.isAccessibilityElement = NO;
  BOOL maybeFocusOnCell;
  switch (static_cast<ItemIdentifier>(identifier.integerValue)) {
    case ItemTypeName:
      cell.textLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_NAME);
      cell.textField.text = _name;
      cell.textField.placeholder = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_NAME);
      cell.textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
      [cell.textField addTarget:self
                         action:@selector(nameDidChange:)
               forControlEvents:UIControlEventEditingChanged];
      maybeFocusOnCell = _action == PinnedSiteAction::kModify;
      break;
    case ItemTypeURL:
      cell.textLabel.text =
          l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL);
      cell.textField.text = _URL;
      cell.textField.placeholder = @"https://example.com";
      cell.textField.keyboardType = UIKeyboardTypeURL;
      cell.textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
      [cell.textField addTarget:self
                         action:@selector(URLDidChange:)
               forControlEvents:UIControlEventEditingChanged];
      maybeFocusOnCell = _action == PinnedSiteAction::kCreate;
      break;
  }
  if (!_canBeginEditing && maybeFocusOnCell) {
    /// Auto focus on the URL field so the user could type immediately,
    /// without having to tap on the cell first.
    [cell.textField becomeFirstResponder];
    _canBeginEditing = YES;
  }
  return cell;
}

/// Handler for changes in the `Name` field.
- (void)nameDidChange:(UITextField*)textField {
  _name = textField.text;
  [self updateApplyButtonState];
}

/// Handler for changes in the `URL` field.
- (void)URLDidChange:(UITextField*)textField {
  _URL = textField.text;
  [self setErrorMessage:nil];
  [self updateApplyButtonState];
}

/// Handles the tap on the "Add" or "Save" button.
- (void)onApplyButtonTap {
  NSString* name = _name;
  if (!IsInputValid(name)) {
    name = _URL;
  }
  PinnedSiteMutationResult result;
  switch (_action) {
    case PinnedSiteAction::kCreate:
      result = [self.mutator addPinnedSiteWithTitle:name URL:_URL];
      break;
    case PinnedSiteAction::kModify:
      result = [self.mutator editPinnedSiteForURL:_originalURL
                                        withTitle:name
                                              URL:_URL];
      break;
  }
  if (result == PinnedSiteMutationResult::kSuccess) {
    RecordPinnedSiteFormUserAction(
        _action, _hasFailedOnce
                     ? MostVisitedPinSiteFormUserAction::kApplyAfterFailure
                     : MostVisitedPinSiteFormUserAction::kApplyImmediately);
    [self dismissModal];
    return;
  }
  [self setErrorMessage:GetErrorMessage(result)];
}

/// Handles the tap on the "Cancel" button or swiping down.
- (void)onCancel {
  RecordPinnedSiteFormUserAction(
      _action, _hasFailedOnce
                   ? MostVisitedPinSiteFormUserAction::kDismissAfterFailure
                   : MostVisitedPinSiteFormUserAction::kDismissImmediately);
  [self dismissModal];
}

/// Enables or disables the top-right button that applies the changes. The
/// button should only be enabled when there are valid inputs in the text
/// fields.
- (void)updateApplyButtonState {
  self.navigationItem.rightBarButtonItem.enabled =
      IsInputValid(_URL) && !_errorMessage;
}

/// Updates the footer of the table.
- (void)updateFooter {
  if (!_errorMessage) {
    self.tableView.tableFooterView = nil;
    return;
  }
  /// Sets up the label.
  UILabel* errorMessage = [[UILabel alloc] initWithFrame:CGRectZero];
  errorMessage.text = _errorMessage;
  errorMessage.textColor = [UIColor colorNamed:kRedColor];
  errorMessage.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  errorMessage.numberOfLines = 0;
  /// Sizes it correctly.
  CGRect readableContentFrame = self.tableView.readableContentGuide.layoutFrame;
  CGSize size = [errorMessage
      sizeThatFits:CGSizeMake(readableContentFrame.size.width, CGFLOAT_MAX)];
  errorMessage.frame =
      CGRectMake(readableContentFrame.origin.x, 0, size.width, size.height);
  UIView* footer = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, self.tableView.bounds.size.width,
                               size.height)];
  [footer addSubview:errorMessage];
  self.tableView.tableFooterView = footer;
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  footer);
}

/// Sets the error message.
- (void)setErrorMessage:(NSString*)message {
  _hasFailedOnce = _hasFailedOnce || message;
  if ((!_errorMessage && !message) || [_errorMessage isEqualToString:message]) {
    return;
  }
  _errorMessage = message;
  [self updateApplyButtonState];
  [self updateFooter];
}

/// Dismiss the modal.
- (void)dismissModal {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
