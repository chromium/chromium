// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Must be in sync with PasswordSuggestionBottomSheetExitReason enum in
// tools/metrics/histograms/enums.xml.
enum class PasswordSuggestionBottomSheetExitReason {
  kDismissal = 0,
  kUsePasswordSuggestion = 1,
  kShowPasswordManager = 2,
  kShowPasswordDetails = 3,
  kBadProvider = 4,
  // Could not present the view controller for the bottom sheet as a modal for
  // other reasons.
  kCouldNotPresent = 5,
  kMaxValue = kCouldNotPresent,
};

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_EXIT_REASON_H_
