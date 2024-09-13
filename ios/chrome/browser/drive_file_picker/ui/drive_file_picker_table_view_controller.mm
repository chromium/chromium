// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_alert_utils.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller_delegate.h"
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

DriveFilePickerItem* FindDriveFilePickerItem(
    NSString* identifier,
    NSArray<DriveFilePickerItem*>* items) {
  for (DriveFilePickerItem* item in items) {
    if ([item.identifier isEqual:identifier]) {
      return item;
    }
  }
  return nil;
}

}  // namespace

@implementation DriveFilePickerTableViewController {
  // The status of file dowload.
  DriveFileDownloadStatus _status;

  // The filtering actions.
  UIAction* _ignoreAcceptedTypesAction;
  UIAction* _showArchiveFilesAction;
  UIAction* _showAudioFilesAction;
  UIAction* _showVideoFilesAction;
  UIAction* _showImageFilesAction;
  UIAction* _showPDFFilesAction;
  UIAction* _showAllFilesAction;

  // The sorting actions
  UIAction* _sortByNameAction;
  UIAction* _sortByModificationTimeAction;
  UIAction* _sortByOpeningTimeAction;

  // The symbols representing ascending/descending sorting directions.
  UIImage* _sortAscendingSymbol;
  UIImage* _sortDescendingSymbol;

  // The filter and sort button.
  UIBarButtonItem* _filterButton;
  UIBarButtonItem* _sortButton;

  // The selected email from the accounts signed in the device.
  NSString* _selectedEmail;

  // Account chooser button.
  UIBarButtonItem* _accountButton;

  // The currently represented folder.
  NSString* _driveFolderTitle;

  UITableViewDiffableDataSource<NSNumber*, NSString*>* _diffableDataSource;
  NSMutableArray<DriveFilePickerItem*>* _items;

  // A loading indocator displayed when the next page is being fetched.
  UIActivityIndicatorView* _loadingIndicator;

  // A loading indocator displayed in the background while the items are being
  // fetched.
  UIActivityIndicatorView* _backgroundLoadingIndicator;

  // Next page availability.
  BOOL _nextPageAvailable;

  // The selected item identifier.
  NSString* _selectedIdentifier;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _status = DriveFileDownloadStatus::kNotStarted;
    [self initFilterActions];
    [self initSortActions];
    [self initSortingDirectionSymbols];
    _nextPageAvailable = YES;
    _items = [NSMutableArray array];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;

  [self configureToolbar];

  self.navigationItem.backAction =
      [UIAction actionWithHandler:^(UIAction* action) {
        [weakSelf backButtonTapped];
      }];
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

  _backgroundLoadingIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  _backgroundLoadingIndicator.hidesWhenStopped = YES;
  self.tableView.backgroundView = _backgroundLoadingIndicator;
  [_backgroundLoadingIndicator startAnimating];

  _loadingIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  _loadingIndicator.hidesWhenStopped = YES;
  self.tableView.tableFooterView = _loadingIndicator;

  auto cellProvider = ^UITableViewCell*(UITableView* tableView,
                                        NSIndexPath* indexPath,
                                        NSString* itemIdentifier) {
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:itemIdentifier];
  };
  _diffableDataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];

  self.tableView.dataSource = _diffableDataSource;

  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);

  [self.mutator fetchNextPage];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if ([self isMovingFromParentViewController]) {
    [self.delegate viewControllerDidDisappear:self];
  }
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

- (void)showInterruptionAlertWithBlock:(ProceduralBlock)block {
  [self presentViewController:InterruptionAlertController(block)
                     animated:YES
                   completion:nil];
}

#pragma mark - UI actions

- (void)confirmSelection {
  [self.mutator submitFileSelection];
}

- (void)didSelectSortingCriteria:(DriveItemsSortingType)sortingCriteria {
  UIAction* selectedSortingAction =
      [self actionForSortingCriteria:sortingCriteria];
  DriveItemsSortingOrder sortingDirection;
  switch (selectedSortingAction.state) {
    case UIMenuElementStateOn:
      // Action was already selected. Inverting sort direction.
      sortingDirection = selectedSortingAction.image == _sortAscendingSymbol
                             ? DriveItemsSortingOrder::kDescending
                             : DriveItemsSortingOrder::kAscending;
      break;
    case UIMenuElementStateOff:
      // Action was not selected.
      switch (sortingCriteria) {
        case DriveItemsSortingType::kName:
          // Default direction for sorting by name is ascending.
          sortingDirection = DriveItemsSortingOrder::kAscending;
          break;
        case DriveItemsSortingType::kModificationTime:
          // Default direction for sorting by modification time is descending.
          sortingDirection = DriveItemsSortingOrder::kDescending;
          break;
        case DriveItemsSortingType::kOpeningTime:
          // Default direction for sorting by opening time is descending.
          sortingDirection = DriveItemsSortingOrder::kDescending;
          break;
      }
      break;
    default:
      NOTREACHED();
  }
  [self.mutator setSortingCriteria:sortingCriteria direction:sortingDirection];
}

