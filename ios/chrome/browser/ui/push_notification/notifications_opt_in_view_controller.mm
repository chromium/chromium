// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_view_controller.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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
  BOOL on;
  BOOL show_separator;
};
// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;
// Table view separator inset.
CGFloat const kTableViewSeparatorInset = 16.0;
// Table view separator inset to use to hide the separator.
CGFloat const kTableViewSeparatorInsetHide = 10000;
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
  UISwitch* _contentToggle;
  UISwitch* _tipsToggle;
  UISwitch* _priceTrackingToggle;
  BOOL _contentNotificationsEnabled;
  BOOL _tipsNotificationsEnabled;
  BOOL _priceTrackingNotificationsEnabled;
}

- (void)viewDidLoad {
  self.titleText = l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_SUBTITLE);
  self.titleHorizontalMargin = kTitleHorizontalMargin;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_NOTIFICATIONS_OPT_IN_ENABLE_BUTTON);
  self.secondaryActionString =
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
  [self setPrimaryButtonConfiguration];
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

  RegisterTableViewCell<TableViewSwitchCell>(_tableView);

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
  switch (identifier) {
    case kContent:
      if (_contentToggle) {
        _contentToggle.on = enabled;
      }
      _contentNotificationsEnabled = enabled;
      break;
    case kTips:
      if (_tipsToggle) {
        _tipsToggle.on = enabled;
      }
      _tipsNotificationsEnabled = enabled;
      break;
    case kPriceTracking:
      if (_priceTrackingToggle) {
        _priceTrackingToggle.on = enabled;
      }
      _priceTrackingNotificationsEnabled = enabled;
      break;
  }
}

#pragma mark - Private

// Creates the table view.
- (UITableView*)createTableView {
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

// Returns the CellConfig for the given itemIdentifier.
- (CellConfig)configForItemIdentifier:
    (NotificationsOptInItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case kContent:
      return {IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_TOGGLE_TITLE,
              IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_FOOTER_TEXT,
              _contentNotificationsEnabled, YES};
    case kTips:
      return {IDS_IOS_SET_UP_LIST_TIPS_TITLE,
              IDS_IOS_NOTIFICATIONS_OPT_IN_TIPS_SETTINGS_TOGGLE_MESSSAGE,
              _tipsNotificationsEnabled, YES};
    case kPriceTracking:
      return {IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_TITLE,
              IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_MESSAGE,
              _priceTrackingNotificationsEnabled, YES};
  }
}

// Configures the the table view cells.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:
                          (NotificationsOptInItemIdentifier)itemIdentifier {
  TableViewSwitchCell* cell =
      DequeueTableViewCell<TableViewSwitchCell>(tableView);

  CellConfig config = [self configForItemIdentifier:itemIdentifier];
  [cell configureCellWithTitle:l10n_util::GetNSString(config.title_id)
                      subtitle:l10n_util::GetNSString(config.subtitle_id)
                 switchEnabled:YES
                            on:config.on];

  switch (itemIdentifier) {
    case kContent:
      _contentToggle = cell.switchView;
      break;
    case kTips:
      _tipsToggle = cell.switchView;
      break;
    case kPriceTracking:
      _priceTrackingToggle = cell.switchView;
      break;
  }
  cell.switchView.tag = itemIdentifier;

  // Make the separator invisible on the last row.
  BOOL lastRow =
      indexPath.row == [tableView numberOfRowsInSection:indexPath.section] - 1;
  CGFloat separatorInset =
      lastRow ? kTableViewSeparatorInsetHide : kTableViewSeparatorInset;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorInset, 0.f, 0.f);

  [cell.switchView addTarget:self
                      action:@selector(switchToggled:)
            forControlEvents:UIControlEventValueChanged];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  return cell;
}

// Invoked when a notification opt-in switch is toggled.
- (void)switchToggled:(UISwitch*)sender {
  [self.notificationsDelegate
      selectionChangedForItemType:static_cast<NotificationsOptInItemIdentifier>(
                                      sender.tag)
                         selected:sender.on];
  [self updatePrimaryButtonState];
}

// Updates the tableView's height constraint.
- (void)updateTableViewHeightConstraint {
  [_tableView layoutIfNeeded];
  _tableViewHeightConstraint.constant = _tableView.contentSize.height;
}

// Sets the configurationUpdateHandler for the primaryActionButton to handle the
// button's state changes. The button is blue when enabled, and grayed out when
// disabled.
- (void)setPrimaryButtonConfiguration {
  self.updateHandler = ^(UIButton* incomingButton) {
    UIButtonConfiguration* updatedConfig = incomingButton.configuration;
    switch (incomingButton.state) {
      case UIControlStateDisabled: {
        updatedConfig.background.backgroundColor =
            [UIColor colorNamed:kUpdatedTertiaryBackgroundColor];
        updatedConfig.baseForegroundColor =
            [UIColor colorNamed:kTextTertiaryColor];
        break;
      }
      case UIControlStateNormal: {
        updatedConfig.background.backgroundColor =
            [UIColor colorNamed:kBlueColor];
        updatedConfig.baseForegroundColor =
            [UIColor colorNamed:kBackgroundColor];
        break;
      }
      default:
        break;
    }
    incomingButton.configuration = updatedConfig;
  };
}

// Enables the primary action button if any one of the toggles are on. Disables
// otherwise.
- (void)updatePrimaryButtonState {
  self.primaryButtonEnabled =
      _contentToggle.isOn || _tipsToggle.isOn || _priceTrackingToggle.isOn;
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
