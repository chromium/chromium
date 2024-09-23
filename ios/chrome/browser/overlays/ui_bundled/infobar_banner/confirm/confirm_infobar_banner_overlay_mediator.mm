// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ui/base/l10n/l10n_util.h"

using confirm_infobar_overlays::ConfirmBannerRequestConfig;

@interface ConfirmInfobarBannerOverlayMediator ()
// The confirm banner config from the request.
@property(nonatomic, readonly) ConfirmBannerRequestConfig* config;
@end

@implementation ConfirmInfobarBannerOverlayMediator

#pragma mark - Accessors

- (ConfirmBannerRequestConfig*)config {
  return self.request ? self.request->GetConfig<ConfirmBannerRequestConfig>()
                      : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return ConfirmBannerRequestConfig::RequestSupport();
}

- (void)finishDismissal {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarBannerRemoveInfobarResponse>()];
}

@end

@implementation ConfirmInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  ConfirmBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer
      setButtonText:base::SysUTF16ToNSString(config->button_label_text())];
  if (!config->icon_image().IsEmpty()) {
    [self.consumer setIconImage:config->icon_image().ToUIImage()];
    [self.consumer setUseIconBackgroundTint:config->use_icon_background_tint()];
  }
  [self.consumer setPresentsModal:NO];
  if (config->title_text().empty()) {
    [self.consumer
        setTitleText:base::SysUTF16ToNSString(config->message_text())];
  } else {
    [self.consumer setTitleText:base::SysUTF16ToNSString(config->title_text())];
    [self.consumer
        setSubtitleText:base::SysUTF16ToNSString(config->message_text())];
  }
}

@end
