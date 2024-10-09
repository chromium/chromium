// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_banner_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_item_identifier.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
enum SectionIdentifier {
  kNotificationOptions,
};
// Table view separator inset.
CGFloat const kTableViewSeparatorInset = 16.0;
// Table view separator inset to use to hide the separator.
CGFloat const kTableViewSeparatorInsetHide = 10000;
// Title's horizontal margin.
CGFloat const kTitleHorizontalMargin = 25.0;
// Constant for the content's width anchor.
CGFloat const kContentWidthConstant = 23.0;
//  Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;
// Space above the title.
CGFloat const kSpaceAboveTitle = 40.0;

// Returns the name of the banner image above the title.
NSString* BannerImageName(bool landscape) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return landscape ? kChromeNotificationsOptInBannerLandscapeImage
                   : kChromeNotificationsOptInBannerImage;
#else
  return landscape ? kChromiumNotificationsOptInBannerLandscapeImage
                   : kChromiumNotificationsOptInBannerImage;
#endif
}

// Returns true if the view is too narrow to show the banner.
bool TooNarrowForBanner(UIView* view) {
  CGFloat minWidth =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET ? 450 : 300;
  return view.bounds.size.width < minWidth;
}

}  // namespace

@interface NotificationsBannerViewController () <UITableViewDelegate>
// All the items for the price notifications section received by mediator.
@property(nonatomic, strong) TableViewItem* priceTrackingItem;
// All the items for the content notifications section received by mediator.
@property(nonatomic, strong) TableViewItem* contentNotificationsItem;
// All the items for the tips notifications section received by mediator.
@property(nonatomic, strong) TableViewSwitchItem* tipsNotificationsItem;
// All the items for the Safety Check notifications section received by
// mediator.
@property(nonatomic, strong) TableViewSwitchItem* safetyCheckItem;
@property(nonatomic, strong)
    TableViewHeaderFooterItem* tipsNotificationsFooterItem;
// All the items for the send tab notifications section received by mediator.
@property(nonatomic, strong) TableViewSwitchItem* sendTabNotificationsItem;

@end

@implementation NotificationsBannerViewController {
  UITableView* _tableView;
  NSLayoutConstraint* _tableViewHeightConstraint;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  NSDiffableDataSourceSnapshot* _snapshot;
  ChromeTableViewStyler* _tableViewStyler;
  // The `viewWillLayoutSubviews` is invoked on creation, dismissal, and
  // backward navigation of the NotificationsBannerViewController. To prevent
  // the view controller styling aspects of the view that will be carried over
  // to other views, `_viewControllerIsBeingDismissed` ivar is set to true when
  // the view controller's `viewWillDisappear` and prevents the view controller
  // from further configuring the view once this occurs.
  bool _viewControllerIsBeingDismissed;
}

- (void)viewDidLoad {
  self.titleText = l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_TITLE);
  self.primaryActionString = @" ";
  self.actionButtonsVisibility = ActionButtonsVisibility::kHidden;
  self.titleHorizontalMargin = kTitleHorizontalMargin;
  self.titleTopMarginWhenNoHeaderImage = kSpaceAboveTitle;
  [self configureBanner];
  self.shouldBannerFillTopSpace = YES;
  self.layoutBehindNavigationBar = YES;
  self.view.accessibilityIdentifier = kNotificationsBannerTableViewId;
  _tableView = [self tableView];
  _viewControllerIsBeingDismissed = NO;
  [self.specificContentView addSubview:_tableView];
  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [_tableView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [_tableView.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],
    [_tableView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];
  [self loadModel];

  if (self.navigationController &&
      [self.navigationController
          isKindOfClass:[SettingsNavigationController class]]) {
    UIBarButtonItem* doneButton =
        [(SettingsNavigationController*)self.navigationController doneButton];
    self.navigationItem.rightBarButtonItem = doneButton;
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
  }

  [super viewDidLoad];

  // Add constraint after it is added to hierarchy.
  [NSLayoutConstraint activateConstraints:@[
    [self.subtitleLabel.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor
                       constant:-kContentWidthConstant],
  ]];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  if (_viewControllerIsBeingDismissed) {
    // The ivar is reset to handle the case where the user navigates back to
    // this view controller via the 'back' navigation item.
    _viewControllerIsBeingDismissed = NO;
    return;
  }
  [self configureBanner];
  [self updateTableViewHeightConstraint];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.navigationController.navigationBar.tintColor = nil;
  _viewControllerIsBeingDismissed = YES;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return YES;
}

