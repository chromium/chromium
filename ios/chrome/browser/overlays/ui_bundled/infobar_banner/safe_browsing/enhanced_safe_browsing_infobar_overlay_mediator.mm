// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/safe_browsing/enhanced_safe_browsing_infobar_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/safe_browsing/model/enhanced_safe_browsing_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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

  if (delegate->Accept()) {
    [self dismissOverlay];
  }
}

@end

@implementation EnhancedSafeBrowsingBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  EnhancedSafeBrowsingInfobarDelegate* delegate =
      self.enhancedSafeBrowsingInfobarDelegate;
  if (!delegate) {
    return;
  }

  NSString* title = base::SysUTF16ToNSString(delegate->GetTitleText());
  NSString* subtitle = base::SysUTF16ToNSString(delegate->GetMessageText());
  NSString* buttonText = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));

  // Default to the info icon.
  UIImage* icon =
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kInfobarSymbolPointSize);

  // Use the shield icon only for the shield type on branded builds.
  if (delegate->GetIconType() == EnhancedSafeBrowsingIconType::kShield) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    icon =
        CustomSymbolWithPointSize(kGoogleShieldSymbol, kInfobarSymbolPointSize);
#endif
  }

  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
  [self.consumer setButtonText:buttonText];
  [self.consumer setIconImage:icon];
  [self.consumer setPresentsModal:NO];
}

@end
