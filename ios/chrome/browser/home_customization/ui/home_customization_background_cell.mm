// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

// Define constants within the namespace
namespace {

// Corner radius applied to the content view.
const CGFloat kContentViewCornerRadius = 10.88;

// Top margin between contentView and borderWrapperView.
const CGFloat kContentViewTopMargin = 12.0;

// Border width for the highlight state.
const CGFloat kHighlightBorderWidth = 7.0;

// Border width for the gap between content and borders.
const CGFloat kGapBorderWidth = 3.78;

// Relative vertical position multiplier for the logoView within its container.
const CGFloat kLogoTopMultiplier = 0.24;

// Fixed width and height for the logoView.
const CGFloat kLogoWidth = 34.0;

// Fixed height for the logoView.
const CGFloat kLogoHeight = 11.0;

// Ccorner radius for small rounded views.
const CGFloat kSmallCornerRadius = 4.5;

// Corner radius for the omnibox view.
const CGFloat kOmniboxCornerRadius = 6.0;

// Top margin between logo view and omnibox.
const CGFloat kOmniboxTopMargin = 10.0;

// Fixed height for omnibox view.
const CGFloat kOmniboxHeight = 12.0;

// Fixed width for omnibox view.
const CGFloat kOmniboxWidth = 64.0;

// Top margin between omnibox and magic stack view.
const CGFloat kMagicStackTopMargin = 5.0;

// Fixed height for magic stack view.
const CGFloat kMagicStackHeight = 26.0;

// Fixed height for magic stack view.
const CGFloat kMagicStackWidth = 70.0;

// Top margin between magic stack and feeds view.
const CGFloat kFeedsTopMargin = 3.0;

// Fixed height for feeds view.
const CGFloat kFeedsHeight = 67.0;

// Fixed height for feeds view.
const CGFloat kFeedsWidth = 70.0;

// When the background is an image, the fake views here should be partially
// transparent.
const CGFloat kAlphaValueWhenImageBackround = 0.6;

}  // namespace

@interface HomeCustomizationBackgroundCell ()

// Container view that provides the outer highlight border.
// Acts as a decorative wrapper for the inner content.
@property(nonatomic, strong) UIView* borderWrapperView;

@end

