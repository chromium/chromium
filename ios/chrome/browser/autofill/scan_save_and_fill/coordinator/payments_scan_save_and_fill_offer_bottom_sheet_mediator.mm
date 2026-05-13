// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "components/autofill/core/browser/form_import/form_data_importer.h"
#import "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/web/public/web_state.h"

@implementation PaymentsScanSaveAndFillOfferBottomSheetMediator {
  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;

  // The object that provides suggestions while filling forms.
  __weak id<FormInputSuggestionsProvider> _provider;

  // The timer started when the view appeared.
  std::optional<base::ElapsedTimer> _viewDidAppearTimer;

  // The WebState this mediator is associated with.
  raw_ptr<web::WebState> _webState;
}

- (instancetype)initWithParams:(autofill::FormActivityParams)params
                      webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _params = std::move(params);
    _webState = webState;
  }
  return self;
}

- (void)setConsumer:
    (id<PaymentsScanSaveAndFillOfferBottomSheetConsumer>)consumer {
  _consumer = consumer;
}

#pragma mark - Public

- (void)didAcceptScanCardSuggestion {
  // Upload saves and resyncs could take a while on a slow connection, and we
  // do not want to offer save card again after the user accepts the scan card
  // offer due to slow sync.
  [self setCardSubmittedThroughScanSaveAndFill];

  if (_viewDidAppearTimer) {
    base::UmaHistogramTimes("IOS.ScanCardBottomSheet.TimeToSelection",
                            _viewDidAppearTimer->Elapsed());
  }

  if (!_provider) {
    return;
  }

  // Create a form suggestion containing and set the suggestion type as
  // `kSaveAndFillCreditCardEntry` value so that the provider can identify it.
  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:nil
                       minorValue:nil
               displayDescription:nil
                             icon:nil
                             type:autofill::SuggestionType::
                                      kSaveAndFillCreditCardEntry
                          payload:autofill::Suggestion::AutofillProfilePayload(
                                      autofill::Suggestion::Guid(""))
      fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                   requiresReauth:NO
       acceptanceA11yAnnouncement:nil];

  [_provider didSelectSuggestion:suggestion
                         atIndex:0
                          params:_params
                      completion:nil];
}

- (void)didCancelScanCardSuggestion {
  // If the user explicitly cancel the scan card bottom sheet offer, we should
  // not offer save card promo after the user submit the same form.
  [self setCardSubmittedThroughScanSaveAndFill];
}

- (void)disconnect {
}

- (void)setProvider:(id<FormInputSuggestionsProvider>)provider {
  _provider = provider;
}

- (void)scanCardBottomSheetViewDidAppear {
  _viewDidAppearTimer = base::ElapsedTimer();
}

- (void)logExitReason:(ScanCardSuggestionBottomSheetExitReason)exitReason {
  base::UmaHistogramEnumeration("IOS.ScanCardBottomSheet.ExitReason",
                                exitReason);
}

#pragma mark - Private

- (void)setCardSubmittedThroughScanSaveAndFill {
  if (_webState) {
    autofill::AutofillClientIOS* client =
        autofill::AutofillClientIOS::FromWebState(_webState);
    if (client) {
      autofill::FormDataImporter* importer = client->GetFormDataImporter();
      if (importer) {
        importer->GetPaymentsFormDataImporter()
            .fetched_payments_data_context()
            .card_submitted_through_save_and_fill = true;
      }
    }
  }
}

@end
