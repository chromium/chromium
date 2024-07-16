// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_toggle_cell.h"

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
                  action:@selector(handleSwitchToggled:)
        forControlEvents:UIControlEventValueChanged];

    _contentStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _iconImageView, _textStackView, _navigationImageView, _switch
    ]];
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

  self.accessibilityIdentifier =
      [HomeCustomizationHelper accessibilityIdentifierForToggleType:type];
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
}

// Handles the cell's UISwitch being toggled.
- (void)handleSwitchToggled:(UISwitch*)sender {
  [self.mutator handleModuleToggledWithType:_type enabled:sender.isOn];
}

@end
