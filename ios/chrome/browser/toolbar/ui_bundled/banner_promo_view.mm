// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/banner_promo_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Size of the close button.
const CGFloat kCloseButtonIconSize = 30;

// Size of logo view.
const CGFloat kLogoSize = 30;

// Point size of the actual logo symbol within the overall logo view.
const CGFloat kLogoPointSize = 18;

// Corner radius for the logo image.
const CGFloat kLogoCornerRadius = 8;

// Spacing for the content stack view.
const CGFloat kContentSpacing = 8;

// Margin on the sides of the content.
const CGFloat kContentHorizontalMargin = 26;

// Margin on the top and bottom of the content.
const CGFloat kContentVerticalMargin = 8.5;

NSString* const kCloseButtonAccessibilityIdentifier = @"PromoCloseButtonAXID";

// Creates the image to go in the close button.
UIImage* CloseButtonImage(BOOL highlighted) {
  NSArray<UIColor*>* palette = @[
    [UIColor colorNamed:@"banner_promo_close_button_x_color"],
    [UIColor colorNamed:@"banner_promo_close_button_background_color"],
  ];

  if (highlighted) {
    NSMutableArray<UIColor*>* transparentPalette =
        [[NSMutableArray alloc] init];
    [palette enumerateObjectsUsingBlock:^(UIColor* color, NSUInteger idx,
                                          BOOL* stop) {
      [transparentPalette addObject:[color colorWithAlphaComponent:0.6]];
    }];
    palette = [transparentPalette copy];
  }

  return SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseButtonIconSize),
      palette);
}

// Creates the close button and attaches the given handler.
UIButton* CloseButton(void (^handler)(UIAction*)) {
  UIButtonConfiguration* closeButtonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  // The image itself is set below in the configurationUpdateHandler, which
  // is called before the button appears for the first time as well.
  closeButtonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
  closeButtonConfiguration.buttonSize = UIButtonConfigurationSizeSmall;
  UIButton* closeButton =
      [UIButton buttonWithConfiguration:closeButtonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_BANNER_PROMO_CLOSE_BUTTON_ACCESSIBILITY_LABEL);
  closeButton.accessibilityIdentifier = kCloseButtonAccessibilityIdentifier;
  closeButton.pointerInteractionEnabled = YES;
  closeButton.configurationUpdateHandler = ^(UIButton* button) {
    UIButtonConfiguration* updatedConfig = button.configuration;
    switch (button.state) {
      case UIControlStateHighlighted:
        updatedConfig.image = CloseButtonImage(YES);
        break;
      case UIControlStateNormal:
        updatedConfig.image = CloseButtonImage(NO);
        break;
    }
    button.configuration = updatedConfig;
  };
  [closeButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                 forAxis:UILayoutConstraintAxisHorizontal];
  return closeButton;
}

}  // namespace

@implementation BannerPromoView {
  // Label for the banner text.
  UILabel* _text;
  // View holding the app logo.
  UIView* _logoView;
  // Button to close the banner.
  UIButton* _closeButton;

  // Stack view to hold all the contents.
  UIStackView* _contentsStackView;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _text = [[UILabel alloc] init];
    _text.translatesAutoresizingMaskIntoConstraints = NO;
    _text.text =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_BROWSER_BANNER_PROMO_TEXT);
    // The promo's height is fixed, so cap the number of lines of text at 2.
    _text.numberOfLines = 2;
    _text.font = [self textFont];
    _text.textColor = [UIColor colorNamed:@"banner_promo_text_color"];
    [_text
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _text.accessibilityTraits |= UIAccessibilityTraitButton;

    _logoView = [[UIView alloc] init];
    _logoView.translatesAutoresizingMaskIntoConstraints = NO;
    _logoView.layer.cornerRadius = kLogoCornerRadius;
    _logoView.backgroundColor = UIColor.whiteColor;
    [NSLayoutConstraint activateConstraints:@[
      [_logoView.heightAnchor constraintEqualToConstant:kLogoSize],
      [_logoView.widthAnchor constraintEqualToAnchor:_logoView.heightAnchor],
    ]];

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    UIImage* logo = MakeSymbolMulticolor(
        CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kLogoPointSize));
#else
    UIImage* logo =
        CustomSymbolWithPointSize(kChromeProductSymbol, kLogoPointSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    UIImageView* logoImageView = [[UIImageView alloc] initWithImage:logo];
    logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_logoView addSubview:logoImageView];

    [NSLayoutConstraint activateConstraints:@[
      [logoImageView.centerXAnchor
          constraintEqualToAnchor:_logoView.centerXAnchor],
      [logoImageView.centerYAnchor
          constraintEqualToAnchor:_logoView.centerYAnchor],
    ]];

    __weak __typeof(self) weakSelf = self;
    _closeButton = CloseButton(^(UIAction*) {
      [weakSelf.delegate bannerPromoCloseButtonWasTapped:weakSelf];
    });

    _contentsStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _logoView, _text, _closeButton ]];
    _contentsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _contentsStackView.spacing = kContentSpacing;
    _contentsStackView.alignment = UIStackViewAlignmentFill;

    [self addSubview:_contentsStackView];

    [NSLayoutConstraint activateConstraints:@[
      [_contentsStackView.leadingAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor
                         constant:kContentHorizontalMargin],
      [self.safeAreaLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_contentsStackView.trailingAnchor
                         constant:kContentHorizontalMargin],
      [_contentsStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:kContentVerticalMargin],
      [self.bottomAnchor constraintEqualToAnchor:_contentsStackView.bottomAnchor
                                        constant:kContentVerticalMargin],
    ]];

    if (@available(iOS 17, *)) {
      UITraitChangeHandler traitChangeHandler =
          ^(id<UITraitEnvironment> traitEnvironment,
            UITraitCollection* previousCollection) {
            [weakSelf updateFontOnTraitChange:previousCollection];
          };
      [self
          registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                      withHandler:traitChangeHandler];
    }

    UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleViewTap:)];
    [self addGestureRecognizer:tap];
  }
  return self;
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateFontOnTraitChange:previousTraitCollection];
}
#endif

- (CGSize)intrinsicContentSize {
  // Promo should be the same height as the toolbar.
  CGFloat height =
      ToolbarExpandedHeight(self.traitCollection.preferredContentSizeCategory);
  return CGSizeMake(UIViewNoIntrinsicMetric, height);
}

#pragma mark - Private

// Updates the `_text`'s font when the device's preferred content size
// category changes.
- (void)updateFontOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    _text.font = [self textFont];
  }
}

// Returns the font to be used for the text label
- (UIFont*)textFont {
  UIContentSizeCategory category = ContentSizeCategoryWithMaxCategory(
      self.traitCollection.preferredContentSizeCategory,
      LocationBarSteadyViewMaxSizeCategory());
  UITraitCollection* traitCollection = [UITraitCollection
      traitCollectionWithPreferredContentSizeCategory:category];

  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleFootnote
             compatibleWithTraitCollection:traitCollection]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  return [UIFont fontWithDescriptor:boldDescriptor size:0.0];
}

- (void)handleViewTap:(UITapGestureRecognizer*)recognizer {
  [self.delegate bannerPromoWasTapped:self];
}

@end
