// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/collaboration_group/collaboration_group_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_group_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface CollaborationGroupInfobarBannerOverlayMediator ()
// The banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
// The delegate attached to the config.
@property(nonatomic, readonly)
    CollaborationGroupInfoBarDelegate* collaborationGroupDelegate;
@end

@implementation CollaborationGroupInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

- (CollaborationGroupInfoBarDelegate*)collaborationGroupDelegate {
  return static_cast<CollaborationGroupInfoBarDelegate*>(
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
  if (!self.collaborationGroupDelegate) {
    return;
  }

  self.collaborationGroupDelegate->Accept();
  [self dismissOverlay];
}

@end

@implementation CollaborationGroupInfobarBannerOverlayMediator (ConsumerSupport)

// Configures consumer from the settings in `config`.
- (void)configureConsumer {
  id<InfobarBannerConsumer> consumer = self.consumer;
  CollaborationGroupInfoBarDelegate* delegate = self.collaborationGroupDelegate;

  [consumer setButtonText:base::SysUTF16ToNSString(delegate->GetButtonLabel(
                              CollaborationGroupInfoBarDelegate::BUTTON_OK))];

  UIImage* avatarImage = delegate->GetAvatarImage();
  if (!avatarImage) {
    // TODO(crbug.com/375595834): Update this once defined in the specs.
    avatarImage = DefaultSymbolTemplateWithPointSize(kPersonFillSymbol,
                                                     kInfobarSymbolPointSize);
    [consumer setUseIconBackgroundTint:YES];
    [consumer setIconBackgroundColor:[UIColor colorNamed:kBlueColor]];
    [consumer
        setIconImageTintColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
  }
  [consumer setIconImage:avatarImage];

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
