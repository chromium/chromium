// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SaveCardInfobarBannerOverlayMediator ()

// The save card banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation SaveCardInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
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

  // Display the modal (thus the ToS) if the card will be uploaded, this is a
  // legal requirement and shouldn't be changed.
  if (delegate->is_for_upload()) {
    [self presentInfobarModalFromBanner];
    return;
  }

  InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
  infobar->set_accepted(delegate->UpdateAndAccept(
      delegate->cardholder_name(), delegate->expiration_date_month(),
      delegate->expiration_date_year()));

  [self dismissOverlay];
}

@end

@implementation SaveCardInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;
  if (!delegate) {
    return;
  }

  std::u16string buttonLabelText =
      delegate->is_for_upload()
          ? l10n_util::GetStringUTF16(IDS_IOS_AUTOFILL_SAVE_ELLIPSIS)
          : delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  [self.consumer setButtonText:base::SysUTF16ToNSString(buttonLabelText)];

  UIImage* iconImage = DefaultSymbolTemplateWithPointSize(
      kCreditCardSymbol, kInfobarSymbolPointSize);
  [self.consumer setIconImage:iconImage];

  [self.consumer
      setTitleText:base::SysUTF16ToNSString(delegate->GetMessageText())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(delegate->card_label())];
}

@end
