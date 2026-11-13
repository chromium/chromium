// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/ui/intelligence_header_view.h"

#import "ios/chrome/browser/intelligence/ui/gradient_activity_indicator_view.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_ui_constants.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Layout dimensions.
const CGFloat kInnerContentSize = 32.0;
const CGFloat kLogoSize = 24.0;
const CGFloat kSpinnerSize = 32.0;
const CGFloat kHeaderMinHeight = 44.0;

// Shadow styling.
const CGFloat kButtonShadowRadius = 6.0;
const CGFloat kButtonShadowOpacity = 0.16f;
const CGFloat kButtonShadowOffset = 2.0;

// TODO(crbug.com/552996431): Centralize default leading logo in a shared symbol
// helper.
UIImage* DefaultLeadingLogo() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SymbolWithPointSize(SymbolGeminiBrandedLogo, kLogoSize);
#else
  return SymbolWithPointSize(SymbolGeminiNonBrandedLogo, kLogoSize);
#endif
}

// Wires `button` to `item`'s action or menu and accessibility properties. The
// action is added as a target rather than passed to the button initializer,
// since the latter would override the configuration's image and title.
void ConfigureButtonForItem(UIButton* button, IntelligenceHeaderItem* item) {
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
}

// Creates an icon button styled by `buttonConfig` and wired to `item`.
UIButton* CreateIconButton(UIButtonConfiguration* buttonConfig,
                           IntelligenceHeaderItem* item) {
  buttonConfig.image = item.icon;
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfig
                                         primaryAction:nil];
  ConfigureButtonForItem(button, item);
  AddSquareConstraints(button, kInnerContentSize);
  return button;
}

// Creates a standalone white circular icon button with a drop shadow.
UIButton* CreateCircularButton(IntelligenceHeaderItem* item) {
  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfig.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  buttonConfig.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  buttonConfig.baseBackgroundColor = [UIColor colorNamed:kSolidWhiteColor];

  UIButton* button = CreateIconButton(buttonConfig, item);
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

// Creates a blue capsule button displaying `item.title`. Its height matches the
// icon buttons so all accessories stay vertically aligned; its width hugs the
// title.
UIButton* CreateCallToActionButton(IntelligenceHeaderItem* item) {
  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfig.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  buttonConfig.baseForegroundColor = [UIColor colorNamed:kSolidButtonTextColor];
  buttonConfig.baseBackgroundColor = [UIColor colorNamed:kBlueColor];
  buttonConfig.title = item.title;
  buttonConfig.titleLineBreakMode = NSLineBreakByTruncatingTail;
  buttonConfig.contentInsets = NSDirectionalEdgeInsetsMake(
      0.0, kIntelligenceSpacingMedium, 0.0, kIntelligenceSpacingMedium);
  UIButton* button = [UIButton buttonWithConfiguration:buttonConfig
                                         primaryAction:nil];
  UIFont* titleFont = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                                UIFontWeightSemibold);
  SetConfigurationFont(button, titleFont);
  ConfigureButtonForItem(button, item);
  [button.heightAnchor constraintEqualToConstant:kInnerContentSize].active =
      YES;
  return button;
}

// Creates a standalone button rendered according to `item.style`.
UIButton* CreateStandaloneButton(IntelligenceHeaderItem* item) {
  switch (item.style) {
    case IntelligenceHeaderItemStyle::kIcon:
      return CreateCircularButton(item);
    case IntelligenceHeaderItemStyle::kCallToAction:
      return CreateCallToActionButton(item);
  }
}

// Creates a gray capsule grouping one borderless icon button per item. Its
// height matches the circular buttons so both styles stay vertically aligned.
UIView* CreateGroupedCapsule(NSArray<IntelligenceHeaderItem*>* items) {
  UIStackView* capsule = [[UIStackView alloc] init];
  for (IntelligenceHeaderItem* item in items) {
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
    buttonConfig.contentInsets = NSDirectionalEdgeInsetsZero;
    [capsule addArrangedSubview:CreateIconButton(buttonConfig, item)];
  }
  capsule.backgroundColor = [UIColor colorNamed:kGrey100Color];
  capsule.layer.cornerRadius = kInnerContentSize / 2.0;
  capsule.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0.0, kIntelligenceSpacingTiny, 0.0, kIntelligenceSpacingTiny);
  capsule.layoutMarginsRelativeArrangement = YES;
  return capsule;
}

