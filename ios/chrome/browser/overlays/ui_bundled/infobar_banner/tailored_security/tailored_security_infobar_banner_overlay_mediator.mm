// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

namespace {

// Returns the branded version of the Google shield symbol.
NSString* GetBrandedGoogleShieldSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return kGoogleShieldSymbol;
#else
  return kShieldSymbol;
#endif
}

}  // namespace

@interface TailoredSecurityInfobarBannerOverlayMediator ()
// The tailored security banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation TailoredSecurityInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (safe_browsing::TailoredSecurityServiceInfobarDelegate*)tailoredDelegate {
  return static_cast<safe_browsing::TailoredSecurityServiceInfobarDelegate*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
  infobar->set_accepted(self.tailoredDelegate->Accept());

  [self dismissOverlay];
}

@end

@implementation TailoredSecurityInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config) {
    return;
  }

  safe_browsing::TailoredSecurityServiceInfobarDelegate* delegate =
      self.tailoredDelegate;
  if (!delegate) {
    return;
  }

  NSString* title = base::SysUTF16ToNSString(delegate->GetMessageText());
  NSString* subtitle = base::SysUTF16ToNSString(delegate->GetDescription());
  NSString* buttonText =
      base::SysUTF16ToNSString(delegate->GetMessageActionText());
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@,%@", title, subtitle];
  NSString* iconImageName =
      [self iconImageNameForState:delegate->message_state()];

  [self.consumer setBannerAccessibilityLabel:bannerAccessibilityLabel];
  [self.consumer setButtonText:buttonText];
  [self.consumer setIconImage:CustomSymbolWithPointSize(
                                  iconImageName, kInfobarSymbolPointSize)];
  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
}

#pragma mark - Private methods

// Returns the icon image name corresponding to the given `state`.
- (NSString*)iconImageNameForState:
    (safe_browsing::TailoredSecurityServiceMessageState)state {
  switch (state) {
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
