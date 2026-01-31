// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_resolution_view_controller.h"

#import "base/check_op.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/data_import/public/accessibility_utils.h"
#import "ios/chrome/browser/data_import/public/credential_item_identifier.h"
#import "ios/chrome/browser/data_import/public/metrics.h"
#import "ios/chrome/browser/data_import/public/passkey_import_item.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/data_import/ui/credential_import_item_cell_content_configuration.h"
#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_mutator.h"
#import "ios/chrome/browser/data_import/ui/data_import_credential_conflict_resolution_view_controller_delegate.h"
#import "ios/chrome/browser/data_import/ui/data_import_import_stage_transition_handler.h"
#import "ios/chrome/browser/data_import/ui/ui_utils.h"
#import "ios/chrome/browser/passwords/coordinator/password_utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// The identifier for the only section in the table.
NSString* const kDataImportCredentialConflictResolutionSection =
    @"DataImportCredentialConflictResolutionSection";
}  // namespace

@interface DataImportCredentialConflictResolutionViewController () <
    UITableViewDelegate>

@end

@implementation DataImportCredentialConflictResolutionViewController {
  /// List of password conflicts.
  NSArray<PasswordImportItem*>* _passwordConflicts;
  /// List of passkey conflicts.
  NSArray<PasskeyImportItem*>* _passkeyConflicts;
  /// List of NSNumber representation of boolean values indicating the password
  /// at the respective index should be unmasked for display.
  NSMutableArray<NSNumber*>* _shouldUnmaskPasswordAtIndex;
  /// The data source painting each cell in the table from `_passwordConflicts`.
  UITableViewDiffableDataSource<NSString*, CredentialItemIdentifier*>*
      _dataSource;
  /// The "select" and "deselect" buttons.
  UIBarButtonItem* _selectButton;
  UIBarButtonItem* _deselectButton;
}

#pragma mark - ChromeTableViewController

- (instancetype)
    initWithPasswordConflicts:(NSArray<PasswordImportItem*>*)passwords
             passkeyConflicts:(NSArray<PasskeyImportItem*>*)passkeys {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _passwordConflicts = passwords;
    _passkeyConflicts = passkeys;
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
      IDS_IOS_CREDENTIAL_IMPORT_CONFLICT_RESOLUTION_TITLE);
  [self setupBarButtons];
  /// Sets up table view properties.
  self.tableView.separatorInset =
      GetDataImportSeparatorInset(/*multiSelectionMode=*/YES);
  self.tableView.accessibilityIdentifier =
      GetCredentialConflictResolutionTableViewAccessibilityIdentifier();
  self.tableView.delegate = self;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.editing = YES;
  /// Register cells.
  RegisterTableViewCell<UITableViewCell>(self.tableView);
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
  int messageId =
      _passkeyConflicts.count > 0u
          ? IDS_IOS_IMPORT_PASSWORD_PASSKEY_CONFLICT_RESOLUTION_DISCLAIMER
          : IDS_IOS_IMPORT_PASSWORD_CONFLICT_RESOLUTION_DISCLAIMER;
  NSMutableAttributedString* attributedText = [[NSMutableAttributedString alloc]
      initWithString:l10n_util::GetNSString(messageId)
          attributes:attributes];
  [header setAttributedString:attributedText];
  return header;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self updateUIForSelection];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self updateUIForSelection];
}

#pragma mark - Button selectors

- (void)didTapCancelButton {
  RecordDataImportDismissCredentialConflictScreen(
      DataImportCredentialConflictScreenAction::kCancel);
  [self.delegate cancelledConflictResolution];
}

- (void)didTapContinueButton {
  RecordDataImportDismissCredentialConflictScreen(
      DataImportCredentialConflictScreenAction::kContinue);
  NSMutableArray<NSNumber*>* passwordIdentifiers = [NSMutableArray array];
  NSMutableArray<NSNumber*>* passkeyIdentifiers = [NSMutableArray array];
  for (NSIndexPath* indexPath in [self.tableView indexPathsForSelectedRows]) {
    CredentialItemIdentifier* identifier =
        [_dataSource itemIdentifierForIndexPath:indexPath];
    if (identifier.type == CredentialType::kPassword) {
      [passwordIdentifiers addObject:@(identifier.index)];
    } else {
      [passkeyIdentifiers addObject:@(identifier.index)];
    }
  }
  [self.mutator continueToImportPasswords:passwordIdentifiers
                                 passkeys:passkeyIdentifiers];
  [self.delegate resolvedCredentialConflicts];
}

