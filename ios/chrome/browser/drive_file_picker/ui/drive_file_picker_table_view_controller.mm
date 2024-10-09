// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_alert_utils.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_empty_view.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat kCellImageDimension = 24.0;
constexpr CGFloat kCellVerticalMarginsText = 12.0;
constexpr CGFloat kCellVerticalMarginsTextAndSecondaryText = 8.0;
constexpr CGFloat kCellTextToSecondaryTextVerticalPadding = 4.0;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
constexpr CGFloat kTitleLogoSpacing = 3.0;
constexpr CGFloat kLogoTitleFontMultiplier = 1.75;

// Creates the google drive branded title view for the navigation.
BrandedNavigationItemTitleView* CreateGoogleDriveImageView() {
  BrandedNavigationItemTitleView* title_view =
      [[BrandedNavigationItemTitleView alloc] init];
  title_view.title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_TO_DRIVE);
  title_view.imageLogo = MakeSymbolMulticolor(CustomSymbolWithPointSize(
      kGoogleFullSymbol, UIFont.labelFontSize * kLogoTitleFontMultiplier));
  title_view.titleLogoSpacing = kTitleLogoSpacing;
  return title_view;
}
#else
// Creates the google drive title label for the navigation.
UILabel* CreateGoogleDriveTitleLabel() {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_GOOGLE_DRIVE);
  titleLabel.textAlignment = NSTextAlignmentLeft;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.1;
  return titleLabel;
}
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPrimaryItems = kSectionIdentifierEnumZero,
  SectionIdentifierSecondaryItems,
};

// Returns the first occurrence of an item with `identifier` either in
// `primary_items` or otherwise in `secondary_items`.
DriveFilePickerItem* FindDriveFilePickerItem(
    NSString* identifier,
    NSArray<DriveFilePickerItem*>* primary_items,
    NSArray<DriveFilePickerItem*>* secondary_items) {
  for (DriveFilePickerItem* item in primary_items) {
    if ([item.identifier isEqual:identifier]) {
      return item;
    }
  }
  for (DriveFilePickerItem* item in secondary_items) {
    if ([item.identifier isEqual:identifier]) {
      return item;
    }
  }
  return nil;
}

// Helper to set the text in `searchBar` to `text`.
void SetSearchBarText(UISearchBar* searchBar, NSString* text) {
  searchBar.text = text;
}

}  // namespace

@interface DriveFilePickerTableViewController () <UISearchControllerDelegate,
                                                  UISearchResultsUpdating>

@end

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

  // The cancel, filter, account and sort button.
  UIBarButtonItem* _cancelButton;
  UIBarButtonItem* _filterButton;
  UIBarButtonItem* _accountButton;
  UIBarButtonItem* _sortButton;

  // The currently represented folder.
  NSString* _driveFolderTitle;

  UITableViewDiffableDataSource<NSNumber*, NSString*>* _diffableDataSource;
  // Primary items i.e. items in the first section.
  NSMutableArray<DriveFilePickerItem*>* _primaryItems;
  // Secondary items i.e. items in the second section.
  NSMutableArray<DriveFilePickerItem*>* _secondaryItems;

  // Search header view presented at the top of the first section.
  UIView* _searchHeader;

  // A loading indocator displayed when the next page is being fetched.
  UIActivityIndicatorView* _loadingIndicator;

  // Background views.
  UIActivityIndicatorView* _backgroundLoadingIndicator;
  UIView* _backgroundEmptyFolderView;
  UIView* _backgroundNoMatchingResultView;

  // Next page availability.
  BOOL _nextPageAvailable;

  // The selected item identifier.
  NSString* _selectedIdentifier;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _status = DriveFileDownloadStatus::kNotStarted;
    _cancelButton = [self createCancelButton];
    [self initFilterActions];
    [self initSortActions];
    [self initToolbarItems];
    [self initSortDirectionSymbols];
    [self initBackgroundViews];
    _nextPageAvailable = YES;
    _primaryItems = [NSMutableArray array];
    _secondaryItems = [NSMutableArray array];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set toolbar items.
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[
    _filterButton, spaceButton, _accountButton, spaceButton, _sortButton
  ]
               animated:NO];

  self.navigationItem.rightBarButtonItem = [self configureRightBarButtonItem];

  // Add the search bar.
  self.navigationItem.searchController = [[UISearchController alloc] init];
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.navigationItem.preferredSearchBarPlacement =
      UINavigationItemSearchBarPlacementStacked;
  self.navigationItem.searchController.searchResultsUpdater = self;
  self.navigationItem.searchController.delegate = self;
  self.navigationItem.searchController.hidesNavigationBarDuringPresentation =
      YES;

  // Initialize the table view.
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  self.navigationController.toolbarHidden = NO;

  _loadingIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  _loadingIndicator.hidesWhenStopped = YES;
  self.tableView.tableFooterView = _loadingIndicator;

  __weak __typeof(self) weakSelf = self;
  auto cellProvider = ^UITableViewCell*(UITableView* tableView,
                                        NSIndexPath* indexPath,
                                        NSString* itemIdentifier) {
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:itemIdentifier];
  };
  _diffableDataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];

  self.tableView.dataSource = _diffableDataSource;

  RegisterTableViewCell<UITableViewCell>(self.tableView);

  // Set up search header.
  UILabel* searchTitle = [[UILabel alloc] init];
  searchTitle.translatesAutoresizingMaskIntoConstraints = NO;
  searchTitle.text =
      l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_RECENT_TITLE);
  searchTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  searchTitle.adjustsFontForContentSizeCategory = YES;
  _searchHeader = [[UIView alloc] init];
  [_searchHeader addSubview:searchTitle];
  AddSameConstraintsWithInsets(searchTitle, _searchHeader,
                               NSDirectionalEdgeInsetsMake(6, 0, 6, 0));

  [self.mutator loadFirstPage];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if ([self isMovingFromParentViewController]) {
    [self.delegate viewControllerDidDisappear:self];
  }
}

