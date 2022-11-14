// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_image_container_view.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_menu_button.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCellContentHeight = 64.0;
const CGFloat kCellContentSpacing = 14;
}  // namespace

@implementation PriceNotificationsTableViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PriceNotificationsTableViewCell class];
  }
  return self;
}

- (void)configureCell:(PriceNotificationsTableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  tableCell.titleLabel.text = self.title;
  tableCell.URLLabel.text = self.entryURL;
  [tableCell setImage:self.productImage];
  [tableCell.priceNotificationsChip setPriceDrop:self.currentPrice
                                   previousPrice:self.previousPrice];
  tableCell.tracking = self.tracking;
  tableCell.accessibilityTraits |= UIAccessibilityTraitButton;
}

@end

#pragma mark - PriceNotificationsTableViewCell

@interface PriceNotificationsTableViewCell ()

// The imageview that is displayed on the leading edge of the cell.
@property(nonatomic, strong)
    PriceNotificationsImageContainerView* priceNotificationsImageContainerView;

@end

@implementation PriceNotificationsTableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel = [[UILabel alloc] init];
    _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _trackButton = [[PriceNotificationsTrackButton alloc] init];
    _menuButton = [[PriceNotificationsMenuButton alloc] init];
    _priceNotificationsChip = [[PriceNotificationsPriceChipView alloc] init];
    _priceNotificationsChip.translatesAutoresizingMaskIntoConstraints = NO;
    _priceNotificationsImageContainerView =
        [[PriceNotificationsImageContainerView alloc] init];
    _priceNotificationsImageContainerView
        .translatesAutoresizingMaskIntoConstraints = NO;

    // Use stack views to layout the subviews except for the Price Notification
    // Image.
    UIStackView* verticalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _titleLabel, _URLLabel, _priceNotificationsChip
        ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.distribution = UIStackViewDistributionEqualSpacing;
    verticalStack.alignment = UIStackViewAlignmentLeading;

    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, _trackButton, _menuButton ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionEqualSpacing;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_priceNotificationsImageContainerView];
    [self.contentView addSubview:horizontalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellContentHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    [NSLayoutConstraint activateConstraints:@[
      [self.priceNotificationsImageContainerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [self.priceNotificationsImageContainerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.priceNotificationsImageContainerView
                                      .trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kCellContentSpacing],
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [horizontalStack.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.bottomAnchor
                                      constant:-kCellContentSpacing],
      heightConstraint
    ]];
  }
  return self;
}

- (void)setImage:(UIImage*)productImage {
  [self.priceNotificationsImageContainerView setImage:productImage];
}

- (void)setTracking:(BOOL)tracking {
  if (tracking) {
    self.trackButton.hidden = YES;
    self.menuButton.hidden = NO;
    return;
  }

  self.trackButton.hidden = NO;
  self.menuButton.hidden = YES;
  _tracking = tracking;
}

@end
