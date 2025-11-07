// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet.h"

#import <UIKit/UIKit.h>

#import "base/ios/ns_range.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet_delegate.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

CGFloat const kVerticalSpacing = 20;
CGFloat const kTitleContainerCornerRadius = 15;
CGFloat const kTitleContainerTopPadding = 33;
CGFloat const kTitleContainerViewSize = 64;
CGFloat const kChromeLogoSize = 52;
CGFloat const kIncognitoLogoSize = 34;
CGFloat const kAnimationDuration = 0.3;
CGFloat const kAnimationDelay = 1;
CGFloat const kAnimationTranslationOffset = 5;
CGFloat const kAnimationScalFactor = 0.5;
CGFloat const kHalfSheetCornerRadius = 20;
CGFloat const kHalfSheetFullHeightProportion = 0.9;
CGFloat const kIncognitoStackWidthOffset = 32.0;
CGFloat const kHorizontalPadding = 20.0;
CGFloat const kButtonHeight = 50;
CGFloat const kBottomGradientViewHeight = 60.0;

NSString* const kPrimaryActionAccessibilityIdentifier =
    @"PrimaryActionAccessibilityIdentifier";

NSString* const kTitleAccessibilityIdentifier = @"TitleAccessibilityIdentifier";

// Helpers copied from IncognitoView.mm
// TODO(crbug.com/442531250): Merge the common utils between
// `YoutubeIncognitoSheet` and `IncognitoView`.
UIFont* BodyFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
}

UIFont* BoldBodyFont() {
  UIFontDescriptor* baseDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* styleDescriptor = [baseDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  return [UIFont fontWithDescriptor:styleDescriptor size:0.0];
}

NSAttributedString* FormatHTMLListForUILabel(NSString* listString) {
  listString = [listString stringByReplacingOccurrencesOfString:@"<ul>"
                                                     withString:@""];
  listString = [listString stringByReplacingOccurrencesOfString:@"</ul>"
                                                     withString:@""];
  listString = [listString
      stringByReplacingOccurrencesOfString:@"\n *<li>"
                                withString:@"\n\u2022  "
                                   options:NSRegularExpressionSearch
                                     range:NSMakeRange(0, [listString length])];
  listString = [listString
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  const StringWithTag parsedString =
      ParseStringWithTag(listString, @"<em>", @"</em>");
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string];
  [attributedText addAttribute:NSFontAttributeName
                         value:BodyFont()
                         range:NSMakeRange(0, attributedText.length)];
  if (parsedString.range.location != NSNotFound) {
    [attributedText addAttribute:NSFontAttributeName
                           value:BoldBodyFont()
                           range:parsedString.range];
  }
  return attributedText;
}

}  // namespace

