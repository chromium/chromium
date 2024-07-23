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

// The horizontal spacing between the cell's icon and text.
const CGFloat kSpacingBetweenIconAndText = 24;

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
  UIImageView* _iconImageView;

  // The navigation icon, visible if the cell can be tapped to access a submenu.
  UIImageView* _navigationImageView;

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

    // Sets a placeholder icon which will be replaced when the cell is
    // configured.
    _iconImageView = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kSliderHorizontalSymbol,
                                                 kToggleIconPointSize)];

    // Creates navigation icon, switch and stack view to lay them out
    // horizontally.
    UIImage* navigationIcon =
        DefaultSymbolWithPointSize(kChevronForwardSymbol, kToggleIconPointSize);
    _navigationImageView = [[UIImageView alloc] initWithImage:navigationIcon];

    _switch = [[UISwitch alloc] init];
    [_switch addTarget:self
                  action:@selector(toggleModuleVisibility:)
        forControlEvents:UIControlEventValueChanged];

    _navigableStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _iconImageView,
      _textStackView,
      _navigationImageView,
    ]];
    _navigableStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _navigableStackView.axis = UILayoutConstraintAxisHorizontal;
    _navigableStackView.alignment = UIStackViewAlignmentCenter;
    _navigableStackView.spacing = UIStackViewSpacingUseSystem;

    _contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _navigableStackView, _switch ]];
    _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _contentStackView.axis = UILayoutConstraintAxisHorizontal;
    _contentStackView.alignment = UIStackViewAlignmentCenter;
    _contentStackView.spacing = UIStackViewSpacingUseSystem;

    [self.contentView addSubview:_contentStackView];

    [_contentStackView setCustomSpacing:kSpacingBetweenIconAndText
                              afterView:_iconImageView];

    AddSameConstraints(_contentStackView, self.contentView.layoutMarginsGuide);
  }
  return self;
}

#pragma mark - Public

- (void)configureCellWithType:(CustomizationToggleType)type
                      enabled:(BOOL)enabled {
  _type = type;

  _title.text = [HomeCustomizationHelper titleForToggleType:type];
  _subtitle.text = [HomeCustomizationHelper subtitleForToggleType:type];
  _iconImageView.image = [HomeCustomizationHelper iconForToggleType:type];
  _switch.on = enabled;
  _navigationImageView.hidden =
      ![HomeCustomizationHelper doesTypeHaveSubmenu:type];

  if (enabled && [HomeCustomizationHelper doesTypeHaveSubmenu:type]) {
    _tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(navigateToSubmenu:)];
    [_navigableStackView addGestureRecognizer:_tapRecognizer];
  }

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
  if ([HomeCustomizationHelper doesTypeHaveSubmenu:_type]) {
    _tapRecognizer.enabled = isEnabling;
  }
  [self.mutator toggleModuleVisibilityForType:_type enabled:isEnabling];
}

// Navigates to the customization submenu for the cell's type.
- (void)navigateToSubmenu:(UIView*)sender {
  CHECK([HomeCustomizationHelper doesTypeHaveSubmenu:_type]);
  [self.mutator navigateToSubmenuForType:_type];
}

@end