// If all rows in the table view are currently selected, deselects all.
// Otherwise, selects all rows in the table view. Updates the toolbar button
// based on the action that was taken ("select all" when all items where
// deselected and "deselect all" otherwise).
- (void)didTapSelectionButton {
  BOOL deselect = [self allItemsCount] == [self selectedItemsCount];
  NSArray<CredentialItemIdentifier*>* identifiers = [[_dataSource snapshot]
      itemIdentifiersInSectionWithIdentifier:
          kDataImportCredentialConflictResolutionSection];
  for (CredentialItemIdentifier* identifier in identifiers) {
    NSIndexPath* indexPath =
        [_dataSource indexPathForItemIdentifier:identifier];
    if (deselect) {
      [self.tableView deselectRowAtIndexPath:indexPath animated:NO];
    } else {
      [self.tableView selectRowAtIndexPath:indexPath
                                  animated:NO
                            scrollPosition:UITableViewScrollPositionNone];
    }
  }
  RecordDataImportDismissCredentialConflictScreen(
      deselect ? DataImportCredentialConflictScreenAction::kDeselectAll
               : DataImportCredentialConflictScreenAction::kSelectAll);
  [self updateUIForSelection];
}

#pragma mark - Private

/// Lazy-loader of the reauthentication module.
- (ReauthenticationModule*)reauthModule {
  if (!_reauthModule) {
    _reauthModule = password_manager::BuildReauthenticationModule();
  }
  return _reauthModule;
}

/// Helper method that returns the number of selected items.
- (NSUInteger)selectedItemsCount {
  NSArray<NSIndexPath*>* selectedRows =
      [self.tableView indexPathsForSelectedRows];
  return selectedRows ? selectedRows.count : 0;
}

/// Returns the count of all items displayed in the table.
- (NSUInteger)allItemsCount {
  return _passwordConflicts.count + _passkeyConflicts.count;
}

/// Returns the cell with the properties of the `item` displayed.
- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(CredentialItemIdentifier*)identifier {
  UITableViewCell* cell = DequeueTableViewCell<UITableViewCell>(self.tableView);
  CredentialImportItemCellContentConfiguration* config;
  CredentialImportItem* item;

  /// Populate cell with information.
  if (identifier.type == CredentialType::kPasskey) {
    PasskeyImportItem* passkeyItem = _passkeyConflicts[identifier.index];
    config = [CredentialImportItemCellContentConfiguration
        cellConfigurationForPasskey:passkeyItem];
    item = passkeyItem;
  } else {
    PasswordImportItem* passwordItem = _passwordConflicts[identifier.index];
    if (_shouldUnmaskPasswordAtIndex[identifier.index].boolValue) {
      config = [CredentialImportItemCellContentConfiguration
          cellConfigurationForUnmaskPassword:passwordItem];
    } else {
      config = [CredentialImportItemCellContentConfiguration
          cellConfigurationForMaskPassword:passwordItem];
    }
    item = passwordItem;
  }

  cell.accessibilityIdentifier =
      GetCredentialConflictResolutionTableViewCellAccessibilityIdentifier(
          identifier);
  if (item.faviconAttributes) {
    config.faviconAttributes = item.faviconAttributes;
  } else {
    __weak __typeof(self) weakSelf = self;
    [item loadFaviconWithUIUpdateHandler:^{
      [weakSelf updateItemWithIdentifier:identifier];
    }];
  }
  cell.contentConfiguration = config;
  cell.editingAccessoryView = [self accessoryViewForItemIdentifier:identifier];
  UIView* selectedBackgroundView = [[UIView alloc] init];
  selectedBackgroundView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
  cell.selectedBackgroundView = selectedBackgroundView;
  return cell;
}