@implementation YoutubeIncognitoSheet {
  UIView* _icognitoIconView;
  UIScrollView* _scrollView;
  UIView* _bottomGradientView;
  CAGradientLayer* _bottomGradientLayer;
}

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.view.backgroundColor = [UIColor systemBackgroundColor];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  _bottomGradientView = [[UIView alloc] init];
  _bottomGradientView.translatesAutoresizingMaskIntoConstraints = NO;
  _bottomGradientView.userInteractionEnabled = NO;
  [self.view addSubview:_bottomGradientView];
  _bottomGradientLayer = [CAGradientLayer layer];
  [_bottomGradientView.layer insertSublayer:_bottomGradientLayer atIndex:0];
  _bottomGradientView.hidden = YES;

  UIColor* backgroundColor = self.view.backgroundColor;
  if (!backgroundColor) {
    backgroundColor = [UIColor blackColor];
  }
  _bottomGradientLayer.colors = @[
    (id)[backgroundColor colorWithAlphaComponent:0.0].CGColor,
    (id)backgroundColor.CGColor
  ];
  _bottomGradientLayer.locations = @[ @(0.0), @(0.8) ];

  _bottomGradientLayer.startPoint = CGPointMake(0.5, 0.0);
  _bottomGradientLayer.endPoint = CGPointMake(0.5, 1.0);

  UIStackView* mainStackView = [[UIStackView alloc] init];
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.spacing = kVerticalSpacing;
  mainStackView.alignment = UIStackViewAlignmentCenter;
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:mainStackView];

  UIView* animatedTitleView = [self animatedTitleView];
  [mainStackView addArrangedSubview:animatedTitleView];

  UILabel* titleLabel = [self createTitleLabel];
  [mainStackView addArrangedSubview:titleLabel];
  [mainStackView setCustomSpacing:kVerticalSpacing afterView:titleLabel];

  // Manually recreated IncognitoView content
  UIStackView* incognitoContentStackView = [[UIStackView alloc] init];
  incognitoContentStackView.axis = UILayoutConstraintAxisVertical;
  incognitoContentStackView.spacing = kVerticalSpacing;
  incognitoContentStackView.alignment = UIStackViewAlignmentLeading;
  [mainStackView addArrangedSubview:incognitoContentStackView];

  UIColor* bodyTextColor = [UIColor colorNamed:kTextSecondaryColor];
  UIColor* linkTextColor = [UIColor colorNamed:kBlueColor];

  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.font = BodyFont();
  subtitleLabel.textColor = bodyTextColor;
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.text =
      l10n_util::GetNSString(IDS_NEW_TAB_OTR_SUBTITLE_WITH_READING_LIST);
  subtitleLabel.adjustsFontForContentSizeCategory = YES;

  UIButton* learnMoreButton = [UIButton buttonWithType:UIButtonTypeCustom];
  [learnMoreButton
      setTitle:l10n_util::GetNSString(IDS_NEW_TAB_OTR_LEARN_MORE_LINK)
      forState:UIControlStateNormal];
  [learnMoreButton setTitleColor:linkTextColor forState:UIControlStateNormal];
  learnMoreButton.titleLabel.font = BodyFont();
  learnMoreButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  [learnMoreButton addTarget:self
                      action:@selector(learnMoreButtonPressed)
            forControlEvents:UIControlEventTouchUpInside];

  UIStackView* subtitleStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ subtitleLabel, learnMoreButton ]];
  subtitleStackView.axis = UILayoutConstraintAxisVertical;
  subtitleStackView.spacing = 0;
  subtitleStackView.alignment = UIStackViewAlignmentLeading;
  [incognitoContentStackView addArrangedSubview:subtitleStackView];

  NSAttributedString* notSavedText = FormatHTMLListForUILabel(
      l10n_util::GetNSString(IDS_NEW_TAB_OTR_NOT_SAVED));
  UILabel* notSavedLabel = [[UILabel alloc] init];
  notSavedLabel.numberOfLines = 0;
  notSavedLabel.attributedText = notSavedText;
  notSavedLabel.textColor = bodyTextColor;
  [incognitoContentStackView addArrangedSubview:notSavedLabel];

  NSAttributedString* visibleDataText =
      FormatHTMLListForUILabel(l10n_util::GetNSString(IDS_NEW_TAB_OTR_VISIBLE));
  UILabel* visibleDataLabel = [[UILabel alloc] init];
  visibleDataLabel.numberOfLines = 0;
  visibleDataLabel.attributedText = visibleDataText;
  visibleDataLabel.textColor = bodyTextColor;
  [incognitoContentStackView addArrangedSubview:visibleDataLabel];
  // End of recreated content

  UIButton* primaryButton = [self createPrimaryActionButton];

  [self.view addSubview:primaryButton];

  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];

  // Only apply a width offset if the device is Ipad.
  CGFloat incognitoStackWidthOffset =
      (idiom == UIUserInterfaceIdiomPad) ? kIncognitoStackWidthOffset : 0;

  [NSLayoutConstraint activateConstraints:@[
    [_scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_scrollView.bottomAnchor constraintEqualToAnchor:primaryButton.topAnchor
                                             constant:-kVerticalSpacing],

    [mainStackView.topAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.topAnchor],
    [mainStackView.bottomAnchor
        constraintEqualToAnchor:_scrollView.contentLayoutGuide.bottomAnchor],
    [mainStackView.leadingAnchor
        constraintEqualToAnchor:_scrollView.frameLayoutGuide.leadingAnchor
                       constant:kHorizontalPadding],
    [mainStackView.trailingAnchor
        constraintEqualToAnchor:_scrollView.frameLayoutGuide.trailingAnchor
                       constant:-kHorizontalPadding],

    [incognitoContentStackView.widthAnchor
        constraintEqualToAnchor:mainStackView.widthAnchor
                       constant:-incognitoStackWidthOffset],

    [primaryButton.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalPadding],
    [primaryButton.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalPadding],
    [primaryButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kVerticalSpacing],
    [primaryButton.heightAnchor constraintEqualToConstant:kButtonHeight],
    [_bottomGradientView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_bottomGradientView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_bottomGradientView.bottomAnchor
        constraintEqualToAnchor:primaryButton.topAnchor],
    [_bottomGradientView.heightAnchor
        constraintEqualToConstant:kBottomGradientViewHeight]
  ]];

  [self.view bringSubviewToFront:_bottomGradientView];
  [self.view bringSubviewToFront:primaryButton];
  [self setUpBottomSheetPresentationController];
  [self setUpBottomSheetDetents];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  _bottomGradientLayer.frame = _bottomGradientView.bounds;
  BOOL isScrollable =
      _scrollView.contentSize.height > _scrollView.bounds.size.height;

  // Show gradient only if scrollable
  _bottomGradientView.hidden = !isScrollable;
}

- (void)primaryButtonTapped {
  [self.delegate didTapPrimaryActionButton];
}

- (void)learnMoreButtonPressed {
  [self.URLLoaderDelegate loadURLInTab:GetLearnMoreIncognitoUrl()];
}

