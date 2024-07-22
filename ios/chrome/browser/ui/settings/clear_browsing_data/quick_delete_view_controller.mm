// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mutator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/table_view_pop_up_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Trash icon view size.
constexpr CGFloat kTrashIconContainerViewSize = 64;

// Trash icon view corner radius.
constexpr CGFloat kTrashIconContainerViewCornerRadius = 15;

// Trash icon size that sits inside the entire view.
constexpr CGFloat kTrashIconSize = 32;

// Top padding for the trash icon view.
constexpr CGFloat kTrashIconContainerViewTopPadding = 33;

// Vertical padding for the title.
constexpr CGFloat kTitleVerticalPadding = 22;

// TableView's header and footer section heights.
constexpr CGFloat kSectionHeaderHeight = 10;
constexpr CGFloat kSectionFooterHeight = 0;

// TableView's corner radius size.
constexpr CGFloat kTableViewCornerRadius = 10;

// Section identifiers in Quick Delete's table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTimeRange = kSectionIdentifierEnumZero,
  SectionIdentifierBrowsingData,
  SectionIdentifierFooter,
};

// Item identifiers in Quick Delete's table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierTimeRange = kItemTypeEnumZero,
  ItemIdentifierBrowsingData,
};

}  // namespace

@interface QuickDeleteViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDelegate,
    TableViewLinkHeaderFooterItemDelegate> {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  UITableView* _tableView;
  browsing_data::TimePeriod _timeRange;
  NSString* _browsingDataSummary;
  BOOL _shouldShowFooter;
  NSLayoutConstraint* _tableViewHeightConstraint;
}
@end

@implementation QuickDeleteViewController

#pragma mark - UIViewController

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  _tableView = [self createTableView];
  _dataSource = [self createAndFillDataSource];
  _tableView.dataSource = _dataSource;

  self.view.accessibilityViewIsModal = YES;

  // Set the properties read by the super when constructing the views in
  // `-[ConfirmationAlertViewController viewDidLoad]`.
  self.aboveTitleView = [self trashIconView];
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.titleString = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  self.customSpacing = kTitleVerticalPadding;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_CANCEL);

  self.underTitleView = _tableView;

  self.showsVerticalScrollIndicator = NO;
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;
  self.customScrollViewBottomInsets = 0;
  self.actionHandler = self;

  [super viewDidLoad];

  [self displayGradientView:NO];

  // Configure the color of the primary button to red, as the default colour is
  // blue.
  UIButtonConfiguration* buttonConfiguration =
      self.primaryActionButton.configuration;
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kRedColor];
  self.primaryActionButton.configuration = buttonConfiguration;

  // Assign the table view's anchors now that it is in the same hierarchy as the
  // top view and that the content has been loaded.
  _tableViewHeightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:_tableView.contentSize.height];

  [NSLayoutConstraint activateConstraints:@[
    [_tableView.widthAnchor
        constraintEqualToAnchor:self.primaryActionButton.widthAnchor],
    _tableViewHeightConstraint
  ]];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  // Update the bottom sheet height since the browsing data row with the detail
  // text is bigger then the standard row height.
  [self updateBottomSheetHeight];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Update the bottomsheet height when trait collection changed (for example
  // when the user uses large font).
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self updateBottomSheetHeight];
  }
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_mutator triggerDeletion];
}

- (void)confirmationAlertSecondaryAction {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [_tableView deselectRowAtIndexPath:indexPath animated:NO];
  ItemIdentifier itemType = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);
  CHECK(itemType == ItemIdentifierBrowsingData) << itemType;
  [self.presentationHandler showBrowsingDataPage];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case SectionIdentifierFooter: {
      if (!_shouldShowFooter) {
        return nil;
      }
      TableViewLinkHeaderFooterView* footer =
          DequeueTableViewHeaderFooter<TableViewLinkHeaderFooterView>(
              _tableView);
      footer.accessibilityIdentifier = kQuickDeleteFooterIdentifier;
      footer.delegate = self;
      footer.urls = @[
        [[CrURL alloc]
            initWithGURL:GURL(kClearBrowsingDataDSESearchUrlInFooterURL)],
        [[CrURL alloc]
            initWithGURL:GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL)]
      ];
      [footer
            setText:l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_FOOTER)
          withColor:[UIColor colorNamed:kTextSecondaryColor]];
      return footer;
    }
    case SectionIdentifierTimeRange:
    case SectionIdentifierBrowsingData: {
      return nil;
    }
  }
  NOTREACHED_NORETURN();
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  if (sectionIdentifier == SectionIdentifierFooter && _shouldShowFooter) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)url {
  DCHECK(url.gurl == kClearBrowsingDataDSESearchUrlInFooterURL ||
         url.gurl == kClearBrowsingDataDSEMyActivityUrlInFooterURL);
  [self.presentationHandler openMyActivityURL:url.gurl];
}

