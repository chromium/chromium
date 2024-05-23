// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ui/gfx/image/image.h"

@interface SaveCardInfobarModalOverlayMediator ()
// The save card modal config from the request.
@property(nonatomic, assign, readonly)
    DefaultInfobarOverlayRequestConfig* config;
@end

@implementation SaveCardInfobarModalOverlayMediator

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

- (void)setConsumer:(id<InfobarSaveCardModalConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;
  if (!delegate) {
    return;
  }

  InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
  NSString* cardNumber = [NSString
      stringWithFormat:@"•••• %@", base::SysUTF16ToNSString(
                                       delegate->card_last_four_digits())];

  // Only allow editing if the card will be uploaded and it hasn't been
  // previously saved.
  BOOL supportsEditing = delegate->is_for_upload() && !infobar->accepted();

  // Convert gfx::Image to UIImage. The NSDictionary below doesn't support nil,
  // so NSNull must be used.
  const gfx::Image& avatarGfx = delegate->displayed_target_account_avatar();
  NSObject* avatar =
      avatarGfx.IsEmpty() ? [NSNull null] : avatarGfx.ToUIImage();

  NSDictionary* prefs = @{
    kCardholderNamePrefKey :
        base::SysUTF16ToNSString(delegate->cardholder_name()),
    kCardIssuerIconNamePrefKey : NativeImage(delegate->issuer_icon_id()),
    kCardNumberPrefKey : cardNumber,
    kExpirationMonthPrefKey :
        base::SysUTF16ToNSString(delegate->expiration_date_month()),
    kExpirationYearPrefKey :
        base::SysUTF16ToNSString(delegate->expiration_date_year()),
    kLegalMessagesPrefKey : [self legalMessages],
    kCurrentCardSaveAcceptedPrefKey : @(infobar->accepted()),
    kSupportsEditingPrefKey : @(supportsEditing),
    kDisplayedTargetAccountEmailPrefKey :
        base::SysUTF16ToNSString(delegate->displayed_target_account_email()),
    kDisplayedTargetAccountAvatarPrefKey : avatar,
  };
  [_consumer setupModalViewControllerWithPrefs:prefs];

  // If Modal has been accepted and card is being uploaded, show Modal in
  // loading state with an activity indicator.
  if (delegate->is_for_upload() && infobar->accepted()) {
    [self.consumer showLoadingState];
  }
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarSaveCardModalDelegate

- (void)saveCardWithCardholderName:(NSString*)cardholderName
                   expirationMonth:(NSString*)month
                    expirationYear:(NSString*)year {
  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;
  InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
  infobar->set_accepted(delegate->UpdateAndAccept(
      base::SysNSStringToUTF16(cardholderName), base::SysNSStringToUTF16(month),
      base::SysNSStringToUTF16(year)));

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    [self.consumer showLoadingState];
  } else {
    [self dismissOverlay];
  }
}

- (void)dismissModalAndOpenURL:(const GURL&)linkURL {
  [self.save_card_delegate pendingURLToLoad:linkURL];
  [self dismissOverlay];
}

#pragma mark - Private

// Returns an array of UI SaveCardMessageWithLinks model objects.
- (NSMutableArray<SaveCardMessageWithLinks*>*)legalMessages {
  autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
      self.saveCardDelegate;
  // Only display legal Messages if the card is being uploaded and there are
  // any.
  if (delegate->is_for_upload() && !delegate->legal_message_lines().empty()) {
    return
        [SaveCardMessageWithLinks convertFrom:delegate->legal_message_lines()];
  }
  return [[NSMutableArray alloc] init];
}

@end
