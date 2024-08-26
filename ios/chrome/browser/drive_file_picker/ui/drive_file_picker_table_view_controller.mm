// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat kCellIconCornerRadius = 10;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDriveMainFolders = kSectionIdentifierEnumZero,
};

}  // namespace

@implementation DriveFilePickerTableViewController {
  // The status of file dowload.
  DriveFileDownloadStatus _status;

  // The sorting order.
  DriveItemsSortingOrder _sortingOrder;

  // The sorting type.
  DriveItemsSortingType _sortingType;

  // The Sorting actions
  UIAction* _sortByNameAction;
  UIAction* _sortByModificationTimeAction;
  UIAction* _sortByOpeningTimeAction;

  // The sort button.
  UIBarButtonItem* _sortButton;

  // The selected email from the accounts signed in the device.
  NSString* _selectedEmail;

  // Account chooser button.
  UIBarButtonItem* _accountButton;

  // The currently represented folder.
  NSString* _driveFolderTitle;

  UITableViewDiffableDataSource<NSString*, DriveItemIdentifier*>*
      _diffableDataSource;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _status = DriveFileDownloadStatus::kNotStarted;
    _sortingOrder = DriveItemsSortingOrder::kDescending;
    _sortingType = DriveItemsSortingType::kModificationTime;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self configureSortButton];
  [self configureToolbar];

  self.navigationItem.rightBarButtonItem = [self configureRightBarButtonItem];

  // Add the search bar.
  self.navigationItem.searchController = [[UISearchController alloc] init];
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.navigationItem.preferredSearchBarPlacement =
      UINavigationItemSearchBarPlacementStacked;

  // Initialize the table view.
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  self.navigationController.toolbarHidden = NO;

  __weak __typeof(self) weakSelf = self;
  auto cellProvider = ^UITableViewCell*(UITableView* tableView,
                                        NSIndexPath* indexPath,
                                        DriveItemIdentifier* itemIdentifier) {
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:itemIdentifier];
  };
  _diffableDataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];

  self.tableView.dataSource = _diffableDataSource;

  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);

  [self.mutator fetchDriveItemsForFolderID];
}

#pragma mark - DriveFilePickerConsumer

- (void)setSelectedUserIdentityEmail:(NSString*)selectedUserIdentityEmail {
  _selectedEmail = selectedUserIdentityEmail;
}

- (void)setCurrentDriveFolderTitle:(NSString*)currentDriveFolderTitle {
  _driveFolderTitle = currentDriveFolderTitle;
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.text = _driveFolderTitle;
  titleLabel.textAlignment = NSTextAlignmentLeft;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.1;
  self.navigationItem.titleView = titleLabel;
}

#pragma mark - UI actions

- (void)confirmSelection {
  // TODO(crbug.com/344812396): Submit the file selection.
}

#pragma mark - Private

// Configures the toolbar with 3 buttons, filterButton <---->
// AccountButton(where the title is the user's email) <----> sortButton(which
// should not be enabled for the root of the navigation controller)
- (void)configureToolbar {
  UIImage* filterIcon = DefaultSymbolTemplateWithPointSize(
      kFilterSymbol, kSymbolAccessoryPointSize);

  // TODO(crbug.com/344812548): Add the action of the filter button.
  UIBarButtonItem* filterButton =
      [[UIBarButtonItem alloc] initWithImage:filterIcon
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:nil];
  filterButton.enabled = YES;

  UIImage* sortIcon = DefaultSymbolTemplateWithPointSize(
      kSortSymbol, kSymbolAccessoryPointSize);

  // TODO(crbug.com/344812548): Add the action of the sort button.
  _sortButton = [[UIBarButtonItem alloc]
      initWithImage:sortIcon
               menu:[UIMenu menuWithChildren:@[
                 _sortByNameAction, _sortByOpeningTimeAction,
                 _sortByModificationTimeAction
               ]]];
  _sortButton.enabled =
      self != self.navigationController.viewControllers.firstObject;

  // TODO(crbug.com/344812548): Add the menu of the account button
  // based of the current identity.
  _accountButton = [[UIBarButtonItem alloc] initWithTitle:_selectedEmail
                                                     menu:nil];

  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[
    filterButton, spaceButton, _accountButton, spaceButton, _sortButton
  ]
               animated:NO];
}

