// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_constants.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_export_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_group_identifier.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_view_controller_items.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Identifier for the table view section that lists the credentials.
NSString* const kCredentialSectionIdentifier = @"CredentialSection";

}  // namespace

@implementation CredentialExportViewController {
  // The data source that will manage the table view.
  UITableViewDiffableDataSource<NSString*, CredentialGroupIdentifier*>*
      _dataSource;

  // The complete list of credential groups.
  std::vector<password_manager::AffiliatedGroup> _affiliatedGroups;

  // Toolbar button to toggle selecting or deselecting all credential items.
  UIBarButtonItem* _toggleAllButton;
}

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS);

  self.navigationItem.rightBarButtonItem = [self createContinueButton];
  _toggleAllButton = [self createToggleAllButton];

  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.editing = YES;
  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];

  // Add a flexible space to ensure the button remains left-aligned.
  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  self.toolbarItems = @[ _toggleAllButton, flexibleSpace ];

  [self configureDataSource];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setToolbarHidden:NO animated:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.navigationController setToolbarHidden:YES animated:animated];
}

#pragma mark - Actions

- (void)didTapDone {
  NSArray<NSIndexPath*>* selectedIndexPaths =
      self.tableView.indexPathsForSelectedRows;
  NSMutableArray<CredentialGroupIdentifier*>* selectedItems =
      [NSMutableArray arrayWithCapacity:selectedIndexPaths.count];
  for (NSIndexPath* indexPath in selectedIndexPaths) {
    CredentialGroupIdentifier* item =
        [_dataSource itemIdentifierForIndexPath:indexPath];
    CHECK(item);
    [selectedItems addObject:item];
  }

  [self.delegate userDidStartExport:selectedItems];
}

- (void)didTapToggleAllButton {
  NSUInteger selectedCount = self.tableView.indexPathsForSelectedRows.count;
  NSUInteger totalCount = _affiliatedGroups.size();

  if (selectedCount == totalCount) {
    [self deselectAllItems];
  } else {
    [self selectAllItems];
  }
}

#pragma mark - CredentialExportConsumer

- (void)setAffiliatedGroups:
    (std::vector<password_manager::AffiliatedGroup>)affiliatedGroups {
  _affiliatedGroups = std::move(affiliatedGroups);

  __weak __typeof(self) weakSelf = self;
  [self applySnapshotAnimated:YES
                   completion:^{
                     [weakSelf selectAllItems];
                   }];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewTextHeaderFooterView* header =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
          self.tableView);

  [header setSubtitle:l10n_util::GetNSString(
                          IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_SUBTITLE)];

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

#pragma mark - Private

// Sets up the diffable data source and applies the initial snapshot.
- (void)configureDataSource {
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSourceCellProvider cellProvider =
      ^UITableViewCell*(UITableView* tableView, NSIndexPath* indexPath,
                        CredentialGroupIdentifier* identifier) {
        return [weakSelf cellForTableView:tableView
                              atIndexPath:indexPath
                           itemIdentifier:identifier];
      };

  _dataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];

  [self applySnapshotAnimated:NO completion:nil];
}

// Builds and applies a new snapshot to the data source.
- (void)applySnapshotAnimated:(BOOL)animated
                   completion:(void (^)(void))completion {
  if (!_dataSource) {
    if (completion) {
      completion();
    }
    return;
  }

  NSDiffableDataSourceSnapshot<NSString*, CredentialGroupIdentifier*>*
      snapshot = [[NSDiffableDataSourceSnapshot alloc] init];

  if (!_affiliatedGroups.empty()) {
    [snapshot appendSectionsWithIdentifiers:@[ kCredentialSectionIdentifier ]];
    NSMutableArray<CredentialGroupIdentifier*>* identifiers =
        [[NSMutableArray alloc] initWithCapacity:_affiliatedGroups.size()];
    for (const password_manager::AffiliatedGroup& group : _affiliatedGroups) {
      CredentialGroupIdentifier* identifier =
          [[CredentialGroupIdentifier alloc] initWithGroup:group];
      [identifiers addObject:identifier];
    }
    [snapshot appendItemsWithIdentifiers:identifiers
               intoSectionWithIdentifier:kCredentialSectionIdentifier];
  }

  [_dataSource applySnapshot:snapshot
        animatingDifferences:animated
                  completion:completion];
}

