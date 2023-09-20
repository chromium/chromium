// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/inline_promo_cell.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Vertical spacing of the stack view.
constexpr CGFloat kStackViewSpacing = 16;

// Content inset of the cell.
constexpr CGFloat kCellContentInset = 16;

// Special content inset of the cell for the close button. This is needed to
// have a big enough target area for the close button while having the "x" icon
// aligned with the rest of the UI.
constexpr CGFloat kCellCloseButtonContentInset = 2;

// Special content inset for the bottom of the cell. This is needed to have a
// big enough target area for the more info button without adding more white
// space at the bottom of the cell.
constexpr CGFloat kCellBottomContentInset = 5;

// Height and width of the close button.
constexpr CGFloat kCloseButtonSize = 16;

// Height and width of the close button's target area.
constexpr CGFloat kCloseButtonTargetAreaSize = 44;

// Height and width of the image.
constexpr CGFloat kImageSize = 86;

// Height of the more info button.
constexpr CGFloat kMoreInfoButtonHeight = 44;

// Content inset of the more info button.
constexpr CGFloat kMoreInfoButtonContentInset = 16;

// Height and width of the new feature badge.
constexpr CGFloat kNewFeatureBadgeSize = 20;

// Font size of the new feature badge label.
constexpr CGFloat kNewFeatureFontSize = 10;

}  // namespace

@implementation InlinePromoCell {
  // New feature badge that is overlaying part of the promo image view.
  NewFeatureBadgeView* _badgeView;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _enabled = YES;
    _closeButton = [self createCloseButton];
    _promoImageView = [self createPromoImageView];
    _badgeView = [self createNewFeatureBadgeView];
    _promoTextLabel = [self createPromoTextLabel];
    _moreInfoButton = [self createMoreInfoButton];

    UIView* badgedImageView =
        [self createBadgedImageViewWithImageView:_promoImageView
                             newFeatureBadgeView:_badgeView];
    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      badgedImageView, _promoTextLabel, _moreInfoButton
    ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.spacing = kStackViewSpacing;

    [self.contentView addSubview:stackView];
    [self.contentView addSubview:_closeButton];

    [NSLayoutConstraint activateConstraints:@[
      // `_closeButton` constraints.
      [_closeButton.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kCellCloseButtonContentInset],
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kCellCloseButtonContentInset],
      [_closeButton.heightAnchor
          constraintEqualToConstant:kCloseButtonTargetAreaSize],
      [_closeButton.widthAnchor
          constraintEqualToConstant:kCloseButtonTargetAreaSize],

      // `stackView` constraints.
      [stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                          constant:kCellContentInset],
      [stackView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kCellBottomContentInset],
      [stackView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kCellContentInset],
      [stackView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kCellContentInset],

      // `badgedImageView` constraints.
      [badgedImageView.topAnchor constraintEqualToAnchor:stackView.topAnchor],
      [badgedImageView.widthAnchor constraintEqualToConstant:kImageSize],
      [badgedImageView.heightAnchor constraintEqualToConstant:kImageSize],

      // `_promoTextLabel` constraints.
      [_promoTextLabel.leadingAnchor
          constraintEqualToAnchor:stackView.leadingAnchor],
      [_promoTextLabel.trailingAnchor
          constraintEqualToAnchor:stackView.trailingAnchor],

      // `_moreInfoButton` constraints.
      [_moreInfoButton.bottomAnchor
          constraintEqualToAnchor:stackView.bottomAnchor],
      [_moreInfoButton.leadingAnchor
          constraintEqualToAnchor:stackView.leadingAnchor],
      [_moreInfoButton.trailingAnchor
          constraintEqualToAnchor:stackView.trailingAnchor],
      [_moreInfoButton.heightAnchor
          constraintGreaterThanOrEqualToConstant:kMoreInfoButtonHeight],
    ]];
  }
  return self;
}

#pragma mark - Setters

- (void)setEnabled:(BOOL)enabled {
  if (_enabled == enabled) {
    return;
  }
  _enabled = enabled;

  if (enabled) {
    [_badgeView setBadgeColor:[UIColor colorNamed:kBlue600Color]];
    _promoTextLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  } else {
    [_badgeView setBadgeColor:[UIColor colorNamed:kTextSecondaryColor]];
    _promoTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  _moreInfoButton.enabled = enabled;
  _closeButton.enabled = enabled;
}

#pragma mark - Private

// Creates and returns the promo's close button.
- (UIButton*)createCloseButton {
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ICON_CLOSE);

  UIImageSymbolConfiguration* symbolConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.image =
      DefaultSymbolWithConfiguration(kXMarkSymbol, symbolConfiguration);
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextTertiaryColor];
  closeButton.configuration = buttonConfiguration;

  return closeButton;
}

// Creates and configures the promo's image view.
- (UIImageView*)createPromoImageView {
  UIImageView* promoImageView = [[UIImageView alloc] init];
  promoImageView.translatesAutoresizingMaskIntoConstraints = NO;
  promoImageView.contentMode = UIViewContentModeScaleAspectFit;

  return promoImageView;
}

// Creates and configures the new feature badge view.
- (NewFeatureBadgeView*)createNewFeatureBadgeView {
  NewFeatureBadgeView* badgeView =
      [[NewFeatureBadgeView alloc] initWithBadgeSize:kNewFeatureBadgeSize
                                            fontSize:kNewFeatureFontSize];
  badgeView.translatesAutoresizingMaskIntoConstraints = NO;
  badgeView.accessibilityElementsHidden = YES;

  return badgeView;
}

// Creates and configures the promo's text label.
- (UILabel*)createPromoTextLabel {
  UILabel* promoTextLabel = [[UILabel alloc] init];
  promoTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  promoTextLabel.isAccessibilityElement = YES;
  promoTextLabel.textAlignment = NSTextAlignmentCenter;
  promoTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  promoTextLabel.adjustsFontForContentSizeCategory = YES;
  promoTextLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  promoTextLabel.numberOfLines = 0;

  return promoTextLabel;
}

// Creates and configures the more info button.
- (UIButton*)createMoreInfoButton {
  UIButton* moreInfoButton = [UIButton buttonWithType:UIButtonTypeSystem];
  moreInfoButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlue600Color];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kMoreInfoButtonContentInset, kMoreInfoButtonContentInset,
      kMoreInfoButtonContentInset, kMoreInfoButtonContentInset);
  buttonConfiguration.titleLineBreakMode = NSLineBreakByWordWrapping;
  buttonConfiguration.titleAlignment =
      UIButtonConfigurationTitleAlignmentCenter;
  moreInfoButton.configuration = buttonConfiguration;

  return moreInfoButton;
}

// Creates and configures the view composed of the image view and the new
// feature badge.
- (UIView*)createBadgedImageViewWithImageView:(UIImageView*)imageView
                          newFeatureBadgeView:(NewFeatureBadgeView*)badgeView {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;

  [view addSubview:imageView];
  [view addSubview:badgeView];

  [NSLayoutConstraint activateConstraints:@[
    // `imageView` constraints.
    [imageView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [imageView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [imageView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [imageView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [imageView.widthAnchor constraintEqualToConstant:kImageSize],
    [imageView.heightAnchor constraintEqualToConstant:kImageSize],

    // `badgeView` constraints.
    [badgeView.widthAnchor constraintEqualToConstant:kNewFeatureBadgeSize],
    [badgeView.heightAnchor constraintEqualToConstant:kNewFeatureBadgeSize],
    [badgeView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [badgeView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
  ]];

  return view;
}

@end
