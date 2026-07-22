// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/forms_ai_private_inference/forms_ai_private_inference_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/autofill/model/forms_ai_private_inference_infobar_delegate_ios.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ui/base/models/image_model.h"

@interface FormsAiPrivateInferenceBannerOverlayMediator ()

// The default banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation FormsAiPrivateInferenceBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

- (FormsAiPrivateInferenceInfoBarDelegateIOS*)privateInferenceDelegate {
  return static_cast<FormsAiPrivateInferenceInfoBarDelegateIOS*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

@end

@implementation FormsAiPrivateInferenceBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config) {
    return;
  }

  FormsAiPrivateInferenceInfoBarDelegateIOS* delegate =
      self.privateInferenceDelegate;
  [self.consumer
      setButtonText:base::SysUTF16ToNSString(delegate->GetButtonLabel(
                        ConfirmInfoBarDelegate::BUTTON_OK))];
  if (!delegate->GetIcon().IsEmpty()) {
    [self.consumer setIconImage:delegate->GetIcon().GetImage().ToUIImage()];
  }

  [self.consumer setPresentsModal:YES];
  [self.consumer
      setTitleText:base::SysUTF16ToNSString(delegate->GetTitleText())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(delegate->GetMessageText())];
}

@end
