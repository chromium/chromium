// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view.h"

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_config.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_updating.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Spacing between items stacked vertically (title, description and allow
// label).
const CGFloat kVerticalStackSpacing = 15.0f;
// Spacing between items stacked horizontally (image and text stack
// (which contains title, description and allow label)).
const CGFloat kHorizontalStackSpacing = 16.0f;
// Inset for image fallback from the UIImageView boundary.
const CGFloat kImageFallbackInset = 10.0f;
// Radius of background circle of image fallback.
const CGFloat kImageFallbackCornerRadius = 25.0;
// Height and width of image fallback.
const CGFloat kImageFallbackSize = 28.0;
// Corner radius for the larger image views. Used for the favicon container.
const CGFloat kLargeCornerRadius = 10.0;
// Corner radius for the medium sized image views. Used for the favicon image.
const CGFloat kMediumCornerRadius = 4.0;
// Width and height of image.
const CGFloat kImageWidthHeight = 48.0;
// Rounded corners of the product image radius
const CGFloat kProductImageCornerRadius = 8.0;
// Alpha for top of gradient overlay on a product image.
const CGFloat kGradientOverlayTopAlpha = 0.0;
// Alpha for bottom of gradienet overlay on a product image.
const CGFloat kGradientOverlayBottomAlpha = 0.14;
// Width and height of the favicon.
const CGFloat kFaviconWidthHeight = 26.0;
// Properties for Favicon image view when displayed over a product image.
const CGFloat kFaviconImageViewCornerRadius = 2.0;
const CGFloat kFaviconImageViewTrailingCornerRadius = 4.0;
const CGFloat kFaviconImageViewHeightWidth = 10.0;
// Properties for Favicon image container.
const CGFloat kFaviconImageContainerCornerRadius = 3.0;
const CGFloat kFaviconImageContainerTrailingCornerRadius = 6.0;
const CGFloat kFaviconImageContainerHeightWidth = 15.0;
const CGFloat kFaviconImageContainerTrailingMargin = -4.62;
// Separator height.
const CGFloat kSeparatorHeight = 0.5;

}  // namespace

@interface StandaloneModuleView () <NewTabPageColorUpdating>
@end

@implementation StandaloneModuleView {
  ContentSuggestionsModuleType _moduleType;
  StandaloneModuleViewConfig* _config;
  UILabel* _titleLabel;
  UILabel* _descriptionLabel;
  UIButton* _button;
  // Ivars for constructing an icon view using a product image, favicon, and
  // gradient overlay.
  UIView* _productImage;
  UIImageView* _productImageView;
  UIImageView* _faviconImageView;
  UIView* _faviconImageContainer;
  UIView* _gradientOverlay;
  UIView* _iconContainerView;
}

#pragma mark - Public

- (void)configureView:(StandaloneModuleViewConfig*)config {
  CHECK(config);
  CHECK(self.subviews.count == 0);
  _moduleType = config.type;
  _config = config;

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = _config.accessibilityIdentifier;

  _titleLabel = [self titleLabel];
  _descriptionLabel = [self descriptionLabel];
  UIView* iconView = [self iconView];
  _button = [self button];

  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];

  UIStackView* textStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _titleLabel, _descriptionLabel, separator, _button
  ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.alignment = UIStackViewAlignmentLeading;
  textStack.spacing = kVerticalStackSpacing;

  [NSLayoutConstraint activateConstraints:@[
    [separator.heightAnchor
        constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
    [separator.leadingAnchor constraintEqualToAnchor:textStack.leadingAnchor],
    [separator.trailingAnchor constraintEqualToAnchor:textStack.trailingAnchor],
  ]];

  UIStackView* contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ iconView, textStack ]];
  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.spacing = kHorizontalStackSpacing;
  contentStack.alignment = UIStackViewAlignmentTop;
  [self addSubview:contentStack];
  AddSameConstraints(contentStack, self);
  [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                     withAction:@selector(hideDescriptionOnTraitChange)];
  if (IsNTPBackgroundCustomizationEnabled()) {
    [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                       withAction:@selector(applyBackgroundColors)];
  }
  [self applyBackgroundColors];
}

