// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

CGFloat const kUnderTitleViewHeightPadding = 50;
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

}  // namespace

@implementation YoutubeIncognitoSheet {
  UIView* _icognitoIconView;
}

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)viewDidLoad {
  self.actionHandler = self;
  self.showDismissBarButton = NO;
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.aboveTitleView = [self animatedTitleView];

  self.titleString =
      l10n_util::GetNSString(IDS_IOS_YOUTUBE_INCOGNITO_SHEET_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_YOUTUBE_INCOGNITO_SHEET_SUBTITLE);
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_YOUTUBE_INCOGNITO_SHEET_PRIMARY_BUTTON_TITLE);

  self.titleTextStyle = UIFontTextStyleTitle3;
  self.scrollEnabled = YES;
  self.customSpacing = kVerticalSpacing;
  [self displayGradientView:YES];

  UIView* underTitleView = [[UIView alloc] init];
  underTitleView.translatesAutoresizingMaskIntoConstraints = NO;

  IncognitoView* incognitoView =
      [[IncognitoView alloc] initWithFrame:CGRectZero
             showTopIncognitoImageAndTitle:NO
                 stackViewHorizontalMargin:0
                         stackViewMaxWidth:CGFLOAT_MAX];
  incognitoView.bounces = NO;
  incognitoView.URLLoaderDelegate = self.URLLoaderDelegate;
  incognitoView.showsHorizontalScrollIndicator = NO;
  incognitoView.translatesAutoresizingMaskIntoConstraints = NO;
  [underTitleView addSubview:incognitoView];

  self.underTitleView = underTitleView;
  [NSLayoutConstraint activateConstraints:@[
    [underTitleView.heightAnchor
        constraintEqualToAnchor:incognitoView.contentLayoutGuide.heightAnchor
                       constant:-kUnderTitleViewHeightPadding],
    [incognitoView.heightAnchor
        constraintEqualToAnchor:incognitoView.contentLayoutGuide.heightAnchor],
    [incognitoView.centerYAnchor
        constraintEqualToAnchor:underTitleView.centerYAnchor],
    [incognitoView.centerXAnchor
        constraintEqualToAnchor:underTitleView.centerXAnchor],
    [incognitoView.widthAnchor
        constraintEqualToAnchor:underTitleView.widthAnchor],
  ]];

  [super viewDidLoad];
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

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.textAlignment = NSTextAlignmentNatural;
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // `Got it` button was clicked.
  [self.delegate didTapPrimaryActionButton];
}

#pragma mark - Private

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

@end
