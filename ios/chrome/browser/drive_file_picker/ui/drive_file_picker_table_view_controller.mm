// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

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

}  // namespace

@implementation DriveFilePickerTableViewController {
  // The status of file dowload.
  DriveFileDownloadStatus _status;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _status = DriveFileDownloadStatus::kNotStarted;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self configureToolbar];

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
  // TODO(crbug.com/344812548): Add a data source to the table view.
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

@end
