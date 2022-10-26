// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_service_infobar_delegate.h"
#import "ios/chrome/browser/ui/icons/buildflags.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of the symbol image.
CGFloat kSymbolImagePointSize = 18.;

// Returns the branded version of the Google shield symbol.
NSString* GetBrandedGoogleShieldSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return kGoogleShieldSymbol;
#else
  return kShieldSymbol;
#endif
}

}  // namespace

using tailored_security_service_infobar_overlays::
    TailoredSecurityServiceBannerRequestConfig;

@interface TailoredSecurityInfobarBannerOverlayMediator ()
// The tailored security banner config from the request.
@property(nonatomic, readonly)
    TailoredSecurityServiceBannerRequestConfig* config;
@end

@implementation TailoredSecurityInfobarBannerOverlayMediator

#pragma mark - Accessors

- (TailoredSecurityServiceBannerRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<TailoredSecurityServiceBannerRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return TailoredSecurityServiceBannerRequestConfig::RequestSupport();
}

@end

@implementation TailoredSecurityInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  TailoredSecurityServiceBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  NSString* title = base::SysUTF16ToNSString(config->message_text());
  NSString* subtitle = base::SysUTF16ToNSString(config->description());
  NSString* buttonText = base::SysUTF16ToNSString(config->button_label_text());
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@,%@", title, subtitle];
  NSString* icon_image_name = [self chooseIconImageName:config];
  [self.consumer setBannerAccessibilityLabel:bannerAccessibilityLabel];
  [self.consumer setButtonText:buttonText];
  [self.consumer setIconImage:CustomSymbolWithPointSize(icon_image_name,
                                                        kSymbolImagePointSize)];
  [self.consumer setPresentsModal:config->has_badge()];
  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
}

#pragma mark - Private methods

// Conversion function to choose NSString needed for UIImage.
- (NSString*)chooseIconImageName:
    (TailoredSecurityServiceBannerRequestConfig*)config {
  safe_browsing::TailoredSecurityServiceMessageState message_state =
      config->message_state();
  switch (message_state) {
    case safe_browsing::TailoredSecurityServiceMessageState::
        kConsentedAndFlowEnabled:
    case safe_browsing::TailoredSecurityServiceMessageState::
        kUnconsentedAndFlowEnabled:
      return GetBrandedGoogleShieldSymbol();
    case safe_browsing::TailoredSecurityServiceMessageState::
        kConsentedAndFlowDisabled:
      return kShieldSymbol;
  }
}

@end
