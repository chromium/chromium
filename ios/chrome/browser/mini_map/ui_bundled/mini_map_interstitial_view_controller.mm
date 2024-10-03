// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_interstitial_view_controller.h"

#import "base/notreached.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Internal URL to catch "open Content Settings" tap.
const char* const kSettingsContentsURL = "internal://settings-contents";

// The size of the Map logo.
constexpr CGFloat kSymbolImagePointSize = 36;

// Returns the branded version of the Google maps symbol.
UIImage* GetBrandedGoogleMapsSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleMapsSymbol, kSymbolImagePointSize));
#else
  return DefaultSymbolWithPointSize(kMapSymbol, kSymbolImagePointSize);
#endif
}

}  // namespace

// A proxy class to rename the PromoStyleViewControllerDelegate methods to
// MiniMapActionHandler methods.
@interface MiniMapInterstitialDelegateMethodProxy
    : NSObject <PromoStyleViewControllerDelegate>
@property(nonatomic, weak) id<MiniMapActionHandler> actionHandler;
@end

@implementation MiniMapInterstitialDelegateMethodProxy
- (void)didTapPrimaryActionButton {
  [self.actionHandler userPressedConsent];
}

- (void)didTapSecondaryActionButton {
  [self.actionHandler userPressedNoThanks];
}

// Invoked when a link in the disclaimer is tapped.
- (void)didTapURLInDisclaimer:(NSURL*)URL {
  [self.actionHandler userPressedContentSettings];
}
@end

@interface MiniMapInterstitialViewController () <MiniMapActionHandler>

@end

@implementation MiniMapInterstitialViewController {
  MiniMapInterstitialDelegateMethodProxy* _delegateMethodProxy;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.titleText = l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_TITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_ACCEPT);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_NO_THANKS);

  self.subtitleText = l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_SUBTITLE);
  self.disclaimerText = l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_FOOTER);
  self.disclaimerURLs = @[ net::NSURLWithGURL(GURL(kSettingsContentsURL)) ];

  self.headerImageType = PromoStyleImageType::kImageWithShadow;
  self.headerImage = GetBrandedGoogleMapsSymbol();
  self.bannerLimitWithRoundedCorner = YES;

  self.bannerName = @"map_blur";
  self.bannerSize = BannerImageSizeType::kExtraTall;
  self.shouldBannerFillTopSpace = YES;
  self.scrollToEndMandatory = YES;
  self.topAlignedLayout = YES;

  self.hideSpecificContentView = YES;

  self.readMoreString = l10n_util::GetNSString(IDS_IOS_MINI_MAP_CONSENT_MORE);
  _delegateMethodProxy = [[MiniMapInterstitialDelegateMethodProxy alloc] init];
  _delegateMethodProxy.actionHandler = self;
  self.delegate = _delegateMethodProxy;
  [super viewDidLoad];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    [self
        registerForTraitChanges:traits
                     withAction:@selector(toggleBannerVisibilityOnTraitChange)];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self toggleBannerVisibilityOnTraitChange];
}
#endif

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.actionHandler dismissed];
}

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle1;
}

- (UIFontTextStyle)disclaimerLabelFontTextStyle {
  return UIFontTextStyleFootnote;
}

#pragma mark - MiniMapActionHandler methods

- (void)userPressedConsent {
  [self.actionHandler userPressedConsent];
}

- (void)userPressedNoThanks {
  [self.actionHandler userPressedNoThanks];
}

- (void)dismissed {
  NOTREACHED_IN_MIGRATION();
}

- (void)userPressedContentSettings {
  [self.actionHandler userPressedContentSettings];
}

#pragma mark - Private

// Sets the `shouldHideBanner` to true if the device's vertical orientation is
// compact.
- (void)toggleBannerVisibilityOnTraitChange {
  self.shouldHideBanner =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
}

@end
