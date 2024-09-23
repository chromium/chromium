// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"

#import "base/check.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// The border radius for the entire cell.
const CGFloat kContainerBorderRadius = 12;

// The margins of the container.
const CGFloat kHorizontalMargin = 12;
const CGFloat kVerticalMargin = 8;

// The width of the main icon container.
const CGFloat kIconImageViewWidth = 32;

// The width of the navigation icon container.
const CGFloat kNavigationIconImageViewWidth = 16;

}  // namespace

@implementation HomeCustomizationToggleCell {
  // The type for this toggle cell, indicating what module it represents.
  CustomizationToggleType _type;

  // The horizontal stack view containing all the cell's content.
  UIStackView* _contentStackView;

  // The horizontal stack view containing the content which can be tapped to
  // navigate to its submenu.
  UIStackView* _navigableStackView;

  // The text stack view containing the title and subtitle.
  UIStackView* _textStackView;
  UILabel* _title;
  UILabel* _subtitle;

  // The main icon representing what the cell controls.
  UIView* _iconContainer;
  UIImageView* _iconImageView;

  // The navigation icon, visible if the cell can be tapped to access a submenu.
  UIView* _navigationIconContainer;
  UIImageView* _navigationImageView;
  UIView* _navigationSeparator;

  // The switch to toggle module visibility.
  UISwitch* _switch;

  // The tap recognizer for navigating to the cell's submenu, if supported.
  UITapGestureRecognizer* _tapRecognizer;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Configure `contentView`, representing the overall container.
    self.contentView.backgroundColor = [UIColor colorNamed:kGrey100Color];
    self.contentView.layer.cornerRadius = kContainerBorderRadius;
    self.contentView.layoutMargins = UIEdgeInsetsMake(
        kVerticalMargin, kHorizontalMargin, kVerticalMargin, kHorizontalMargin);

    // Sets the title, subtitle and stack view to lay them out vertically.
    _title = [[UILabel alloc] init];
    _title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _title.numberOfLines = 0;
    _title.textColor = [UIColor colorNamed:kTextPrimaryColor];

    _subtitle = [[UILabel alloc] init];
    _subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitle.numberOfLines = 2;
    _subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];

    _textStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _title, _subtitle ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textStackView.axis = UILayoutConstraintAxisVertical;
    _textStackView.alignment = UIStackViewAlignmentLeading;
    _textStackView.spacing = 0;

    // Creates the image with a placeholder icon which will be replaced when the
    // cell is configured.
    _iconContainer = [[UIView alloc] init];
    _iconImageView = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kSliderHorizontalSymbol,
                                                 kToggleIconPointSize)];
    _iconImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    [_iconContainer addSubview:_iconImageView];
    _iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_iconContainer.widthAnchor
          constraintEqualToConstant:kIconImageViewWidth],
      [_iconContainer.centerXAnchor
          constraintEqualToAnchor:_iconImageView.centerXAnchor],
      [_iconContainer.centerYAnchor
          constraintEqualToAnchor:_iconImageView.centerYAnchor],
    ]];

    // Creates navigation icon, switch and stack view to lay them out
    // horizontally.
    _navigationIconContainer = [[UIView alloc] init];
    _navigationImageView = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kChevronForwardSymbol,
                                                 kToggleIconPointSize)];
    _navigationImageView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
    [_navigationIconContainer addSubview:_navigationImageView];
    _navigationIconContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _navigationImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_navigationIconContainer.widthAnchor
          constraintEqualToConstant:kNavigationIconImageViewWidth],
      [_navigationIconContainer.centerXAnchor
          constraintEqualToAnchor:_navigationImageView.centerXAnchor],
      [_navigationIconContainer.centerYAnchor
          constraintEqualToAnchor:_navigationImageView.centerYAnchor],
    ]];

    _switch = [[UISwitch alloc] init];
    [_switch addTarget:self
                  action:@selector(toggleModuleVisibility:)
        forControlEvents:UIControlEventValueChanged];

    _navigationSeparator = [[UIView alloc] init];
    _navigationSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    _navigationSeparator.backgroundColor = [UIColor colorNamed:kGrey200Color];

    _navigableStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _iconContainer,
      _textStackView,
      _navigationIconContainer,
      _navigationSeparator,
    ]];
    _navigableStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _navigableStackView.axis = UILayoutConstraintAxisHorizontal;
    _navigableStackView.alignment = UIStackViewAlignmentCenter;
    _navigableStackView.spacing = UIStackViewSpacingUseSystem;

    // Anchor separator to the full height of the cell, which includes the
    // margin.
    [NSLayoutConstraint activateConstraints:@[
      [_navigationSeparator.widthAnchor constraintEqualToConstant:1],
      [_navigationSeparator.heightAnchor
          constraintEqualToAnchor:_navigableStackView.heightAnchor
                         constant:kVerticalMargin * 2],
    ]];

    _contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _navigableStackView, _switch ]];
    _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _contentStackView.axis = UILayoutConstraintAxisHorizontal;
    _contentStackView.alignment = UIStackViewAlignmentCenter;
    _contentStackView.spacing = UIStackViewSpacingUseSystem;

    [self.contentView addSubview:_contentStackView];

    AddSameConstraints(_contentStackView, self.contentView.layoutMarginsGuide);
  }
  return self;
}

