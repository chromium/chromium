// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_

// Captures exit reasons and dismissal outcomes for the modal passkey suggestion
// bottom sheet.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logged as `PasskeySuggestionBottomSheetExitReason` for the
// `IOS.PasskeySuggestionBottomSheet.ExitReason` histogram.
// LINT.IfChange(PasskeySuggestionBottomSheetExitReason)
enum class PasskeySuggestionBottomSheetExitReason {
  // Bottom sheet was dismissed without selecting a passkey (e.g. swipe down).
  kDismissal = 0,
  // User selected a passkey suggestion.
  kUsePasskey = 1,
  // User tapped the "More Passkeys" secondary button to defer to the OS.
  kMorePasskeys = 2,
  // User accessed Password Manager through the long press menu.
  kShowPasswordManager = 3,
  // User accessed passkey details through the long press menu.
  kShowPasswordDetails = 4,
  // Bottom sheet could not be presented.
  kCouldNotPresent = 5,
  kMaxValue = kCouldNotPresent,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:PasskeySuggestionBottomSheetExitReason)

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_
