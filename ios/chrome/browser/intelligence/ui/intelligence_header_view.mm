// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/ui/intelligence_header_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/notreached.h"
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

// Returns whether a button built for `old_item` can be updated in place to
// display `new_item`.
bool CanReuseButton(IntelligenceHeaderItem* old_item,
                    IntelligenceHeaderItem* new_item) {
  return old_item && new_item && old_item.style == new_item.style &&
         (old_item.menu == nil) == (new_item.menu == nil) &&
         [old_item.identifier isEqualToString:new_item.identifier];
}

// Returns whether all buttons built for `old_items` can be updated in place to
// display `new_items`. Matching count and styles also guarantees that grouping
// is unchanged.
bool CanReuseSecondaryButtons(NSArray<IntelligenceHeaderItem*>* old_items,
                              NSArray<IntelligenceHeaderItem*>* new_items) {
  if (old_items.count == 0 || old_items.count != new_items.count) {
    return false;
  }
  for (NSUInteger i = 0; i < old_items.count; ++i) {
    if (!CanReuseButton(old_items[i], new_items[i])) {
      return false;
    }
  }
  return true;
}

// Applies `item`'s visual content, menu, and accessibility properties to
// `button`. Shared by initial button creation and in-place reuse so the two
// paths cannot drift apart.
void ApplyItemToButton(UIButton* button, IntelligenceHeaderItem* item) {
  UIButtonConfiguration* config = button.configuration;
  switch (item.style) {
    case IntelligenceHeaderItemStyle::kIcon:
      config.image = item.icon;
      button.configuration = config;
      break;
    case IntelligenceHeaderItemStyle::kCallToAction: {
      config.title = item.title;
      button.configuration = config;
      UIFont* title_font = PreferredFontForTextStyle(UIFontTextStyleSubheadline,
                                                     UIFontWeightSemibold);
      SetConfigurationFont(button, title_font);
      break;
    }
  }
  button.accessibilityLabel = item.title;
  button.accessibilityIdentifier = item.accessibilityIdentifier;
  if (item.menu) {
    button.menu = item.menu;
    button.showsMenuAsPrimaryAction = YES;
  }
}

// Wires `button`'s initial Auto Layout state, tap action, and `item` content.
// The action is added as a target rather than passed to the button initializer,
// since the latter would override the configuration's image and title.
void ConfigureButtonForItem(UIButton* button,
                            IntelligenceHeaderItem* item,
                            IntelligenceHeaderView* header) {
  button.translatesAutoresizingMaskIntoConstraints = NO;
  if (!item.menu) {
    NSString* identifier = item.identifier;
    __weak IntelligenceHeaderView* weak_header = header;
    UIAction* action = [UIAction actionWithHandler:^(UIAction*) {
      [weak_header.delegate headerView:weak_header
          didTriggerItemWithIdentifier:identifier];
    }];
    [button addAction:action
        forControlEvents:UIControlEventPrimaryActionTriggered];
  }
  ApplyItemToButton(button, item);
}

// Creates an icon button styled by `config` and wired to `item`.
UIButton* CreateIconButton(UIButtonConfiguration* config,
                           IntelligenceHeaderItem* item,
                           IntelligenceHeaderView* header) {
  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  ConfigureButtonForItem(button, item, header);
  AddSquareConstraints(button, kInnerContentSize);
  return button;
}

