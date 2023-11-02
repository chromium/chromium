// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_track_button.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
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
  tableCell.tracking = self.tracking;
  tableCell.accessibilityTraits |= UIAccessibilityTraitButton;
}

@end

#pragma mark - PriceNotificationsTableViewCell

@interface PriceNotificationsTableViewCell ()
@end

@implementation PriceNotificationsTableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    // TODO(crbug.com/1368700) Once the PriceNotificationImageContainerView,
    // PriceNotificationsPriceChipView, and PriceNotificationsMenuItem is
    // created, instatiate instances of each class and extend the TableViewCell
    // to stores these instances as properties. In addition, this class will
    // need to be adapted to integrate the UI elements in to the table cell.

    _titleLabel = [[UILabel alloc] init];
    _URLLabel = [[UILabel alloc] init];
    _trackButton = [[PriceNotificationsTrackButton alloc] init];

    _titleLabel.font =
        [self preferredFontWithTextStyle:UIFontTextStyleSubheadline
                                  weight:UIFontWeightSemibold];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ self.titleLabel, self.URLLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.distribution = UIStackViewDistributionEqualSpacing;
    verticalStack.alignment = UIStackViewAlignmentLeading;

    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          verticalStack,
          self.trackButton,
        ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionEqualSpacing;
    horizontalStack.alignment = UIStackViewAlignmentCenter;

    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:horizontalStack];

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellContentHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    [NSLayoutConstraint activateConstraints:@[
      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kCellContentSpacing],
      [horizontalStack.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.bottomAnchor
                                      constant:-kCellContentSpacing],
      heightConstraint
    ]];
  }
  return self;
}

- (void)setTracking:(BOOL)tracking {
  if (tracking) {
    self.trackButton.hidden = YES;
    return;
  }

  self.trackButton.hidden = NO;
  _tracking = tracking;
}

#pragma mark - Helpers

// Creates a dynamically scablable custom font based on the given parameters.
- (UIFont*)preferredFontWithTextStyle:(UIFontTextStyle)style
                               weight:(UIFontWeight)weight {
  UIFontMetrics* fontMetrics = [[UIFontMetrics alloc] initForTextStyle:style];
  UIFontDescriptor* fontDescriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:style];
  UIFont* font = [UIFont systemFontOfSize:fontDescriptor.pointSize
                                   weight:weight];
  return [fontMetrics scaledFontForFont:font];
}

@end