// Returns the right bar button based on the status.
- (UIBarButtonItem*)configureRightBarButtonItem {
  switch (_status) {
    case DriveFileDownloadStatus::kInProgress:
      return [self activityIndicatorButtonItem];
    case DriveFileDownloadStatus::kSuccess:
      return [self confirmButtonItem];
    case DriveFileDownloadStatus::kInterrupted:
    case DriveFileDownloadStatus::kFailed:
    case DriveFileDownloadStatus::kNotStarted: {
      UIBarButtonItem* rightBarButton = [self confirmButtonItem];
      rightBarButton.enabled = NO;
      return rightBarButton;
    }
  }
}

// Returns a button with the title `Confirm`.
- (UIBarButtonItem*)confirmButtonItem {
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_CONFIRM)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(confirmSelection)];
  return confirmButton;
}

// Returns an activity indicator when the download is in progress.
- (UIBarButtonItem*)activityIndicatorButtonItem {
  UIActivityIndicatorView* activityIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  [activityIndicator startAnimating];

  UIBarButtonItem* activityIndicatorButton =
      [[UIBarButtonItem alloc] initWithCustomView:activityIndicator];
  activityIndicatorButton.enabled = YES;
  return activityIndicatorButton;
}

// Configures the sort button by attaching a UIMenu of the actions to it.
- (void)configureSortButton {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramSortDriveItemsEntry];

  __weak __typeof(self) weakSelf = self;
  _sortByNameAction = [actionFactory actionToSortDriveItemsByNameWithBlock:^{
    [weakSelf turnOnSortByName];
    // TODO(crbug.com/344812548): Add the sorting request.
  }];
  _sortByModificationTimeAction =
      [actionFactory actionToSortDriveItemsByModificationTimeWithBlock:^{
        [weakSelf turnOnSortByModificationTime];
        // TODO(crbug.com/344812548): Add the sorting request.
      }];
  _sortByModificationTimeAction.state = UIMenuElementStateOn;
  _sortByModificationTimeAction.image =
      DefaultSymbolWithPointSize(kChevronDownSymbol, kSymbolAccessoryPointSize);
  _sortByOpeningTimeAction =
      [actionFactory actionToSortDriveItemsByOpeningTimeWithBlock:^{
        [weakSelf turnOnSortByOpeningTime];
        // TODO(crbug.com/344812548): Add the sorting request.
      }];
}

// Turns on the state of `_sortByNameAction` if it was previously off, if not,
// switches the sorting order.
- (void)turnOnSortByName {
  if (_sortingType == DriveItemsSortingType::kName) {
    [self switchSortingOrderForType:DriveItemsSortingType::kName];
  } else {
    [self selectSortingType:DriveItemsSortingType::kName];
  }
  _sortButton.menu = [_sortButton.menu menuByReplacingChildren:@[
    _sortByNameAction, _sortByOpeningTimeAction, _sortByModificationTimeAction
  ]];
}

// Turns on the state of `_sortByModificationTimeAction` if it was previously
// off, if not, switches the sorting order.
- (void)turnOnSortByModificationTime {
  if (_sortingType == DriveItemsSortingType::kModificationTime) {
    [self switchSortingOrderForType:DriveItemsSortingType::kModificationTime];
  } else {
    [self selectSortingType:DriveItemsSortingType::kModificationTime];
  }
  _sortButton.menu = [_sortButton.menu menuByReplacingChildren:@[
    _sortByNameAction, _sortByOpeningTimeAction, _sortByModificationTimeAction
  ]];
}