#pragma mark - QuickDeleteConsumer

- (void)setTimeRange:(browsing_data::TimePeriod)timeRange {
  if (_timeRange == timeRange) {
    return;
  }
  _timeRange = timeRange;

  // Reload the time range row with the new value.
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ @(ItemIdentifierTimeRange) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO completion:nil];
}

- (void)setBrowsingDataSummary:(NSString*)summary {
  if ([_browsingDataSummary isEqualToString:summary]) {
    return;
  }
  _browsingDataSummary = summary;

  // Reload the browsing data row with the new summary.
  __weak __typeof(self) weakSelf = self;
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ @(ItemIdentifierBrowsingData) ]];
  [_dataSource applySnapshot:snapshot
        animatingDifferences:NO
                  completion:^{
                    // Update the bottom sheet height since the browsing data
                    // row can change height depending on the length of summary.
                    [weakSelf updateBottomSheetHeight];
                  }];
}

- (void)setShouldShowFooter:(BOOL)shouldShowFooter {
  if (_shouldShowFooter == shouldShowFooter) {
    return;
  }
  _shouldShowFooter = shouldShowFooter;
  // Reload the footer section.
  __weak __typeof(self) weakSelf = self;
  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [_dataSource snapshot];
  [snapshot reloadSectionsWithIdentifiers:@[ @(SectionIdentifierFooter) ]];
  [_dataSource applySnapshot:snapshot
        animatingDifferences:NO
                  completion:^{
                    // Update the bottom sheet height in case the footer is
                    // added or removed.
                    [weakSelf updateBottomSheetHeight];
                  }];
}

- (void)updateAutofillWithResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  // TODO(crbug.com/341107834): Refactor summary using this result.
}

- (void)setAutofillSelection:(BOOL)selected {
  // TODO(crbug.com/341107834): Refactor summary using this type selection.
}

#pragma mark - Private

// Updates the bottom sheet height by also updating the table view height. The
// table view might have a different height after the browsing data summary is
// updated.
- (void)updateBottomSheetHeight {
  // Trigger any pending layout updates.
  [self.view layoutIfNeeded];

  _tableViewHeightConstraint.constant = _tableView.contentSize.height;
  [self setUpBottomSheetDetents];
}

// Returns `_tableView` used to show the time range and browsing data rows.
- (UITableView*)createTableView {
  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:ChromeTableViewStyle()];
  tableView.layer.cornerRadius = kTableViewCornerRadius;
  tableView.sectionHeaderHeight = kSectionHeaderHeight;
  tableView.sectionFooterHeight = kSectionFooterHeight;
  tableView.scrollEnabled = NO;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.delegate = self;
  tableView.userInteractionEnabled = YES;

  tableView.translatesAutoresizingMaskIntoConstraints = NO;

  tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  tableView.backgroundColor = UIColor.clearColor;

  // Remove extra space from UITableViewWrapperView.
  tableView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
  tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  return tableView;
}

// Returns `_tableView`'s data source with the time range and browsing data rows
// in different sections.
- (UITableViewDiffableDataSource<NSNumber*, NSNumber*>*)
    createAndFillDataSource {
  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSource* dataSource =
      [[UITableViewDiffableDataSource alloc]
          initWithTableView:_tableView
               cellProvider:^UITableViewCell*(UITableView* tableView,
                                              NSIndexPath* indexPath,
                                              NSNumber* itemIdentifier) {
                 return [weakSelf
                     cellForTableView:tableView
                            indexPath:indexPath
                       itemIdentifier:static_cast<ItemIdentifier>(
                                          itemIdentifier.integerValue)];
               }];

  RegisterTableViewCell<TableViewPopUpCell>(_tableView);
  RegisterTableViewCell<TableViewDetailTextCell>(_tableView);
  RegisterTableViewHeaderFooter<TableViewLinkHeaderFooterView>(_tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierTimeRange), @(SectionIdentifierBrowsingData),
    @(SectionIdentifierFooter)
  ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierTimeRange) ]
             intoSectionWithIdentifier:@(SectionIdentifierTimeRange)];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierBrowsingData) ]
             intoSectionWithIdentifier:@(SectionIdentifierBrowsingData)];

  [dataSource applySnapshot:snapshot animatingDifferences:NO];

  return dataSource;
}

