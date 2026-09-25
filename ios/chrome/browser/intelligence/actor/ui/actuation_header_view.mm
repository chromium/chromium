// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/gradient_activity_indicator_view.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;
using intelligence::actor::kSpacingTiny;
using intelligence::actor::kTimelineGutterWidth;

// Layout dimensions.
const CGFloat kInnerContentSize = 32.0;
const CGFloat kLogoSize = 24.0;
const CGFloat kSpinnerSize = 32.0;
const CGFloat kHeaderMinHeight = 44.0;

// Shadow styling.
const CGFloat kButtonShadowRadius = 6.0;
const CGFloat kButtonShadowOpacity = 0.16f;
const CGFloat kButtonShadowOffset = 2.0;

// TODO(crbug.com/552996431): Centralize Gemini logo in a shared symbol helper.
UIImage* DefaultGeminiLogo() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SymbolWithPointSize(SymbolGeminiBrandedLogo, kLogoSize);
#else
  return SymbolWithPointSize(SymbolGeminiNonBrandedLogo, kLogoSize);
#endif
}

// Creates an icon button styled by `buttonConfig` and wired to `item`. The
// action is added as a target rather than passed to the button initializer,
// since the latter would render the action title next to the icon.
UIButton* CreateButton(UIButtonConfiguration* buttonConfig,
                       ActuationHeaderItem* item) {
  buttonConfig.image = item.icon;
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfig
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityLabel = item.title;
  button.accessibilityIdentifier = item.accessibilityIdentifier;
  if (item.menu) {
    button.menu = item.menu;
    button.showsMenuAsPrimaryAction = YES;
  } else {
    [button addAction:item.action
        forControlEvents:UIControlEventPrimaryActionTriggered];
  }
  AddSquareConstraints(button, kInnerContentSize);
  return button;
}

// Creates a standalone white circular icon button with a drop shadow.
UIButton* CreateCircularButton(ActuationHeaderItem* item) {
  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfig.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  buttonConfig.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  buttonConfig.baseBackgroundColor = [UIColor colorNamed:kSolidWhiteColor];

  UIButton* button = CreateButton(buttonConfig, item);
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
  button.layer.shadowPath =
      [UIBezierPath bezierPathWithOvalInRect:CGRectMake(0, 0, kInnerContentSize,
                                                        kInnerContentSize)]
          .CGPath;
  return button;
}

// Creates a gray capsule grouping one borderless icon button per item. Its
// height matches the circular buttons so both styles stay vertically aligned.
UIView* CreateGroupedCapsule(NSArray<ActuationHeaderItem*>* items) {
  UIStackView* capsule = [[UIStackView alloc] init];
  for (ActuationHeaderItem* item in items) {
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
    buttonConfig.contentInsets = NSDirectionalEdgeInsetsZero;
    [capsule addArrangedSubview:CreateButton(buttonConfig, item)];
  }
  capsule.backgroundColor = [UIColor colorNamed:kGrey100Color];
  capsule.layer.cornerRadius = kInnerContentSize / 2.0;
  capsule.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0.0, kSpacingTiny, 0.0, kSpacingTiny);
  capsule.layoutMarginsRelativeArrangement = YES;
  return capsule;
}

}  // namespace

@implementation ActuationHeaderView {
  UIView* _iconContainer;
  UIImageView* _imageView;
  GradientActivityIndicatorView* _activityIndicator;

  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UIStackView* _textStackView;

  UIStackView* _accessoryStackView;
  UIStackView* _contentStackView;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _actuating = NO;

    [self setupSubviews];
    [self setupConstraints];
  }
  return self;
}

- (void)reset {
  self.title = nil;
  self.subtitle = nil;
  self.actuating = NO;
  self.primaryItem = nil;
  self.secondaryItems = nil;
}

- (void)setTitle:(NSString*)title {
  _title = [title copy];
  _titleLabel.text = _title;
  _titleLabel.hidden = (_title.length == 0);
  // TODO(crbug.com/552512657): Configure accessibility properties and labels.
}