@implementation HomeCustomizationBackgroundCell {
  // Associated background configuration.
  id<BackgroundCustomizationConfiguration> _backgroundConfiguration;

  // The background image of the cell.
  HomeCustomizationImageView* _backgroundImageView;

  // Search Engine Logo Mediator controlling the Logo display.
  SearchEngineLogoMediator* _searchEngineLogoMediator;

  // View representing the omnibox.
  UIView* _omniboxView;

  // View representing the magic stack.
  UIView* _magicStackView;

  // View representing the feeds.
  UIView* _feedsView;

  // The view holding the default search engine logo.
  UIView* _logoView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor = UIColor.clearColor;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    // Outer container view that holds the highlight border.
    self.borderWrapperView = [[UIView alloc] init];
    self.borderWrapperView.translatesAutoresizingMaskIntoConstraints = NO;
    self.borderWrapperView.backgroundColor = UIColor.clearColor;
    self.borderWrapperView.layer.cornerRadius = 2 * kContentViewCornerRadius;
    self.borderWrapperView.layer.masksToBounds = YES;
    [self.contentView addSubview:self.borderWrapperView];

    // Inner content view, placed with a gap inside the border wrapper view.
    // This holds the actual content.
    self.innerContentView = [[UIStackView alloc] init];
    self.innerContentView.axis = UILayoutConstraintAxisVertical;
    self.innerContentView.alignment = UIStackViewAlignmentCenter;
    self.innerContentView.translatesAutoresizingMaskIntoConstraints = NO;
    self.innerContentView.layer.cornerRadius = kContentViewCornerRadius;
    self.innerContentView.layer.masksToBounds = YES;
    self.innerContentView.axis = UILayoutConstraintAxisVertical;
    self.innerContentView.layer.borderWidth = 1;

    // Adds the empty background image.
    _backgroundImageView = [[HomeCustomizationImageView alloc] init];
    _backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.innerContentView addSubview:_backgroundImageView];
    AddSameConstraints(_backgroundImageView, self.innerContentView);

    [self.borderWrapperView addSubview:self.innerContentView];

    // Constraints for positioning the border wrapper view inside the cell.
    [NSLayoutConstraint activateConstraints:@[
      [self.borderWrapperView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kContentViewTopMargin],
      [self.borderWrapperView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [self.borderWrapperView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [self.borderWrapperView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];

    AddSameConstraintsWithInset(self.innerContentView, self.borderWrapperView,
                                kGapBorderWidth + kHighlightBorderWidth);

    [self setupContentView:self.innerContentView];

    [self registerForTraitChanges:
              @[ NewTabPageTrait.class, NewTabPageImageBackgroundTrait.class ]
                       withAction:@selector(applyTheme)];

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(updateCGColors)];
    [self updateCGColors];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  // Clear any trait overrides.
  CustomUITraitAccessor* traitAccessor =
      [[CustomUITraitAccessor alloc] initWithMutableTraits:self.traitOverrides];
  [traitAccessor setObjectForNewTabPageTrait:[NewTabPageTrait defaultValue]];
  [traitAccessor
      setBoolForNewTabPageImageBackgroundTrait:[NewTabPageImageBackgroundTrait
                                                   defaultValue]];

  [_backgroundImageView setImage:nil framingCoordinates:nil];
  _backgroundConfiguration = nil;
  [_logoView removeFromSuperview];
  _logoView = nil;
}

- (void)setupContentView:(UIStackView*)contentView {
  UIView* spacerView = [[UIView alloc] init];
  spacerView.translatesAutoresizingMaskIntoConstraints = NO;

  _omniboxView =
      [self createContentViewWithBackgroundColor:
                [UIColor colorNamed:kMiniFakeOmniboxBackgroundColor]
                                    cornerRadius:kOmniboxCornerRadius];
  _magicStackView =
      [self createContentViewWithBackgroundColor:
                [[UIColor colorNamed:kBackgroundColor]
                    colorWithAlphaComponent:kAlphaValueWhenImageBackround]
                                    cornerRadius:kSmallCornerRadius];
  _feedsView =
      [self createContentViewWithBackgroundColor:
                [[UIColor colorNamed:kBackgroundColor]
                    colorWithAlphaComponent:kAlphaValueWhenImageBackround]
                                    cornerRadius:kSmallCornerRadius];

  [contentView addArrangedSubview:spacerView];
  [contentView addArrangedSubview:_omniboxView];
  [contentView addArrangedSubview:_magicStackView];
  [contentView addArrangedSubview:_feedsView];

  [contentView setCustomSpacing:kMagicStackTopMargin afterView:_omniboxView];
  [contentView setCustomSpacing:kFeedsTopMargin afterView:_magicStackView];

  [NSLayoutConstraint activateConstraints:@[
    [spacerView.heightAnchor constraintEqualToAnchor:contentView.heightAnchor
                                          multiplier:kLogoTopMultiplier],

    [_omniboxView.widthAnchor constraintEqualToConstant:kOmniboxWidth],
    [_omniboxView.heightAnchor constraintEqualToConstant:kOmniboxHeight],

    [_magicStackView.widthAnchor constraintEqualToConstant:kMagicStackWidth],
    [_magicStackView.heightAnchor constraintEqualToConstant:kMagicStackHeight],

    [_feedsView.widthAnchor constraintEqualToConstant:kFeedsWidth],
    [_feedsView.heightAnchor constraintEqualToConstant:kFeedsHeight]
  ]];
}

- (void)configureWithBackgroundOption:
            (id<BackgroundCustomizationConfiguration>)option
             searchEngineLogoMediator:
                 (SearchEngineLogoMediator*)searchEngineLogoMediator {
  if (_backgroundConfiguration) {
    return;
  }

  if (searchEngineLogoMediator && searchEngineLogoMediator.view) {
    _logoView = searchEngineLogoMediator.view;
    _logoView.translatesAutoresizingMaskIntoConstraints = NO;
    _logoView.userInteractionEnabled = NO;

    // Insert the logo view right after the spacer.
    [self.innerContentView insertArrangedSubview:_logoView atIndex:1];

    [NSLayoutConstraint activateConstraints:@[
      [_logoView.widthAnchor constraintEqualToConstant:kLogoWidth],
      [_logoView.heightAnchor constraintEqualToConstant:kLogoHeight],
    ]];

    [self.innerContentView setCustomSpacing:kOmniboxTopMargin
                                  afterView:_logoView];
  }

  _backgroundConfiguration = option;
  _searchEngineLogoMediator = searchEngineLogoMediator;
  self.accessibilityLabel = option.accessibilityName;
  self.accessibilityValue = option.accessibilityValue;

  [self applyTheme];
}

- (void)updateBackgroundImage:(UIImage*)image
           framingCoordinates:
               (HomeCustomizationFramingCoordinates*)framingCoordinates {
  [_backgroundImageView setImage:image framingCoordinates:framingCoordinates];
}

#pragma mark - Setters

- (void)setSelected:(BOOL)selected {
  if (self.selected == selected) {
    return;
  }

  [super setSelected:selected];

  if (selected) {
    self.borderWrapperView.layer.borderColor =
        [UIColor colorNamed:kStaticBlueColor].CGColor;
    self.borderWrapperView.layer.borderWidth = kHighlightBorderWidth;
  } else {
    self.borderWrapperView.layer.borderColor = nil;
    self.borderWrapperView.layer.borderWidth = 0;
  }
}

#pragma mark - Private

// Creates and returns a `UIView` instance with the specified background color
// and corner radius.
- (UIView*)createContentViewWithBackgroundColor:(UIColor*)backgroundColor
                                   cornerRadius:(CGFloat)cornerRadius {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.backgroundColor = backgroundColor;
  view.layer.cornerRadius = cornerRadius;
  return view;
}

// Updates all view state to match the current theme.
- (void)applyTheme {
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];
  BOOL hasImageBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];

  UIView* logoView = _searchEngineLogoMediator.view;

  if (hasImageBackground) {
    _searchEngineLogoMediator.usesMonochromeLogo = YES;
    logoView.tintColor = [UIColor whiteColor];
    _omniboxView.backgroundColor =
        [UIColor colorNamed:kMiniFakeOmniboxBackgroundColor];
    _magicStackView.backgroundColor = [[UIColor colorNamed:kBackgroundColor]
        colorWithAlphaComponent:kAlphaValueWhenImageBackround];
    _feedsView.backgroundColor = [[UIColor colorNamed:kBackgroundColor]
        colorWithAlphaComponent:kAlphaValueWhenImageBackround];
    return;
  }

  if (colorPalette) {
    _searchEngineLogoMediator.usesMonochromeLogo = YES;
    logoView.tintColor = colorPalette.tintColor;
    self.innerContentView.backgroundColor = colorPalette.primaryColor;
    _omniboxView.backgroundColor = colorPalette.omniboxColor;
    _magicStackView.backgroundColor = colorPalette.secondaryCellColor;
    _feedsView.backgroundColor = colorPalette.secondaryCellColor;
    return;
  }

  _searchEngineLogoMediator.usesMonochromeLogo = NO;
  logoView.tintColor = nil;
  self.innerContentView.backgroundColor =
      [UIColor colorNamed:@"ntp_background_color"];
  _omniboxView.backgroundColor =
      [UIColor colorNamed:kMiniFakeOmniboxBackgroundColor];
  _magicStackView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  _feedsView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
}

// Updates CGColors when the user interface style changes, as they do not
// update automatically.
- (void)updateCGColors {
  self.innerContentView.layer.borderColor =
      [UIColor colorNamed:kGrey200Color].CGColor;
}

@end
