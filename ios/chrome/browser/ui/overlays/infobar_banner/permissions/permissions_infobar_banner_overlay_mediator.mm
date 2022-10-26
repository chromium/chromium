// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_banner/permissions_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PermissionsBannerOverlayMediator ()
// The permissions banner config from the request.
@property(nonatomic, readonly) PermissionsBannerRequestConfig* config;
@end

@implementation PermissionsBannerOverlayMediator

#pragma mark - Accessors

- (PermissionsBannerRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<PermissionsBannerRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return PermissionsBannerRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Present the modal if the 'Edit' button is pressed.
  [self presentInfobarModalFromBanner];
}

@end

@implementation PermissionsBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  PermissionsBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer setTitleText:config->title_text()];
  [self.consumer setButtonText:config->button_text()];

  UIImage* iconImage =
      config->is_camera_accessible()
          ? CustomSymbolWithPointSize(kCameraFillSymbol,
                                      kInfobarSymbolPointSize)
          : DefaultSymbolWithPointSize(kMicrophoneFillSymbol,
                                       kInfobarSymbolPointSize);
  [self.consumer setIconImage:iconImage];
  [self.consumer setPresentsModal:NO];
}

@end
