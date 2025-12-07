// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/save_to_drive/ui_bundled/file_destination_picker_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/save_to_drive/ui_bundled/file_destination_picker_action_delegate.h"
#import "ios/chrome/browser/save_to_drive/ui_bundled/file_destination_picker_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
// Names of icons used for items in the file destination picker.
NSString* const kFilesAppWithBackgroundImage =
    @"apple_files_app_with_background";
NSString* const kDriveAppWithBackgroundImage =
    @"google_drive_app_with_background";
#endif

// Bottom inset to compensate for empty space at the bottom of the table view.
constexpr CGFloat kContentInsetBottom = -16.;

// The inset for the table view separator
// = kTableViewHorizontalSpacing + <image size = 30> +
// kTableViewSubViewHorizontalSpacing
constexpr CGFloat kSeparatorInset = 58.;

// Identifier to table view sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDestination,
};

// Identifiers for table view items.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierFiles,
  ItemIdentifierDrive,
  ItemIdentifierFilesSelected,
  ItemIdentifierDriveSelected,
};

// Returns selection state (selected or not) associated with `identifier`.
bool ItemIsSelected(ItemIdentifier identifier) {
  switch (identifier) {
    case ItemIdentifierFiles:
    case ItemIdentifierDrive:
      return false;
    case ItemIdentifierFilesSelected:
    case ItemIdentifierDriveSelected:
      return true;
  }
}

// Returns the `FileDestination` associated with `identifier`.
FileDestination ItemFileDestination(ItemIdentifier identifier) {
  switch (identifier) {
    case ItemIdentifierFiles:
    case ItemIdentifierFilesSelected:
      return FileDestination::kFiles;
    case ItemIdentifierDrive:
    case ItemIdentifierDriveSelected:
      return FileDestination::kDrive;
  }
}

using FileDestinationPickerDataSource =
    UITableViewDiffableDataSource<NSNumber*, NSNumber*>;
using FileDestinationPickerDataSourceSnapshot =
    NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>;

// Sets the background color of `cell` for the highlighted state.
void SetHighlightedBackgroundColorForCell(UITableViewCell* cell) {
  cell.backgroundColor = [UIColor colorNamed:kGrey300Color];
}

// Sets the background color of `cell` for the unhighlighted state.
void SetUnhighlightedBackgroundColorForCell(UITableViewCell* cell) {
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
}

}  // namespace

@interface FileDestinationPickerViewController () <UITableViewDelegate>

@end

@implementation FileDestinationPickerViewController {
  FileDestinationPickerDataSource* _dataSource;
  FileDestination _selectedDestination;
  NSLayoutConstraint* _tableViewHeightConstraint;
}

#pragma mark - UITableViewController

- (instancetype)init {
  return [super initWithStyle:UITableViewStyleInsetGrouped];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // Set up table view.
  self.tableView.scrollEnabled = NO;
  self.tableView.alwaysBounceVertical = NO;
  self.tableView.backgroundColor = UIColor.clearColor;
  self.tableView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
  self.tableView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  self.tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  self.tableView.showsVerticalScrollIndicator = NO;
  self.tableView.showsHorizontalScrollIndicator = NO;
  [self.tableView setSeparatorInset:UIEdgeInsetsMake(0, kSeparatorInset, 0, 0)];
  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];
  // Constrain table view height to its content height.
  _tableViewHeightConstraint = [self.tableView.heightAnchor
      constraintEqualToConstant:self.tableView.contentSize.height +
                                kContentInsetBottom];
  _tableViewHeightConstraint.active = YES;
  // Set up data source.
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[FileDestinationPickerDataSource alloc]
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
  [self updateDataSource];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  _tableViewHeightConstraint.constant =
      self.tableView.contentSize.height + kContentInsetBottom;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  const ItemIdentifier selectedIdentifier = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);
  const FileDestination destination = ItemFileDestination(selectedIdentifier);
  [self.actionDelegate fileDestinationPicker:self
                        didSelectDestination:destination];
}

- (void)tableView:(UITableView*)tableView
    didHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  SetHighlightedBackgroundColorForCell(cell);
}

- (void)tableView:(UITableView*)tableView
    didUnhighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  SetUnhighlightedBackgroundColorForCell(cell);
}

#pragma mark - FileDestinationPickerConsumer

- (void)setSelectedDestination:(FileDestination)destination {
  _selectedDestination = destination;
  [self updateDataSource];
}

#pragma mark - Private

// Returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  const bool selected = ItemIsSelected(itemIdentifier);
  FileDestination destination = ItemFileDestination(itemIdentifier);
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  ColorfulSymbolContentConfiguration* imageConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  imageConfiguration.symbolImage =
      destination == FileDestination::kFiles
          ? [UIImage imageNamed:kFilesAppWithBackgroundImage]
          : [UIImage imageNamed:kDriveAppWithBackgroundImage];
  configuration.leadingConfiguration = imageConfiguration;
#endif
  configuration.title =
      destination == FileDestination::kFiles
          ? l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_TO_FILES)
          : l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_TO_DRIVE);

  // Checks if download should be restricted.
  if ([self.actionDelegate shouldBlockDownloadToFile] &&
      destination == FileDestination::kFiles) {
    cell.userInteractionEnabled = NO;
    configuration.textDisabled = YES;
    configuration.subtitle =
        l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION);
  }

  cell.accessoryType = selected ? UITableViewCellAccessoryCheckmark
                                : UITableViewCellAccessoryNone;
  SetUnhighlightedBackgroundColorForCell(cell);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.accessibilityIdentifier =
      destination == FileDestination::kFiles
          ? [self.actionDelegate shouldBlockDownloadToFile]
                ? kFileDestinationPickerDownloadRestrictionFilesAccessibilityIdentifier
                : kFileDestinationPickerFilesAccessibilityIdentifier
          : kFileDestinationPickerDriveAccessibilityIdentifier;
  cell.contentConfiguration = configuration;
  return cell;
}

// Updates data source as a function of `_selectedDestination`.
- (void)updateDataSource {
  FileDestinationPickerDataSourceSnapshot* snapshot =
      [[FileDestinationPickerDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierDestination) ]];
  ItemIdentifier fileItem = _selectedDestination == FileDestination::kFiles
                                ? ItemIdentifierFilesSelected
                                : ItemIdentifierFiles;
  ItemIdentifier driveItem = _selectedDestination == FileDestination::kDrive
                                 ? ItemIdentifierDriveSelected
                                 : ItemIdentifierDrive;
  [snapshot appendItemsWithIdentifiers:@[ @(fileItem), @(driveItem) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