// Creates a standalone white circular icon button with a drop shadow.
UIButton* CreateCircularButton(IntelligenceHeaderItem* item,
                               IntelligenceHeaderView* header) {
  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  config.baseBackgroundColor = [UIColor colorNamed:kSolidWhiteColor];

  UIButton* button = CreateIconButton(config, item, header);
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
UIButton* CreateCallToActionButton(IntelligenceHeaderItem* item,
                                   IntelligenceHeaderView* header) {
  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.baseForegroundColor = [UIColor colorNamed:kSolidButtonTextColor];
  config.baseBackgroundColor = [UIColor colorNamed:kBlueColor];
  config.titleLineBreakMode = NSLineBreakByTruncatingTail;
  config.contentInsets = NSDirectionalEdgeInsetsMake(
      0.0, kIntelligenceSpacingMedium, 0.0, kIntelligenceSpacingMedium);
  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  ConfigureButtonForItem(button, item, header);
  [button.heightAnchor constraintEqualToConstant:kInnerContentSize].active =
      YES;
  return button;
}

// Creates a standalone button rendered according to `item.style`.
UIButton* CreateStandaloneButton(IntelligenceHeaderItem* item,
                                 IntelligenceHeaderView* header) {
  switch (item.style) {
    case IntelligenceHeaderItemStyle::kIcon:
      return CreateCircularButton(item, header);
    case IntelligenceHeaderItemStyle::kCallToAction:
      return CreateCallToActionButton(item, header);
  }
  NOTREACHED();
}

// Creates a gray capsule grouping one borderless icon button per item. Its
// height matches the circular buttons so both styles stay vertically aligned.
UIView* CreateGroupedCapsule(NSArray<IntelligenceHeaderItem*>* items,
                             IntelligenceHeaderView* header) {
  UIStackView* capsule = [[UIStackView alloc] init];
  capsule.translatesAutoresizingMaskIntoConstraints = NO;
  for (IntelligenceHeaderItem* item in items) {
    UIButtonConfiguration* config =
        [UIButtonConfiguration plainButtonConfiguration];
    config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
    config.contentInsets = NSDirectionalEdgeInsetsZero;
    [capsule addArrangedSubview:CreateIconButton(config, item, header)];
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
  UIButton* _primaryButton;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _activity = IntelligenceHeaderActivity::kIdle;
    _density = IntelligenceHeaderDensity::kRegular;

    [self setupSubviews];
    [self setupConstraints];
  }
  return self;
}

- (void)applyConfiguration:(IntelligenceHeaderConfiguration*)configuration {
  CHECK(configuration);
  self.leadingIcon = configuration.leadingIcon;
  self.title = configuration.title;
  self.subtitle = configuration.subtitle;
  self.activity = configuration.activity;
  self.density = configuration.density;
  self.primaryItem = configuration.primaryItem;
  self.secondaryItems = configuration.secondaryItems;
}

- (void)reset {
  self.leadingIcon = nil;
  self.title = nil;
  self.subtitle = nil;
  self.activity = IntelligenceHeaderActivity::kIdle;
  self.primaryItem = nil;
  self.secondaryItems = nil;
}

- (void)setLeadingIcon:(UIImage*)leadingIcon {
  if (_leadingIcon == leadingIcon) {
    return;
  }
  _leadingIcon = leadingIcon;
  _imageView.image = _leadingIcon ?: DefaultLeadingLogo();
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

- (void)setActivity:(IntelligenceHeaderActivity)activity {
  if (_activity == activity) {
    return;
  }
  _activity = activity;
  switch (_activity) {
    case IntelligenceHeaderActivity::kIdle:
      [_activityIndicator stopAnimating];
      break;
    case IntelligenceHeaderActivity::kActive:
      [_activityIndicator startAnimating];
      break;
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
  IntelligenceHeaderItem* previousItem = _primaryItem;
  _primaryItem = primaryItem;
  if (!primaryItem) {
    [_primaryButton removeFromSuperview];
    _primaryButton = nil;
  } else if (CanReuseButton(previousItem, primaryItem)) {
    ApplyItemToButton(_primaryButton, primaryItem);
  } else {
    [_primaryButton removeFromSuperview];
    _primaryButton = CreateStandaloneButton(primaryItem, self);
    [_accessoryStackView addArrangedSubview:_primaryButton];
  }
  _accessoryStackView.hidden =
      (_accessoryStackView.arrangedSubviews.count == 0);
}

- (void)setSecondaryItems:(NSArray<IntelligenceHeaderItem*>*)secondaryItems {
  NSArray<IntelligenceHeaderItem*>* previousItems = _secondaryItems;
  _secondaryItems = [secondaryItems copy];
  if (CanReuseSecondaryButtons(previousItems, _secondaryItems)) {
    NSArray<UIView*>* buttonViews =
        ShouldGroupItems(_secondaryItems)
            ? base::apple::ObjCCastStrict<UIStackView>(
                  _accessoryStackView.arrangedSubviews.firstObject)
                  .arrangedSubviews
            : _accessoryStackView.arrangedSubviews;
    for (NSUInteger i = 0; i < _secondaryItems.count; ++i) {
      ApplyItemToButton(base::apple::ObjCCastStrict<UIButton>(buttonViews[i]),
                        _secondaryItems[i]);
    }
    return;
  }

  for (UIView* view in [_accessoryStackView.arrangedSubviews copy]) {
    if (view != _primaryButton) {
      [view removeFromSuperview];
    }
  }
  if (ShouldGroupItems(_secondaryItems)) {
    [_accessoryStackView
        insertArrangedSubview:CreateGroupedCapsule(_secondaryItems, self)
                      atIndex:0];
  } else {
    for (NSUInteger i = 0; i < _secondaryItems.count; ++i) {
      [_accessoryStackView
          insertArrangedSubview:CreateStandaloneButton(_secondaryItems[i], self)
                        atIndex:i];
    }
  }
  _accessoryStackView.hidden =
      (_accessoryStackView.arrangedSubviews.count == 0);
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

@end