- (void)viewDidAppear:(BOOL)animated {
  __weak __typeof(self) weakSelf = self;
  [super viewDidAppear:animated];
  [UIView animateWithDuration:kAnimationDuration
                        delay:kAnimationDelay
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     [weakSelf titleViewAnimation];
                   }
                   completion:nil];
}

// Configures the bottom sheet's presentation controller appearance.
- (void)setUpBottomSheetPresentationController {
  self.modalPresentationStyle = UIModalPresentationFormSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
}

// Configures the bottom sheet's detents.
- (void)setUpBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return context.maximumDetentValue * kHalfSheetFullHeightProportion;
  };
  UISheetPresentationControllerDetent* customDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:@"customMaximizedDetent"
                            resolver:resolver];
  presentationController.detents = @[ customDetent ];
  presentationController.selectedDetentIdentifier = @"customMaximizedDetent";
}

- (UIView*)animatedTitleView {
  UIView* chromeIconView = [[UIView alloc] init];
  chromeIconView.translatesAutoresizingMaskIntoConstraints = NO;
  chromeIconView.layer.cornerRadius = kTitleContainerCornerRadius;
  chromeIconView.backgroundColor = [UIColor whiteColor];
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* chromeLogo = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kChromeLogoSize));
#else
  UIImage* chromeLogo =
      CustomSymbolWithPointSize(kChromeProductSymbol, kChromeLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

  UIImageView* chromeLogoView = [[UIImageView alloc] initWithImage:chromeLogo];
  chromeLogoView.translatesAutoresizingMaskIntoConstraints = NO;
  [chromeIconView addSubview:chromeLogoView];

  _icognitoIconView = [[UIView alloc] init];
  _icognitoIconView.translatesAutoresizingMaskIntoConstraints = NO;
  _icognitoIconView.layer.cornerRadius = kTitleContainerCornerRadius;
  _icognitoIconView.backgroundColor = LargeIncognitoBackgroundColor();
  UIImage* incognitoLogo =
      CustomSymbolWithPointSize(kIncognitoSymbol, kIncognitoLogoSize);
  UIImageView* incognitoLogoView =
      [[UIImageView alloc] initWithImage:incognitoLogo];
  incognitoLogoView.tintColor = LargeIncognitoForegroundColor();
  incognitoLogoView.translatesAutoresizingMaskIntoConstraints = NO;
  [_icognitoIconView addSubview:incognitoLogoView];

  [NSLayoutConstraint activateConstraints:@[
    [chromeIconView.widthAnchor
        constraintEqualToConstant:kTitleContainerViewSize],
    [chromeIconView.heightAnchor
        constraintEqualToConstant:kTitleContainerViewSize],
  ]];
  AddSameCenterConstraints(chromeIconView, chromeLogoView);

  UIView* outerView = [[UIView alloc] init];
  [outerView addSubview:chromeIconView];
  [outerView addSubview:_icognitoIconView];

  AddSameCenterXConstraint(outerView, chromeIconView);
  AddSameConstraints(chromeIconView, _icognitoIconView);
  AddSameCenterConstraints(_icognitoIconView, incognitoLogoView);
  AddSameConstraintsToSidesWithInsets(
      chromeIconView, outerView, LayoutSides::kTop | LayoutSides::kBottom,
      NSDirectionalEdgeInsetsMake(kTitleContainerTopPadding, 0, 0, 0));
  return outerView;
}

- (void)titleViewAnimation {
  CGFloat translationX =
      _icognitoIconView.frame.size.width / 2 - kAnimationTranslationOffset;
  CGFloat translationY =
      _icognitoIconView.frame.size.height / 2 - kAnimationTranslationOffset;
  _icognitoIconView.transform = CGAffineTransformConcat(
      CGAffineTransformScale(CGAffineTransformIdentity, kAnimationScalFactor,
                             kAnimationScalFactor),
      CGAffineTransformMakeTranslation(translationX, translationY));
}

- (ChromeButton*)createPrimaryActionButton {
  ChromeButton* primaryActionButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  [primaryActionButton addTarget:self
                          action:@selector(primaryButtonTapped)
                forControlEvents:UIControlEventTouchUpInside];
  primaryActionButton.title = l10n_util::GetNSString(
      IDS_IOS_YOUTUBE_INCOGNITO_SHEET_PRIMARY_BUTTON_TITLE);
  primaryActionButton.accessibilityIdentifier =
      kPrimaryActionAccessibilityIdentifier;

  return primaryActionButton;
}

- (UILabel*)createTitleLabel {
  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle2];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:UIFontTextStyleTitle3];
  title.font = [fontMetrics scaledFontForFont:font];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.text = l10n_util::GetNSString(IDS_IOS_YOUTUBE_INCOGNITO_SHEET_TITLE);
  title.textAlignment = NSTextAlignmentCenter;
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.adjustsFontForContentSizeCategory = YES;
  title.accessibilityIdentifier = kTitleAccessibilityIdentifier;
  title.accessibilityTraits = UIAccessibilityTraitHeader;
  return title;
}

@end