#pragma mark - UI actions

- (void)cancelSelection {
  __weak __typeof(self) weakSelf = self;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf
                                   .driveFilePickerHandler hideDriveFilePicker];
                         }];
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

// Creates `_cancelButton`.
- (UIBarButtonItem*)createCancelButton {
  __weak __typeof(self) weakSelf = self;
  UIAction* primaryAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.mutator hideSearchItemsOrCancelFileSelection];
  }];
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                    primaryAction:primaryAction];
}

// Initializes the toolbar filter, account and sort buttons.
- (void)initToolbarItems {
  // Init filter button.
  UIImage* filterIcon = DefaultSymbolTemplateWithPointSize(
      kFilterSymbol, kSymbolAccessoryPointSize);
  UIMenu* filterButtonMenu = [self createFilterButtonMenu];
  _filterButton = [[UIBarButtonItem alloc] initWithImage:filterIcon
                                                    menu:filterButtonMenu];
  _filterButton.enabled = YES;
  _filterButton.preferredMenuElementOrder =
      UIContextMenuConfigurationElementOrderFixed;
  _filterButton.accessibilityIdentifier =
      kDriveFilePickerFilterButtonIdentifier;

  // Init account button.
  _accountButton = [[UIBarButtonItem alloc] init];
  _accountButton.accessibilityIdentifier = kDriveFilePickerIdentityIdentifier;

  // Init sort button.
  UIImage* sortIcon = DefaultSymbolTemplateWithPointSize(
      kSortSymbol, kSymbolAccessoryPointSize);
  UIMenu* sortButtonMenu = [self createSortButtonMenu];
  _sortButton = [[UIBarButtonItem alloc] initWithImage:sortIcon
                                                  menu:sortButtonMenu];
  _sortButton.enabled = YES;
  _sortButton.accessibilityIdentifier = kDriveFilePickerSortButtonIdentifier;
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
             target:self.mutator
             action:@selector(submitFileSelection)];
  confirmButton.accessibilityIdentifier =
      kDriveFilePickerConfirmButtonIdentifier;
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
- (void)initSortDirectionSymbols {
  _sortAscendingSymbol =
      DefaultSymbolWithPointSize(kChevronUpSymbol, kSymbolAccessoryPointSize);
  _sortDescendingSymbol =
      DefaultSymbolWithPointSize(kChevronDownSymbol, kSymbolAccessoryPointSize);
}

