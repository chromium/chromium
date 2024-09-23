// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Proportion of Cell's textLabel/detailTextLabel. This guarantees that the
// textLabel occupies 75% of the row space and detailTextLabel occupies 25%.
constexpr CGFloat kCellLabelsWidthProportion = 3.f;

// Minimum cell height when the cell has 2 lines.
constexpr CGFloat kChromeTableViewTwoLinesCellHeight = 58.f;

// kDotSize represents the size of the dot (i.e. its height and width).
constexpr CGFloat kDotSize = 10.f;

// kMarginAroundDot represents the amount of space before and after the badge,
// between itself and the surrounding UILabels `text` and `detailText`.
constexpr CGFloat kMarginAroundBadge = 5.0;

// The size of the "new" IPH badge.
constexpr CGFloat kNewIPHBadgeSize = 20.0;
// The font size of the "N" in the "new" IPH badge.
constexpr CGFloat kNewIPHBadgeFontSize = 10.0;

// kDefaultTextLabelSpacing represents the default spacing between the text
// labels when no dot is present.
constexpr CGFloat kDefaultTextLabelSpacing = 4;

// By default, the maximum number of lines to be displayed for the detail text
// should be one.
const NSInteger kDefaultDetailTextNumberOfLines = 1;

// The extra vertical spacing of the icon when it's top aligned with the text
// labels.
constexpr CGFloat kIconTopAlignmentVerticalSpacing = 2.0;

// Returns the notification dot view for `TableViewDetailIconCell`.
UIView* NotificationDotView() {
  UIView* notificationDotUIView = [[UIView alloc] init];
  notificationDotUIView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* dotUIView = [[UIView alloc] init];
  dotUIView.translatesAutoresizingMaskIntoConstraints = NO;
  dotUIView.layer.cornerRadius = kDotSize / 2;
  dotUIView.backgroundColor = [UIColor colorNamed:kBlue600Color];
  [notificationDotUIView addSubview:dotUIView];

  [NSLayoutConstraint activateConstraints:@[
    [notificationDotUIView.widthAnchor
        constraintGreaterThanOrEqualToConstant:kDotSize],
    [dotUIView.widthAnchor constraintEqualToConstant:kDotSize],
    [dotUIView.heightAnchor constraintEqualToConstant:kDotSize],
    [dotUIView.leadingAnchor
        constraintEqualToAnchor:notificationDotUIView.leadingAnchor],
    [dotUIView.centerYAnchor
        constraintEqualToAnchor:notificationDotUIView.centerYAnchor],
  ]];
  return notificationDotUIView;
}

// Returns the "new" ("N") IPH badge view for `TableViewDetailIconCell`.
NewFeatureBadgeView* NewIPHBadgeView() {
  NewFeatureBadgeView* newIPHBadgeView =
      [[NewFeatureBadgeView alloc] initWithBadgeSize:kNewIPHBadgeSize
                                            fontSize:kNewIPHBadgeFontSize];
  newIPHBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  return newIPHBadgeView;
}

}  // namespace

@implementation TableViewDetailIconItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewDetailIconCell class];
    self.badgeType = BadgeType::kNone;
    _detailTextNumberOfLines = kDefaultDetailTextNumberOfLines;
    _iconCenteredVertically = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewDetailIconCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  [cell setDetailText:self.detailText];

  [cell setIconImage:self.iconImage
            tintColor:self.iconTintColor
      backgroundColor:self.iconBackgroundColor
         cornerRadius:self.iconCornerRadius];
  [cell setTextLayoutConstraintAxis:self.textLayoutConstraintAxis];
  [cell setBadgeType:self.badgeType];

  [cell setDetailTextNumberOfLines:self.detailTextNumberOfLines];
  [cell setIconCenteredVertically:self.iconCenteredVertically];
}

@end

#pragma mark - TableViewDetailIconCell

@interface TableViewDetailIconCell ()

// View containing UILabels `text` and `detailText`.
@property(nonatomic, strong) UIStackView* textStackView;
// Padding layout constraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* verticalPaddingConstraints;
// Constraint to set the cell minimum height.
@property(nonatomic, strong) NSLayoutConstraint* minimumCellHeightConstraint;
// Text width constraint between the title and the detail text.
@property(nonatomic, strong) NSLayoutConstraint* textWidthConstraint;
// Detail text. Can be nil if no text is set.
@property(nonatomic, strong) UILabel* detailTextLabel;

@end