#pragma mark - NotificationsConsumer

- (void)reloadData {
  [self loadModel];
  [_tableView reloadData];
}

- (void)reconfigureCellsForItems:(NSArray*)items {
  for (TableViewItem* item in items) {
    NotificationsItemIdentifier itemIdentifier =
        static_cast<NotificationsItemIdentifier>(item.type);
    if ([self.snapshot indexOfItemIdentifier:@(itemIdentifier)] != NSNotFound) {
      NSIndexPath* indexPath =
          [_dataSource indexPathForItemIdentifier:@(itemIdentifier)];
      UITableViewCell* cell = [_tableView cellForRowAtIndexPath:indexPath];
      // `cell` may be nil if the row is not currently on screen.
      if (cell) {
        TableViewCell* tableViewCell =
            base::apple::ObjCCastStrict<TableViewCell>(cell);
        [item configureCell:tableViewCell withStyler:[self tableViewStyler]];
      }
    }
  }
}

- (void)reloadCellsForItems:(NSArray*)items
           withRowAnimation:(UITableViewRowAnimation)rowAnimation {
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePriceNotificationsSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePriceNotificationsSettingsBack"));
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:_tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return [weakSelf
                 cellForTableView:tableView
                        indexPath:indexPath
                   itemIdentifier:static_cast<NotificationsItemIdentifier>(
                                      itemIdentifier.integerValue)];
           }];

  RegisterTableViewCell<TableViewSwitchCell>(_tableView);
  RegisterTableViewCell<TableViewMultiDetailTextCell>(_tableView);

  [_dataSource applySnapshot:self.snapshot animatingDifferences:NO];
}

- (NSDiffableDataSourceSnapshot*)snapshot {
  if (!_snapshot) {
    _snapshot = [[NSDiffableDataSourceSnapshot alloc] init];
    [_snapshot appendSectionsWithIdentifiers:@[
      @(SectionIdentifier::kNotificationOptions)
    ]];
    if ([self isContentNotificationEnabled]) {
      [_snapshot appendItemsWithIdentifiers:@[
        @(NotificationsItemIdentifier::ItemIdentifierContent)
      ]];
    }
    if (base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
      [_snapshot appendItemsWithIdentifiers:@[
        @(NotificationsItemIdentifier::ItemIdentifierSendTab)
      ]];
    }
    if (IsIOSTipsNotificationsEnabled()) {
      [_snapshot appendItemsWithIdentifiers:@[
        @(NotificationsItemIdentifier::ItemIdentifierTips)
      ]];
    }
    [_snapshot appendItemsWithIdentifiers:@[
      @(NotificationsItemIdentifier::ItemIdentifierPriceTracking)
    ]];
    if (IsSafetyCheckNotificationsEnabled()) {
      [_snapshot appendItemsWithIdentifiers:@[
        @(NotificationsItemIdentifier::ItemIdentifierSafetyCheck)
      ]];
    }
  }
  return _snapshot;
}

#pragma mark - PromoStyleViewController

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle2;
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate notificationsBannerViewControllerDidRemove:self];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NotificationsItemIdentifier selectedItem =
      static_cast<NotificationsItemIdentifier>(
          [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);
  TableViewItem* tableItem = [self tableItemForItemIdentifier:selectedItem];
  [self.modelDelegate didSelectItem:tableItem];
}

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  cell.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
}

#pragma mark - Private

// Creates the table view.
- (UITableView*)tableView {
  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStylePlain];
  tableView.layer.cornerRadius = kTableViewCornerRadius;
  tableView.estimatedRowHeight = UITableViewAutomaticDimension;
  tableView.scrollEnabled = NO;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.delegate = self;
  tableView.userInteractionEnabled = YES;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  tableView.separatorInset = UIEdgeInsetsZero;
  _tableViewHeightConstraint =
      [tableView.heightAnchor constraintEqualToConstant:0];
  _tableViewHeightConstraint.active = YES;

  return tableView;
}

