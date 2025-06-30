// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/collaboration_out_of_date/collaboration_out_of_date_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_out_of_date_infobar_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ui/base/models/image_model.h"

@interface CollaborationOutOfDateInfobarBannerOverlayMediator ()
// The banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
// The delegate attached to the config.
@property(nonatomic, readonly)
    CollaborationOutOfDateInfoBarDelegate* collaborationOutOfDateDelegate;
@end

@implementation CollaborationOutOfDateInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

- (CollaborationOutOfDateInfoBarDelegate*)collaborationOutOfDateDelegate {
  return static_cast<CollaborationOutOfDateInfoBarDelegate*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // This can happen if the user quickly navigates to another website while the
  // banner is still appearing, causing the banner to be triggered before being
  // removed.
  if (!self.collaborationOutOfDateDelegate) {
    return;
  }

  self.collaborationOutOfDateDelegate->Accept();
  [self dismissOverlay];
}

@end

@implementation
    CollaborationOutOfDateInfobarBannerOverlayMediator (ConsumerSupport)

// Configures consumer from the settings in `config`.
- (void)configureConsumer {
  id<InfobarBannerConsumer> consumer = self.consumer;
  CollaborationOutOfDateInfoBarDelegate* delegate =
      self.collaborationOutOfDateDelegate;

  [consumer
      setButtonText:base::SysUTF16ToNSString(delegate->GetButtonLabel(
                        CollaborationOutOfDateInfoBarDelegate::BUTTON_OK))];

  UIImage* symbolImage = delegate->GetIcon().GetImage().ToUIImage();
  [consumer setIconImage:symbolImage];

  [consumer setPresentsModal:NO];
  [consumer setTitleText:base::SysUTF16ToNSString(delegate->GetMessageText())];
}

@end
