// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_password_conflict_resolution_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_password_conflict_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// The identifier for the only section in the table.
NSString* const kSafariDataImportPasswordConflictResolutionSection =
    @"SafariDataImportPasswordConflictResolutionSection";
}  // namespace

@interface SafariDataImportPasswordConflictResolutionViewController () <
    UITableViewDelegate>

@end

@implementation SafariDataImportPasswordConflictResolutionViewController {
  /// List of password conflicts.
  NSArray<PasswordImportItem*>* _passwordConflicts;
  /// List of NSNumber representation of boolean values indicating the password
  /// at the respective index should be unmasked for display.
  NSMutableArray<NSNumber*>* _shouldUnmaskPasswordAtIndex;
  /// The data source painting each cell in the table from `_passwordConflicts`.
  UITableViewDiffableDataSource<NSString*, NSNumber*>* _dataSource;
  /// The "select" and "deselect" buttons.
  UIBarButtonItem* _selectButton;
  UIBarButtonItem* _deselectButton;
  /// Module for reauthentication when user wants to see unmasked passwords.
  ReauthenticationModule* _reauthModule;
}

#pragma mark - ChromeTableViewController

- (instancetype)initWithPasswordConflicts:
    (NSArray<PasswordImportItem*>*)passwords {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _passwordConflicts = passwords;
    _shouldUnmaskPasswordAtIndex = [NSMutableArray array];
    NSUInteger count = _passwordConflicts.count;
    for (NSUInteger i = 0; i < count; i++) {
      [_shouldUnmaskPasswordAtIndex addObject:@NO];
    }
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_TITLE);
  [self setupBarButtons];
  /// Sets up table view properties.
  self.tableView.accessibilityIdentifier =
      GetPasswordConflictResolutionTableViewAccessibilityIdentifier();
  self.tableView.delegate = self;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.editing = YES;
  /// Register cells.
  RegisterTableViewCell<TableViewURLCell>(self.tableView);
  RegisterTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
      self.tableView);
  /// Initialize table.
  [self initializeDataSourceAndTable];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewAttributedStringHeaderFooterView* header =
      DequeueTableViewHeaderFooter<TableViewAttributedStringHeaderFooterView>(
          tableView);
  NSDictionary* attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  NSMutableAttributedString* attributedText = [[NSMutableAttributedString alloc]
      initWithString:
          l10n_util::GetNSString(
              IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_DISCLAIMER)
          attributes:attributes];
  [header setAttributedString:attributedText];
  return header;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self selectedItemsCount] == _passwordConflicts.count) {
    [self updateSelectionButton];
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self selectedItemsCount] == _passwordConflicts.count - 1) {
    [self updateSelectionButton];
  }
}

#pragma mark - Button selectors

- (void)didTapCancelButton {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)didTapContinueButton {
  NSMutableArray<NSNumber*>* passwordIdentifiers = [NSMutableArray array];
  for (NSIndexPath* indexPath in [self.tableView indexPathsForSelectedRows]) {
    [passwordIdentifiers
        addObject:[_dataSource itemIdentifierForIndexPath:indexPath]];
  }
  [self.mutator continueToImportPasswords:passwordIdentifiers];
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)didTapSelectionButton {
  NSUInteger totalCount = _passwordConflicts.count;
  BOOL deselect = totalCount == [self selectedItemsCount];
  for (NSUInteger idx = 0; idx < totalCount; idx++) {
    NSIndexPath* indexPath = [_dataSource indexPathForItemIdentifier:@(idx)];
    if (deselect) {
      [self.tableView deselectRowAtIndexPath:indexPath animated:NO];
    } else {
      [self.tableView selectRowAtIndexPath:indexPath
                                  animated:NO
                            scrollPosition:UITableViewScrollPositionNone];
    }
  }
  [self updateSelectionButton];
}

#pragma mark - Private

/// Lazy-loader of the reauthentication module.
- (ReauthenticationModule*)reauthenticationModule {
  if (!_reauthModule) {
    _reauthModule = [[ReauthenticationModule alloc] init];
  }
  return _reauthModule;
}

/// Helper method that returns the number of selected items.
- (NSUInteger)selectedItemsCount {
  NSArray<NSIndexPath*>* selectedRows =
      [self.tableView indexPathsForSelectedRows];
  return selectedRows ? selectedRows.count : 0;
}

/// Returns the cell with the properties of the `item` displayed.
- (TableViewURLCell*)cellForIndexPath:(NSIndexPath*)indexPath
                       itemIdentifier:(NSNumber*)identifier {
  TableViewURLCell* cell =
      DequeueTableViewCell<TableViewURLCell>(self.tableView);
  cell.accessibilityIdentifier =
      GetPasswordConflictResolutionTableViewCellAccessibilityIdentifier(
          indexPath.item);
  /// Populate cell with information.
  PasswordImportItem* item = _passwordConflicts[identifier.intValue];
  cell.titleLabel.text = item.url;
  cell.URLLabel.text = item.username;
  if (item.faviconAttributes) {
    [cell.faviconView configureWithAttributes:item.faviconAttributes];
  } else {
    __weak __typeof(self) weakSelf = self;
    [item loadFaviconWithCompletionHandler:^{
      [weakSelf updateItemWithIdentifier:identifier];
    }];
  }
  BOOL shouldUnmaskPassword =
      _shouldUnmaskPasswordAtIndex[identifier.intValue].boolValue;
  cell.thirdRowLabel.numberOfLines = 1;
  cell.thirdRowLabel.text =
      shouldUnmaskPassword ? item.password : kMaskedPassword;
  cell.thirdRowLabel.lineBreakMode = shouldUnmaskPassword
                                         ? NSLineBreakByClipping
                                         : NSLineBreakByTruncatingTail;
  cell.editingAccessoryView = [self accessoryViewForItemIdentifier:identifier];
  [cell configureUILayout];
  UIView* selectedBackgroundView = [[UIView alloc] init];
  selectedBackgroundView.backgroundColor = [UIColor clearColor];
  cell.selectedBackgroundView = selectedBackgroundView;
  return cell;
}

