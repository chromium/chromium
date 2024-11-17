// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/standalone_module_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/icon_detail_view.h"
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
// Width and height of the favicon.
const CGFloat kFaviconWidthHeight = 26.0;
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
  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
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
- (UIView*)iconView {
  UIImage* faviconImage = _config.faviconImage;
  return faviconImage ? [self faviconImageView] : [self fallbackImageView];
}

// Creates the icon view using the `faviconImage` from the config.
- (UIView*)faviconImageView {
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

  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  iconContainerView.layer.cornerRadius = kLargeCornerRadius;
  iconContainerView.layer.masksToBounds = NO;
  iconContainerView.clipsToBounds = YES;
  [iconContainerView addSubview:faviconImageView];
  AddSameCenterConstraints(faviconImageView, iconContainerView);
  [NSLayoutConstraint activateConstraints:@[
    [iconContainerView.widthAnchor constraintEqualToConstant:kImageWidthHeight],
    [iconContainerView.widthAnchor
        constraintEqualToAnchor:iconContainerView.heightAnchor],
  ]];
  return iconContainerView;
}

// Creates the icon view using the `fallbackSymbolImage` from the config.
- (UIView*)fallbackImageView {
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
