// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/save_card_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using save_card_infobar_overlays::SaveCardModalRequestConfig;
using save_card_infobar_overlays::SaveCardMainAction;

@interface SaveCardInfobarModalOverlayMediator ()
// The save card modal config from the request.
@property(nonatomic, assign, readonly) SaveCardModalRequestConfig* config;
@end

@implementation SaveCardInfobarModalOverlayMediator

#pragma mark - Accessors

- (SaveCardModalRequestConfig*)config {
  return self.request ? self.request->GetConfig<SaveCardModalRequestConfig>()
                      : nullptr;
}

- (void)setConsumer:(id<InfobarSaveCardModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  SaveCardModalRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  NSString* cardNumber = [NSString
      stringWithFormat:@"•••• %@", base::SysUTF16ToNSString(
                                       config->card_last_four_digits())];
  // Only allow editing if the card will be uploaded and it hasn't been
  // previously saved.
  BOOL supportsEditing =
      config->should_upload_credentials() && !config->current_card_saved();

  // Convert gfx::Image to UIImage. The NSDictionary below doesn't support nil,
  // so NSNull must be used.
  const gfx::Image& avatar_gfx = config->displayed_target_account_avatar();
  NSObject* avatar =
      avatar_gfx.IsEmpty() ? [NSNull null] : avatar_gfx.ToUIImage();

  NSDictionary* prefs = @{
    kCardholderNamePrefKey :
        base::SysUTF16ToNSString(config->cardholder_name()),
    kCardIssuerIconNamePrefKey : NativeImage(config->issuer_icon_id()),
    kCardNumberPrefKey : cardNumber,
    kExpirationMonthPrefKey :
        base::SysUTF16ToNSString(config->expiration_date_month()),
    kExpirationYearPrefKey :
        base::SysUTF16ToNSString(config->expiration_date_year()),
    kLegalMessagesPrefKey : config->legal_message_lines(),
    kCurrentCardSavedPrefKey : @(config->current_card_saved()),
    kSupportsEditingPrefKey : @(supportsEditing),
    kDisplayedTargetAccountEmailPrefKey :
        base::SysUTF16ToNSString(config->displayed_target_account_email()),
    kDisplayedTargetAccountAvatarPrefKey : avatar,
  };
  [_consumer setupModalViewControllerWithPrefs:prefs];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveCardModalRequestConfig::RequestSupport();
}

#pragma mark - InfobarSaveCardModalDelegate

- (void)saveCardWithCardholderName:(NSString*)cardholderName
                   expirationMonth:(NSString*)month
                    expirationYear:(NSString*)year {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<SaveCardMainAction>(
                             cardholderName, month, year)];
  [self dismissOverlay];
}

- (void)dismissModalAndOpenURL:(const GURL&)linkURL {
  [self.save_card_delegate pendingURLToLoad:linkURL];
  [self dismissOverlay];
}

@end
