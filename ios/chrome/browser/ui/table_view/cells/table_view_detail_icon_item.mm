// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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
  [cell setTextLayoutConstraintAxis:self.textLayoutConstraintAxis];
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
// View containing UILabels |text| and |detailText|.
@property(nonatomic, strong) UIStackView* textStackView;
@end

@implementation TableViewDetailIconCell {
  UIImageView* _iconImageView;
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

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.cr_labelColor;
    _textLabel.backgroundColor = UIColor.clearColor;

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColor.cr_secondaryLabelColor;
    _detailTextLabel.backgroundColor = UIColor.clearColor;

    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textStackView];

    // Set up the constraints for when the icon is visible and hidden.  One of
    // these will be active at a time, defaulting to hidden.
    _iconHiddenConstraint = [_textStackView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    _iconVisibleConstraint = [_textStackView.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kTableViewImagePadding];

    // In case the two labels don't fit in width, have the |textLabel| be 3
    // times the width of the |detailTextLabel| (so 75% / 25%).
    NSLayoutConstraint* widthConstraint = [_textLabel.widthAnchor
        constraintEqualToAnchor:_detailTextLabel.widthAnchor
                     multiplier:kCellLabelsWidthProportion];
    // Set low priority to the proportion constraint between |_textLabel| and
    // |_detailTextLabel|, so that it won't break other layouts.
    widthConstraint.priority = UILayoutPriorityDefaultLow;

    NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Don't set the priority to required to avoid clashing with the estimated
    // height.
    heightConstraint.priority = UILayoutPriorityRequired - 1;

    _standardConstraints = @[
      [_textStackView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      widthConstraint,
      heightConstraint,
    ];

    _accessibilityConstraints = @[
      [_textStackView.topAnchor
          constraintEqualToAnchor:contentView.topAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_textStackView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],

    ];

    [NSLayoutConstraint activateConstraints:@[
      // Image.
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconImageView.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_iconImageView.heightAnchor
          constraintEqualToAnchor:_iconImageView.widthAnchor],
      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      // Text labels.
      [_textStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewTrailingContentPadding],
      _iconHiddenConstraint,

      // Leading constraint for |customSeparator|.
      [self.customSeparator.leadingAnchor
          constraintEqualToAnchor:_textStackView.leadingAnchor],
    ]];

    AddOptionalVerticalPadding(contentView, _textStackView,
                               kTableViewOneLabelCellVerticalSpacing);

    [self updateForAccessibilityContentSizeCategory:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

- (void)setIconImage:(UIImage*)image {
  if (image == nil && _iconImageView.image == nil) {
    return;
  }

  BOOL hidden = (image == nil);
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

- (void)setTextLayoutConstraintAxis:
    (UILayoutConstraintAxis)textLayoutConstraintAxis {
  UIFontTextStyle preferredFont =
      textLayoutConstraintAxis == UILayoutConstraintAxisVertical
          ? kTableViewSublabelFontStyle
          : UIFontTextStyleBody;
  self.detailTextLabel.font = [UIFont preferredFontForTextStyle:preferredFont];

  self.textStackView.axis = textLayoutConstraintAxis;

  [self updateForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

#pragma mark - Properties

- (UILayoutConstraintAxis)textLayoutConstraintAxis {
  return self.textStackView.axis;
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

  [self setTextLayoutConstraintAxis:UILayoutConstraintAxisHorizontal];
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
    // alignment with non-accessibility content size category if in horizontal
    // axis layout.
    if (_textStackView.axis == UILayoutConstraintAxisHorizontal) {
      _detailTextLabel.textAlignment =
          self.effectiveUserInterfaceLayoutDirection ==
                  UIUserInterfaceLayoutDirectionLeftToRight
              ? NSTextAlignmentRight
              : NSTextAlignmentLeft;
    } else {
      _detailTextLabel.textAlignment = NSTextAlignmentNatural;
    }
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