#pragma mark - NewTabPageColorUpdating

- (void)applyBackgroundColors {
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];
  if (colorPalette) {
    [_button setTitleColor:colorPalette.tintColor
                  forState:UIControlStateNormal];
    _iconContainerView.backgroundColor = colorPalette.tertiaryColor;
  } else {
    [_button setTitleColor:[UIColor colorNamed:kBlueColor]
                  forState:UIControlStateNormal];
    _iconContainerView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  }
}

#pragma mark - Private

// Creates and returns the title label for the view.
- (UILabel*)titleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.numberOfLines = 1;
  titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold);
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.text = _config.titleText;
  titleLabel.isAccessibilityElement = YES;
  return titleLabel;
}

// Creates and returns the description label for the view.
- (UILabel*)descriptionLabel {
  UILabel* descriptionLabel = [[UILabel alloc] init];
  descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  descriptionLabel.numberOfLines = 2;
  descriptionLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  descriptionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  descriptionLabel.adjustsFontForContentSizeCategory = YES;
  descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descriptionLabel.text = _config.bodyText;
  descriptionLabel.isAccessibilityElement = YES;
  return descriptionLabel;
}

// Creates and returns the icon image view for the view. Uses the `productImage`
// and/or `faviconImage` from the config if it is set. Otherwise, uses the the
// `fallbackSymbolImage` from the config.
- (UIView*)iconView {
  if (_config.productImage) {
    return [self productImage];
  } else if (_config.faviconImage) {
    return [self faviconImageView];
  } else {
    return [self fallbackImageView];
  }
}

// Creates the icon view using the `productImage` from the config.
- (UIView*)productImage {
  CHECK(_config.productImage);
  _productImage = [[UIView alloc] init];
  _productImageView = [[UIImageView alloc] init];
  _productImageView.image = _config.productImage;
  _productImageView.contentMode = UIViewContentModeScaleAspectFill;
  _productImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _productImageView.layer.borderWidth = 0;
  _productImageView.layer.cornerRadius = kProductImageCornerRadius;
  _productImageView.layer.masksToBounds = YES;
  _productImageView.backgroundColor = UIColor.whiteColor;

  _gradientOverlay = [[GradientView alloc]
      initWithTopColor:[[UIColor blackColor]
                           colorWithAlphaComponent:kGradientOverlayTopAlpha]
           bottomColor:[[UIColor blackColor] colorWithAlphaComponent:
                                                 kGradientOverlayBottomAlpha]];
  _gradientOverlay.translatesAutoresizingMaskIntoConstraints = NO;
  _gradientOverlay.layer.cornerRadius = kProductImageCornerRadius;
  _gradientOverlay.layer.zPosition = 1;

  if (_config.faviconImage) {
    [self populateProductImageFaviconContainerAndView:_config.faviconImage];
  }

  [_productImage addSubview:_productImageView];
  [_productImageView addSubview:_gradientOverlay];
  if (_faviconImageContainer) {
    [self addFaviconToProductImage];
  }

  [self addConstraintsForProductImage];
  if (_faviconImageContainer) {
    [self addConstraintsForProductImageFavicon];
  }
  return _productImage;
}

