// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_METRICS_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_METRICS_UTILS_H_

namespace password_manager {

// Represent the actions that the use can take on the delete all data
// confirmation dialog. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused.
//
// LINT.IfChange
enum class IOSDeleteAllSavedCredentialsActions {
  kDeleteAllSavedCredentialsConfirmed = 0,
  kDeleteAllSavedCredentialsCancelled = 1,
  kMaxValue = kDeleteAllSavedCredentialsCancelled,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_METRICS_UTILS_H_
