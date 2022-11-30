// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/sync_error/sync_error_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/sync_error_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_error_infobar_overlays::SyncErrorBannerRequestConfig;

@interface SyncErrorInfobarBannerOverlayMediator ()
// The sync error banner config from the request.
@property(nonatomic, readonly) SyncErrorBannerRequestConfig* config;
@end

@implementation SyncErrorInfobarBannerOverlayMediator

#pragma mark - Accessors

// SyncErrorBannerRequestConfig.
- (SyncErrorBannerRequestConfig*)config {
  return self.request ? self.request->GetConfig<SyncErrorBannerRequestConfig>()
                      : nullptr;
}

#pragma mark - OverlayRequestMediator

// RequestSupport.
+ (const OverlayRequestSupport*)requestSupport {
  return SyncErrorBannerRequestConfig::RequestSupport();
}

- (void)finishDismissal {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarBannerRemoveInfobarResponse>()];
}

@end

@implementation SyncErrorInfobarBannerOverlayMediator (ConsumerSupport)

// Configures consumer from the settings in `config`.
- (void)configureConsumer {
  id<InfobarBannerConsumer> consumer = self.consumer;
  SyncErrorBannerRequestConfig* config = self.config;
  if (!consumer || !config)
    return;

  [consumer
      setButtonText:base::SysUTF16ToNSString(config->button_label_text())];
  if (!config->icon_image().IsEmpty()) {
    [consumer setIconImage:config->icon_image().ToUIImage()];
    [consumer setUseIconBackgroundTint:config->use_icon_background_tint()];
    [consumer setIconBackgroundColor:config->background_tint_color()];
    [consumer setIconImageTintColor:config->icon_image_tint_color()];
  }
  [consumer setPresentsModal:NO];
  if (config->title_text().empty()) {
    [consumer setTitleText:base::SysUTF16ToNSString(config->message_text())];
  } else {
    [consumer setTitleText:base::SysUTF16ToNSString(config->title_text())];
    [consumer setSubtitleText:base::SysUTF16ToNSString(config->message_text())];
  }
}

@end
