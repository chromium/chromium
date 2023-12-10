// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_observer.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"

@protocol AutofillBottomSheetObserving <NSObject>
@optional
// Invoked by AutofillBottomSheetObserverBridge::WillShowPaymentsBottomSheet.
- (void)willShowPaymentsBottomSheetWithParams:
    (const autofill::FormActivityParams&)params;

@end

namespace autofill {

// Use this class to be notified of the autofill bottom sheet activity in an
// Objective-C class. Implement the AutofillBottomSheetObserving protocol and
// create a AutofillBottomSheetObserverBridge passing self and
// AutofillBottomSheetTabHelper.
class AutofillBottomSheetObserverBridge : public AutofillBottomSheetObserver {
 public:
  // `owner` will not be retained.
  AutofillBottomSheetObserverBridge(id<AutofillBottomSheetObserving> owner,
                                    AutofillBottomSheetTabHelper* helper);
  ~AutofillBottomSheetObserverBridge() override;

  // AutofillBottomSheetObserver overrides:
  void WillShowPaymentsBottomSheet(const FormActivityParams& params) override;

 private:
  __weak id<AutofillBottomSheetObserving> owner_ = nil;
  base::ScopedObservation<AutofillBottomSheetTabHelper,
                          AutofillBottomSheetObserver>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_OBSERVER_BRIDGE_H_
