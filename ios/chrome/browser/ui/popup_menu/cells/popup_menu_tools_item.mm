// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"

#import <stdlib.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_constants.h"
#import "ios/chrome/browser/ui/reading_list/number_badge_view.h"
#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kImageLength = 28;
const CGFloat kCellHeight = 44;
const CGFloat kInnerMargin = 11;
const CGFloat kMargin = 15;
const CGFloat kTopMargin = 8;
const CGFloat kMaxHeight = 100;
NSString* const kToolsMenuTextBadgeAccessibilityIdentifier =
    @"kToolsMenuTextBadgeAccessibilityIdentifier";
}  // namespace

@implementation PopupMenuToolsItem

@synthesize actionIdentifier = _actionIdentifier;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuToolsCell class];
    _enabled = YES;
  }
  return self;
}

- (void)configureCell:(PopupMenuToolsCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.titleLabel.text = self.title;
  cell.imageView.image = self.image;
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  cell.userInteractionEnabled = self.enabled;
  cell.destructiveAction = self.destructiveAction;
  [cell setBadgeNumber:self.badgeNumber];
  [cell setBadgeText:self.badgeText];
  cell.additionalAccessibilityLabel = self.additionalAccessibilityLabel;
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/41380449): This should be done at the table view level.
  static PopupMenuToolsCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[PopupMenuToolsCell alloc] init];
    [cell registerForContentSizeUpdates];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

#pragma mark - PopupMenuToolsCell

@interface PopupMenuToolsCell ()

// Title label for the cell, redefined as readwrite.
@property(nonatomic, strong, readwrite) UILabel* titleLabel;
// Image view for the cell, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIImageView* imageView;
// Badge displaying a number.
@property(nonatomic, strong) NumberBadgeView* numberBadgeView;
// Badge displaying text.
@property(nonatomic, strong) TextBadgeView* textBadgeView;
// Constraints between the trailing of the label and the badges.
@property(nonatomic, strong) NSLayoutConstraint* titleToBadgeConstraint;
// Color for the title and the image.
@property(nonatomic, strong, readonly) UIColor* contentColor;

@end

@implementation PopupMenuToolsCell

@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* selectedBackgroundView = [[UIView alloc] init];
    selectedBackgroundView.backgroundColor =
        [UIColor colorNamed:kTableViewRowHighlightColor];
    self.selectedBackgroundView = selectedBackgroundView;

    _titleLabel = [[UILabel alloc] init];
    if (IsWebChannelsEnabled()) {
      _titleLabel.numberOfLines = 2;
    } else {
      _titleLabel.numberOfLines = 0;
    }
    _titleLabel.font = [self titleFont];
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultLow - 1
                                   forAxis:UILayoutConstraintAxisHorizontal];
    // The compression resistance has to be higher priority than the minimal
    // height constraint so it can increase the height of the cell to be
    // displayed on multiple lines.
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:UILayoutConstraintAxisVertical];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;

    _numberBadgeView = [[NumberBadgeView alloc] init];
    _numberBadgeView.translatesAutoresizingMaskIntoConstraints = NO;

    _textBadgeView = [[TextBadgeView alloc] initWithText:nil];
    _textBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
    _textBadgeView.accessibilityIdentifier =
        kToolsMenuTextBadgeAccessibilityIdentifier;
    _textBadgeView.hidden = YES;

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_imageView];
    [self.contentView addSubview:_numberBadgeView];
    [self.contentView addSubview:_textBadgeView];

    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      // Align the center of image with the center of the capital letter of the
      // first line of the title.
      [_imageView.centerYAnchor
          constraintEqualToAnchor:_titleLabel.firstBaselineAnchor
                         constant:-[self titleFont].capHeight / 2.0],
      [_numberBadgeView.centerYAnchor
          constraintEqualToAnchor:_imageView.centerYAnchor],
      [_textBadgeView.centerYAnchor
          constraintEqualToAnchor:_imageView.centerYAnchor],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kCellHeight],
    ]];
    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[image(imageLength)]-(innerMargin)-[label]",
          @"H:[numberBadge]-(margin)-|", @"H:[textBadge]-(margin)-|",
          @"V:|-(>=topMargin)-[label]-(>=topMargin)-|"
        ],
        @{
          @"image" : _imageView,
          @"label" : _titleLabel,
          @"numberBadge" : _numberBadgeView,
          @"textBadge" : _textBadgeView
        },
        @{
          @"margin" : @(kMargin),
          @"innerMargin" : @(kInnerMargin),
          @"topMargin" : @(kTopMargin),
          @"imageLength" : @(kImageLength),
        });

    // The height constraint is used to have something as small as possible when
    // calculating the size of the prototype cell.
    NSLayoutConstraint* heightConstraint =
        [self.contentView.heightAnchor constraintEqualToConstant:kCellHeight];
    heightConstraint.priority = UILayoutPriorityDefaultLow;

    NSLayoutConstraint* trailingEdge = [_titleLabel.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kMargin];
    trailingEdge.priority = UILayoutPriorityDefaultHigh - 2;
    [NSLayoutConstraint
        activateConstraints:@[ trailingEdge, heightConstraint ]];

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setBadgeNumber:(NSInteger)badgeNumber {
  BOOL wasHidden = self.numberBadgeView.hidden;
  [self.numberBadgeView setNumber:badgeNumber animated:NO];
  // If the number badge is shown, then the text badge must be hidden.
  if (!self.numberBadgeView.hidden && !self.textBadgeView.hidden) {
    [self setBadgeText:nil];
  }
  if (!self.numberBadgeView.hidden && wasHidden) {
    self.titleToBadgeConstraint.active = NO;
    self.titleToBadgeConstraint = [self.numberBadgeView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.titleLabel.trailingAnchor
                                    constant:kInnerMargin];
    self.titleToBadgeConstraint.active = YES;
  } else if (self.numberBadgeView.hidden && !wasHidden) {
    self.titleToBadgeConstraint.active = NO;
  }
}

