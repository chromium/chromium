// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_observer_bridge.h"

namespace autofill {
AutofillBottomSheetObserverBridge::AutofillBottomSheetObserverBridge(
    id<AutofillBottomSheetObserving> owner,
    AutofillBottomSheetTabHelper* helper)
    : owner_(owner) {
  scoped_observation_.Observe(helper);
}

AutofillBottomSheetObserverBridge::~AutofillBottomSheetObserverBridge() =
    default;

void AutofillBottomSheetObserverBridge::WillShowPaymentsBottomSheet(
    const FormActivityParams& params) {
  [owner_ willShowPaymentsBottomSheetWithParams:params];
}
}  // namespace autofill