#pragma mark - Private

- (void)backButtonTapped {
  [self.mutator browseToParent];
}

// Configures the toolbar with 3 buttons, filterButton <---->
// AccountButton(where the title is the user's email) <----> sortButton(which
// should not be enabled for the root of the navigation controller)
- (void)configureToolbar {
  UIImage* filterIcon = DefaultSymbolTemplateWithPointSize(
      kFilterSymbol, kSymbolAccessoryPointSize);
  UIMenu* filterButtonMenu = [self createFilterButtonMenu];
  _filterButton = [[UIBarButtonItem alloc] initWithImage:filterIcon
                                                    menu:filterButtonMenu];
  _filterButton.enabled = YES;
  _filterButton.preferredMenuElementOrder =
      UIContextMenuConfigurationElementOrderFixed;

  UIImage* sortIcon = DefaultSymbolTemplateWithPointSize(
      kSortSymbol, kSymbolAccessoryPointSize);

  // TODO(crbug.com/344812548): Add the action of the sort button.
  UIMenu* sortButtonMenu = [self createSortButtonMenu];
  _sortButton = [[UIBarButtonItem alloc] initWithImage:sortIcon
                                                  menu:sortButtonMenu];
  _sortButton.enabled = YES;

  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[
    _filterButton, spaceButton, _accountButton, spaceButton, _sortButton
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

// Initializes the filter actions.
- (void)initFilterActions {
  __weak __typeof(self) weakSelf = self;
  _ignoreAcceptedTypesAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_DRIVE_FILE_PICKER_FILTER_ENABLE_ALL_TITLE)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                BOOL oldAcceptedTypesIgnored =
                    action.state == UIMenuElementStateOn;
                BOOL newAcceptedTypesIgnored = !oldAcceptedTypesIgnored;
                [weakSelf.mutator
                    setAcceptedTypesIgnored:newAcceptedTypesIgnored];
              }];
  _ignoreAcceptedTypesAction.subtitle = l10n_util::GetNSString(
      IDS_IOS_DRIVE_FILE_PICKER_FILTER_ENABLE_ALL_DESCRIPTION);
  _showArchiveFilesAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_DRIVE_FILE_PICKER_FILTER_ARCHIVES)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.mutator
                    setFilter:DriveFilePickerFilter::kOnlyShowArchives];
              }];
  _showAudioFilesAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_DRIVE_FILE_PICKER_FILTER_AUDIO)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator
                              setFilter:DriveFilePickerFilter::kOnlyShowAudio];
                        }];
  _showVideoFilesAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_DRIVE_FILE_PICKER_FILTER_VIDEOS)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator
                              setFilter:DriveFilePickerFilter::kOnlyShowVideos];
                        }];
  _showImageFilesAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_DRIVE_FILE_PICKER_FILTER_IMAGES)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator
                              setFilter:DriveFilePickerFilter::kOnlyShowImages];
                        }];
  _showPDFFilesAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_DRIVE_FILE_PICKER_FILTER_PDF)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator
                              setFilter:DriveFilePickerFilter::kOnlyShowPDFs];
                        }];
  _showAllFilesAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_DRIVE_FILE_PICKER_FILTER_ALL_FILES)
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf.mutator
                              setFilter:DriveFilePickerFilter::kShowAllFiles];
                        }];
}

// Initializes the sorting actions.
- (void)initSortActions {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramSortDriveItemsEntry];
  __weak __typeof(self) weakSelf = self;
  _sortByNameAction = [actionFactory actionToSortDriveItemsByNameWithBlock:^{
    [weakSelf didSelectSortingCriteria:DriveItemsSortingType::kName];
  }];
  _sortByModificationTimeAction =
      [actionFactory actionToSortDriveItemsByModificationTimeWithBlock:^{
        [weakSelf
            didSelectSortingCriteria:DriveItemsSortingType::kModificationTime];
      }];
  _sortByOpeningTimeAction =
      [actionFactory actionToSortDriveItemsByOpeningTimeWithBlock:^{
        [weakSelf didSelectSortingCriteria:DriveItemsSortingType::kOpeningTime];
      }];
}

