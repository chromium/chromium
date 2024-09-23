// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Must be in sync with PaymentsSuggestionBottomSheetExitReason enum in
// tools/metrics/histograms/metadata/ios/enums.xml.
// LINT.IfChange
enum class PaymentsSuggestionBottomSheetExitReason {
  kDismissal = 0,
  kUsePaymentsSuggestion = 1,
  kShowPaymentMethods = 2,
  kShowPaymentDetails = 3,
  kBadProvider = 4,
  // Could not present the view controller for the bottom sheet as a modal for
  // other reasons.
  kCouldNotPresent = 5,
  kMaxValue = kCouldNotPresent,
};
// LINT.ThenChange(tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_PAYMENTS_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_
