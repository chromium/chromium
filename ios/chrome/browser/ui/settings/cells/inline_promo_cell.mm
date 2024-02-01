// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/inline_promo_cell.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing of the stack views.
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

// Content inset for the top, leading and bottom anchors of the badged image
// view. Used for the wide layout only.
constexpr CGFloat kBadgedImageViewContentInsetWideLayout = 4;

// Horizontal spacing betwen the stack view and the close button. Used for the
// wide layout only.
constexpr CGFloat kStackViewCloseButtonContentInsetWideLayout = 8;

// Mimimum height of the promo text label. Used so that the more info button
// doesn't take too much space in the vertical stack view. Used for the wide
// layout only.
constexpr CGFloat kPromoTextLabelMinHeightWideLayout = 60;

}  // namespace

@implementation InlinePromoCell {
  // New feature badge that is overlaying part of the promo image view.
  NewFeatureBadgeView* _badgeView;

  // View containing the image view and the new feature badge.
  UIView* _badgedImageView;

  // Vertical stack used to lay out elements both in the narrow and wide
  // layouts.
  UIStackView* _verticalStackView;

  // Horizontal stack used to lay out elements in the wide layout.
  UIStackView* _horizontalStackView;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _enabled = YES;
    _closeButton = [self createCloseButton];
    _promoImageView = [self createPromoImageView];
    _badgeView = [self createNewFeatureBadgeView];
    _badgedImageView = [self createBadgedImageViewWithImageView:_promoImageView
                                            newFeatureBadgeView:_badgeView];
    _promoTextLabel = [self createPromoTextLabel];
    _moreInfoButton = [self createMoreInfoButton];
    _verticalStackView = [self createVerticalStackView];
    _horizontalStackView = [self createHorizontalStackView];

    [self.contentView addSubview:_closeButton];

    if (_shouldHaveWideLayout) {
      [self setUpCellForWideLayoutWith:self.contentView
                           closeButton:_closeButton
                       badgedImageView:_badgedImageView
                        promoTextLabel:_promoTextLabel
                        moreInfoButton:_moreInfoButton
                     verticalStackView:_verticalStackView
                   horizontalStackView:_horizontalStackView];

    } else {
      [self setUpCellForNarrowLayoutWith:self.contentView
                         badgedImageView:_badgedImageView
                          promoTextLabel:_promoTextLabel
                          moreInfoButton:_moreInfoButton
                       verticalStackView:_verticalStackView];
    }

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

      // `_badgedImageView` constraints.
      [_badgedImageView.widthAnchor constraintEqualToConstant:kImageSize],
      [_badgedImageView.heightAnchor constraintEqualToConstant:kImageSize],

      // `_moreInfoButton` constraints.
      [_moreInfoButton.heightAnchor
          constraintGreaterThanOrEqualToConstant:kMoreInfoButtonHeight],
    ]];

    // Make sure the `_closeButton` is not behind a stack view.
    [self.contentView bringSubviewToFront:_closeButton];
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

- (void)setShouldHaveWideLayout:(BOOL)shouldHaveWideLayout {
  if (_shouldHaveWideLayout == shouldHaveWideLayout) {
    return;
  }

  _shouldHaveWideLayout = shouldHaveWideLayout;
  [self configureCellForLayoutChangeWith:shouldHaveWideLayout
                             closeButton:_closeButton
                         badgedImageView:_badgedImageView
                          promoTextLabel:_promoTextLabel
                          moreInfoButton:_moreInfoButton
                       verticalStackView:_verticalStackView
                     horizontalStackView:_horizontalStackView];
}

#pragma mark - Private

// Creates and configures the promo's close button.
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
  moreInfoButton.contentHorizontalAlignment =
      UIControlContentHorizontalAlignmentCenter;

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

