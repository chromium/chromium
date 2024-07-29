// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_table_view_controller.h"

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
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

@implementation DriveFilePickerTableViewController

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

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

  // Add a "Confirm" button to confirm the selection.
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_CONFIRM)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(confirmSelection)];
  confirmButton.enabled = NO;
  self.navigationItem.rightBarButtonItem = confirmButton;

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

@end
