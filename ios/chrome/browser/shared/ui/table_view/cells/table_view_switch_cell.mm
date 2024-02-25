// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Padding used between the `switchView` and the end of the `contentView`.
const CGFloat kSwitchTrailingPadding = 22;

}  // namespace

@interface TableViewSwitchCell ()

// The image view for the leading icon.
@property(nonatomic, readonly, strong) UIImageView* iconImageView;

// Constraints that are used when the iconImageView is visible and hidden.
@property(nonatomic, strong) NSLayoutConstraint* iconVisibleConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconHiddenConstraint;

// Constraints that are used when the preferred content size is an
// "accessibility" category.
@property(nonatomic, strong) NSArray* accessibilityConstraints;
// Constraints that are used when the preferred content size is *not* an
// "accessibility" category.
@property(nonatomic, strong) NSArray* standardConstraints;
// Custom label defined via the setter, if any.
@property(nonatomic, strong) NSString* customAccessibilityLabel;

@end

@implementation TableViewSwitchCell {
  UIView* _iconBackground;
}

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    _iconBackground = [[UIView alloc] init];
    _iconBackground.translatesAutoresizingMaskIntoConstraints = NO;
    _iconBackground.hidden = YES;
    [self.contentView addSubview:_iconBackground];

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.contentMode = UIViewContentModeCenter;
    [_iconBackground addSubview:_iconImageView];

    AddSameCenterConstraints(_iconBackground, _iconImageView);

    UILayoutGuide* textLayoutGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:textLayoutGuide];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailTextLabel.numberOfLines = 0;
    [self.contentView addSubview:_detailTextLabel];

    _switchView = [[UISwitch alloc] initWithFrame:CGRectZero];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    [_switchView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _switchView.isAccessibilityElement = YES;
    _switchView.accessibilityHint =
        l10n_util::GetNSString(IDS_IOS_TOGGLE_SWITCH_ACCESSIBILITY_HINT);
    [self.contentView addSubview:_switchView];

    // Set up the constraints assuming that the icon image is hidden.
    _iconVisibleConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_iconBackground.trailingAnchor
                       constant:kTableViewImagePadding];
    _iconHiddenConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];

    _standardConstraints = @[
      [_switchView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_switchView.leadingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [textLayoutGuide.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      [_switchView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kSwitchTrailingPadding],
    ];
    _accessibilityConstraints = @[
      [_switchView.topAnchor
          constraintEqualToAnchor:textLayoutGuide.bottomAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_switchView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_switchView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_iconBackground.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconBackground.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_iconBackground.heightAnchor
          constraintEqualToAnchor:_iconBackground.widthAnchor],

      [_iconBackground.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],

      _iconHiddenConstraint,

      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_detailTextLabel.leadingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_detailTextLabel.trailingAnchor],
      [textLayoutGuide.topAnchor constraintEqualToAnchor:_textLabel.topAnchor],
      [textLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.bottomAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],

      // Leading constraint for `customSepartor`.
      [self.customSeparator.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
    ]];

    if (UIContentSizeCategoryIsAccessibilityCategory(
            self.traitCollection.preferredContentSizeCategory)) {
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }

    AddOptionalVerticalPadding(self.contentView, textLayoutGuide,
                               kTableViewOneLabelCellVerticalSpacing);
  }
  return self;
}

- (void)configureCellWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                 switchEnabled:(BOOL)enabled
                            on:(BOOL)on {
  self.textLabel.text = title;
  self.detailTextLabel.text = subtitle;
  self.switchView.enabled = enabled;
  self.switchView.on = on;
  self.switchView.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@, switch", title];

  UIColor* textColor = enabled ? [UIColor colorNamed:kTextPrimaryColor]
                               : [UIColor colorNamed:kTextSecondaryColor];
  self.textLabel.textColor = textColor;
  self.selectionStyle = UITableViewCellSelectionStyleNone;
}

- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius
         borderWidth:(CGFloat)borderWidth {
  BOOL hidden = (image == nil);

  self.iconImageView.image = image;
  self.iconImageView.tintColor = tintColor;

  _iconBackground.backgroundColor = backgroundColor;
  _iconBackground.layer.cornerRadius = cornerRadius;
  _iconBackground.layer.borderColor =
      [UIColor colorNamed:kGrey200Color].CGColor;
  _iconBackground.layer.borderWidth = borderWidth;

  _iconBackground.hidden = hidden;
  if (hidden) {
    self.iconVisibleConstraint.active = NO;
    self.iconHiddenConstraint.active = YES;
  } else {
    self.iconHiddenConstraint.active = NO;
    self.iconVisibleConstraint.active = YES;
  }
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentContentSizeAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory) !=
      isCurrentContentSizeAccessibility) {
    if (isCurrentContentSizeAccessibility) {
      [NSLayoutConstraint deactivateConstraints:_standardConstraints];
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  [self setIconImage:nil
            tintColor:nil
      backgroundColor:nil
         cornerRadius:0
          borderWidth:0];
  [_switchView removeTarget:nil
                     action:nil
           forControlEvents:[_switchView allControlEvents]];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  // Center the activation point over the switch, so that double-tapping toggles
  // the switch.
  CGRect switchFrame =
      UIAccessibilityConvertFrameToScreenCoordinates(_switchView.frame, self);
  return CGPointMake(CGRectGetMidX(switchFrame), CGRectGetMidY(switchFrame));
}

- (NSString*)accessibilityHint {
  if (_switchView.enabled) {
    return _switchView.accessibilityHint;
  } else {
    return @"";
  }
}

- (NSString*)accessibilityLabel {
  if (self.customAccessibilityLabel) {
    return self.customAccessibilityLabel;
  }

  if (!self.detailTextLabel.text) {
    return self.textLabel.text;
  }
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

- (void)setAccessibilityLabel:(NSString*)label {
  self.customAccessibilityLabel = [label copy];
}

- (NSString*)accessibilityValue {
  if (_switchView.on) {
    return l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else {
    return l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }
}

- (UIAccessibilityTraits)accessibilityTraits {
  UIAccessibilityTraits accessibilityTraits = super.accessibilityTraits;
  if (!self.switchView.isEnabled) {
    accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  return accessibilityTraits;
}

@end
