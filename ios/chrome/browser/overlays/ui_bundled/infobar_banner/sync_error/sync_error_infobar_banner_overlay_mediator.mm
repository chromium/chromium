// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/sync_error/sync_error_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_error_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface SyncErrorInfobarBannerOverlayMediator ()
// The sync error banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation SyncErrorInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (SyncErrorInfoBarDelegate*)syncErrorDelegate {
  return static_cast<SyncErrorInfoBarDelegate*>(self.config->delegate());
}

#pragma mark - OverlayRequestMediator

// RequestSupport.
+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // This can happen if the user quickly navigates to another website while the
  // banner is still appearing, causing the banner to be triggered before being
  // removed.
  if (!self.syncErrorDelegate) {
    return;
  }

  self.syncErrorDelegate->Accept();

  [self dismissOverlay];
}

@end

@implementation SyncErrorInfobarBannerOverlayMediator (ConsumerSupport)

// Configures consumer from the settings in `config`.
- (void)configureConsumer {
  id<InfobarBannerConsumer> consumer = self.consumer;
  SyncErrorInfoBarDelegate* delegate = self.syncErrorDelegate;

  [consumer setButtonText:base::SysUTF16ToNSString(delegate->GetButtonLabel(
                              SyncErrorInfoBarDelegate::BUTTON_OK))];

  UIImage* iconImage = DefaultSymbolTemplateWithPointSize(
      kSyncErrorSymbol, kInfobarSymbolPointSize);

  [consumer setIconImage:iconImage];
  [consumer setUseIconBackgroundTint:YES];
  [consumer setIconBackgroundColor:[UIColor colorNamed:kRed500Color]];
  [consumer setIconImageTintColor:[UIColor colorNamed:kPrimaryBackgroundColor]];

  [consumer setPresentsModal:NO];
  if (delegate->GetTitleText().empty()) {
    [consumer
        setTitleText:base::SysUTF16ToNSString(delegate->GetMessageText())];
  } else {
    [consumer setTitleText:base::SysUTF16ToNSString(delegate->GetTitleText())];
    [consumer
        setSubtitleText:base::SysUTF16ToNSString(delegate->GetMessageText())];
  }
}

@end