// Initializes the sorting direction symbols.
- (void)initSortingDirectionSymbols {
  _sortAscendingSymbol =
      DefaultSymbolWithPointSize(kChevronUpSymbol, kSymbolAccessoryPointSize);
  _sortDescendingSymbol =
      DefaultSymbolWithPointSize(kChevronDownSymbol, kSymbolAccessoryPointSize);
}

// Returns the action corresponding to a given `sortingCriteria`.
- (UIAction*)actionForSortingCriteria:(DriveItemsSortingType)sortingCriteria {
  switch (sortingCriteria) {
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
                      itemIdentifier:(NSString*)itemIdentifier {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);
  DriveFilePickerItem* item = FindDriveFilePickerItem(itemIdentifier, _items);
  CHECK(item);

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  [cell.textLabel setText:item.title];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  if (!item.icon) {
    [self.mutator fetchIconForDriveItem:itemIdentifier];
  } else {
    [cell setIconImage:item.icon
              tintColor:nil
        backgroundColor:cell.backgroundColor
           cornerRadius:kCellIconCornerRadius];
  }

  if (item.type == DriveItemType::kFile) {
    [cell setDetailText:item.creationDate];
    [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];
    cell.accessoryType = [itemIdentifier isEqual:_selectedIdentifier]
                             ? UITableViewCellAccessoryCheckmark
                             : UITableViewCellAccessoryNone;
  }

  cell.userInteractionEnabled = item.enabled;
  cell.textLabel.enabled = item.enabled;
  cell.detailTextLabel.enabled = item.enabled;
  return cell;
}

#pragma mark - DriveFilePickerConsumer

- (void)populateItems:(NSArray<DriveFilePickerItem*>*)driveItems
               append:(BOOL)append
    nextPageAvailable:(BOOL)nextPageAvailable {
  if (append) {
    [_items addObjectsFromArray:driveItems];
  } else {
    _items = [driveItems mutableCopy];
  }

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierDriveMainFolders) ]];
  NSMutableArray<NSString*>* identifiers = [NSMutableArray array];
  for (DriveFilePickerItem* item in _items) {
    [identifiers addObject:item.identifier];
  }
  [snapshot appendItemsWithIdentifiers:identifiers];

  _nextPageAvailable = nextPageAvailable;
  [_loadingIndicator stopAnimating];
  [_backgroundLoadingIndicator stopAnimating];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)setEmailsMenu:(UIMenu*)emailsMenu {
  _accountButton = [[UIBarButtonItem alloc] initWithTitle:_selectedEmail
                                                     menu:emailsMenu];
}

- (void)reconfigureDriveItem:(DriveFilePickerItem*)driveItem {
  for (size_t i = 0; i < _items.count; ++i) {
    if ([_items[i].identifier isEqual:driveItem.identifier]) {
      _items[i] = driveItem;
    }
  }
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:@[ driveItem.identifier ]];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)setIcon:(UIImage*)iconImage forItem:(NSString*)itemIdentifier {
  for (size_t i = 0; i < _items.count; ++i) {
    if ([_items[i].identifier isEqual:itemIdentifier]) {
      _items[i].icon = iconImage;
      break;
    }
  }
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:@[ itemIdentifier ]];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)setDownloadStatus:(DriveFileDownloadStatus)downloadStatus {
  _status = downloadStatus;
  self.navigationItem.rightBarButtonItem = [self configureRightBarButtonItem];
}

- (void)setEnabledItems:(NSSet<NSString*>*)identifiers {
  NSMutableArray<NSString*>* identifiersToReconfigure = [NSMutableArray array];
  for (DriveFilePickerItem* item in _items) {
    BOOL itemShouldBeEnabled = [identifiers containsObject:item.identifier];
    if (item.enabled == itemShouldBeEnabled) {
      continue;
    }
    item.enabled = itemShouldBeEnabled;
    [identifiersToReconfigure addObject:item.identifier];
  }
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:identifiersToReconfigure];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)setAllFilesEnabled:(BOOL)allFilesEnabled {
  _ignoreAcceptedTypesAction.state =
      allFilesEnabled ? UIMenuElementStateOn : UIMenuElementStateOff;
  // The menu needs to be reset for the new state to appear.
  _filterButton.menu = [self createFilterButtonMenu];
}