// TODO(crbug.com/454566693): Add EGTest.
// Updates the title, "Continue" button state, and "Select All" button text
// based on the current number of selected items.
- (void)updateUIForSelection {
  CHECK_GT(_affiliatedGroups.size(), 0U);

  NSUInteger selectedCount = self.tableView.indexPathsForSelectedRows.count;
  NSUInteger totalCount = _affiliatedGroups.size();

  self.navigationItem.rightBarButtonItem.enabled = (selectedCount > 0);

  if (selectedCount == 0) {
    self.title = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS);
  } else {
    self.title = l10n_util::GetPluralNSStringF(
        IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_COUNT, selectedCount);
  }

  if (selectedCount == totalCount) {
    _toggleAllButton.title = l10n_util::GetNSString(
        IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_DESELECT_ALL_BUTTON);
  } else {
    _toggleAllButton.title = l10n_util::GetNSString(
        IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_SELECT_ALL_BUTTON);
  }
}

// Selects all rows in the table view.
- (void)selectAllItems {
  NSInteger sectionIndex = [[_dataSource snapshot]
      indexOfSectionIdentifier:kCredentialSectionIdentifier];
  if (sectionIndex == NSNotFound) {
    [self updateUIForSelection];
    return;
  }

  NSInteger count = _affiliatedGroups.size();
  for (NSInteger row = 0; row < count; row++) {
    NSIndexPath* indexPath = [NSIndexPath indexPathForRow:row
                                                inSection:sectionIndex];
    [self.tableView selectRowAtIndexPath:indexPath
                                animated:NO
                          scrollPosition:UITableViewScrollPositionNone];
  }
  [self updateUIForSelection];
}

// Deselects all rows in the table view.
- (void)deselectAllItems {
  for (NSIndexPath* indexPath in self.tableView.indexPathsForSelectedRows) {
    [self.tableView deselectRowAtIndexPath:indexPath animated:NO];
  }
  [self updateUIForSelection];
}

// Creates the button that toggles between selecting and deselecting all items.
- (UIBarButtonItem*)createToggleAllButton {
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_SELECT_ALL_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(didTapToggleAllButton)];
  button.accessibilityIdentifier =
      kCredentialExportSelectAllButtonAccessibilityIdentifier;
  return button;
}

// Creates the "Continue" button for the navigation bar.
- (UIBarButtonItem*)createContinueButton {
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_CONTINUE)
              style:UIBarButtonItemStyleProminent
             target:self
             action:@selector(didTapDone)];
  button.accessibilityIdentifier =
      kCredentialExportContinueButtonAccessibilityIdentifier;
  button.enabled = NO;
  return button;
}

// Helper to generate the specific subtitle text for each cell.
- (NSString*)detailTextForGroup:
    (const password_manager::AffiliatedGroup&)group {
  base::span<const password_manager::CredentialUIEntry> credentials =
      group.GetCredentials();

  CHECK(!credentials.empty());

  if (credentials.size() == 1) {
    const password_manager::CredentialUIEntry& credential = credentials[0];
    return base::SysUTF16ToNSString(credential.username);
  }

  return l10n_util::GetNSStringF(IDS_IOS_SETTINGS_PASSWORDS_NUMBER_ACCOUNT,
                                 base::NumberToString16(credentials.size()));
}

// Helper method to encapsulate cell configuration logic.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                         atIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(CredentialGroupIdentifier*)identifier {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView
                                                 forIndexPath:indexPath];

  const password_manager::AffiliatedGroup& group = identifier.affiliatedGroup;

  TableViewCellContentConfiguration* contentConfig =
      [[TableViewCellContentConfiguration alloc] init];
  contentConfig.title = base::SysUTF8ToNSString(group.GetDisplayName());
  contentConfig.subtitle = [self detailTextForGroup:group];
  contentConfig.titleNumberOfLines = 2;
  contentConfig.titleLineBreakMode = NSLineBreakByTruncatingTail;
  contentConfig.subtitleNumberOfLines = 1;

  cell.contentConfiguration = contentConfig;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  // TODO(crbug.com/461479302): Load favicon.

  return cell;
}

@end
