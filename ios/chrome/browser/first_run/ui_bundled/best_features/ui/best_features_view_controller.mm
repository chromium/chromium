// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/not_fatal_until.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_cell.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFeature1 = kSectionIdentifierEnumZero,
  SectionIdentifierFeature2,
  SectionIdentifierFeature3,
};

// The size of the logo image.
const CGFloat kLogoSize = 45;
// The bottom margin of the header view.
const CGFloat kHeaderBottomMargin = 20;
// Spacing between table view sections.
const CGFloat kTableViewSectionHeaderTopPadding = 5;
const CGFloat kTableViewSectionHeaderHeight = 10;
const CGFloat kTableViewSectionFooterHeight = 3;

}  // namespace

@interface BestFeaturesViewController () <UITableViewDelegate>

@end

@implementation BestFeaturesViewController {
  // List of Best Feature items to display.
  NSArray<BestFeaturesItem*>* _bestFeaturesItems;
  // Table view displaying a Best Feature item in each row.
  UITableView* _tableView;
  // Height constraint for the table view.
  NSLayoutConstraint* _tableViewHeightConstraint;
  // Data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kBestFeaturesMainScreenAccessibilityIdentifier;

  _tableView = [self createTableView];
  _dataSource = [self createAndFillDataSource];
  _tableView.dataSource = _dataSource;

  [self.specificContentView addSubview:_tableView];
  AddSameConstraintsToSides(
      _tableView, self.specificContentView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  [NSLayoutConstraint activateConstraints:@[
    [self.specificContentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_tableView.bottomAnchor],
  ]];

  self.shouldHideBanner = YES;
  self.usePromoStyleBackground = YES;
  self.hideHeaderOnTallContent = YES;
  self.headerImageType = PromoStyleImageType::kImageWithShadow;
  self.headerViewForceStyleLight = YES;
  self.headerImageBottomMargin = kHeaderBottomMargin;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logo = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kLogoSize));
#else
  UIImage* logo = CustomSymbolWithPointSize(kChromeProductSymbol, kLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.headerImage = logo;

  self.titleText = l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_SUBTITLE);
  self.configuration.primaryActionString = [self primaryActionString];

  [super viewDidLoad];

  // Assign the table view's height anchors now that the content has been
  // loaded.
  _tableViewHeightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:_tableView.contentSize.height];
  _tableViewHeightConstraint.active = YES;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateTableViewHeightConstraint];
}

#pragma mark - BestFeaturesScreenConsumer

- (void)setBestFeaturesItems:(NSArray<BestFeaturesItem*>*)items {
  CHECK([items count] == 3);
  _bestFeaturesItems = items;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [_tableView deselectRowAtIndexPath:indexPath animated:NO];
  BestFeaturesItem* item = base::apple::ObjCCastStrict<BestFeaturesItem>(
      [_dataSource itemIdentifierForIndexPath:indexPath]);
  [self.bestFeaturesDelegate didTapBestFeaturesItem:item];
}

#pragma mark - Private

// Returns the primary action string based off the order of the screen in the
// FRE.
- (NSString*)primaryActionString {
  using enum first_run::BestFeaturesScreenVariationType;
  switch (first_run::GetBestFeaturesScreenVariationType()) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kShoppingUsersWithFallbackAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      return l10n_util::GetNSString(
          IDS_IOS_BEST_FEATURES_START_BROWSING_BUTTON);
    case kGeneralScreenBeforeDBPromo:
      return l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_CONTINUE_BUTTON);
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
    case kDisabled:
      NOTREACHED();
  }
}

// Creates the table view.
- (UITableView*)createTableView {
  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStyleInsetGrouped];
  tableView.sectionHeaderTopPadding = kTableViewSectionHeaderTopPadding;
  tableView.sectionHeaderHeight = kTableViewSectionHeaderHeight;
  tableView.sectionFooterHeight = kTableViewSectionFooterHeight;
  tableView.estimatedRowHeight = UITableViewAutomaticDimension;
  tableView.scrollEnabled = NO;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.delegate = self;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  tableView.backgroundColor = UIColor.clearColor;
  tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  RegisterTableViewCell<BestFeaturesCell>(tableView);

  // Remove extra space from UITableViewWrapperView.
  tableView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
  tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  return tableView;
}

// Creates the data source for the table view.
- (UITableViewDiffableDataSource<NSNumber*, NSNumber*>*)
    createAndFillDataSource {
  CHECK(_bestFeaturesItems);
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* dataSource =
      [[UITableViewDiffableDataSource alloc]
          initWithTableView:_tableView
               cellProvider:^UITableViewCell*(
                   UITableView* view, NSIndexPath* indexPath,
                   BestFeaturesItem* itemIdentifier) {
                 return [weakSelf cellForTableView:view
                                         indexPath:indexPath
                                    itemIdentifier:itemIdentifier];
               }];

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierFeature1), @(SectionIdentifierFeature2),
    @(SectionIdentifierFeature3)
  ]];
  [snapshot appendItemsWithIdentifiers:@[ _bestFeaturesItems[0] ]
             intoSectionWithIdentifier:@(SectionIdentifierFeature1)];
  [snapshot appendItemsWithIdentifiers:@[ _bestFeaturesItems[1] ]
             intoSectionWithIdentifier:@(SectionIdentifierFeature2)];
  [snapshot appendItemsWithIdentifiers:@[ _bestFeaturesItems[2] ]
             intoSectionWithIdentifier:@(SectionIdentifierFeature3)];
  [dataSource applySnapshot:snapshot animatingDifferences:NO];

  return dataSource;
}

// Configures the table view cells.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(BestFeaturesItem*)itemIdentifier {
  BestFeaturesCell* cell = DequeueTableViewCell<BestFeaturesCell>(tableView);
  [cell setBestFeaturesItem:itemIdentifier];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  return cell;
}

// Updates the tableView's height constraint.
- (void)updateTableViewHeightConstraint {
  [_tableView layoutIfNeeded];
  _tableViewHeightConstraint.constant = _tableView.contentSize.height;
}

@end