// Creates and configures the vertical stack view.
- (UIStackView*)createVerticalStackView {
  UIStackView* verticalStackView = [[UIStackView alloc] init];
  verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStackView.axis = UILayoutConstraintAxisVertical;
  verticalStackView.alignment = UIStackViewAlignmentCenter;
  verticalStackView.spacing = kStackViewSpacing;

  return verticalStackView;
}

// Creates and configures the horizontal stack view.
- (UIStackView*)createHorizontalStackView {
  UIStackView* horizontalStackView = [[UIStackView alloc] init];
  horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  horizontalStackView.alignment = UIStackViewAlignmentCenter;
  horizontalStackView.spacing = kStackViewSpacing;

  return horizontalStackView;
}

// Sets up the subviews for the cell's narrow layout.
- (void)setUpCellForNarrowLayoutWith:(UIView*)contentView
                     badgedImageView:(UIView*)badgedImageView
                      promoTextLabel:(UILabel*)promoTextLabel
                      moreInfoButton:(UIButton*)moreInfoButton
                   verticalStackView:(UIStackView*)verticalStackView {
  [verticalStackView addArrangedSubview:badgedImageView];
  [verticalStackView addArrangedSubview:promoTextLabel];
  [verticalStackView addArrangedSubview:moreInfoButton];

  [contentView addSubview:verticalStackView];

  [NSLayoutConstraint activateConstraints:@[
    // `verticalStackView` constraints.
    [verticalStackView.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                                constant:kCellContentInset],
    [verticalStackView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kCellContentInset],
    [verticalStackView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kCellContentInset],
    [verticalStackView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kCellContentInset],

    // `badgedImageView` constraints.
    [badgedImageView.topAnchor
        constraintEqualToAnchor:verticalStackView.topAnchor],

    // `promoTextLabel` constraints.
    [promoTextLabel.leadingAnchor
        constraintEqualToAnchor:verticalStackView.leadingAnchor],
    [promoTextLabel.trailingAnchor
        constraintEqualToAnchor:verticalStackView.trailingAnchor],

    // `moreInfoButton` constraints.
    [moreInfoButton.leadingAnchor
        constraintEqualToAnchor:verticalStackView.leadingAnchor],
    [moreInfoButton.trailingAnchor
        constraintEqualToAnchor:verticalStackView.trailingAnchor],
    [moreInfoButton.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kCellBottomContentInset],
  ]];
}

// Sets up the subviews for the cell's wide layout.
- (void)setUpCellForWideLayoutWith:(UIView*)contentView
                       closeButton:(UIButton*)closeButton
                   badgedImageView:(UIView*)badgedImageView
                    promoTextLabel:(UILabel*)promoTextLabel
                    moreInfoButton:(UIButton*)moreInfoButton
                 verticalStackView:(UIStackView*)verticalStackView
               horizontalStackView:(UIStackView*)horizontalStackView {
  [verticalStackView addArrangedSubview:promoTextLabel];
  [verticalStackView addArrangedSubview:moreInfoButton];

  [horizontalStackView addArrangedSubview:badgedImageView];
  [horizontalStackView addArrangedSubview:verticalStackView];

  [contentView addSubview:horizontalStackView];

  [NSLayoutConstraint activateConstraints:@[

    // `horizontalStackView` constraints.
    [horizontalStackView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kCellContentInset],
    [horizontalStackView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-kCellContentInset],
    [horizontalStackView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kCellContentInset],
    [horizontalStackView.trailingAnchor
        constraintEqualToAnchor:closeButton.leadingAnchor
                       constant:-kStackViewCloseButtonContentInsetWideLayout],

    // `badgedImageView` constraints.
    [badgedImageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:horizontalStackView.topAnchor
                                    constant:
                                        kBadgedImageViewContentInsetWideLayout],
    [badgedImageView.bottomAnchor
        constraintLessThanOrEqualToAnchor:horizontalStackView.bottomAnchor
                                 constant:
                                     -kBadgedImageViewContentInsetWideLayout],
    [badgedImageView.leadingAnchor
        constraintEqualToAnchor:horizontalStackView.leadingAnchor
                       constant:kBadgedImageViewContentInsetWideLayout],

    // `promoTextLabel` constraints.
    [promoTextLabel.leadingAnchor
        constraintEqualToAnchor:verticalStackView.leadingAnchor],
    [promoTextLabel.trailingAnchor
        constraintEqualToAnchor:verticalStackView.trailingAnchor],
    [promoTextLabel.heightAnchor constraintGreaterThanOrEqualToConstant:
                                     kPromoTextLabelMinHeightWideLayout],

    // `moreInfoButton` constraints.
    [moreInfoButton.leadingAnchor
        constraintEqualToAnchor:verticalStackView.leadingAnchor],
    [moreInfoButton.trailingAnchor
        constraintEqualToAnchor:verticalStackView.trailingAnchor],
    [moreInfoButton.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kCellBottomContentInset],
  ]];
}