// Turns on the state of `_sortByOpeningTimeAction` if it was previously off, if
// not, switches the sorting order.
- (void)turnOnSortByOpeningTime {
  if (_sortingType == DriveItemsSortingType::kOpeningTime) {
    [self switchSortingOrderForType:DriveItemsSortingType::kOpeningTime];
  } else {
    [self selectSortingType:DriveItemsSortingType::kOpeningTime];
  }
  _sortButton.menu = [_sortButton.menu menuByReplacingChildren:@[
    _sortByNameAction, _sortByOpeningTimeAction, _sortByModificationTimeAction
  ]];
}

// Switches the recent sorting order and updates the actions icons accordinly.
- (void)switchSortingOrderForType:(DriveItemsSortingType)sortingType {
  UIAction* sortingAction = [self actionForSortingType:sortingType];
  switch (_sortingOrder) {
    case DriveItemsSortingOrder::kAscending:
      _sortingOrder = DriveItemsSortingOrder::kDescending;
      sortingAction.image = DefaultSymbolWithPointSize(
          kChevronDownSymbol, kSymbolAccessoryPointSize);
      break;
    case DriveItemsSortingOrder::kDescending:
      _sortingOrder = DriveItemsSortingOrder::kAscending;
      sortingAction.image = DefaultSymbolWithPointSize(
          kChevronUpSymbol, kSymbolAccessoryPointSize);
      break;
  }
}

// Resets the states and icons of the sorting actions.
- (void)resetSortingActionsStates {
  _sortByNameAction.state = UIMenuElementStateOff;
  _sortByNameAction.image = nil;
  _sortByOpeningTimeAction.state = UIMenuElementStateOff;
  _sortByOpeningTimeAction.image = nil;
  _sortByModificationTimeAction.state = UIMenuElementStateOff;
  _sortByModificationTimeAction.image = nil;
}

// Updates the status and icon of the action corresponding to a given sorting
// type.
- (void)selectSortingType:(DriveItemsSortingType)sortingType {
  UIAction* sortingAction = [self actionForSortingType:sortingType];
  [self resetSortingActionsStates];
  sortingAction.state = UIMenuElementStateOn;
  sortingAction.image =
      DefaultSymbolWithPointSize(kChevronDownSymbol, kSymbolAccessoryPointSize);
  _sortingType = sortingType;
  _sortingOrder = DriveItemsSortingOrder::kDescending;
}

// Returns the action corresponding to a given `sortingType`.
- (UIAction*)actionForSortingType:(DriveItemsSortingType)sortingType {
  switch (sortingType) {
    case DriveItemsSortingType::kName:
      return _sortByNameAction;
    case DriveItemsSortingType::kModificationTime:
      return _sortByModificationTimeAction;
    case DriveItemsSortingType::kOpeningTime:
      return _sortByOpeningTimeAction;
  }
}

// Deques and sets up a cell for a drive item.
- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(DriveItemIdentifier*)itemIdentifier {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;
  [cell.textLabel setText:itemIdentifier.title];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  [cell setIconImage:itemIdentifier.icon
            tintColor:nil
      backgroundColor:cell.backgroundColor
         cornerRadius:kCellIconCornerRadius];

  if (itemIdentifier.type == DriveItemType::kFile) {
    [cell setDetailText:itemIdentifier.creationDate];
    [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];
    cell.accessoryType = UITableViewCellAccessoryNone;
  }
  return cell;
}

#pragma mark - DriveFilePickerConsumer

- (void)populateItems:(NSArray<DriveItemIdentifier*>*)driveItems {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierDriveMainFolders) ]];
  [snapshot appendItemsWithIdentifiers:driveItems];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DriveItemIdentifier* driveItem =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  if (driveItem.type == DriveItemType::kFolder) {
    [self.mutator selectDriveItem:driveItem];
  }
  // TODO(crbug.com/344812396): Add the download in case of files.
}

@end