/// Helper method to update the cell with `identifier`.
- (void)updateItemWithIdentifier:(NSNumber*)identifier {
  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ identifier ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Helper method to create a `UIBarButtonItem` with action handler `[self
/// selector]`.
- (UIBarButtonItem*)newButtonWithTitle:(NSString*)title action:(SEL)selector {
  return [[UIBarButtonItem alloc] initWithTitle:title
                                          style:UIBarButtonItemStylePlain
                                         target:self
                                         action:selector];
}

/// Sets up buttons in the navigation bar and toolbar.
- (void)setupBarButtons {
  /// Navigation bar: cancel button.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancelButton)];
  /// Navigation bar: continue button.
  NSString* continueButtonTitle = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_CONTINUE);
  UIBarButtonItem* continueButton =
      [self newButtonWithTitle:continueButtonTitle
                        action:@selector(didTapContinueButton)];
  NSDictionary<NSString*, UIFont*>* continueButtonAttributes = @{
    NSFontAttributeName : [UIFont systemFontOfSize:UIFont.buttonFontSize
                                            weight:UIFontWeightSemibold]
  };
  [continueButton setTitleTextAttributes:continueButtonAttributes
                                forState:UIControlStateNormal];
  self.navigationItem.rightBarButtonItem = continueButton;
  /// Toolbar select buttons.
  NSString* selectButtonTitle = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_SELECT_ALL);
  NSString* deselectButtonTitle = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_DESELECT_ALL);
  _selectButton = [self newButtonWithTitle:selectButtonTitle
                                    action:@selector(didTapSelectionButton)];
  _deselectButton = [self newButtonWithTitle:deselectButtonTitle
                                      action:@selector(didTapSelectionButton)];
  [self updateSelectionButton];
}

/// Sets `_dataSource` and fills the table with data from `_passwordConflicts`.
- (void)initializeDataSourceAndTable {
  /// Set up data source.
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
  [snapshot appendSectionsWithIdentifiers:@[
    kSafariDataImportPasswordConflictResolutionSection
  ]];
  NSMutableArray* indicesForPasswordConflicts = [NSMutableArray array];
  for (NSUInteger i = 0; i < _passwordConflicts.count; i++) {
    [indicesForPasswordConflicts addObject:@(i)];
  }
  [snapshot appendItemsWithIdentifiers:indicesForPasswordConflicts
             intoSectionWithIdentifier:
                 kSafariDataImportPasswordConflictResolutionSection];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Shows or updates the bottom button's text and behavior. Should show
/// "Deselect all" when all items are selected, and "Select all" otherwise.
- (void)updateSelectionButton {
  UIBarButtonItem* selectionButton =
      [self selectedItemsCount] == _passwordConflicts.count ? _deselectButton
                                                            : _selectButton;
  self.toolbarItems = @[ selectionButton ];
}

/// Helper method to set up the accessory view.
- (UIView*)accessoryViewForItemIdentifier:(NSNumber*)identifier {
  if (![[self reauthenticationModule] canAttemptReauth]) {
    return nil;
  }
  BOOL forUnmaskAction =
      !(_shouldUnmaskPasswordAtIndex[identifier.intValue].boolValue);
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  NSString* symbol_name =
      forUnmaskAction ? kShowActionSymbol : kHideActionSymbol;
  UIImage* buttonImage = DefaultSymbolWithConfiguration(  // Store the image
      symbol_name, [UIImageSymbolConfiguration
                       configurationWithWeight:UIImageSymbolWeightMedium]);
  configuration.image = buttonImage;
  configuration.contentInsets = NSDirectionalEdgeInsetsZero;
  __weak __typeof(self) weakSelf = self;
  UIAction* updatePasswordField =
      [UIAction actionWithHandler:^(UIAction* action) {
        [weakSelf maybeUpdatePasswordMasking:forUnmaskAction
                       forItemWithIdentifier:identifier
                               authenticated:NO];
      }];
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:updatePasswordField];
  /// Make sure the button is positioned correctly; not adding the following
  /// line triggers a bug where the accessory view appears on the top-leading
  /// edge of the cell.
  button.frame =
      CGRectMake(0, 0, buttonImage.size.width, buttonImage.size.height);
  return button;
}

/// Reveal password if `shouldUnmask` is YES and user is authenticated to view
/// passwords; mask password if otherwise.
- (void)maybeUpdatePasswordMasking:(BOOL)shouldUnmask
             forItemWithIdentifier:(NSNumber*)identifier
                     authenticated:(BOOL)authenticated {
  if (!shouldUnmask || authenticated ||
      ![[self reauthenticationModule] canAttemptReauth]) {
    _shouldUnmaskPasswordAtIndex[identifier.intValue] = @(shouldUnmask);
    [self updateItemWithIdentifier:identifier];
    return;
  }
  /// Show reauthentication.
  __weak __typeof(self) weakSelf = self;
  auto handler = ^(ReauthenticationResult result) {
    if (result == ReauthenticationResult::kFailure) {
      return;
    }
    [weakSelf maybeUpdatePasswordMasking:shouldUnmask
                   forItemWithIdentifier:identifier
                           authenticated:YES];
  };
  NSString* reason = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_UNMASK_REAUTH_REASON);
  [[self reauthenticationModule] attemptReauthWithLocalizedReason:reason
                                             canReusePreviousAuth:YES
                                                          handler:handler];
}

@end
