// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_link_cell.h"

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

}  // namespace

@implementation HomeCustomizationLinkCell {
  // The type for this link cell, indicating what URL it represents.
  CustomizationLinkType _type;

  // The stack view containing all the content of the cell.
  UIStackView* _contentStackView;

  // The text stack view containing the title and subtitle.
  UIStackView* _textStackView;
  UILabel* _title;
  UILabel* _subtitle;

  // The icon on the right-hand side of the cell.
  UIImageView* _iconImageView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Configure `contentView`, representing the overall container.
    self.contentView.backgroundColor = [UIColor colorNamed:kGrey100Color];
    self.contentView.layer.cornerRadius = kContainerBorderRadius;
    self.contentView.layoutMargins = UIEdgeInsetsMake(
        kVerticalMargin, kHorizontalMargin, kVerticalMargin, kHorizontalMargin);

    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(navigateToLink:)];
    [self.contentView addGestureRecognizer:tapRecognizer];

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

    _iconImageView = [[UIImageView alloc]
        initWithImage:DefaultSymbolWithPointSize(kExternalLinkSymbol,
                                                 kToggleIconPointSize)];

    _contentStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textStackView, _iconImageView ]];
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

- (void)configureCellWithType:(CustomizationLinkType)type {
  _type = type;

  _title.text = [HomeCustomizationHelper titleForLinkType:type];
  _subtitle.text = [HomeCustomizationHelper subtitleForLinkType:type];

  self.accessibilityIdentifier =
      [HomeCustomizationHelper accessibilityIdentifierForLinkType:type];
}

#pragma mark - Private

// Prepares the cell for reuse by the collection view.
- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessibilityIdentifier = nil;
  _title.text = nil;
  _subtitle.text = nil;
}

// Navigates to the external URL for the cell's type.
- (void)navigateToLink:(UIView*)sender {
  [self.mutator navigateToLinkForType:_type];
}

@end