// Initializes background views.
- (void)initBackgroundViews {
  _backgroundLoadingIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  _backgroundEmptyFolderView = [DriveFilePickerEmptyView emptyDriveFolderView];
  _backgroundNoMatchingResultView =
      [DriveFilePickerEmptyView noMatchingResultView];
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
  UITableViewCell* cell = DequeueTableViewCell<UITableViewCell>(self.tableView);
  DriveFilePickerItem* item =
      FindDriveFilePickerItem(itemIdentifier, _primaryItems, _secondaryItems);
  if (!item) {
    // When an item is removed from the data source in an animated way, the data
    // source might still want to configure the associated cell for the removal
    // animation. Since the item is not available anymore however, return any
    // dequeued cell as-is.
    return cell;
  }

  UIListContentConfiguration* contentConfiguration =
      item.subtitle ? [UIListContentConfiguration subtitleCellConfiguration]
                    : [UIListContentConfiguration cellConfiguration];

  // Set up cell image.
  UIListContentImageProperties* imageProperties =
      contentConfiguration.imageProperties;
  contentConfiguration.image = item.icon;
  imageProperties.reservedLayoutSize = CGSizeMake(
      UIListContentImageStandardDimension, UIListContentImageStandardDimension);
  imageProperties.maximumSize =
      CGSize(kCellImageDimension, kCellImageDimension);
  // Set image tint color.
  if (item.icon.isSymbolImage) {
    imageProperties.tintColor = [UIColor colorNamed:kGrey600Color];
    imageProperties.cornerRadius = 0;
  } else {
    imageProperties.tintColor = nil;
    imageProperties.cornerRadius = item.type == DriveItemType::kSharedDrive
                                       ? kCellImageDimension / 2.0
                                       : 0;
  }

  // Set up text.
  UIListContentTextProperties* textProperties =
      contentConfiguration.textProperties;
  textProperties.color = item.enabled ? [UIColor colorNamed:kTextPrimaryColor]
                                      : [UIColor colorNamed:kDisabledTintColor];
  textProperties.numberOfLines = 1;
  textProperties.allowsDefaultTighteningForTruncation = YES;
  UIFont* textFont = textProperties.font;
  if (item.titleRangeToEmphasize.location == NSNotFound) {
    contentConfiguration.text = item.title;
  } else {
    // If there is a range to emphasize in the title, use bold font for this
    // range.
    NSMutableAttributedString* attributedTitle =
        [[NSMutableAttributedString alloc] initWithString:item.title];
    UIFontDescriptor* boldFontDescriptor = [textFont.fontDescriptor
        fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
    UIFont* boldFont = [UIFont fontWithDescriptor:boldFontDescriptor
                                             size:textFont.pointSize];
    [attributedTitle setAttributes:@{NSFontAttributeName : boldFont}
                             range:item.titleRangeToEmphasize];
    contentConfiguration.attributedText = attributedTitle;
  }

  // Set up subtitle (secondary text).
  if (item.subtitle) {
    contentConfiguration.secondaryText = item.subtitle;
    contentConfiguration.secondaryTextProperties.color =
        item.enabled ? [UIColor colorNamed:kTextSecondaryColor]
                     : [UIColor colorNamed:kDisabledTintColor];
    contentConfiguration.secondaryTextProperties.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  }

  // Set up layout.
  NSDirectionalEdgeInsets directionalLayoutMargins =
      contentConfiguration.directionalLayoutMargins;
  directionalLayoutMargins.top = item.subtitle
                                     ? kCellVerticalMarginsTextAndSecondaryText
                                     : kCellVerticalMarginsText;
  directionalLayoutMargins.bottom =
      item.subtitle ? kCellVerticalMarginsTextAndSecondaryText
                    : kCellVerticalMarginsText;
  contentConfiguration.directionalLayoutMargins = directionalLayoutMargins;
  contentConfiguration.textToSecondaryTextVerticalPadding =
      kCellTextToSecondaryTextVerticalPadding;

  cell.contentConfiguration = contentConfiguration;

  // Set up background.
  UIBackgroundConfiguration* backgroundConfiguration =
      [UIBackgroundConfiguration listGroupedCellConfiguration];
  backgroundConfiguration.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.backgroundConfiguration = backgroundConfiguration;

  // Set other cell properties.
  if (item.type == DriveItemType::kFile) {
    cell.accessoryType = [itemIdentifier isEqual:_selectedIdentifier]
                             ? UITableViewCellAccessoryCheckmark
                             : UITableViewCellAccessoryNone;
  } else {
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.accessibilityIdentifier = item.identifier;

  return cell;
}

#pragma mark - DriveFilePickerConsumer

- (void)setSelectedUserIdentityEmail:(NSString*)selectedUserIdentityEmail {
  _accountButton.title = selectedUserIdentityEmail;
}

- (void)setTitle:(NSString*)title {
  if (!title) {
    self.navigationItem.titleView = nil;
    return;
  }
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.text = title;
  titleLabel.textAlignment = NSTextAlignmentLeft;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.1;
  self.navigationItem.titleView = titleLabel;
}

- (void)setRootTitle {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.navigationItem.titleView = CreateGoogleDriveImageView();
#else
  self.navigationItem.titleView = CreateGoogleDriveTitleLabel();
#endif
  self.navigationItem.titleView.accessibilityIdentifier =
      kDriveFilePickerRootTitleAccessibilityIdentifier;
}

- (void)setBackground:(DriveFilePickerBackground)background {
  UIView* newBackgroundView;
  switch (background) {
    case DriveFilePickerBackground::kNoBackground:
      newBackgroundView = nil;
      break;
    case DriveFilePickerBackground::kLoadingIndicator:
      newBackgroundView = _backgroundLoadingIndicator;
      break;
    case DriveFilePickerBackground::kEmptyFolder:
      newBackgroundView = _backgroundEmptyFolderView;
      break;
    case DriveFilePickerBackground::kNoMatchingResults:
      newBackgroundView = _backgroundNoMatchingResultView;
      break;
  }
  if (self.tableView.backgroundView == newBackgroundView) {
    return;
  }
  if (self.tableView.backgroundView == _backgroundLoadingIndicator) {
    [_backgroundLoadingIndicator stopAnimating];
  }
  self.tableView.backgroundView = newBackgroundView;
  if (self.tableView.backgroundView == _backgroundLoadingIndicator) {
    [_backgroundLoadingIndicator startAnimating];
  }
}

- (void)populatePrimaryItems:(NSArray<DriveFilePickerItem*>*)primaryItems
              secondaryItems:(NSArray<DriveFilePickerItem*>*)secondaryItems
                      append:(BOOL)append
            showSearchHeader:(BOOL)showSearchHeader
           nextPageAvailable:(BOOL)nextPageAvailable
                    animated:(BOOL)animated {
  // Reset scroll if necessary.
  if (!append) {
    [self.view layoutIfNeeded];
    [self.tableView
        setContentOffset:CGPointMake(0,
                                     -self.tableView.adjustedContentInset.top)
                animated:NO];
  }

  if (append) {
    [_primaryItems addObjectsFromArray:primaryItems];
    [_secondaryItems addObjectsFromArray:secondaryItems];
  } else {
    _primaryItems = [primaryItems mutableCopy];
    _secondaryItems = [secondaryItems mutableCopy];
  }

  // Rebuild the list of identifiers.
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  // First section.
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierPrimaryItems) ]];
  NSMutableArray<NSString*>* primaryIdentifiers = [NSMutableArray array];
  for (DriveFilePickerItem* item in _primaryItems) {
    [primaryIdentifiers addObject:item.identifier];
  }
  [snapshot appendItemsWithIdentifiers:primaryIdentifiers];
  // Second section.
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierSecondaryItems) ]];
  NSMutableArray<NSString*>* secondaryIdentifiers = [NSMutableArray array];
  for (DriveFilePickerItem* item in _secondaryItems) {
    [secondaryIdentifiers addObject:item.identifier];
  }
  [snapshot appendItemsWithIdentifiers:secondaryIdentifiers];

  _nextPageAvailable = nextPageAvailable;
  // Update the loading indicator.
  [_loadingIndicator stopAnimating];
  // Update the search header.
  _searchHeader.hidden = !showSearchHeader;
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:animated];
}