- (void)setBadgeText:(NSString*)badgeText {
  // Only 1 badge can be visible at a time, and the number badge takes priority.
  if (badgeText && !self.numberBadgeView.isHidden) {
    return;
  }

  if (badgeText) {
    [self.textBadgeView setText:badgeText];
    if (self.textBadgeView.hidden) {
      self.textBadgeView.hidden = NO;
      self.titleToBadgeConstraint.active = NO;
      self.titleToBadgeConstraint = [self.textBadgeView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.titleLabel.trailingAnchor
                                      constant:kInnerMargin];
      self.titleToBadgeConstraint.active = YES;
      self.textBadgeView.alpha = 1;
    }
  } else if (!self.textBadgeView.hidden) {
    self.textBadgeView.hidden = YES;
    self.titleToBadgeConstraint.active = NO;
  }
}

- (void)setDestructiveAction:(BOOL)destructiveAction {
  _destructiveAction = destructiveAction;
  if (self.userInteractionEnabled) {
    self.titleLabel.textColor = self.contentColor;
    self.imageView.tintColor = self.contentColor;
  }
}

- (UIColor*)contentColor {
  if (self.destructiveAction)
    return [UIColor colorNamed:kRedColor];
  return [UIColor colorNamed:kBlueColor];
}

- (void)registerForContentSizeUpdates {
  // This is needed because if the cell is static (used for height),
  // adjustsFontForContentSizeCategory isn't working.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(preferredContentSizeDidChange:)
             name:UIContentSizeCategoryDidChangeNotification
           object:nil];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);

  CGFloat trailingMargin = kMargin;
  if (!self.textBadgeView.hidden) {
    trailingMargin += self.textBadgeView.bounds.size.width + kInnerMargin;
  }
  CGFloat leadingMargin = kMargin + kImageLength + kInnerMargin;

  self.titleLabel.preferredMaxLayoutWidth =
      parentWidth - leadingMargin - trailingMargin;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.userInteractionEnabled = YES;
  self.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
}

- (void)setUserInteractionEnabled:(BOOL)userInteractionEnabled {
  [super setUserInteractionEnabled:userInteractionEnabled];
  if (userInteractionEnabled) {
    self.titleLabel.textColor = self.contentColor;
    self.imageView.tintColor = self.contentColor;
  } else {
    self.titleLabel.textColor = [UIColor colorNamed:kDisabledTintColor];
    self.imageView.tintColor = [UIColor colorNamed:kDisabledTintColor];
    self.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  if (self.additionalAccessibilityLabel) {
    return [NSString stringWithFormat:@"%@, %@", self.titleLabel.text,
                                      self.additionalAccessibilityLabel];
  } else {
    return self.titleLabel.text;
  }
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  // The name for Voice Control shouldn't include any data from the badge.
  return @[ self.titleLabel.text ];
}

#pragma mark - Private

// Callback when the preferred Content Size change.
- (void)preferredContentSizeDidChange:(NSNotification*)notification {
  self.titleLabel.font = [self titleFont];
}

// Font to be used for the title.
- (UIFont*)titleFont {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
}

@end