- (void)setSubtitle:(NSString*)subtitle {
  _subtitle = [subtitle copy];
  _subtitleLabel.text = _subtitle;
  _subtitleLabel.hidden = (_subtitle.length == 0);
  // TODO(crbug.com/552512657): Add support for layout progress and
  // interpolation of subtitle visibility during detent changes.
}

- (void)setActuating:(BOOL)actuating {
  if (_actuating == actuating) {
    return;
  }
  _actuating = actuating;
  if (_actuating) {
    [_activityIndicator startAnimating];
  } else {
    [_activityIndicator stopAnimating];
  }
}

- (void)setPrimaryItem:(ActuationHeaderItem*)primaryItem {
  _primaryItem = primaryItem;
  [self updateAccessoryStack];
}

- (void)setSecondaryItems:(NSArray<ActuationHeaderItem*>*)secondaryItems {
  _secondaryItems = [secondaryItems copy];
  [self updateAccessoryStack];
}

#pragma mark - Private

// Creates and configures the subviews including the root horizontal stack.
- (void)setupSubviews {
  _iconContainer = [[UIView alloc] init];
  _iconContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _imageView = [[UIImageView alloc] initWithImage:DefaultGeminiLogo()];
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [_iconContainer addSubview:_imageView];

  _activityIndicator =
      [[GradientActivityIndicatorView alloc] initWithFrame:CGRectZero];
  _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  [_iconContainer addSubview:_activityIndicator];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleSubheadline, UIFontWeightBold);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _subtitleLabel.hidden = YES;

  _textStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
  _textStackView.axis = UILayoutConstraintAxisVertical;
  _textStackView.alignment = UIStackViewAlignmentLeading;
  _textStackView.spacing = kSpacingTiny;
  [_textStackView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _accessoryStackView = [[UIStackView alloc] init];
  _accessoryStackView.alignment = UIStackViewAlignmentCenter;
  _accessoryStackView.spacing = kSpacingSmall;
  [_accessoryStackView
      setContentHuggingPriority:UILayoutPriorityRequired
                        forAxis:UILayoutConstraintAxisHorizontal];
  [_accessoryStackView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  _accessoryStackView.hidden = YES;

  _contentStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _iconContainer, _textStackView, _accessoryStackView
  ]];
  _contentStackView.axis = UILayoutConstraintAxisHorizontal;
  _contentStackView.alignment = UIStackViewAlignmentCenter;
  _contentStackView.spacing = 0.0;
  [_contentStackView setCustomSpacing:kSpacingSmall afterView:_textStackView];
  _contentStackView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kSpacingSmall, 0.0, kSpacingSmall, kSpacingLarge);
  _contentStackView.layoutMarginsRelativeArrangement = YES;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentStackView];
}

// Configures layout constraints.
- (void)setupConstraints {
  AddSameConstraints(_contentStackView, self);
  [self.heightAnchor constraintGreaterThanOrEqualToConstant:kHeaderMinHeight]
      .active = YES;
  [_iconContainer.widthAnchor constraintEqualToConstant:kTimelineGutterWidth]
      .active = YES;
  AddSameCenterConstraints(_imageView, _iconContainer);
  AddSquareConstraints(_imageView, kLogoSize);
  AddSameCenterConstraints(_activityIndicator, _imageView);
  AddSquareConstraints(_activityIndicator, kSpinnerSize);
}

// Rebuilds the accessory buttons stack in deterministic order:
// `[secondaryItems (leading), primaryItem (trailing)]`. Buttons are recreated
// because their style depends on the number of secondary items.
- (void)updateAccessoryStack {
  for (UIView* view in _accessoryStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }

  if (_secondaryItems.count == 1) {
    [_accessoryStackView
        addArrangedSubview:CreateCircularButton(_secondaryItems.firstObject)];
  } else if (_secondaryItems.count > 1) {
    [_accessoryStackView
        addArrangedSubview:CreateGroupedCapsule(_secondaryItems)];
  }
  if (_primaryItem) {
    [_accessoryStackView addArrangedSubview:CreateCircularButton(_primaryItem)];
  }
  _accessoryStackView.hidden =
      (_accessoryStackView.arrangedSubviews.count == 0);
}

@end
