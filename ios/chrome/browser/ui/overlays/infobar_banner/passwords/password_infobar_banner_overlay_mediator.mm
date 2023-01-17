// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordInfobarBannerOverlayMediator ()
// The password banner config from the request.
@property(nonatomic, readonly)
    PasswordInfobarBannerOverlayRequestConfig* config;
@end

@implementation PasswordInfobarBannerOverlayMediator

#pragma mark - Accessors

- (PasswordInfobarBannerOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<PasswordInfobarBannerOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return PasswordInfobarBannerOverlayRequestConfig::RequestSupport();
}

#pragma mark - Private

// The icon image, can be from the config or not from it.
- (UIImage*)iconImageWithConfig:
    (PasswordInfobarBannerOverlayRequestConfig*)config {
  UIImage* image;
  if (UseSymbols()) {
    image = CustomSymbolWithPointSize(kPasswordSymbol, kInfobarSymbolPointSize);
  } else {
    image = [UIImage imageNamed:@"password_key"];
  }
  return image;
}

@end

@implementation PasswordInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  PasswordInfobarBannerOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  NSString* title = config->title();
  NSString* subtitle = config->subtitle();
  NSString* bannerAccessibilityLabel = config->customAccessibilityLabel();
  if (bannerAccessibilityLabel) {
    [self.consumer setBannerAccessibilityLabel:bannerAccessibilityLabel];
  }
  [self.consumer setButtonText:config->button_text()];
  [self.consumer setIconImage:[self iconImageWithConfig:config]];
  [self.consumer setPresentsModal:YES];
  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
}

@end