// Returns a cell for the specified `indexPath`.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierTimeRange: {
      TableViewPopUpCell* timeRangeCell =
          DequeueTableViewCell<TableViewPopUpCell>(tableView);
      [timeRangeCell setMenu:[self timeRangeMenu]];
      [timeRangeCell
          setTitle:l10n_util::GetNSString(
                       IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE)];
      timeRangeCell.userInteractionEnabled = YES;

      return timeRangeCell;
    }
    case ItemIdentifierBrowsingData: {
      TableViewDetailTextCell* browsingDataCell =
          DequeueTableViewCell<TableViewDetailTextCell>(tableView);
      browsingDataCell.textLabel.text =
          l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_TITLE);
      browsingDataCell.detailTextLabel.text = _browsingDataSummary;
      browsingDataCell.detailTextLabel.textColor =
          [UIColor colorNamed:kTextSecondaryColor];
      browsingDataCell.allowMultilineDetailText = YES;
      browsingDataCell.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
      browsingDataCell.userInteractionEnabled = YES;
      browsingDataCell.backgroundColor =
          [UIColor colorNamed:kSecondaryBackgroundColor];
      browsingDataCell.accessibilityIdentifier =
          kQuickDeleteBrowsingDataButtonIdentifier;
      return browsingDataCell;
    }
  }
  NOTREACHED_NORETURN();
}

// Returns a UIMenu with all supported time ranges for deletion.
- (UIMenu*)timeRangeMenu {
  return [UIMenu
      menuWithTitle:@""
              image:nil
         identifier:nil
            options:UIMenuOptionsSingleSelection
           children:@[
             [self timeRangeAction:browsing_data::TimePeriod::LAST_15_MINUTES],
             [self timeRangeAction:browsing_data::TimePeriod::LAST_HOUR],
             [self timeRangeAction:browsing_data::TimePeriod::LAST_DAY],
             [self timeRangeAction:browsing_data::TimePeriod::LAST_WEEK],
             [self timeRangeAction:browsing_data::TimePeriod::FOUR_WEEKS],
             [self timeRangeAction:browsing_data::TimePeriod::ALL_TIME],
           ]];
}

// Returns a UIAction for the specified `timeRange`.
- (UIAction*)timeRangeAction:(browsing_data::TimePeriod)timeRange {
  __weak __typeof(self) weakSelf = self;
  UIAction* action =
      [UIAction actionWithTitle:[self titleForTimeRange:timeRange]
                          image:nil
                     identifier:nil
                        handler:^(id ignored) {
                          [weakSelf handleAction:timeRange];
                        }];

  if (timeRange == _timeRange) {
    action.state = UIMenuElementStateOn;
  }
  return action;
}

// Handle invoked when a `timeRange` is selected.
- (void)handleAction:(browsing_data::TimePeriod)timeRange {
  _timeRange = timeRange;
  [_mutator timeRangeSelected:_timeRange];
}

// Returns the title string based on the `timeRange`.
- (NSString*)titleForTimeRange:(browsing_data::TimePeriod)timeRange {
  switch (timeRange) {
    case browsing_data::TimePeriod::LAST_15_MINUTES:
      return l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_15_MINUTES);
    case browsing_data::TimePeriod::LAST_HOUR:
      return l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR);
    case browsing_data::TimePeriod::LAST_DAY:
      return l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_DAY);
    case browsing_data::TimePeriod::LAST_WEEK:
      return l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK);
    case browsing_data::TimePeriod::FOUR_WEEKS:
      return l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS);
    case browsing_data::TimePeriod::ALL_TIME:
      return l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME);
    case browsing_data::TimePeriod::OLDER_THAN_30_DAYS:
      // This value should not be reached since it's not a part of the menu.
      break;
  }
  NOTREACHED_NORETURN();
}

// Returns a view of a trash icon with a red background with vertical padding.
- (UIView*)trashIconView {
  // Container of the trash icon that has the red background.
  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainerView.layer.cornerRadius = kTrashIconContainerViewCornerRadius;
  iconContainerView.backgroundColor = [UIColor colorNamed:kRed50Color];

  // Trash icon that inside the container with the red background.
  UIImageView* icon =
      [[UIImageView alloc] initWithImage:DefaultSymbolTemplateWithPointSize(
                                             kTrashSymbol, kTrashIconSize)];
  icon.clipsToBounds = YES;
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.tintColor = [UIColor colorNamed:kRedColor];
  [iconContainerView addSubview:icon];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainerView.widthAnchor
        constraintEqualToConstant:kTrashIconContainerViewSize],
    [iconContainerView.heightAnchor
        constraintEqualToConstant:kTrashIconContainerViewSize],
  ]];
  AddSameCenterConstraints(iconContainerView, icon);

  // Padding for the icon container view.
  UIView* outerView = [[UIView alloc] init];
  [outerView addSubview:iconContainerView];
  AddSameCenterXConstraint(outerView, iconContainerView);
  AddSameConstraintsToSidesWithInsets(
      iconContainerView, outerView, LayoutSides::kTop | LayoutSides::kBottom,
      NSDirectionalEdgeInsetsMake(kTrashIconContainerViewTopPadding, 0, 0, 0));

  return outerView;
}

@end
