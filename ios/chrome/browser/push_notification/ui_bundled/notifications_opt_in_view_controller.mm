// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/ui_bundled/notifications_opt_in_view_controller.h"

#import "ios/chrome/browser/push_notification/ui_bundled/push_notifications_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
enum SectionIdentifier {
  kNotificationOptions,
};
struct CellConfig {
  int title_id;
  int subtitle_id;
  NSString* accessibility_id;
  BOOL on;
};
// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;
// Table view separator inset.
CGFloat const kTableViewSeparatorInset = 16.0;
// Space above the title.
CGFloat const kSpaceAboveTitle = 20.0;
// Accessibility identifier.
NSString* const kNotificationsOptInScreenAxId = @"NotificationsOptInScreenAxId";
// Constant for the subtitleLabel's width anchor.
CGFloat const kSubtitleWidthConstant = 23.0;
// Title's horizontal margin.
CGFloat const kTitleHorizontalMargin = 25.0;

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

@interface NotificationsOptInViewController () <UITableViewDelegate>

@end

@implementation NotificationsOptInViewController {
  UITableView* _tableView;
  NSLayoutConstraint* _tableViewHeightConstraint;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  BOOL _contentNotificationsEnabled;
  BOOL _tipsNotificationsEnabled;
  BOOL _priceTrackingNotificationsEnabled;
  BOOL _safetyCheckNotificationsEnabled;
}

- (void)viewDidLoad {
  self.titleText = l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_SUBTITLE);
  self.titleHorizontalMargin = kTitleHorizontalMargin;
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_ENABLE_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_ALERT_CANCEL);
  self.titleTopMarginWhenNoHeaderImage = kSpaceAboveTitle;
  [self configureBanner];
  self.shouldBannerFillTopSpace = YES;
  self.view.accessibilityIdentifier = kNotificationsOptInScreenAxId;
  _tableView = [self createTableView];
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
  [self updatePrimaryButtonState];
  [super viewDidLoad];

  // Add subtitle constraint after it is added to hierarchy.
  [NSLayoutConstraint activateConstraints:@[
    [self.subtitleLabel.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor
                       constant:-kSubtitleWidthConstant],
  ]];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self configureBanner];
  [self updateTableViewHeightConstraint];
}

#pragma mark - PromoStyleViewController

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle2;
}

- (UILabel*)createSubtitleLabel {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCallout];
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitleLabel.text = self.subtitleText;
  subtitleLabel.textAlignment = NSTextAlignmentCenter;
  subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  return subtitleLabel;
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
                   itemIdentifier:static_cast<NotificationsOptInItemIdentifier>(
                                      itemIdentifier.integerValue)];
           }];

  [TableViewCellContentConfiguration registerCellForTableView:_tableView];

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifier::kNotificationOptions)
  ]];
  if ([self isContentNotificationEnabled]) {
    [snapshot appendItemsWithIdentifiers:@[
      @(NotificationsOptInItemIdentifier::kContent)
    ]];
  }
  [snapshot appendItemsWithIdentifiers:@[
    @(NotificationsOptInItemIdentifier::kTips),
    @(NotificationsOptInItemIdentifier::kPriceTracking)
  ]];
  if (IsSafetyCheckNotificationsEnabled()) {
    [snapshot appendItemsWithIdentifiers:@[
      @(NotificationsOptInItemIdentifier::kSafetyCheck),
    ]];
  }
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  cell.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
}

#pragma mark - NotificationsOptInConsumer

- (void)setOptInItem:(NotificationsOptInItemIdentifier)identifier
             enabled:(BOOL)enabled {
  [self updateStatusForItem:identifier enabled:enabled];
  NSDiffableDataSourceSnapshot* snapshot = [_dataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ @(identifier) ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];
}

#pragma mark - Private