- (void)setEmailsMenu:(UIMenu*)emailsMenu {
  _accountButton.menu = emailsMenu;
}

- (void)setFetchedIcon:(UIImage*)iconImage
              forItems:(NSSet<NSString*>*)itemIdentifiers {
  NSMutableArray<NSString*>* itemsToReconfigure = [NSMutableArray array];
  for (DriveFilePickerItem* primaryItem in _primaryItems) {
    if ([itemIdentifiers containsObject:primaryItem.identifier]) {
      primaryItem.icon = iconImage;
      primaryItem.shouldFetchIcon = NO;
      [itemsToReconfigure addObject:primaryItem.identifier];
    }
  }
  for (DriveFilePickerItem* secondaryItem in _secondaryItems) {
    if ([itemIdentifiers containsObject:secondaryItem.identifier]) {
      secondaryItem.icon = iconImage;
      secondaryItem.shouldFetchIcon = NO;
      [itemsToReconfigure addObject:secondaryItem.identifier];
    }
  }
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:itemsToReconfigure];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)reconfigureItemsWithIdentifiers:(NSArray<NSString*>*)identifiers {
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:identifiers];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)setDownloadStatus:(DriveFileDownloadStatus)downloadStatus {
  _status = downloadStatus;
  self.navigationItem.rightBarButtonItem = [self configureRightBarButtonItem];
}

