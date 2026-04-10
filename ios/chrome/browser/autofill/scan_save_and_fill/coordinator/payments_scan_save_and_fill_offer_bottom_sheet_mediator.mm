// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"

@implementation PaymentsScanSaveAndFillOfferBottomSheetMediator {
  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;

  // The object that provides suggestions while filling forms.
  __weak id<FormInputSuggestionsProvider> _provider;
}

- (instancetype)initWithParams:(autofill::FormActivityParams)params {
  self = [super init];
  if (self) {
    _params = std::move(params);
  }
  return self;
}

- (void)setConsumer:
    (id<PaymentsScanSaveAndFillOfferBottomSheetConsumer>)consumer {
  _consumer = consumer;
}

#pragma mark - Public

- (void)didAcceptScanCardSuggestion {
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

- (void)disconnect {
}

- (void)setProvider:(id<FormInputSuggestionsProvider>)provider {
  _provider = provider;
}

@end
