// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_METRICS_H_

enum class SafariDataImportEntryPoint;
enum class SafariDataImportStage;

#pragma mark - Entry Point

// LINT.IfChange(SafariDataImportEntryPointAction)
enum class SafariDataImportEntryPointAction {
  kDismiss = 0,
  kImport = 1,
  kRemindMeLater = 2,
  kMaxValue = kRemindMeLater,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:SafariDataImportEntryPointAction)

// Records a user action on the Safari import entry point.
void RecordSafariImportActionOnEntryPoint(
    SafariDataImportEntryPointAction action,
    SafariDataImportEntryPoint entry_point);

#pragma mark - Export Education

// Available user actions on the export education screen.
// LINT.IfChange(SafariDataImportExportEducationAction)
enum class SafariDataImportExportEducationAction {
  kCancel = 0,
  kGoToSetting = 1,
  kContinue = 2,
  kMaxValue = kContinue,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:SafariDataImportExportEducationAction)

// Records a user action on the Safari import screen for export education.
void RecordActionOnSafariExportEducationScreen(
    SafariDataImportExportEducationAction action);

#pragma mark - Import

// Records requests to display invalid passwords.
void RecordSafariDataImportInvalidPasswordDisplay();

// Records file preparation failures, and whether the failure alert has
// successfully displayed.
void RecordSafariDataImportFailure(bool alert_displayed);

// Records the current stage of import when the user taps the "back" button.
// Only applicable for users who reach the import screen.
void RecordSafariDataImportTapsBackAtImportStage(SafariDataImportStage stage);

// Records the current stage of import when the user exits the workflow. Only
// applicable for users who reach the import screen.
void RecordSafariDataImportEndsAtImportStage(SafariDataImportStage stage);

// Records whether user asks Chrome to delete the imported ZIP file on prompted.
void RecordSafariDataImportFileDeletionDecision(bool delete_file);

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_METRICS_H_