// Creates the icon view using the `faviconImage` from the config.
- (UIView*)faviconImageView {
  CHECK(_config.faviconImage);
  UIImageView* faviconImageView = [[UIImageView alloc] init];
  faviconImageView.image = _config.faviconImage;
  faviconImageView.contentMode = UIViewContentModeScaleAspectFill;
  faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconImageView.layer.borderWidth = 0;
  faviconImageView.layer.masksToBounds = NO;
  faviconImageView.backgroundColor = UIColor.whiteColor;

  [NSLayoutConstraint activateConstraints:@[
    [faviconImageView.heightAnchor
        constraintEqualToConstant:kFaviconWidthHeight],
    [faviconImageView.widthAnchor
        constraintEqualToAnchor:faviconImageView.heightAnchor],
  ]];

  faviconImageView.layer.masksToBounds = YES;
  faviconImageView.layer.cornerRadius = kMediumCornerRadius;
  faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;

  _iconContainerView = [[UIView alloc] init];
  _iconContainerView.layer.cornerRadius = kLargeCornerRadius;
  _iconContainerView.layer.masksToBounds = NO;
  _iconContainerView.clipsToBounds = YES;
  [_iconContainerView addSubview:faviconImageView];
  AddSameCenterConstraints(faviconImageView, _iconContainerView);
  [NSLayoutConstraint activateConstraints:@[
    [_iconContainerView.widthAnchor
        constraintEqualToConstant:kImageWidthHeight],
    [_iconContainerView.widthAnchor
        constraintEqualToAnchor:_iconContainerView.heightAnchor],
  ]];
  return _iconContainerView;
}

// Creates the icon view using the `fallbackSymbolImage` from the config.
- (UIView*)fallbackImageView {
  CHECK(_config.fallbackSymbolImage);
  UIView* iconView = [[UIView alloc] init];
  UIImageView* fallbackImageView = [[UIImageView alloc] init];
  fallbackImageView.image = _config.fallbackSymbolImage;
  fallbackImageView.contentMode = UIViewContentModeScaleAspectFit;
  fallbackImageView.translatesAutoresizingMaskIntoConstraints = NO;
  fallbackImageView.layer.borderWidth = 0;

  [NSLayoutConstraint activateConstraints:@[
    [fallbackImageView.widthAnchor
        constraintEqualToConstant:kImageFallbackSize],
    [fallbackImageView.widthAnchor
        constraintEqualToAnchor:fallbackImageView.heightAnchor],
  ]];

  iconView.layer.cornerRadius = kImageFallbackCornerRadius;
  iconView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];

  [iconView addSubview:fallbackImageView];

  AddSameConstraintsWithInset(fallbackImageView, iconView, kImageFallbackInset);
  return iconView;
}

// Creates and returns the action button for the view.
- (UIButton*)button {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button setTitle:_config.buttonText forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  button.isAccessibilityElement = YES;
  button.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  UITapGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(buttonTapped:)];
  [button addGestureRecognizer:tapRecognizer];
  return button;
}

// Handles the tap action for the button.
- (void)buttonTapped:(UIGestureRecognizer*)sender {
  [self.tapDelegate buttonTappedForModuleType:_moduleType];
}

- (void)hideDescriptionOnTraitChange {
  _descriptionLabel.hidden = self.traitCollection.preferredContentSizeCategory >
                             UIContentSizeCategoryExtraExtraLarge;
}

// Populates `_faviconImageContainer` and `_faviconImageView` for a product
// image with a `faviconImage`.
- (void)populateProductImageFaviconContainerAndView:(UIImage*)faviconImage {
  if (_faviconImageView) {
    _faviconImageView.image = faviconImage;
    return;
  }

  _faviconImageContainer = [[UIView alloc] init];
  _faviconImageContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconImageContainer.layer.borderWidth = 0;
  _faviconImageContainer.backgroundColor =
      [UIColor colorNamed:kBackgroundColor];
  _faviconImageContainer.layer.cornerRadius =
      kFaviconImageContainerCornerRadius;
  _faviconImageContainer.layer.masksToBounds = YES;
  // Apply bottom right radius mask
  _faviconImageContainer.layer.mask = [self
      productImageFaviconMaskWithRadius:
          kFaviconImageContainerTrailingCornerRadius
                       imageHeightWidth:kFaviconImageContainerHeightWidth];

  _faviconImageView = [[UIImageView alloc] init];
  _faviconImageView.image = faviconImage;
  _faviconImageView.contentMode = UIViewContentModeScaleAspectFill;
  _faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconImageView.layer.borderWidth = 0;
  _faviconImageView.layer.cornerRadius = kFaviconImageViewCornerRadius;
  _faviconImageView.layer.masksToBounds = YES;
  _faviconImageView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  // Apply bottom right radius mask
  _faviconImageView.layer.mask = [self
      productImageFaviconMaskWithRadius:kFaviconImageViewTrailingCornerRadius
                       imageHeightWidth:kFaviconImageViewHeightWidth];
}

