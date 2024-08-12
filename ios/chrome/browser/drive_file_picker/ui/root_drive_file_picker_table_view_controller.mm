// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/root_drive_file_picker_table_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_item.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat kCellIconCornerRadius = 10;

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
  SectionIdentifierDriveMainFolders = kSectionIdentifierEnumZero,
  SectionIdentifierDriveSecondaryFolders,
};

}  // namespace

@interface RootDriveFilePickerTableViewController () <UITableViewDelegate>

@end

@implementation RootDriveFilePickerTableViewController {
  UITableViewDiffableDataSource<NSString*, DriveItem*>* _diffableDataSource;
}

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self configureToolbar];
  self.navigationController.toolbarHidden = NO;

  // If this is the root of the navigation controller, add a "Cancel" button and
  // the branded google drive title.
  if (self == self.navigationController.viewControllers.firstObject) {
    UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(cancelSelection)];
    self.navigationItem.leftBarButtonItem = cancelButton;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    self.navigationItem.titleView = CreateGoogleDriveImageView();
#else
    self.navigationItem.titleView = CreateGoogleDriveTitleLabel();
#endif
  }

  self.navigationItem.rightBarButtonItem = [self confirmButtonItem];

  // Add the search bar.
  self.navigationItem.searchController = [[UISearchController alloc] init];
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.navigationItem.preferredSearchBarPlacement =
      UINavigationItemSearchBarPlacementStacked;

  // Initialize the table view.
  self.tableView.delegate = self;
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  __weak __typeof(self) weakSelf = self;
  auto cellProvider = ^UITableViewCell*(UITableView* tableView,
                                        NSIndexPath* indexPath,
                                        DriveItem* itemIdentifier) {
    return [weakSelf cellForIndexPath:indexPath itemIdentifier:itemIdentifier];
  };

  // Initialize and load the data source.
  _diffableDataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];

  self.tableView.dataSource = _diffableDataSource;

  RegisterTableViewCell<TableViewDetailIconCell>(self.tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierDriveMainFolders) ]];
  [snapshot appendItemsWithIdentifiers:[self mainFoldersSectionItems]];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierDriveSecondaryFolders)
  ]];
  [snapshot appendItemsWithIdentifiers:[self secondaryFoldersSectionItems]];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - UI actions

- (void)cancelSelection {
  // TODO(crbug.com/344812987): If files are downloading, present an alert
  // instead of dismissing the view controller.
  __weak __typeof(self) weakSelf = self;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf
                                   .driveFilePickerHandler hideDriveFilePicker];
                         }];
}

- (void)confirmSelection {
  // TODO(crbug.com/344812396): Submit the file selection.
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DriveItem* driveItem =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  [self.mutator selectDriveItem:driveItem];
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
  UIBarButtonItem* sortButton =
      [[UIBarButtonItem alloc] initWithImage:sortIcon
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:nil];
  sortButton.enabled =
      self != self.navigationController.viewControllers.firstObject;

  // TODO(crbug.com/344812548): Add the menu and the title of the account button
  // based of the current identity.
  UIBarButtonItem* accountButton =
      [[UIBarButtonItem alloc] initWithTitle:@"dummyEmail@gmail.com" menu:nil];

  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[
    filterButton, spaceButton, accountButton, spaceButton, sortButton
  ]
               animated:NO];
}

// Returns a button with the title `Confirm`.
- (UIBarButtonItem*)confirmButtonItem {
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_CONFIRM)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(confirmSelection)];
  confirmButton.enabled = NO;
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

// Returns the items in the main folder section (My drive, shared drives and
// starred).
- (NSArray<DriveItem*>*)mainFoldersSectionItems {
  return @[
    [DriveItem myDriveItem], [DriveItem sharedDrivesItem],
    [DriveItem computersItem], [DriveItem starredItem]
  ];
}

// Returns the items in the secondary folder section (recent and shared drives).
- (NSArray<DriveItem*>*)secondaryFoldersSectionItems {
  return @[ [DriveItem recentItem], [DriveItem sharedWithMeItem] ];
}

// Deques and sets up a cell for a drive item.
- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(DriveItem*)itemIdentifier {
  TableViewDetailIconCell* cell =
      DequeueTableViewCell<TableViewDetailIconCell>(self.tableView);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;
  [cell.textLabel setText:itemIdentifier.title];
  [cell setDetailText:nil];
  [cell setIconImage:itemIdentifier.icon
            tintColor:[UIColor grayColor]
      backgroundColor:cell.backgroundColor
         cornerRadius:kCellIconCornerRadius];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return cell;
}

@end
