// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_android_switcher_screen_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_view_controller+subclassing.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"
#import "ios/chrome/browser/shared/ui/image/image_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the leading symbol icon in each feature row.
constexpr CGFloat kLeadingSymbolImagePointSize = 18.0;

// Leading inset for the separator between feature rows.
constexpr CGFloat kTableViewSeparatorLeadingInset = 60.0;

// Identifier for the single table view section.
NSString* const kMainSectionIdentifier = @"MainSectionIdentifier";

// Feature rows displayed on the Android Switcher Default Browser promo screen.
enum SwitcherFeatureItemType {
  kYouTube,
  kGoogleMaps,
  kDriveAndPhotos,
};

// Returns the localized title for `item_type`.
NSString* TitleForItemType(SwitcherFeatureItemType item_type) {
  switch (item_type) {
    case kYouTube:
      return l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_YOUTUBE_ROW);
    case kGoogleMaps:
      return l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_MAPS_ROW);
    case kDriveAndPhotos:
      return l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_DRIVE_PHOTOS_ROW);
  }
  NOTREACHED();
}

// Returns the leading symbol image for `item_type`.
UIImage* IconForItemType(SwitcherFeatureItemType item_type) {
  Symbol symbol;
  switch (item_type) {
    case kYouTube:
      symbol = SymbolExternalLink;
      break;
    case kGoogleMaps:
      symbol = SymbolLocation;
      break;
    case kDriveAndPhotos:
      symbol = SymbolShare;
      break;
  }
  return SymbolTemplateWithPointSize(symbol, kLeadingSymbolImagePointSize);
}

}  // namespace

@implementation DefaultBrowserAndroidSwitcherScreenViewController {
  // Diffable data source for the feature rows table view.
  UITableViewDiffableDataSource<NSString*, NSNumber*>* _dataSource;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = first_run::
      kFirstRunDefaultBrowserAndroidSwitcherScreenAccessibilityIdentifier;
}

#pragma mark - DefaultBrowserScreenViewController (Subclassing)

- (NSString*)bannerNameForPromo {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return kChromeDefaultBrowser1pBannerImage;
#else
  return kChromiumDefaultBrowser1pBannerImage;
#endif
}

- (NSString*)defaultSubtitleText {
  BOOL usesTablet = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  return l10n_util::GetNSString(
      usesTablet
          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SUBTITLE_IPAD
          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SUBTITLE);
}

- (UIView*)contentViewForPromo {
  SelfSizingTableView* tableView = [self createTableView];
  _dataSource = [self createAndFillDataSourceForTableView:tableView];
  return tableView;
}

#pragma mark - Private

// Creates and configures the table view displaying the switcher feature rows.
- (SelfSizingTableView*)createTableView {
  SelfSizingTableView* tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:ChromeTableViewStyle()];
  tableView.scrollEnabled = NO;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.userInteractionEnabled = NO;
  tableView.backgroundColor = [UIColor clearColor];
  tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorLeadingInset, 0, 0);
  tableView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
  tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  [TableViewCellContentConfiguration registerCellForTableView:tableView];
  return tableView;
}

// Creates and populates the diffable data source for `tableView`.
- (UITableViewDiffableDataSource<NSString*, NSNumber*>*)
    createAndFillDataSourceForTableView:(UITableView*)tableView {
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSource<NSString*, NSNumber*>* dataSource =
      [[UITableViewDiffableDataSource alloc]
          initWithTableView:tableView
               cellProvider:^UITableViewCell*(UITableView* cellTableView,
                                              NSIndexPath* indexPath,
                                              NSNumber* itemIdentifier) {
                 return [weakSelf cellForTableView:cellTableView
                                         indexPath:indexPath
                                    itemIdentifier:itemIdentifier];
               }];

  NSDiffableDataSourceSnapshot<NSString*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kMainSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(kYouTube),
    @(kGoogleMaps),
    @(kDriveAndPhotos),
  ]
             intoSectionWithIdentifier:kMainSectionIdentifier];
  [dataSource applySnapshot:snapshot animatingDifferences:NO];
  return dataSource;
}

// Returns a configured cell for `itemIdentifier` at `indexPath`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(NSNumber*)itemIdentifier {
  SwitcherFeatureItemType itemType =
      static_cast<SwitcherFeatureItemType>(itemIdentifier.integerValue);

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = TitleForItemType(itemType);
  configuration.titleColor = [UIColor colorNamed:kTextSecondaryColor];

  ColorfulSymbolContentConfiguration* symbolConfiguration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  symbolConfiguration.symbolImage = IconForItemType(itemType);
  symbolConfiguration.symbolBackgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  symbolConfiguration.symbolTintColor = [UIColor colorNamed:kBlueColor];
  configuration.leadingConfiguration = symbolConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView
                                                 forIndexPath:indexPath];
  cell.contentConfiguration = configuration;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  return cell;
}

@end
