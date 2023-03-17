// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_card_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardBannerRequestConfig;
using save_card_infobar_overlays::SaveCardMainAction;

@interface SaveCardInfobarBannerOverlayMediator ()
// The save card banner config from the request.
@property(nonatomic, readonly) SaveCardBannerRequestConfig* config;
@end

@implementation SaveCardInfobarBannerOverlayMediator

#pragma mark - Accessors

- (SaveCardBannerRequestConfig*)config {
  return self.request ? self.request->GetConfig<SaveCardBannerRequestConfig>()
                      : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveCardBannerRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Display the modal (thus the ToS) if the card will be uploaded, this is a
  // legal requirement and shouldn't be changed.
  if (self.config->should_upload_credentials()) {
    [self dispatchResponse:OverlayResponse::CreateWithInfo<
                               InfobarBannerShowModalResponse>()];
    return;
  }
  // Notify the model layer to perform the infobar's main action before
  // dismissing the banner.
  [self dispatchResponse:OverlayResponse::CreateWithInfo<SaveCardMainAction>(
                             base::SysUTF16ToNSString(
                                 self.config->cardholder_name()),
                             base::SysUTF16ToNSString(
                                 self.config->expiration_date_month()),
                             base::SysUTF16ToNSString(
                                 self.config->expiration_date_year()))];
  [self dismissOverlay];
}

@end

@implementation SaveCardInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  SaveCardBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer
      setButtonText:base::SysUTF16ToNSString(self.config->button_label_text())];
  UIImage* iconImage = DefaultSymbolTemplateWithPointSize(
      kCreditCardSymbol, kInfobarSymbolPointSize);
  [self.consumer setIconImage:iconImage];
  [self.consumer
      setTitleText:base::SysUTF16ToNSString(self.config->message_text())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(self.config->card_label())];
}

@end
