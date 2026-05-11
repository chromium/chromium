// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

class WebStateList;

// LINT.IfChange(ScanCardSuggestionBottomSheetExitReason)
enum class ScanCardSuggestionBottomSheetExitReason {
  kIgnore = 0,
  kAcceptSuggestion = 1,
  kRejectSuggestion = 2,
  // Could not present the view controller for the bottom sheet as a modal for
  // other reasons.
  kCouldNotPresent = 3,
  kMaxValue = kCouldNotPresent,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ScanCardSuggestionBottomSheetExitReason)

@protocol PaymentsScanSaveAndFillOfferBottomSheetConsumer;

@interface PaymentsScanSaveAndFillOfferBottomSheetMediator : NSObject
// Designated initializer. `webStateList` must not be nil.
- (instancetype)initWithParams:(autofill::FormActivityParams)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PaymentsScanSaveAndFillOfferBottomSheetConsumer>
    consumer;

- (void)didAcceptScanCardSuggestion;

// Disconnects the mediator.
- (void)disconnect;

// Replaces the object in charge of providing suggestions.
- (void)setProvider:(id<FormInputSuggestionsProvider>)provider;

// Called when the view appeared.
- (void)scanCardBottomSheetViewDidAppear;

// Logs the exit reason for the scan card bottom sheet.
- (void)logExitReason:(ScanCardSuggestionBottomSheetExitReason)exitReason;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_
