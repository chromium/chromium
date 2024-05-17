// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_mutator.h"
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

// Bottom padding for the trash icon view.
constexpr CGFloat kTrashIconContainerViewBottomPadding = 18;

// Top padding for the trash icon view.
constexpr CGFloat kTrashIconContainerViewTopPadding = 33;

// Section identifiers in Quick Delete's table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTimeRange,
};

// Item identifiers in Quick Delete's table view.
typedef NS_ENUM(NSInteger, ItemIdentifier) {
  ItemIdentifierTimeRange,
};

}  // namespace

@interface QuickDeleteViewController () <ConfirmationAlertActionHandler> {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  UITableView* _tableView;
  browsing_data::TimePeriod _timeRange;
}
@end

@implementation QuickDeleteViewController

#pragma mark - UIViewController

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  // TODO(crbug.com/335387869): Add browsing data row.

  self.aboveTitleView = [self trashIconView];
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.titleString = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  self.primaryActionString = l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DELETE_BROWSING_DATA_CANCEL);

  self.actionHandler = self;

  [super viewDidLoad];

  UIButtonConfiguration* buttonConfiguration =
      self.primaryActionButton.configuration;
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kRedColor];
  self.primaryActionButton.configuration = buttonConfiguration;

  // TODO(crbug.com/340793372): The parent of this class,
  // TableViewBottomSheetViewController, has a customisation that selects the
  // first row by default. We should be able to disable this behaviour instead
  // of deselecting the row here..
  [_tableView deselectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]
                            animated:NO];
}

#pragma mark - TableViewBottomSheetViewController

- (UITableView*)createTableView {
  _tableView = [super createTableView];
  _tableView.separatorStyle = UITableViewCellSeparatorStyleNone;

  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:_tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  RegisterTableViewCell<TableViewPopUpCell>(_tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(SectionIdentifierTimeRange) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(ItemIdentifierTimeRange) ]
             intoSectionWithIdentifier:@(SectionIdentifierTimeRange)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
  _tableView.dataSource = _dataSource;

  return _tableView;
}

- (NSUInteger)rowCount {
  // TODO(crbug.com/335387869): Add row for the browsing data.
  return 1;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  UITableViewCell* cell = [[UITableViewCell alloc] init];
  // Setup UI as a real cell for an accurate height calculation.
  CGFloat tableWidth = [self tableViewWidth];
  // TODO(crbug.com/335387869): Add case for the browsing data row.
  cell = [self
      cellForTableView:_tableView
             indexPath:[NSIndexPath indexPathForRow:ItemIdentifierTimeRange
                                          inSection:SectionIdentifierTimeRange]
        itemIdentifier:ItemIdentifierTimeRange];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)].height;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/335387869): Trigger deletion.
}

- (void)confirmationAlertSecondaryAction {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  CHECK(self.presentationHandler);
  [self.presentationHandler dismissQuickDelete];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [_tableView deselectRowAtIndexPath:indexPath animated:NO];

  // TODO(crbug.com/335387869): Deal with tap on Browing Data row.
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  // no-op, overriding parent method since we do not want to add a checkmark
  // when the user selects a row.
  // TODO(crbug.com/340793372): Remove this method when customizing
  // TableViewBottomSheetViewController to allow disabling showing checkmarks
  // when a row is taped.
}

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  // no-op, overriding parent method since we want selection to be set on cell
  // construction.
  // TODO(crbug.com/340793372): Remove this method when customizing
  // TableViewBottomSheetViewController to allow disabling showing checkmarks
  // when a row is taped.
}

#pragma mark - BrowsingDataConsumer

- (void)setTimeRange:(browsing_data::TimePeriod)timeRange {
  _timeRange = timeRange;
}

#pragma mark - Private

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
    case browsing_data::TimePeriod::LAST_15_MINUTES:
      // Those values should not be reached since they're not a part of the
      // menu.
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
      NSDirectionalEdgeInsetsMake(kTrashIconContainerViewTopPadding, 0,
                                  kTrashIconContainerViewBottomPadding, 0));

  return outerView;
}

@end
