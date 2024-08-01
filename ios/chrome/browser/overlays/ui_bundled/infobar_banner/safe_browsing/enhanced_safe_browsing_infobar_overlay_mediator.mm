// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/safe_browsing/enhanced_safe_browsing_infobar_overlay_mediator.h"

#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/safe_browsing/model/enhanced_safe_browsing_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface EnhancedSafeBrowsingBannerOverlayMediator ()

@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation EnhancedSafeBrowsingBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (EnhancedSafeBrowsingInfobarDelegate*)enhancedSafeBrowsingInfobarDelegate {
  return static_cast<EnhancedSafeBrowsingInfobarDelegate*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  EnhancedSafeBrowsingInfobarDelegate* delegate =
      self.enhancedSafeBrowsingInfobarDelegate;
  if (!delegate) {
    // This can happen if the user quickly navigates to another website while
    // the banner is still appearing, where the infobar owning the delegate is
    // deleted before handling the button action.
    return;
  }

  [self dismissOverlay];

  delegate->ShowSafeBrowsingSettings();
}

@end

@implementation EnhancedSafeBrowsingBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INFOBAR_TITLE);
  NSString* subtitle = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INFOBAR_DESCRIPTION);
  NSString* buttonText = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INFOBAR_BUTTON_TEXT);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* gIcon =
      CustomSymbolWithPointSize(kGoogleShieldSymbol, kInfobarSymbolPointSize);
#else
  UIImage* gIcon =
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kInfobarSymbolPointSize);
#endif

  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
  [self.consumer setButtonText:buttonText];
  [self.consumer setIconImage:gIcon];
  [self.consumer setPresentsModal:NO];
}

@end