// Returns the TableViewItem for the given itemIdentifier.
- (TableViewItem*)tableItemForItemIdentifier:
    (NotificationsItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierContent:
      return self.contentNotificationsItem;
    case ItemIdentifierTips:
      return self.tipsNotificationsItem;
    case ItemIdentifierPriceTracking:
      return self.priceTrackingItem;
    case ItemIdentifierSafetyCheck:
      return self.safetyCheckItem;
    case ItemIdentifierSendTab:
      return self.sendTabNotificationsItem;
    case ItemIdentifierTipsNotificationsFooter:
      NOTREACHED();
  }
}

// Configures the the table view cells.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:
                          (NotificationsItemIdentifier)itemIdentifier {
  TableViewItem* item = [self tableItemForItemIdentifier:itemIdentifier];
  UITableViewCell* cell;
  if ([item isKindOfClass:[TableViewSwitchItem class]]) {
    cell = [self switchCellForTableView:tableView
                                   item:item
                         itemIdentifier:itemIdentifier];
  } else {
    cell = [self detailCellForTableView:tableView item:item];
  }
  // Make the separator invisible on the last row.
  BOOL lastRow =
      indexPath.row == [tableView numberOfRowsInSection:indexPath.section] - 1;
  CGFloat separatorInset =
      lastRow ? kTableViewSeparatorInsetHide : kTableViewSeparatorInset;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorInset, 0.f, 0.f);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  return cell;
}

// Returns the corresponding switch cell for the given TableViewSwitchItem
// `item`.
- (UITableViewCell*)switchCellForTableView:(UITableView*)tableView
                                      item:(TableViewItem*)item
                            itemIdentifier:
                                (NotificationsItemIdentifier)itemIdentifier {
  TableViewSwitchCell* cell =
      DequeueTableViewCell<TableViewSwitchCell>(tableView);
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(item);
  [switchItem configureCell:cell withStyler:[self tableViewStyler]];
  cell.switchView.tag = itemIdentifier;
  [cell.switchView addTarget:self
                      action:@selector(switchAction:)
            forControlEvents:UIControlEventValueChanged];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  return cell;
}

// Returns the corresponding detail cell for the given TableViewDetailIconItem
// `item`.
- (UITableViewCell*)detailCellForTableView:(UITableView*)tableView
                                      item:(TableViewItem*)item {
  TableViewMultiDetailTextCell* cell =
      DequeueTableViewCell<TableViewMultiDetailTextCell>(tableView);
  TableViewMultiDetailTextItem* detailItem =
      base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(item);
  [detailItem configureCell:cell withStyler:[self tableViewStyler]];
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;
  cell.accessibilityIdentifier = detailItem.accessibilityIdentifier;
  return cell;
}

// Called when switch is toggled.
- (void)switchAction:(UISwitch*)sender {
  TableViewItem* item =
      [self tableItemForItemIdentifier:static_cast<NotificationsItemIdentifier>(
                                           sender.tag)];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(item);
  DCHECK(switchItem);
  [self.modelDelegate didToggleSwitchItem:switchItem withValue:sender.isOn];
}

// Updates the tableView's height constraint.
- (void)updateTableViewHeightConstraint {
  [_tableView layoutIfNeeded];
  _tableViewHeightConstraint.constant = _tableView.contentSize.height;
}

// ChromeTableViewStyler for the cells.
- (ChromeTableViewStyler*)tableViewStyler {
  if (!_tableViewStyler) {
    _tableViewStyler = [[ChromeTableViewStyler alloc] init];
    _tableViewStyler.cellBackgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
  }
  return _tableViewStyler;
}

// Configures the banner based on the view's size.
- (void)configureBanner {
  if (IsCompactHeight(self.traitCollection) || TooNarrowForBanner(self.view)) {
    self.bannerName = nil;
    self.shouldHideBanner = YES;
  } else if (IsCompactWidth(self.traitCollection)) {
    self.bannerSize = BannerImageSizeType::kShort;
    self.bannerName = BannerImageName(false);
    self.shouldHideBanner = NO;
  } else {
    // iPad, full window.
    self.bannerSize = BannerImageSizeType::kStandard;
    self.bannerName = BannerImageName(true);
    self.shouldHideBanner = NO;
  }
  self.navigationController.navigationBar.tintColor =
      self.shouldHideBanner ? nil : UIColor.whiteColor;
}

@end