#pragma mark - Public

- (void)configureCellWithType:(CustomizationToggleType)type
                      enabled:(BOOL)enabled {
  [self prepareForReuse];

  _type = type;

  _title.text = [HomeCustomizationHelper titleForToggleType:type];
  _subtitle.text = [HomeCustomizationHelper subtitleForToggleType:type];
  _iconImageView.image = [HomeCustomizationHelper iconForToggleType:type];
  _switch.on = enabled;

  if ([HomeCustomizationHelper doesTypeHaveSubmenu:type]) {
    _tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(navigateToSubmenu:)];
    [_navigableStackView addGestureRecognizer:_tapRecognizer];
  }

  [self updateNavigationIconForSwitchEnabled:enabled];

  self.accessibilityIdentifier =
      [HomeCustomizationHelper accessibilityIdentifierForToggleType:type];

  _navigableStackView.accessibilityIdentifier = [HomeCustomizationHelper
      navigableAccessibilityIdentifierForToggleType:type];
}

#pragma mark - Private

// Prepares the cell for reuse by the collection view.
- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessibilityIdentifier = nil;
  _title.text = nil;
  _subtitle.text = nil;
  _iconImageView.image = nil;
  _switch.on = NO;
  _navigationImageView.hidden = NO;
  [_navigableStackView removeGestureRecognizer:_tapRecognizer];
  _tapRecognizer = nil;
}

// Handles the cell's UISwitch being toggled, which triggers a change in the
// module's visibility.
- (void)toggleModuleVisibility:(UISwitch*)sender {
  BOOL isEnabling = sender.isOn;
  [self.mutator toggleModuleVisibilityForType:_type enabled:isEnabling];
}

// Navigates to the customization submenu for the cell's type.
- (void)navigateToSubmenu:(UIView*)sender {
  CHECK([HomeCustomizationHelper doesTypeHaveSubmenu:_type]);
  [self.mutator navigateToSubmenuForType:_type];
}

// Updates the navigation icon's visibility and the tap recognizer for the
// current state of the switch.
- (void)updateNavigationIconForSwitchEnabled:(BOOL)enabled {
  // Show/hide the navigation icon when toggling the switch.
  BOOL shouldShowNavigationIcon =
      [HomeCustomizationHelper doesTypeHaveSubmenu:_type] && enabled;
  _navigationImageView.hidden = !shouldShowNavigationIcon;
  _navigationSeparator.hidden = !shouldShowNavigationIcon;
  _title.accessibilityTraits |= shouldShowNavigationIcon
                                    ? UIAccessibilityTraitButton
                                    : UIAccessibilityTraitNone;

  // Enable/disable the navigation on tap.
  [_tapRecognizer setEnabled:enabled];
}

@end
