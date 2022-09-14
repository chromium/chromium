// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/save_password_infobar_banner_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SavePasswordInfobarBannerOverlayMediator ()
// The save password banner config from the request.
@property(nonatomic, readonly)
    SavePasswordInfobarBannerOverlayRequestConfig* config;
@end

@implementation SavePasswordInfobarBannerOverlayMediator

#pragma mark - Accessors

- (SavePasswordInfobarBannerOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<SavePasswordInfobarBannerOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SavePasswordInfobarBannerOverlayRequestConfig::RequestSupport();
}

@end

@implementation SavePasswordInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  SavePasswordInfobarBannerOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  NSString* title = config->message();
  NSString* username = config->username();
  NSString* password = [@"" stringByPaddingToLength:config->password_length()
                                         withString:@"•"
                                    startingAtIndex:0];
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@,%@, %@", title, username,
                                 l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL)];
  [self.consumer setBannerAccessibilityLabel:bannerAccessibilityLabel];
  [self.consumer setButtonText:config->button_text()];
  [self.consumer setIconImage:[UIImage imageNamed:config->icon_image_name()]];
  [self.consumer setPresentsModal:YES];
  [self.consumer setTitleText:title];
  [self.consumer
      setSubtitleText:[NSString stringWithFormat:@"%@ %@", username, password]];
}

@end
