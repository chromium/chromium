// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/autofill/model/credit_card/autofill_save_card_infobar_delegate_ios.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_coordinator+modal_configuration.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/save_card/save_card_infobar_modal_overlay_mediator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ui/gfx/image/image.h"

namespace {
// Time duration to wait before auto-closing modal in save card success
// confirmation state.
static constexpr base::TimeDelta kConfirmationStateDuration =
    base::Seconds(1.5);

// Time duration to wait before auto-closing modal in save card success
// confirmation state when VoiceOver is running. This is slightly greater than
// `kConfirmationStateDuration` to give VoiceOver enough time to read the
// required content.
// TODO(crbug.com/339887700): When VO is running do not use this and listen for
// VO announcement to finish before auto-closing the modal in confirmation
// state.
static constexpr base::TimeDelta kConfirmationStateDurationIfVoiceOverRunning =
    base::Seconds(5);

}  // namespace

@interface SaveCardInfobarModalOverlayMediator ()
// The save card modal config from the request.
@property(nonatomic, assign, readonly)
    DefaultInfobarOverlayRequestConfig* config;
@end

@implementation SaveCardInfobarModalOverlayMediator {
  // Timer that controls auto closure of modal in save card success confirmation
  // state.
  base::OneShotTimer _autoCloseConfirmationTimer;

  // Holds a value when loading and confirmation is enabled. `NO` indicates
  // modal is in loading state. `YES` indicates modal is in confirmation state.
  std::optional<BOOL> _creditCardUploadCompleted;

  BOOL _loadingDismissedByUser;
}

- (instancetype)initWithRequest:(OverlayRequest*)request {
  self = [super initWithRequest:request];
  if (self) {
    DefaultInfobarOverlayRequestConfig* config =
        request ? request->GetConfig<DefaultInfobarOverlayRequestConfig>()
                : nullptr;

    if (config) {
      autofill::AutofillSaveCardInfoBarDelegateIOS* delegate =
          static_cast<autofill::AutofillSaveCardInfoBarDelegateIOS*>(
              config->delegate());
      __weak __typeof__(self) weakSelf = self;
      delegate->SetCreditCardUploadCompletionCallback(
          base::BindOnce(^(BOOL card_saved) {
            [weakSelf creditCardUploadCompleted:card_saved];
          }));
    }
  }
  return self;
}

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config or `nullptr` if there is no
// config.
- (autofill::AutofillSaveCardInfoBarDelegateIOS*)saveCardDelegate {
  return self.config
             ? static_cast<autofill::AutofillSaveCardInfoBarDelegateIOS*>(
                   self.config->delegate())
             : nullptr;
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

  delegate->SetInfobarIsPresenting(YES);

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

  if (delegate->is_for_upload() && infobar->accepted() &&
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    // If the infobar has been accepted and the card upload is in progress or
    // complete, display the appropriate progress state (loading or
    // confirmation).
    [self.consumer
        showProgressWithUploadCompleted:delegate->IsCreditCardUploadComplete()];
  }
}

#pragma mark - Public

- (void)creditCardUploadCompleted:(BOOL)card_saved {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    if (!_loadingDismissedByUser) {
      autofill::autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
          autofill::autofill_metrics::SaveCardPromptResult::kNotInteracted);
    }
    if (card_saved) {
      autofill::autofill_metrics::
          LogCreditCardUploadConfirmationViewShownMetric(
              /*is_shown=*/true, /*is_card_uploaded=*/true);

      _creditCardUploadCompleted = YES;
      [self.consumer showProgressWithUploadCompleted:YES];

      // Auto close modal after showing successful card save confirmation.
      __weak __typeof(self) weakSelf = self;
      _autoCloseConfirmationTimer.Start(
          FROM_HERE,
          UIAccessibilityIsVoiceOverRunning()
              ? kConfirmationStateDurationIfVoiceOverRunning
              : kConfirmationStateDuration,
          base::BindOnce(^{
            [weakSelf dimissConfirmationStateOnTimeout];
          }));
    } else {
      // On card save failure, this modal is dimissed and user is shown an error
      // dialog triggered from IOSChromePaymentsAutofillClient.
      [self dismissOverlay];
    }
  }
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

- (void)dismissOverlay {
  if (self.saveCardDelegate) {
    self.saveCardDelegate->SetCreditCardUploadCompletionCallback(
        base::NullCallback());
    self.saveCardDelegate->SetInfobarIsPresenting(NO);
  }
  [super dismissOverlay];
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
    autofill::autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
        /*is_shown=*/true);
    _creditCardUploadCompleted = NO;
    [self.consumer showProgressWithUploadCompleted:NO];
  } else {
    autofill::autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
        /*is_shown=*/false);
    [self dismissOverlay];
  }
}

- (void)dismissModalAndOpenURL:(const GURL&)linkURL {
  [self.save_card_delegate pendingURLToLoad:linkURL];
  [self dismissOverlay];
}

- (void)dimissConfirmationStateOnTimeout {
  [self dismissOverlay];
  [self onConfirmationClosedWithAutoClose:YES];
}

- (void)dismissInfobarModal:(id)infobarModal {
  base::RecordAction(base::UserMetricsAction(kInfobarModalCancelButtonTapped));
  [self dismissOverlay];

  // When loading and confirmation feature is enabled and credit card upload is
  // completed, modal would be showing a success confirmation and value of
  // `_creditCardUploadCompleted` would be `YES`. Modal getting closed from here
  // means user dismissed it using the close button.
  if (_creditCardUploadCompleted.has_value() &&
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation)) {
    if (_creditCardUploadCompleted.value()) {
      [self onConfirmationClosedWithAutoClose:NO];
    } else {
      _loadingDismissedByUser = YES;
      autofill::autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
          autofill::autofill_metrics::SaveCardPromptResult::kClosed);
    }
  }
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

// Called when modal gets closed in confirmation state. Logs how the modal got
// closed and calls `AutofillSaveCardInfoBarDelegateIOS::OnConfirmationClosed`.
- (void)onConfirmationClosedWithAutoClose:(BOOL)autoClosed {
  autofill::autofill_metrics::LogCreditCardUploadConfirmationViewResultMetric(
      autoClosed
          ? autofill::autofill_metrics::SaveCardPromptResult::kNotInteracted
          : autofill::autofill_metrics::SaveCardPromptResult::kClosed,
      /*is_card_uploaded=*/true);
  _autoCloseConfirmationTimer.Stop();
  if (!self.saveCardDelegate) {
    return;
  }
  self.saveCardDelegate->OnConfirmationClosed();
}

@end
