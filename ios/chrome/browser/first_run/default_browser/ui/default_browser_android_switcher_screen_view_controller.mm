// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_android_switcher_screen_view_controller.h"

#import "build/branding_buildflags.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_view_controller+subclassing.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/shared/ui/image/image_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the leading symbol icon in each feature row.
constexpr CGFloat kLeadingSymbolImagePointSize = 18.0;

// Bottom margin between the subtitle and the feature card.
constexpr CGFloat kSubtitleBottomMargin = 24.0;

// Top margin above the header image.
constexpr CGFloat kHeaderImageTopMargin = 24.0;

// Returns the header image name for the promo screen.
NSString* HeaderImageName() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return kChromeDefaultBrowser1pBannerImage;
#else
  return kChromiumDefaultBrowser1pBannerImage;
#endif
}

// Creates the leading icon view for a feature row with `symbol`.
UIView* CreateFeatureIconView(Symbol symbol) {
  ColorfulSymbolContentConfiguration* configuration =
      [[ColorfulSymbolContentConfiguration alloc] init];
  configuration.symbolImage =
      SymbolTemplateWithPointSize(symbol, kLeadingSymbolImagePointSize);
  configuration.symbolBackgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  configuration.symbolTintColor = [UIColor colorNamed:kBlueColor];
  return [configuration makeContentView];
}

}  // namespace

@implementation DefaultBrowserAndroidSwitcherScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Top-align the layout instead of using the percentage-based top margin
  // applied to header background images, and add a fixed top margin so the
  // header keeps some breathing room while still fitting in iPad form sheets.
  self.topAlignedLayout = YES;
  self.additionalSafeAreaInsets =
      UIEdgeInsetsMake(kHeaderImageTopMargin, 0, 0, 0);
  self.subtitleBottomMargin = kSubtitleBottomMargin;
  [super viewDidLoad];
  self.view.accessibilityIdentifier = first_run::
      kFirstRunDefaultBrowserAndroidSwitcherScreenAccessibilityIdentifier;
}

#pragma mark - DefaultBrowserScreenViewController (Subclassing)

- (void)configureBanner {
  self.shouldHideBanner = YES;
  self.hideHeaderOnTallContent = YES;
  self.headerImageType = PromoStyleImageType::kImage;
  self.headerBackgroundImage = [UIImage imageNamed:HeaderImageName()];
}

- (BOOL)shouldTopAlignContentView {
  return YES;
}

- (NSString*)defaultSubtitleText {
  BOOL usesTablet = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  return l10n_util::GetNSString(
      usesTablet
          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SUBTITLE_IPAD
          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SUBTITLE);
}

- (UIView*)contentViewForPromo {
  NSArray<NSString*>* instructions = @[
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_YOUTUBE_ROW),
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_MAPS_ROW),
    l10n_util::GetNSString(
        IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_DRIVE_PHOTOS_ROW),
  ];
  NSArray<UIView*>* iconViews = @[
    CreateFeatureIconView(SymbolExternalLink),
    CreateFeatureIconView(SymbolLocation),
    CreateFeatureIconView(SymbolShare),
  ];

  return [[InstructionView alloc] initWithList:instructions
                                         style:InstructionViewStyleDefault
                                     iconViews:iconViews];
}

@end
