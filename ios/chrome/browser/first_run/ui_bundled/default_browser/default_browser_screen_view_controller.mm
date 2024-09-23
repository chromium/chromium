// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_view_controller.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation DefaultBrowserScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier;
  self.bannerSize = BannerImageSizeType::kStandard;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.bannerName = kChromeDefaultBrowserScreenBannerImage;
#else
  self.bannerName = kChromiumDefaultBrowserScreenBannerImage;
#endif
  if (![self.titleText length] || ![self.subtitleText length]) {
    // Sets default promo text if title and subtitle text are not explicitly
    // set.
    CHECK(![self.titleText length]);
    CHECK(![self.subtitleText length]);
    [self setPromoTitle:
              l10n_util::GetNSString(
                  UseIPadTailoredStringForDefaultBrowserPromo()
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)];
    [self setPromoSubtitle:
              l10n_util::GetNSString(
                  UseIPadTailoredStringForDefaultBrowserPromo()
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE)];
  }
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_PRIMARY_ACTION);

  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);

  NSArray* defaultBrowserSteps = @[
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
    l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP)
  ];

  UIView* instructionView =
      [[InstructionView alloc] initWithList:defaultBrowserSteps];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.specificContentView addSubview:instructionView];

  [NSLayoutConstraint activateConstraints:@[
    [instructionView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [instructionView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [instructionView.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],
    [instructionView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .topAnchor],
  ]];

  [super viewDidLoad];
}

#pragma mark - DefaultBrowserScreenConsumer

- (void)setPromoTitle:(NSString*)titleText {
  self.titleText = titleText;
}

- (void)setPromoSubtitle:(NSString*)subtitleText {
  self.subtitleText = subtitleText;
}

@end
