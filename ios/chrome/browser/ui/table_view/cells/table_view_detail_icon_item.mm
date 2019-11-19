// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"

#include <algorithm>

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used between the icon and the text labels.
const CGFloat kIconTrailingPadding = 12;
// Size of the icon image.
const CGFloat kIconImageSize = 28;
// Proportion of Cell's textLabel/detailTextLabel. This guarantees that the
// textLabel occupies 75% of the row space and detailTextLabel occupies 25%.
const CGFloat kCellLabelsWidthProportion = 3.0f;

}  // namespace

@implementation TableViewDetailIconItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewDetailIconCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewDetailIconCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;

  // Update the icon image, if one is present.
  UIImage* iconImage = nil;
  if ([self.iconImageName length]) {
    iconImage = [UIImage imageNamed:self.iconImageName];
  }
  [cell setIconImage:iconImage];
}

@end

#pragma mark - TableViewDetailIconCell

@interface TableViewDetailIconCell ()

// When they are activated, the labels are on one line.
// They conflict with the accessibilityConstraints.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* standardConstraints;
// When they are activated, each label is on its own line, with no line number
// limit. They conflict with the standardConstraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;

@end

@implementation TableViewDetailIconCell {
  UIImageView* _iconImageView;
  UILayoutGuide* _labelContainerGuide;
  NSLayoutConstraint* _iconHiddenConstraint;
  NSLayoutConstraint* _iconVisibleConstraint;
}

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.hidden = YES;
    [contentView addSubview:_iconImageView];

    // Constrain the labels inside a container view, to make width computations
    // easier.
    _labelContainerGuide = [[UILayoutGuide alloc] init];
    [contentView addLayoutGuide:_labelContainerGuide];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.cr_labelColor;
    _textLabel.backgroundColor = UIColor.clearColor;
    [contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
    _detailTextLabel.backgroundColor = UIColor.clearColor;
    [contentView addSubview:_detailTextLabel];

    // Set up the constraints for when the icon is visible and hidden.  One of
    // these will be active at a time, defaulting to hidden.
    _iconHiddenConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    _iconVisibleConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kIconTrailingPadding];

    // Rules between |_textLabel| width and |_detailTextLabel| width:
    //   1. Widths are represented by the percentage according to the
    //      available space inside |_labelContainerGuide|;
    //   2. |_textLabel| has a width quota of 75%;
    //   3. |_detailTextLabe| has a width quota of 25%;
    //   4. When label A fits in A's quota, rest of A's quota can be used by
    //      the other label(i.e. B can exceed B's quota), and vice versa;
    //   5. When both labels exceed their quota, they occupy their quotas.
    //
    // Expected space allocation result:
    //   Rows are |_textLabel|'s quota.
    //   Columns are |_detailTextLabel|'s quota.
    //   Pairs are actual width for (|_textLabel|, |_detailTextLabel|).
    //
    //                20%             60%             90%
    //
    //   20%       (20%, 20%)      (20%, 60%)      (20%, 80%)
    //   60%       (60%, 20%)      (60%, 40%)      (60%, 30%)
    //   90%       (80%, 20%)      (75%, 25%)      (75%, 25%)
    //
    NSLayoutConstraint* widthConstraint = [_textLabel.widthAnchor
        constraintEqualToAnchor:_detailTextLabel.widthAnchor
                     multiplier:kCellLabelsWidthProportion];
    // Set low priority to the proportion constraint between |_textLabel| and
    // |_detailTextLabel|, so that it won't break other layouts.
    widthConstraint.priority = UILayoutPriorityDefaultLow;
    _standardConstraints = @[
      // Set up the vertical constraints and align the baselines of the two text
      // labels.
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_detailTextLabel.leadingAnchor
                                   constant:-kTableViewHorizontalSpacing],
      [_detailTextLabel.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:_labelContainerGuide.trailingAnchor],
      widthConstraint,
    ];

    _accessibilityConstraints = @[
      [_textLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_detailTextLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor
                         constant:-kTableViewLargeVerticalSpacing],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kTableViewHorizontalSpacing],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_detailTextLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_labelContainerGuide.trailingAnchor
                                   constant:-kTableViewHorizontalSpacing],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconImageSize],

      // Fix the edges of the text labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_labelContainerGuide.leadingAnchor],
      [_labelContainerGuide.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHiddenConstraint,
    ]];

    AddOptionalVerticalPadding(contentView, _textLabel,
                               kTableViewOneLabelCellVerticalSpacing);

    [self updateForAccessibilityContentSizeCategory:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

- (void)setIconImage:(UIImage*)image {
  BOOL hidden = (image == nil);
  if (hidden == _iconImageView.hidden) {
    return;
  }

  _iconImageView.image = image;
  _iconImageView.hidden = hidden;
  if (hidden) {
    _iconVisibleConstraint.active = NO;
    _iconHiddenConstraint.active = YES;
  } else {
    _iconHiddenConstraint.active = NO;
    _iconVisibleConstraint.active = YES;
  }
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  [self setIconImage:nil];
}

#pragma mark - Private

// Updates the cell such as it is layouted correctly with regard to the
// preferred content size category, if it is an
// |accessibilityContentSizeCategory| or not.
- (void)updateForAccessibilityContentSizeCategory:
    (BOOL)accessibilityContentSizeCategory {
  if (accessibilityContentSizeCategory) {
    [NSLayoutConstraint deactivateConstraints:_standardConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    // detailTextLabel is laid below textLabel with accessibility content size
    // category.
    _detailTextLabel.textAlignment = NSTextAlignmentNatural;
    _detailTextLabel.numberOfLines = 0;
    _textLabel.numberOfLines = 0;
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_standardConstraints];
    // detailTextLabel is laid after textLabel and should have a trailing text
    // alignment with non-accessibility content size category.
    _detailTextLabel.textAlignment =
        self.effectiveUserInterfaceLayoutDirection ==
                UIUserInterfaceLayoutDirectionLeftToRight
            ? NSTextAlignmentRight
            : NSTextAlignmentLeft;
    _detailTextLabel.numberOfLines = 1;
    _textLabel.numberOfLines = 1;
  }
}

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  return @[ self.textLabel.text ];
}

@end
