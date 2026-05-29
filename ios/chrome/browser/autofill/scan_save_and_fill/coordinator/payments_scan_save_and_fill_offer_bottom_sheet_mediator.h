// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
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

namespace web {
class WebState;
}  // namespace web

@protocol PaymentsScanSaveAndFillOfferBottomSheetConsumer;

@interface PaymentsScanSaveAndFillOfferBottomSheetMediator : NSObject
// Designated initializer. `webStateList` must not be nil.
- (instancetype)initWithParams:(autofill::FormActivityParams)params
                      webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PaymentsScanSaveAndFillOfferBottomSheetConsumer>
    consumer;

// Performs all operations that should happen when the scan card suggestion is
// accepted before dismissal.
- (void)didAcceptScanCardSuggestion;

// Returns a block to be executed after the bottom sheet is dismissed.
- (ProceduralBlock)postDismissBlock;

- (void)didCancelScanCardSuggestion;

// Disconnects the mediator.
- (void)disconnect;

// Replaces the object in charge of providing suggestions.
- (void)setProvider:(id<FormInputSuggestionsProvider>)provider;

// Refocuses the field that was blurred to show the bottom sheet, if deemed
// needed.
- (void)refocus;

// Called when the view appeared.
- (void)scanCardBottomSheetViewDidAppear;

// Logs the exit reason for the scan card bottom sheet.
- (void)logExitReason:(ScanCardSuggestionBottomSheetExitReason)exitReason;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_COORDINATOR_PAYMENTS_SCAN_SAVE_AND_FILL_OFFER_BOTTOM_SHEET_MEDIATOR_H_
