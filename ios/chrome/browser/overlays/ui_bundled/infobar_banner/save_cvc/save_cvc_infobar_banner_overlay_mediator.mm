// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_cvc/save_cvc_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SaveCVCInfobarBannerOverlayMediator ()

// The banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation SaveCVCInfobarBannerOverlayMediator {
  // `YES` if the banner's button was pressed by the user.
  BOOL _bannerButtonWasPressed;
}

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  if (auto* request = self.request) {
    return request->GetConfig<DefaultInfobarOverlayRequestConfig>();
  }
  return nullptr;
}

// Returns the delegate attached to the config.
- (autofill::AutofillSaveCardInfoBarDelegateIOS*)saveCardDelegate {
  return static_cast<autofill::AutofillSaveCardInfoBarDelegateIOS*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;

  _bannerButtonWasPressed = YES;

  delegate->LogSaveCvcInfoBarResultMetric(
      autofill::autofill_metrics::SaveCvcPromptResultIOS::kAccepted);

  // For both local and upload CVC saves, accept immediately without showing a
  // modal.
  delegate->UpdateAndAccept(
      delegate->cardholder_name(), delegate->expiration_date_month(),
      delegate->expiration_date_year(), delegate->card_cvc());
  [self dismissOverlay];
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;
  if (delegate) {
    if (!userInitiated) {
      // Banner is dismissed without user interaction when it times out.
      delegate->LogSaveCvcInfoBarResultMetric(
          autofill::autofill_metrics::SaveCvcPromptResultIOS::kTimedOut);
    } else if (userInitiated && !_bannerButtonWasPressed) {
      // Banner is dismissed by swiping.
      delegate->LogSaveCvcInfoBarResultMetric(
          autofill::autofill_metrics::SaveCvcPromptResultIOS::kSwiped);
    }
  }
  [super dismissInfobarBannerForUserInteraction:userInitiated];
}

@end

@implementation SaveCVCInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config) {
    return;
  }

  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;
  if (!delegate) {
    return;
  }

  [self.consumer
      setButtonText:base::SysUTF16ToNSString(delegate->GetButtonLabel(
                        ConfirmInfoBarDelegate::BUTTON_OK))];

  UIImage* iconImage = DefaultSymbolTemplateWithPointSize(
      kCreditCardSymbol, kInfobarSymbolPointSize);
  [self.consumer setIconImage:iconImage];

  [self.consumer
      setTitleText:base::SysUTF16ToNSString(delegate->GetMessageText())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(delegate->GetDescriptionText())];
}

@end