/// Helper method to update the cell with `identifier`.
- (void)updateItemWithIdentifier:(CredentialItemIdentifier*)identifier {
  NSDiffableDataSourceSnapshot<NSString*, CredentialItemIdentifier*>* snapshot =
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
  if (@available(iOS 26, *)) {
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(didTapContinueButton)];
  } else {
    NSString* continueButtonTitle = l10n_util::GetNSString(IDS_CONTINUE);
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
  }
  /// Toolbar select buttons.
  NSString* selectButtonTitle = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_SELECT_ALL);
  NSString* deselectButtonTitle = l10n_util::GetNSString(
      IDS_IOS_SAFARI_IMPORT_PASSWORD_CONFLICT_RESOLUTION_BUTTON_DESELECT_ALL);
  _selectButton = [self newButtonWithTitle:selectButtonTitle
                                    action:@selector(didTapSelectionButton)];
  _deselectButton = [self newButtonWithTitle:deselectButtonTitle
                                      action:@selector(didTapSelectionButton)];
  [self updateUIForSelection];
}

/// Sets `_dataSource` and fills the table with data from `_passwordConflicts`
/// and `_passkeyConflicts`.
- (void)initializeDataSourceAndTable {
  /// Set up data source.
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSourceCellProvider cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath,
      CredentialItemIdentifier* itemIdentifier) {
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
    kDataImportCredentialConflictResolutionSection
  ]];

  NSMutableArray<CredentialItemIdentifier*>* itemIdentifiers =
      [NSMutableArray array];
  for (NSUInteger i = 0; i < _passwordConflicts.count; i++) {
    [itemIdentifiers addObject:[[CredentialItemIdentifier alloc]
                                   initWithType:CredentialType::kPassword
                                          index:i]];
  }
  for (NSUInteger i = 0; i < _passkeyConflicts.count; i++) {
    [itemIdentifiers addObject:[[CredentialItemIdentifier alloc]
                                   initWithType:CredentialType::kPasskey
                                          index:i]];
  }

  [snapshot appendItemsWithIdentifiers:itemIdentifiers
             intoSectionWithIdentifier:
                 kDataImportCredentialConflictResolutionSection];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Updates the title to reflect the count of items selected or a generic title
/// if no items are selected. Updates the bottom button to display "Deselect
/// all" when all items are selected, and "Select all" otherwise.
- (void)updateUIForSelection {
  NSUInteger selectedItemsCount = [self selectedItemsCount];
  if (selectedItemsCount == 0) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_IMPORT_CONFLICT_RESOLUTION_TITLE);
  } else {
    self.title = l10n_util::GetPluralNSStringF(
        IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_COUNT, selectedItemsCount);
  }

  UIBarButtonItem* selectionButton = selectedItemsCount == [self allItemsCount]
                                         ? _deselectButton
                                         : _selectButton;
  self.toolbarItems = @[ selectionButton ];
}

/// Helper method to set up the accessory view.
- (UIView*)accessoryViewForItemIdentifier:
    (CredentialItemIdentifier*)identifier {
  if (identifier.type == CredentialType::kPasskey ||
      ![self.reauthModule canAttemptReauth]) {
    return nil;
  }
  BOOL forUnmaskAction =
      !(_shouldUnmaskPasswordAtIndex[identifier.index].boolValue);
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
  /// The following line calculates the correct size so the icon isn't cut off,
  /// and ensures the button appears in the correct place within the cell.
  [button sizeToFit];
  return button;
}

/// Reveal password if `shouldUnmask` is YES and user is authenticated to view
/// passwords; mask password if otherwise.
- (void)maybeUpdatePasswordMasking:(BOOL)shouldUnmask
             forItemWithIdentifier:(CredentialItemIdentifier*)identifier
                     authenticated:(BOOL)authenticated {
  if (identifier.type == CredentialType::kPasskey) {
    return;
  }

  if (!shouldUnmask || authenticated || ![self.reauthModule canAttemptReauth]) {
    _shouldUnmaskPasswordAtIndex[identifier.index] = @(shouldUnmask);
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
  [self.reauthModule attemptReauthWithLocalizedReason:reason
                                 canReusePreviousAuth:YES
                                              handler:handler];
}

@end