- (void)setEnabledItems:(NSSet<NSString*>*)identifiers {
  NSMutableArray<NSString*>* identifiersToReconfigure = [NSMutableArray array];
  for (DriveFilePickerItem* item in _primaryItems) {
    BOOL itemShouldBeEnabled = [identifiers containsObject:item.identifier];
    if (item.enabled == itemShouldBeEnabled) {
      continue;
    }
    item.enabled = itemShouldBeEnabled;
    [identifiersToReconfigure addObject:item.identifier];
  }
  for (DriveFilePickerItem* item in _secondaryItems) {
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
  BOOL filterSelected = filter != DriveFilePickerFilter::kShowAllFiles;
  NSString* symbol = filterSelected ? kSelectedFilterSymbol : kFilterSymbol;
  UIImage* filterIcon =
      DefaultSymbolTemplateWithPointSize(symbol, kSymbolAccessoryPointSize);
  _filterButton.image = filterIcon;
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

- (void)setSearchBarFocused:(BOOL)focused searchText:(NSString*)searchText {
  UISearchController* searchController = self.navigationItem.searchController;
  UISearchBar* searchBar = searchController.searchBar;
  if (searchController.active == focused) {
    if ([searchBar.text isEqualToString:searchText]) {
      return;
    }
    // Temporarily setting the search controller's search results updater to nil
    // while programmatically changing the search bar text.
    searchController.searchResultsUpdater = nil;
    searchBar.text = searchText;
    searchController.searchResultsUpdater = self;
    return;
  }
  // Temporarily setting the search controller's delegate and search results
  // updater to nil while programmatically changing its activation state.
  searchController.searchResultsUpdater = nil;
  searchController.delegate = nil;
  searchController.active = focused;
  searchController.searchResultsUpdater = self;
  searchController.delegate = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(SetSearchBarText, searchBar, searchText));
}

- (void)setCancelButtonVisible:(BOOL)visible {
  self.navigationItem.leftBarButtonItem = visible ? _cancelButton : nil;
  __weak __typeof(self) weakSelf = self;
  // If `backAction` is not nil then the "Back" button will be visible.
  self.navigationItem.backAction =
      visible ? nil : [UIAction actionWithHandler:^(UIAction* action) {
        [weakSelf.mutator hideSearchItemsOrBrowseBack];
      }];
}

- (void)setShouldFetchIcon:(BOOL)shouldFetchIcon
                  forItems:(NSSet<NSString*>*)itemIdentifiers {
  for (DriveFilePickerItem* primaryItem in _primaryItems) {
    if ([itemIdentifiers containsObject:primaryItem.identifier]) {
      primaryItem.shouldFetchIcon = shouldFetchIcon;
    }
  }
  for (DriveFilePickerItem* secondaryItem in _secondaryItems) {
    if ([itemIdentifiers containsObject:secondaryItem.identifier]) {
      secondaryItem.shouldFetchIcon = shouldFetchIcon;
    }
  }
}

- (void)showDownloadFailureAlertWithRetryBlock:(ProceduralBlock)retryBlock {
  UIAlertController* failureAlert = FailAlertController(retryBlock, nil);
  [self presentViewController:failureAlert animated:YES completion:nil];
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

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  // This is expected to be called when applying a snapshot to the data source.
  if (section == 0 && !_searchHeader.hidden) {
    return UITableViewAutomaticDimension;
  }
  return 0;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  DriveFilePickerItem* item =
      FindDriveFilePickerItem(itemIdentifier, _primaryItems, _secondaryItems);
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
  NSString* itemIdentifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  DriveFilePickerItem* item =
      FindDriveFilePickerItem(itemIdentifier, _primaryItems, _secondaryItems);
  CHECK(item);
  if (item.shouldFetchIcon) {
    [self.mutator fetchIconForDriveItem:itemIdentifier];
  }

  // If this is the last item and the next page is available, load it.
  NSDiffableDataSourceSnapshot* snapshot = _diffableDataSource.snapshot;
  if (indexPath.row == snapshot.numberOfItems - 1 && _nextPageAvailable) {
    [_loadingIndicator startAnimating];
    [self.mutator loadNextPage];
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  if (section == 0) {
    return _searchHeader;
  }
  return nil;
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:(UISearchController*)controller {
  [self.mutator setSearchText:controller.searchBar.text];
}

#pragma mark - UISearchControllerDelegate

- (void)willDismissSearchController:(UISearchController*)searchController {
  [self.mutator setSearchBarFocused:NO];
}

- (void)willPresentSearchController:(UISearchController*)searchController {
  [self.mutator setSearchBarFocused:YES];
}

@end