// Configure elements with properties that depend on the type of layout.
- (void)configureElementsForLayoutChangeWith:(UILabel*)promoTextLabel
                              moreInfoButton:(UIButton*)moreInfoButton
                           verticalStackView:(UIStackView*)verticalStackView {
  CGFloat moreInfoButtonLeftEdgeInset = 0;

  if (self.shouldHaveWideLayout) {
    promoTextLabel.textAlignment = NSTextAlignmentNatural;
    moreInfoButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentLeft;
    verticalStackView.alignment = UIStackViewAlignmentFill;
    verticalStackView.spacing = 0;
  } else {
    promoTextLabel.textAlignment = NSTextAlignmentCenter;
    moreInfoButton.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentCenter;
    moreInfoButtonLeftEdgeInset = kMoreInfoButtonContentInset;
    verticalStackView.alignment = UIStackViewAlignmentCenter;
    verticalStackView.spacing = kStackViewSpacing;
  }

  UIButtonConfiguration* buttonConfiguration = moreInfoButton.configuration;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kMoreInfoButtonContentInset, moreInfoButtonLeftEdgeInset,
      kMoreInfoButtonContentInset, kMoreInfoButtonContentInset);
  moreInfoButton.configuration = buttonConfiguration;
}

// Configure elements according to the expected layout (narrow or wide).
- (void)configureCellForLayoutChangeWith:(BOOL)shouldHaveWideLayout
                             closeButton:(UIButton*)closeButton
                         badgedImageView:(UIView*)badgedImageView
                          promoTextLabel:(UILabel*)promoTextLabel
                          moreInfoButton:(UIButton*)moreInfoButton
                       verticalStackView:(UIStackView*)verticalStackView
                     horizontalStackView:(UIStackView*)horizontalStackView {
  // Remove subviews to reset their interdependent constraints.
  [badgedImageView removeFromSuperview];
  [promoTextLabel removeFromSuperview];
  [moreInfoButton removeFromSuperview];
  [verticalStackView removeFromSuperview];
  [horizontalStackView removeFromSuperview];

  [self configureElementsForLayoutChangeWith:promoTextLabel
                              moreInfoButton:moreInfoButton
                           verticalStackView:verticalStackView];

  if (shouldHaveWideLayout) {
    [self setUpCellForWideLayoutWith:self.contentView
                         closeButton:closeButton
                     badgedImageView:badgedImageView
                      promoTextLabel:promoTextLabel
                      moreInfoButton:moreInfoButton
                   verticalStackView:verticalStackView
                 horizontalStackView:horizontalStackView];

  } else {
    [self setUpCellForNarrowLayoutWith:self.contentView
                       badgedImageView:badgedImageView
                        promoTextLabel:promoTextLabel
                        moreInfoButton:moreInfoButton
                     verticalStackView:verticalStackView];
  }

  // Make sure the `closeButton` is not behind a stack view.
  [self.contentView bringSubviewToFront:closeButton];
}

@end