// Returns whether `items` should be rendered as a grouped capsule. Grouping
// only applies to two or more icon items; any other combination is rendered as
// standalone buttons in the given order.
bool ShouldGroupItems(NSArray<IntelligenceHeaderItem*>* items) {
  if (items.count < 2) {
    return false;
  }
  for (IntelligenceHeaderItem* item in items) {
    if (item.style != IntelligenceHeaderItemStyle::kIcon) {
      return false;
    }
  }
  return true;
}

}  // namespace

@implementation IntelligenceHeaderView {
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
    _loading = NO;
    _density = IntelligenceHeaderDensity::kRegular;

    [self setupSubviews];
    [self setupConstraints];
  }
  return self;
}

- (void)reset {
  self.title = nil;
  self.subtitle = nil;
  self.loading = NO;
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

- (void)setLoading:(BOOL)loading {
  if (_loading == loading) {
    return;
  }
  _loading = loading;
  if (_loading) {
    [_activityIndicator startAnimating];
  } else {
    [_activityIndicator stopAnimating];
  }
}

- (void)setDensity:(IntelligenceHeaderDensity)density {
  if (_density == density) {
    return;
  }
  _density = density;
  [self updateContentMargins];
}

- (void)setPrimaryItem:(IntelligenceHeaderItem*)primaryItem {
  _primaryItem = primaryItem;
  [self updateAccessoryStack];
}

- (void)setSecondaryItems:(NSArray<IntelligenceHeaderItem*>*)secondaryItems {
  _secondaryItems = [secondaryItems copy];
  [self updateAccessoryStack];
}

#pragma mark - Private

// Creates and configures the subviews including the root horizontal stack.
- (void)setupSubviews {
  _iconContainer = [[UIView alloc] init];
  _iconContainer.translatesAutoresizingMaskIntoConstraints = NO;

  _imageView = [[UIImageView alloc] initWithImage:DefaultLeadingLogo()];
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
  _textStackView.spacing = kIntelligenceSpacingTiny;
  [_textStackView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _accessoryStackView = [[UIStackView alloc] init];
  _accessoryStackView.alignment = UIStackViewAlignmentCenter;
  _accessoryStackView.spacing = kIntelligenceSpacingSmall;
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
  [_contentStackView setCustomSpacing:kIntelligenceSpacingSmall
                            afterView:_textStackView];
  _contentStackView.layoutMarginsRelativeArrangement = YES;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentStackView];
  [self updateContentMargins];
}

// Configures layout constraints.
- (void)setupConstraints {
  AddSameConstraints(_contentStackView, self);
  [self.heightAnchor constraintGreaterThanOrEqualToConstant:kHeaderMinHeight]
      .active = YES;
  [_iconContainer.widthAnchor
      constraintEqualToConstant:kIntelligenceHeaderLeadingIconContainerWidth]
      .active = YES;
  AddSameCenterConstraints(_imageView, _iconContainer);
  AddSquareConstraints(_imageView, kLogoSize);
  AddSameCenterConstraints(_activityIndicator, _imageView);
  AddSquareConstraints(_activityIndicator, kSpinnerSize);
}

// Applies the content margins matching `density`. The vertical margins are
// kept equal so that the content stays vertically centered.
- (void)updateContentMargins {
  CGFloat vertical = _density == IntelligenceHeaderDensity::kRegular
                         ? kIntelligenceSpacingSmall
                         : kIntelligenceSpacingTiny;
  _contentStackView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      vertical, 0.0, vertical, kIntelligenceSpacingLarge);
}

// Rebuilds the accessory buttons stack in deterministic order:
// `[secondaryItems (leading), primaryItem (trailing)]`. Buttons are recreated
// because their style depends on the composition of the secondary items.
- (void)updateAccessoryStack {
  for (UIView* view in _accessoryStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }

  if (ShouldGroupItems(_secondaryItems)) {
    [_accessoryStackView
        addArrangedSubview:CreateGroupedCapsule(_secondaryItems)];
  } else {
    for (IntelligenceHeaderItem* item in _secondaryItems) {
      [_accessoryStackView addArrangedSubview:CreateStandaloneButton(item)];
    }
  }
  if (_primaryItem) {
    [_accessoryStackView
        addArrangedSubview:CreateStandaloneButton(_primaryItem)];
  }
  _accessoryStackView.hidden =
      (_accessoryStackView.arrangedSubviews.count == 0);
}

@end