@implementation TableViewDetailIconCell {
  UIView* _iconBackground;
  UIImageView* _iconImageView;
  NSLayoutConstraint* _iconHiddenConstraint;
  NSLayoutConstraint* _iconVisibleConstraint;
  NSLayoutConstraint* _iconCenterAlignment;
  NSLayoutConstraint* _iconTopAlignment;
  NSLayoutConstraint* _iconBackgroundDefaultWidthConstraint;
  NSLayoutConstraint* _iconBackgroundCustomWidthConstraint;

  // View representing the current badge view.
  UIView* _badgeView;
  // Badge type of current badge view.
  BadgeType _badgeType;
}

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _detailTextNumberOfLines = kDefaultDetailTextNumberOfLines;
    _iconCenteredVertically = YES;

    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _iconBackground = [[UIView alloc] init];
    _iconBackground.translatesAutoresizingMaskIntoConstraints = NO;
    _iconBackground.hidden = YES;
    [contentView addSubview:_iconBackground];

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.contentMode = UIViewContentModeCenter;
    [_iconBackground addSubview:_iconImageView];

    AddSameCenterConstraints(_iconBackground, _iconImageView);

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.backgroundColor = UIColor.clearColor;
    _textLabel.numberOfLines = 2;

    _textStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _textLabel ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textStackView.spacing = kDefaultTextLabelSpacing;
    [contentView addSubview:_textStackView];

    // Set up the constraints for when the icon is visible and hidden.  One of
    // these will be active at a time, defaulting to hidden.
    _iconHiddenConstraint = [_textStackView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    _iconVisibleConstraint = [_textStackView.leadingAnchor
        constraintEqualToAnchor:_iconBackground.trailingAnchor
                       constant:kTableViewImagePadding];

    _minimumCellHeightConstraint = [contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Lower the priority for transition. The content view has autoresizing mask
    // to have the same height than the cell. To avoid breaking the constaints
    // while updating the minimum height constant, the constraint has to have
    // a lower priority.
    _minimumCellHeightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
    _minimumCellHeightConstraint.active = YES;

    // Set up the constrains for the icon's vertical alignment. One of these
    // will be active at the time, defaulting to center alignment.
    _iconCenterAlignment = [_iconBackground.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor];
    _iconTopAlignment = [_iconBackground.topAnchor
        constraintEqualToAnchor:_textStackView.topAnchor
                       constant:kIconTopAlignmentVerticalSpacing];
    [self updateIconAlignment];

    _iconBackgroundDefaultWidthConstraint = [_iconBackground.widthAnchor
        constraintEqualToConstant:kTableViewIconImageSize];
    _iconBackgroundDefaultWidthConstraint.active = YES;

    [_iconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired - 1
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _iconBackgroundCustomWidthConstraint = [_iconBackground.widthAnchor
        constraintEqualToAnchor:_iconImageView.widthAnchor];
    _iconBackgroundCustomWidthConstraint.active = NO;

    [NSLayoutConstraint activateConstraints:@[
      // Icon.
      [_iconBackground.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconBackground.heightAnchor
          constraintEqualToAnchor:_iconBackground.widthAnchor],

      // Text labels.
      [_textStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_textStackView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHiddenConstraint,

      // Leading constraint for `customSeparator`.
      [self.customSeparator.leadingAnchor
          constraintEqualToAnchor:_textStackView.leadingAnchor],
    ]];

    _verticalPaddingConstraints = AddOptionalVerticalPadding(
        contentView, _textStackView, kTableViewTwoLabelsCellVerticalSpacing);

    [self updateCellForAccessibilityContentSizeCategory:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius {
  if (image == nil && _iconImageView.image == nil) {
    return;
  }

  if (tintColor) {
    image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  _iconImageView.image = image;
  _iconImageView.tintColor = tintColor;
  _iconBackground.backgroundColor = backgroundColor;
  _iconBackground.layer.cornerRadius = cornerRadius;

  BOOL hidden = (image == nil);
  _iconBackground.hidden = hidden;
  if (hidden) {
    _iconVisibleConstraint.active = NO;
    _iconHiddenConstraint.active = YES;
  } else {
    _iconHiddenConstraint.active = NO;
    _iconVisibleConstraint.active = YES;
  }
}

#pragma mark - Properties

- (void)setIconCenteredVertically:(BOOL)iconCenteredVertically {
  _iconCenteredVertically = iconCenteredVertically;
  [self updateIconAlignment];
}

- (void)setDetailTextNumberOfLines:(NSInteger)detailTextNumberOfLines {
  _detailTextNumberOfLines = detailTextNumberOfLines;

  [self updateCellForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

- (void)setTextLayoutConstraintAxis:
    (UILayoutConstraintAxis)textLayoutConstraintAxis {
  CGFloat verticalPaddingConstant = 0;
  switch (textLayoutConstraintAxis) {
    case UILayoutConstraintAxisVertical:
      verticalPaddingConstant = kTableViewTwoLabelsCellVerticalSpacing;
      self.minimumCellHeightConstraint.constant =
          kChromeTableViewTwoLinesCellHeight;
      DCHECK(self.detailTextLabel);
      break;
    case UILayoutConstraintAxisHorizontal:
      verticalPaddingConstant = kTableViewOneLabelCellVerticalSpacing;
      self.minimumCellHeightConstraint.constant = kChromeTableViewCellHeight;
      break;
  }
  DCHECK(verticalPaddingConstant);
  for (NSLayoutConstraint* constraint in self.verticalPaddingConstraints) {
    constraint.constant = verticalPaddingConstant;
  }

  self.textStackView.axis = textLayoutConstraintAxis;

  [self updateCellForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

- (UILayoutConstraintAxis)textLayoutConstraintAxis {
  return self.textStackView.axis;
}

- (void)setDetailText:(NSString*)detailText {
  if (detailText.length > 0) {
    if (!self.detailTextLabel) {
      [self createDetailTextLabel];
    }
    self.detailTextLabel.text = detailText;
  } else if (self.detailTextLabel) {
    [self removeDetailTextLabel];
  }
}

- (void)setBadgeType:(BadgeType)badgeType {
  if (badgeType == BadgeType::kNone) {
    [self removeBadgeView];
  } else if (_badgeType != badgeType) {
    [self removeBadgeView];
    [self addBadgeViewOfType:badgeType];
  }
  _badgeType = badgeType;
}

- (void)updateIconBackgroundWidthToFitContent:(BOOL)useCustomWidth {
  if (useCustomWidth) {
    _iconBackgroundDefaultWidthConstraint.active = NO;
    _iconBackgroundCustomWidthConstraint.active = YES;
    return;
  }
  _iconBackgroundCustomWidthConstraint.active = NO;
  _iconBackgroundDefaultWidthConstraint.active = YES;
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
    [self updateCellForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  [self setTextLayoutConstraintAxis:UILayoutConstraintAxisHorizontal];
  [self setIconImage:nil tintColor:nil backgroundColor:nil cornerRadius:0];
  [self setDetailText:nil];
  [self setBadgeType:BadgeType::kNone];
  [self updateIconBackgroundWidthToFitContent:NO];
}

#pragma mark - Private

- (void)updateIconAlignment {
  if (_iconCenteredVertically) {
    _iconTopAlignment.active = NO;
    _iconCenterAlignment.active = YES;
  } else {
    _iconCenterAlignment.active = NO;
    _iconTopAlignment.active = YES;
  }
}

- (void)createDetailTextLabel {
  if (self.detailTextLabel) {
    return;
  }
  self.detailTextLabel = [[UILabel alloc] init];
  self.detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.detailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.detailTextLabel.adjustsFontForContentSizeCategory = YES;
  self.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.detailTextLabel.backgroundColor = UIColor.clearColor;
  [self.textStackView addArrangedSubview:self.detailTextLabel];
  // In case the two labels don't fit in width, have the `textLabel` be 3
  // times the width of the `detailTextLabel` (so 75% / 25%).
  self.textWidthConstraint = [self.textLabel.widthAnchor
      constraintEqualToAnchor:self.detailTextLabel.widthAnchor
                   multiplier:kCellLabelsWidthProportion];
  // Set low priority to the proportion constraint between `self.textLabel` and
  // `self.detailTextLabel`, so that it won't break other layouts.
  self.textWidthConstraint.priority = UILayoutPriorityDefaultLow;
  [self updateCellForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

- (void)removeDetailTextLabel {
  if (!self.detailTextLabel) {
    return;
  }
  [self.detailTextLabel removeFromSuperview];
  self.detailTextLabel = nil;
  self.textWidthConstraint = nil;
  self.textLayoutConstraintAxis = UILayoutConstraintAxisHorizontal;
}

// Returns the view corresponding to `badgeType`. Should not be called with
// BadgeType::kNone.
- (UIView*)badgeViewOfType:(BadgeType)badgeType {
  switch (badgeType) {
    case BadgeType::kNotificationDot:
      return NotificationDotView();
    case BadgeType::kNew:
      return NewIPHBadgeView();
    case BadgeType::kNone: {
      NOTREACHED_IN_MIGRATION();
      return nil;
    }
  }
}

// Creates the badge of `badgeType` and insert it after the `textLabel`.
- (void)addBadgeViewOfType:(BadgeType)badgeType {
  CHECK(badgeType != BadgeType::kNone);
  if (_badgeView) {
    // Previous badge view shoulb be removed before adding a new type of badge
    // view.
    CHECK(badgeType == _badgeType);
    return;
  }

  // Since we're inserting the view at index 1, this CHECK checks to make sure
  // the main text's UILabel subview is there.
  CHECK(self.textStackView.subviews.count > 0);

  // Since only the horizontal axis is supported for the badge, make
  // sure we currently have the right axis.
  CHECK([self textLayoutConstraintAxis] == UILayoutConstraintAxisHorizontal);

  // Make sure the badge is always snug next to the main text.
  [self.textLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                    forAxis:UILayoutConstraintAxisHorizontal];

  _badgeView = [self badgeViewOfType:badgeType];

  [self.textStackView insertArrangedSubview:_badgeView atIndex:1];
  [self.textStackView setCustomSpacing:kMarginAroundBadge
                             afterView:self.textLabel];
  [self.textStackView setCustomSpacing:kMarginAroundBadge afterView:_badgeView];

  [NSLayoutConstraint activateConstraints:@[
    [_badgeView.centerYAnchor
        constraintEqualToAnchor:self.textLabel.centerYAnchor],
  ]];
}

// Removes the badge view from the UI.
- (void)removeBadgeView {
  if (!_badgeView) {
    return;
  }

  [self.textStackView setCustomSpacing:UIStackViewSpacingUseDefault
                             afterView:self.textLabel];
  [_badgeView removeFromSuperview];
  _badgeView = nil;
}

// Updates the cell such as it is layouted correctly with regard to the
// preferred content size category, if it is an
// `accessibilityContentSizeCategory` or not.
- (void)updateCellForAccessibilityContentSizeCategory:
    (BOOL)accessibilityContentSizeCategory {
  if (accessibilityContentSizeCategory) {
    _textWidthConstraint.active = NO;
    // detailTextLabel is laid below textLabel with accessibility content size
    // category.
    _detailTextLabel.textAlignment = NSTextAlignmentNatural;
    _detailTextLabel.numberOfLines = 0;
    _textLabel.numberOfLines = 0;
  } else {
    _textWidthConstraint.active = YES;
    // detailTextLabel is laid after textLabel and should have a trailing text
    // alignment with non-accessibility content size category if in horizontal
    // axis layout.
    if (_textStackView.axis == UILayoutConstraintAxisHorizontal) {
      _detailTextLabel.textAlignment =
          self.effectiveUserInterfaceLayoutDirection ==
                  UIUserInterfaceLayoutDirectionLeftToRight
              ? NSTextAlignmentRight
              : NSTextAlignmentLeft;
      _detailTextLabel.numberOfLines = kDefaultDetailTextNumberOfLines;
    } else {
      _detailTextLabel.textAlignment = NSTextAlignmentNatural;
      _detailTextLabel.numberOfLines = _detailTextNumberOfLines;
    }
    _textLabel.numberOfLines = 2;
  }
  UIFontTextStyle preferredFont =
      _textStackView.axis == UILayoutConstraintAxisVertical
          ? UIFontTextStyleFootnote
          : UIFontTextStyleBody;
  _detailTextLabel.font = [UIFont preferredFontForTextStyle:preferredFont];
}

- (NSString*)accessibilityLabel {
  if (self.customAccessibilityLabel) {
    return self.customAccessibilityLabel;
  }
  if (_badgeView) {
    switch (_badgeType) {
      case BadgeType::kNotificationDot:
        return [NSString
            stringWithFormat:@"%@, %@", self.textLabel.text,
                             l10n_util::GetNSString(
                                 IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT)];
      case BadgeType::kNew:
        return [NSString
            stringWithFormat:@"%@, %@", self.textLabel.text,
                             l10n_util::GetNSString(
                                 IDS_IOS_NEW_FEATURE_ACCESSIBILITY_HINT)];
      case BadgeType::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  if (self.customAccessibilityLabel) {
    // If the cell already has an accessibility label for the whole cell, remove
    // the one specifically for the detail text, so that information is not
    // repeated by voice over.
    return @"";
  }
  return self.detailTextLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  if (_badgeView) {
    switch (_badgeType) {
      case BadgeType::kNotificationDot:
        return @[ [NSString
            stringWithFormat:@"%@, %@", self.textLabel.text,
                             l10n_util::GetNSString(
                                 IDS_IOS_NEW_ITEM_ACCESSIBILITY_HINT)] ];
      case BadgeType::kNew:
        return @[ [NSString
            stringWithFormat:@"%@, %@", self.textLabel.text,
                             l10n_util::GetNSString(
                                 IDS_IOS_NEW_FEATURE_ACCESSIBILITY_HINT)] ];
      case BadgeType::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return @[ self.textLabel.text ];
}

@end
