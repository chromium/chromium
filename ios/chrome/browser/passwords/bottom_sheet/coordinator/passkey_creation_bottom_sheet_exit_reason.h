// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_EXIT_REASON_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_EXIT_REASON_H_

// Captures exit reasons and dismissal outcomes for the passkey creation bottom
// sheet.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logged as PasskeyCreationBottomSheetExitReason for the
// IOS.PasskeyCreationBottomSheet.ExitReason histogram.
// LINT.IfChange(PasskeyCreationBottomSheetExitReason)
enum class PasskeyCreationBottomSheetExitReason {
  // User tapped "Create" and `StartPasskeyCreation()` was invoked (UV not
  // required or reauth succeeded/skipped).
  kCreatePasskey = 0,
  // User tapped "Save another way" (deferred to renderer).
  kSaveAnotherWay = 1,
  // User interactively dismissed the sheet (swipe down / scrim tap).
  kDismissed = 2,
  // User tapped "Create" and biometric/passcode reauth failed or was
  // cancelled.
  kReauthFailed = 3,
  // User tapped "Create", UV was required, and device cannot attempt reauth
  // (deferred to renderer).
  kReauthNotPossible = 4,
  // Sheet failed to present (deferred to renderer).
  kCouldNotPresent = 5,
  // Sheet dismissed without user interaction on the sheet (`web::WebState`
  // change, `AbortSignal`, or browser teardown).
  kDismissedExternally = 6,
  // Request metadata (`rpId` or UV lookup) was missing or expired.
  kInvalidRequest = 7,
  kMaxValue = kInvalidRequest,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:PasskeyCreationBottomSheetExitReason)

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_EXIT_REASON_H_
