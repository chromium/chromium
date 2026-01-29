// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/pinned_site_form_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/metrics.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/pinned_site_action.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_pinned_site_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
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

}  // namespace

@interface PinnedSiteFormViewController () <UITableViewDelegate>

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
  /// URL validation result.
  BOOL _urlValidationFailed;
}

- (instancetype)initWithAction:(PinnedSiteAction)action
                       forItem:(MostVisitedItem*)item {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _action = action;
    if (_action == PinnedSiteAction::kModify) {
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
      doneButtonTextId =
          IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ADD_PINNED_SITE_APPLY_BUTTON;
      break;
    case PinnedSiteAction::kModify:
      titleId = IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_EDIT_PINNED_SITE_TITLE;
      doneButtonTextId =
          IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_EDIT_PINNED_SITE_APPLY_BUTTON;
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
  self.tableView.delegate = self;
  RegisterTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
      self.tableView);
  [self loadModel];
  [self updateApplyButtonState];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordPinnedSiteFormUserAction(
      _action, _urlValidationFailed
                   ? MostVisitedPinSiteFormUserAction::kDismissAfterFailure
                   : MostVisitedPinSiteFormUserAction::kDismissImmediately);
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  NSMutableAttributedString* attributedString;
  if (_urlValidationFailed) {
    NSDictionary* attributes = @{
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
      NSForegroundColorAttributeName : [UIColor colorNamed:kRedColor]
    };
    attributedString = [[NSMutableAttributedString alloc]
        initWithString:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL_VALIDATION_FAILED)
            attributes:attributes];
  }
  if (_action == PinnedSiteAction::kModify) {
    NSDictionary* attributes = @{
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
      NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
    };
    NSMutableAttributedString* disclaimer = [[NSMutableAttributedString alloc]
        initWithString:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_FOOTER)
            attributes:attributes];
    if (attributedString) {
      [attributedString appendAttributedString:[[NSAttributedString alloc]
                                                   initWithString:@"\n"]];
      [attributedString appendAttributedString:disclaimer];
    } else if (disclaimer) {
      attributedString = disclaimer;
    }
  }
  if (!attributedString) {
    return nil;
  }
  TableViewAttributedStringHeaderFooterView* footer =
      DequeueTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
          tableView);
  [footer setAttributedString:attributedString];
  return footer;
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
  switch (static_cast<ItemIdentifier>(identifier.integerValue)) {
    case ItemTypeName:
      cell.textLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_NAME);
      cell.textField.text = _name;
      [cell.textField addTarget:self
                         action:@selector(nameDidChange:)
               forControlEvents:UIControlEventEditingChanged];
      break;
    case ItemTypeURL:
      cell.textLabel.text =
          l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL);
      cell.textField.text = _URL;
      [cell.textField addTarget:self
                         action:@selector(URLDidChange:)
               forControlEvents:UIControlEventEditingChanged];
      break;
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
  [self updateApplyButtonState];
}

/// Handles the tap on the "Add" or "Save" button.
- (void)onApplyButtonTap {
  BOOL success;
  switch (_action) {
    case PinnedSiteAction::kCreate:
      success = [self.mutator addPinnedSiteWithTitle:_name URL:_URL];
      break;
    case PinnedSiteAction::kModify:
      success = [self.mutator editPinnedSiteForURL:_originalURL
                                         withTitle:_name
                                               URL:_URL];
      break;
  }
  if (success) {
    RecordPinnedSiteFormUserAction(
        _action, _urlValidationFailed
                     ? MostVisitedPinSiteFormUserAction::kApplyAfterFailure
                     : MostVisitedPinSiteFormUserAction::kApplyImmediately);
    [self dismissModal];
  } else {
    _urlValidationFailed = YES;
    NSDiffableDataSourceSnapshot* snapshot = [_dataSource snapshot];
    [snapshot reloadSectionsWithIdentifiers:@[ @(kSection) ]];
    [_dataSource applySnapshot:snapshot animatingDifferences:NO];
  }
}

/// Handles the tap on the "Cancel" button or swiping down.
- (void)onCancel {
  RecordPinnedSiteFormUserAction(
      _action, _urlValidationFailed
                   ? MostVisitedPinSiteFormUserAction::kDismissAfterFailure
                   : MostVisitedPinSiteFormUserAction::kDismissImmediately);
  [self dismissModal];
}

/// Enables or disables the top-right button that applies the changes. The
/// button should only be enabled when there are valid inputs in the text
/// fields.
- (void)updateApplyButtonState {
  self.navigationItem.rightBarButtonItem.enabled =
      IsInputValid(_name) && IsInputValid(_URL);
}

/// Dismiss the modal.
- (void)dismissModal {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
