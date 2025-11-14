// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_METRICS_H_

// Available user actions on password conflicts screen.
// LINT.IfChange(SafariDataImportPasswordConflictScreenAction)
enum class SafariDataImportPasswordConflictScreenAction {
  kCancel = 0,
  kDeselectAll = 1,
  kSelectAll = 2,
  kContinue = 3,
  kMaxValue = kContinue,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:SafariDataImportPasswordConflictScreenAction)

// Records `action` in conflict resolution screen.
void RecordSafariDataImportDismissPasswordConflictScreen(
    SafariDataImportPasswordConflictScreenAction action);

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_METRICS_H_
