// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_color_palette_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

// Define constants within the namespace
namespace {

// Corner radius applied to the content view.
const CGFloat kContentViewCornerRadius = 10.88;

// Corner radius applied to the highlight border view.
const CGFloat kHighlightCornerRadius = 16.0;

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

}  // namespace

@interface HomeCustomizationBackgroundCell ()

// Container view that provides the outer highlight border.
// Acts as a decorative wrapper for the inner content.
@property(nonatomic, strong) UIView* borderWrapperView;

// Main content view rendered inside the border wrapper.
// Displays the core visual element.
@property(nonatomic, strong) UIStackView* innerContentView;

@end

@implementation HomeCustomizationBackgroundCell {
  // Associated background configuration.
  id<BackgroundCustomizationConfiguration> _backgroundConfiguration;

  // The background image of the cell.
  UIImageView* _backgroundImageView;

  // Tracks whether the cell has already been configured with option.
  BOOL _isConfigured;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor = UIColor.clearColor;

    // Outer container view that holds the highlight border.
    self.borderWrapperView = [[UIView alloc] init];
    self.borderWrapperView.translatesAutoresizingMaskIntoConstraints = NO;
    self.borderWrapperView.backgroundColor = UIColor.clearColor;
    self.borderWrapperView.layer.cornerRadius = kHighlightCornerRadius;
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

    // Adds the empty background image.
    _backgroundImageView = [[UIImageView alloc] init];
    _backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
    _backgroundImageView.clipsToBounds = YES;
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
  }
  return self;
}

- (void)setupContentView:(UIStackView*)contentView {
  contentView.backgroundColor = [UIColor colorNamed:@"ntp_background_color"];

  UIView* spacerView = [[UIView alloc] init];
  spacerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* omniboxView =
      [self createContentViewWithBackgroundColor:
                [UIColor colorNamed:kMiniFakeOmniboxBackgroundColor]
                                    cornerRadius:kOmniboxCornerRadius];
  UIView* magicStackView = [self
      createContentViewWithBackgroundColor:[[UIColor
                                               colorNamed:kBackgroundColor]
                                               colorWithAlphaComponent:0.6]
                              cornerRadius:kSmallCornerRadius];
  UIView* feedsView = [self
      createContentViewWithBackgroundColor:[[UIColor
                                               colorNamed:kBackgroundColor]
                                               colorWithAlphaComponent:0.6]
                              cornerRadius:kSmallCornerRadius];

  [contentView addArrangedSubview:spacerView];
  [contentView addArrangedSubview:omniboxView];
  [contentView addArrangedSubview:magicStackView];
  [contentView addArrangedSubview:feedsView];

  [contentView setCustomSpacing:kMagicStackTopMargin afterView:omniboxView];
  [contentView setCustomSpacing:kFeedsTopMargin afterView:magicStackView];

  [NSLayoutConstraint activateConstraints:@[
    [spacerView.heightAnchor constraintEqualToAnchor:contentView.heightAnchor
                                          multiplier:kLogoTopMultiplier],

    [omniboxView.widthAnchor constraintEqualToConstant:kOmniboxWidth],
    [omniboxView.heightAnchor constraintEqualToConstant:kOmniboxHeight],

    [magicStackView.widthAnchor constraintEqualToConstant:kMagicStackWidth],
    [magicStackView.heightAnchor constraintEqualToConstant:kMagicStackHeight],

    [feedsView.widthAnchor constraintEqualToConstant:kFeedsWidth],
    [feedsView.heightAnchor constraintEqualToConstant:kFeedsHeight]
  ]];
}

- (void)configureWithBackgroundOption:
            (id<BackgroundCustomizationConfiguration>)option
                           logoVendor:(id<LogoVendor>)logoVendor
                         colorPalette:
                             (HomeCustomizationColorPaletteConfiguration*)
                                 colorPalette {
  if (_isConfigured) {
    return;
  }
  _backgroundConfiguration = option;
  BOOL imageBackground = !option.thumbnailURL.is_empty();

  logoVendor.usesMonochromeLogo = colorPalette || imageBackground;
  UIView* logoView = logoVendor.view;
  logoView.translatesAutoresizingMaskIntoConstraints = NO;

  // Change the tint of the logo based on the background.
  if (imageBackground) {
    logoView.tintColor = [UIColor whiteColor];
  } else if (colorPalette) {
    logoView.tintColor = colorPalette.darkColor;
  } else {
    logoView.tintColor = logoView.tintColor;
  }

  // Insert the logo view right after the spacer.
  [self.innerContentView insertArrangedSubview:logoView atIndex:1];

  [NSLayoutConstraint activateConstraints:@[
    [logoView.widthAnchor constraintEqualToConstant:kLogoWidth],
    [logoView.heightAnchor constraintEqualToConstant:kLogoHeight],
  ]];

  [self.innerContentView setCustomSpacing:kOmniboxTopMargin afterView:logoView];
  _isConfigured = YES;
}

- (void)updateBackgroundImage:(UIImage*)image {
  [_backgroundImageView setImage:image];
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

@end
