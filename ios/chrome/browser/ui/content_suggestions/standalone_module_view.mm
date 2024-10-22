// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/standalone_module_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/standalone_module_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/standalone_module_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Spacing between items stacked vertically (title, description and allow
// label).
const CGFloat kVerticalStackSpacing = 15.0f;
// Spacing between items stacked horizontally (image and text stack
// (which contains title, description and allow label)).
const CGFloat kHorizontalStackSpacing = 16.0f;
// Alpha for top of gradient overlay.
const CGFloat kGradientOverlayTopAlpha = 0.0;
// Alpha for bottom of gradienet overlay.
const CGFloat kGradientOverlayBottomAlpha = 0.14;
// Inset for image fallback from the UIImageView boundary.
const CGFloat kImageFallbackInset = 10.0f;
// Radius of background circle of image fallback.
const CGFloat kImageFallbackCornerRadius = 25.0;
// Height and width of image fallback.
const CGFloat kImageFallbackSize = 28.0;
// Rounded corners of the image radius.
const CGFloat kImageCornerRadius = 8.0;
// Width and height of image.
const CGFloat kImageWidthHeight = 48.0;
// Separator height.
const CGFloat kSeparatorHeight = 0.5;

}  // namespace

@implementation StandaloneModuleView {
  UILabel* _titleLabel;
  UILabel* _descriptionLabel;
  UIButton* _button;
  ContentSuggestionsModuleType _moduleType;
  StandaloneModuleItem* _config;
}

- (void)configureView:(StandaloneModuleItem*)config {
  CHECK(config);
  CHECK(self.subviews.count == 0);
  _moduleType = config.type;
  _config = config;

  self.translatesAutoresizingMaskIntoConstraints = NO;

  _titleLabel = [self titleLabel];
  _descriptionLabel = [self descriptionLabel];
  UIView* iconImage = [self iconImage];
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
      [[UIStackView alloc] initWithArrangedSubviews:@[ iconImage, textStack ]];
  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.spacing = kHorizontalStackSpacing;
  contentStack.alignment = UIStackViewAlignmentTop;
  [self addSubview:contentStack];
  AddSameConstraints(contentStack, self);
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(hideDescriptionOnTraitChange)];
  }
}

#pragma mark - UITraitEnvironment

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self hideDescriptionOnTraitChange];
}
#endif

#pragma mark - Private

// Creates and returns the title label for the view.
- (UILabel*)titleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.numberOfLines = 1;
  titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  titleLabel.font =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold, self);
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

// Creates and returns the icon image view for the view. Uses the `faviconImage`
// from the config if it is set. Otherwise, uses the the `fallbackSymbolImage`
// from the config.
- (UIView*)iconImage {
  UIView* iconImage = [[UIView alloc] init];
  UIImage* faviconImage = _config.faviconImage;
  if (faviconImage) {
    UIImageView* imageView = [[UIImageView alloc] init];
    imageView.image = faviconImage;
    imageView.contentMode = UIViewContentModeScaleAspectFill;
    imageView.translatesAutoresizingMaskIntoConstraints = NO;
    imageView.layer.borderWidth = 0;
    imageView.layer.cornerRadius = kImageCornerRadius;
    imageView.layer.masksToBounds = YES;
    imageView.backgroundColor = UIColor.whiteColor;

    GradientView* gradientOverlay = [[GradientView alloc]
        initWithTopColor:[[UIColor blackColor]
                             colorWithAlphaComponent:kGradientOverlayTopAlpha]
             bottomColor:
                 [[UIColor blackColor]
                     colorWithAlphaComponent:kGradientOverlayBottomAlpha]];
    gradientOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    gradientOverlay.layer.cornerRadius = kImageCornerRadius;
    gradientOverlay.layer.zPosition = 1;

    [NSLayoutConstraint activateConstraints:@[
      [iconImage.heightAnchor constraintEqualToConstant:kImageWidthHeight],
      [iconImage.widthAnchor constraintEqualToAnchor:iconImage.heightAnchor],
      [imageView.heightAnchor constraintEqualToConstant:kImageWidthHeight],
      [imageView.widthAnchor constraintEqualToAnchor:imageView.heightAnchor],
      [gradientOverlay.heightAnchor
          constraintEqualToConstant:kImageWidthHeight],
      [gradientOverlay.widthAnchor
          constraintEqualToAnchor:gradientOverlay.heightAnchor],
    ]];

    [iconImage addSubview:imageView];
    [imageView addSubview:gradientOverlay];

  } else {
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

    iconImage.layer.cornerRadius = kImageFallbackCornerRadius;
    iconImage.backgroundColor = [UIColor colorNamed:kBlueHaloColor];

    [iconImage addSubview:fallbackImageView];

    AddSameConstraintsWithInset(fallbackImageView, iconImage,
                                kImageFallbackInset);
  }

  return iconImage;
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
  [self.delegate buttonTappedForModuleType:_moduleType];
}

- (void)hideDescriptionOnTraitChange {
  _descriptionLabel.hidden = self.traitCollection.preferredContentSizeCategory >
                             UIContentSizeCategoryExtraExtraLarge;
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

@end