- (void)setFilter:(DriveFilePickerFilter)filter {
  _showArchiveFilesAction.state = UIMenuElementStateOff;
  _showAudioFilesAction.state = UIMenuElementStateOff;
  _showVideoFilesAction.state = UIMenuElementStateOff;
  _showImageFilesAction.state = UIMenuElementStateOff;
  _showPDFFilesAction.state = UIMenuElementStateOff;
  _showAllFilesAction.state = UIMenuElementStateOff;
  switch (filter) {
    case DriveFilePickerFilter::kOnlyShowArchives:
      _showArchiveFilesAction.state = UIMenuElementStateOn;
      break;
    case DriveFilePickerFilter::kOnlyShowAudio:
      _showAudioFilesAction.state = UIMenuElementStateOn;
      break;
    case DriveFilePickerFilter::kOnlyShowVideos:
      _showVideoFilesAction.state = UIMenuElementStateOn;
      break;
    case DriveFilePickerFilter::kOnlyShowImages:
      _showImageFilesAction.state = UIMenuElementStateOn;
      break;
    case DriveFilePickerFilter::kOnlyShowPDFs:
      _showPDFFilesAction.state = UIMenuElementStateOn;
      break;
    case DriveFilePickerFilter::kShowAllFiles:
      _showAllFilesAction.state = UIMenuElementStateOn;
      break;
    default:
      break;
  }
  // The menu needs to be reset for the new state to appear.
  _filterButton.menu = [self createFilterButtonMenu];
}

- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction {
  _sortByNameAction.state = UIMenuElementStateOff;
  _sortByNameAction.image = nil;
  _sortByOpeningTimeAction.state = UIMenuElementStateOff;
  _sortByOpeningTimeAction.image = nil;
  _sortByModificationTimeAction.state = UIMenuElementStateOff;
  _sortByModificationTimeAction.image = nil;
  UIAction* enabledSortingAction = [self actionForSortingCriteria:criteria];
  enabledSortingAction.image = direction == DriveItemsSortingOrder::kAscending
                                   ? _sortAscendingSymbol
                                   : _sortDescendingSymbol;
  enabledSortingAction.state = UIMenuElementStateOn;
  // The menu needs to be reset for the new state to appear.
  _sortButton.menu = [self createSortButtonMenu];
}

- (void)setSelectedItemIdentifier:(NSString*)selectedIdentifier {
  if ([_selectedIdentifier isEqual:selectedIdentifier]) {
    return;
  }
  NSString* previousSelectedIdentifier = _selectedIdentifier;
  _selectedIdentifier = selectedIdentifier;
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  NSMutableArray* identifiersToReconfigure = [NSMutableArray array];
  for (NSString* itemIdentifier in snapshot.itemIdentifiers) {
    if ([itemIdentifier isEqual:previousSelectedIdentifier] ||
        [itemIdentifier isEqual:_selectedIdentifier]) {
      [identifiersToReconfigure addObject:itemIdentifier];
    }
  }
  [snapshot reconfigureItemsWithIdentifiers:identifiersToReconfigure];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)disableConfirmation {
}

#pragma mark - UI element creation helpers

// Helper to create the menu presented by `_filterButton`.
- (UIMenu*)createFilterButtonMenu {
  UIMenu* moreOptionsMenu =
      [UIMenu menuWithTitle:l10n_util::GetNSString(
                                IDS_IOS_DRIVE_FILE_PICKER_FILTER_MORE_OPTIONS)
                   children:@[ _ignoreAcceptedTypesAction ]];
  UIMenu* showFileTypeMenu = [UIMenu
      menuWithTitle:@""
              image:nil
         identifier:nil
            options:UIMenuOptionsDisplayInline
           children:@[
             _showArchiveFilesAction, _showAudioFilesAction,
             _showVideoFilesAction, _showImageFilesAction, _showPDFFilesAction
           ]];
  return [UIMenu menuWithChildren:@[
    moreOptionsMenu, showFileTypeMenu, _showAllFilesAction
  ]];
}

// Helper to create the menu presented by `_sortButton`.
- (UIMenu*)createSortButtonMenu {
  return [UIMenu menuWithChildren:@[
    _sortByNameAction, _sortByOpeningTimeAction, _sortByModificationTimeAction
  ]];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  DriveFilePickerItem* item = FindDriveFilePickerItem(itemIdentifier, _items);
  CHECK(item);
  if (!item.enabled) {
    // If selecting a disabled item, nothing should happen.
    return;
  }
  [self.mutator selectDriveItem:itemIdentifier];
}

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  if (indexPath.row == snapshot.numberOfItems - 1 && _nextPageAvailable) {
    [_loadingIndicator startAnimating];
    [self.mutator fetchNextPage];
  }
}

@end
