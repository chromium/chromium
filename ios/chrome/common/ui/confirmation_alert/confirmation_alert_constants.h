// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_CONSTANTS_H_
#define IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_CONSTANTS_H_

// Enum actions for the IOS.ConfirmationAlertSheet.Outcome histogram.
// LINT.IfChange(ConfirmationAlertSheetAction)
enum class ConfirmationAlertSheetAction {
  kPrimaryButtonTapped = 1,
  kSecondaryButtonTapped = 2,
  kTertiaryButtonTapped = 3,
  kDismissButtonTapped = 4,
  kMaxValue = kDismissButtonTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ConfirmationAlertSheetAction)

#endif  // IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_CONSTANTS_H_