// Updates the enabled status for an item.
- (void)updateStatusForItem:(NotificationsOptInItemIdentifier)identifier
                    enabled:(BOOL)enabled {
  switch (identifier) {
    case kContent:
      _contentNotificationsEnabled = enabled;
      break;
    case kTips:
      _tipsNotificationsEnabled = enabled;
      break;
    case kPriceTracking:
      _priceTrackingNotificationsEnabled = enabled;
      break;
    case kSafetyCheck:
      _safetyCheckNotificationsEnabled = enabled;
      break;
  }
}

// Creates the table view.
- (UITableView*)createTableView {
  UITableView* tableView =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStylePlain];
  tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.layer.cornerRadius = kTableViewCornerRadius;
  tableView.estimatedRowHeight = UITableViewAutomaticDimension;
  tableView.scrollEnabled = NO;
  tableView.showsVerticalScrollIndicator = NO;
  tableView.delegate = self;
  tableView.userInteractionEnabled = YES;
  tableView.translatesAutoresizingMaskIntoConstraints = NO;
  tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0);
  _tableViewHeightConstraint =
      [tableView.heightAnchor constraintEqualToConstant:0];
  _tableViewHeightConstraint.active = YES;

  return tableView;
}

// Returns the CellConfig for the given itemIdentifier.
- (CellConfig)configForItemIdentifier:
    (NotificationsOptInItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case kContent:
      return {IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_TOGGLE_TITLE,
              IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_FOOTER_TEXT,
              kNotificationsOptInContentAccessibilityID,
              _contentNotificationsEnabled};
    case kTips:
      return {IDS_IOS_MAGIC_STACK_TIP_TITLE,
              IDS_IOS_NOTIFICATIONS_OPT_IN_TIPS_SETTINGS_TOGGLE_MESSSAGE,
              kNotificationsOptInTipsAccessibilityID,
              _tipsNotificationsEnabled};
    case kPriceTracking:
      return {IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_TITLE,
              IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_MESSAGE,
              kNotificationsOptInPriceTrackingAccessibilityID,
              _priceTrackingNotificationsEnabled};
    case kSafetyCheck:
      return {IDS_IOS_SAFETY_CHECK_TITLE,
              IDS_IOS_SAFETY_CHECK_DESCRIPTION_DEFAULT,
              kNotificationsOptInSafetyCheckAccessibilityID,
              _safetyCheckNotificationsEnabled};
  }
}

// Configures the the table view cells.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:
                          (NotificationsOptInItemIdentifier)itemIdentifier {
  CellConfig config = [self configForItemIdentifier:itemIdentifier];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = l10n_util::GetNSString(config.title_id);
  configuration.subtitle = l10n_util::GetNSString(config.subtitle_id);

  SwitchContentConfiguration* switchConfiguration =
      [[SwitchContentConfiguration alloc] init];
  switchConfiguration.target = self;
  switchConfiguration.selector = @selector(switchToggled:);
  switchConfiguration.on = config.on;
  switchConfiguration.tag = itemIdentifier;

  configuration.trailingConfiguration = switchConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier = config.accessibility_id;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  return cell;
}

// Invoked when a notification opt-in switch is toggled.
- (void)switchToggled:(UISwitch*)sender {
  NotificationsOptInItemIdentifier identifier =
      static_cast<NotificationsOptInItemIdentifier>(sender.tag);
  [self.notificationsDelegate selectionChangedForItemType:identifier
                                                 selected:sender.on];
  [self updateStatusForItem:identifier enabled:sender.on];
  [self updatePrimaryButtonState];
}

// Updates the tableView's height constraint.
- (void)updateTableViewHeightConstraint {
  [_tableView layoutIfNeeded];
  _tableViewHeightConstraint.constant = _tableView.contentSize.height;
}

// Enables the primary action button if any one of the toggles are on. Disables
// otherwise.
- (void)updatePrimaryButtonState {
  self.configuration.primaryActionEnabled =
      _contentNotificationsEnabled || _tipsNotificationsEnabled ||
      _priceTrackingNotificationsEnabled || _safetyCheckNotificationsEnabled;
  [self reloadConfiguration];
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
}

@end