// Create a mask with radius applied to the trailing corner.
- (CAShapeLayer*)productImageFaviconMaskWithRadius:(CGFloat)trailingCornerRadius
                                  imageHeightWidth:(CGFloat)imageHeightWidth {
  UIRectCorner bottomTrail =
      base::i18n::IsRTL() ? UIRectCornerBottomLeft : UIRectCornerBottomRight;
  CGSize cornerRadiusSize =
      CGSizeMake(trailingCornerRadius, trailingCornerRadius);
  UIBezierPath* bezierPath =
      [UIBezierPath bezierPathWithRoundedRect:CGRectMake(0, 0, imageHeightWidth,
                                                         imageHeightWidth)
                            byRoundingCorners:bottomTrail
                                  cornerRadii:cornerRadiusSize];
  CAShapeLayer* mask = [CAShapeLayer layer];
  mask.path = bezierPath.CGPath;
  return mask;
}

// Adds a favicon overlay to a product image.
- (void)addFaviconToProductImage {
  [_productImage addSubview:_faviconImageContainer];
  [_faviconImageContainer addSubview:_faviconImageView];
}

// Adds constraints for a product image.
- (void)addConstraintsForProductImage {
  NSMutableArray<NSLayoutConstraint*>* constraints = [[NSMutableArray alloc]
      initWithObjects:[_productImage.heightAnchor
                          constraintEqualToConstant:kImageWidthHeight],
                      [_productImage.widthAnchor
                          constraintEqualToAnchor:_productImage.heightAnchor],
                      nil];
  if (_productImageView) {
    [constraints addObjectsFromArray:@[
      [_productImageView.heightAnchor
          constraintEqualToConstant:kImageWidthHeight],
      [_productImageView.widthAnchor
          constraintEqualToAnchor:_productImageView.heightAnchor]
    ]];
  }

  if (_gradientOverlay) {
    [constraints addObjectsFromArray:@[
      [_gradientOverlay.heightAnchor
          constraintEqualToConstant:kImageWidthHeight],
      [_gradientOverlay.widthAnchor
          constraintEqualToAnchor:_gradientOverlay.heightAnchor]
    ]];
  }
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds constraints for a favicon overlayed on a product image.
- (void)addConstraintsForProductImageFavicon {
  [NSLayoutConstraint activateConstraints:@[
    [_faviconImageContainer.heightAnchor
        constraintEqualToConstant:kFaviconImageContainerHeightWidth],
    [_faviconImageContainer.widthAnchor
        constraintEqualToAnchor:_faviconImageContainer.heightAnchor],
    [_faviconImageContainer.trailingAnchor
        constraintEqualToAnchor:_productImage.trailingAnchor
                       constant:kFaviconImageContainerTrailingMargin],
    [_faviconImageContainer.bottomAnchor
        constraintEqualToAnchor:_productImage.bottomAnchor
                       constant:kFaviconImageContainerTrailingMargin],
    [_faviconImageView.heightAnchor
        constraintEqualToConstant:kFaviconImageViewHeightWidth],
    [_faviconImageView.widthAnchor
        constraintEqualToAnchor:_faviconImageView.heightAnchor],
    [_faviconImageView.centerXAnchor
        constraintEqualToAnchor:_faviconImageContainer.centerXAnchor],
    [_faviconImageView.centerYAnchor
        constraintEqualToAnchor:_faviconImageContainer.centerYAnchor],
  ]];
}

#pragma mark - Testing category methods

- (NSString*)titleLabelTextForTesting {
  return _titleLabel.text;
}

- (NSString*)descriptionLabelTextForTesting {
  return _descriptionLabel.text;
}

- (NSString*)allowLabelTextForTesting {
  return _button.currentTitle;
}

- (void)addConstraintsForProductImageForTesting {
  [self addConstraintsForProductImage];
}

@end
