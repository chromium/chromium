// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_detail_item.h"

#include <algorithm>

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell and between the
// two labels.
const CGFloat kHorizontalPadding = 16;

// Padding used between the icon and the text labels.
const CGFloat kIconTrailingPadding = 12;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Size of the icon image.
const CGFloat kIconImageSize = 28;

// Minimum proportion of the available width to guarantee to the main and detail
// labels.
const CGFloat kMinTextWidthRatio = 0.75f;
const CGFloat kMinDetailTextWidthRatio = 0.25f;
}  // namespace

@implementation SettingsDetailItem

@synthesize accessoryType = _accessoryType;
@synthesize iconImageName = _iconImageName;
@synthesize text = _text;
@synthesize detailText = _detailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsDetailCell class];
    _cellBackgroundColor = [UIColor whiteColor];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(SettingsDetailCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.accessoryType = self.accessoryType;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.backgroundColor = self.cellBackgroundColor;

  // Update the icon image, if one is present.
  UIImage* iconImage = nil;
  if ([self.iconImageName length]) {
    iconImage = [UIImage imageNamed:self.iconImageName];
  }
  [cell setIconImage:iconImage];
}

@end

#pragma mark - SettingsDetailCell

@interface SettingsDetailCell ()

// When they are activated, the labels are on one line.
// They conflict with the accessibilityConstraints.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* standardConstraints;
// When they are activated, each label is on its own line, with no line number
// limit. They conflict with the standardConstraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;

@end

@implementation SettingsDetailCell {
  UIImageView* _iconImageView;
  UILayoutGuide* _labelContainerGuide;
  NSLayoutConstraint* _iconHiddenConstraint;
  NSLayoutConstraint* _iconVisibleConstraint;
  NSLayoutConstraint* _textLabelWidthConstraint;
  NSLayoutConstraint* _detailTextLabelWidthConstraint;
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
    _textLabel.textColor = [UIColor blackColor];
    _textLabel.backgroundColor = [UIColor clearColor];
    [contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColorFromRGB(kSettingsCellsDetailTextColor);
    _detailTextLabel.backgroundColor = [UIColor clearColor];
    [contentView addSubview:_detailTextLabel];

    // Set up the width constraints. They are activated here and updated in
    // layoutSubviews.
    _textLabelWidthConstraint =
        [_textLabel.widthAnchor constraintEqualToConstant:0];
    _detailTextLabelWidthConstraint =
        [_detailTextLabel.widthAnchor constraintEqualToConstant:0];

    // Set up the constraints for when the icon is visible and hidden.  One of
    // these will be active at a time, defaulting to hidden.
    _iconHiddenConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kHorizontalPadding];
    _iconVisibleConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kIconTrailingPadding];

    _standardConstraints = @[
      _textLabelWidthConstraint,
      _detailTextLabelWidthConstraint,
      // Set up the vertical constraints and align the baselines of the two text
      // labels.
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_detailTextLabel.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:_labelContainerGuide.trailingAnchor],
    ];

    _accessibilityConstraints = @[
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kVerticalPadding],
      [_detailTextLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor
                         constant:-kVerticalPadding],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kHorizontalPadding],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_detailTextLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_labelContainerGuide.trailingAnchor
                                   constant:-kHorizontalPadding],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconImageSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconImageSize],

      // Fix the edges of the text labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_labelContainerGuide.leadingAnchor],
      [_labelContainerGuide.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],

      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHiddenConstraint,
    ]];

    AddOptionalVerticalPadding(contentView, _textLabel, kVerticalPadding);

    [self updateForAccessibilityContentSizeCategory:
              ContentSizeCategoryIsAccessibilityCategory(
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
      ContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      ContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

// Updates the layout constraints of the text labels and then calls the
// superclass's implementation of layoutSubviews which can then take account of
// the new constraints.
- (void)layoutSubviews {
  [super layoutSubviews];

  // Size the labels in order to determine how much width they want.
  [self.textLabel sizeToFit];
  [self.detailTextLabel sizeToFit];

  // Update the width constraints.
  _textLabelWidthConstraint.constant = self.textLabelTargetWidth;
  _detailTextLabelWidthConstraint.constant = self.detailTextLabelTargetWidth;

  // Now invoke the layout.
  [super layoutSubviews];
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
    _detailTextLabel.numberOfLines = 0;
    _textLabel.numberOfLines = 0;
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_standardConstraints];
    _detailTextLabel.numberOfLines = 1;
    _textLabel.numberOfLines = 1;
  }
}

- (CGFloat)textLabelTargetWidth {
  CGFloat availableWidth =
      CGRectGetWidth(_labelContainerGuide.layoutFrame) - kHorizontalPadding;
  CGFloat textLabelWidth = self.textLabel.frame.size.width;
  CGFloat detailTextLabelWidth = self.detailTextLabel.frame.size.width;

  if (textLabelWidth + detailTextLabelWidth <= availableWidth)
    return textLabelWidth;

  return std::max(
      availableWidth - detailTextLabelWidth,
      std::min(availableWidth * kMinTextWidthRatio, textLabelWidth));
}

- (CGFloat)detailTextLabelTargetWidth {
  CGFloat availableWidth =
      CGRectGetWidth(_labelContainerGuide.layoutFrame) - kHorizontalPadding;
  CGFloat textLabelWidth = self.textLabel.frame.size.width;
  CGFloat detailTextLabelWidth = self.detailTextLabel.frame.size.width;

  if (textLabelWidth + detailTextLabelWidth <= availableWidth)
    return detailTextLabelWidth;

  return std::max(availableWidth - textLabelWidth,
                  std::min(availableWidth * kMinDetailTextWidthRatio,
                           detailTextLabelWidth));
}

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
