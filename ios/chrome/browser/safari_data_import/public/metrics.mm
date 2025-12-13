// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"

void RecordSafariImportActionOnEntryPoint(
    SafariDataImportEntryPointAction action,
    SafariDataImportEntryPoint entry_point) {
  std::string prefix = "IOS.SafariImport.EntryPoint";
  std::string suffix = ".Action";
  std::string entry_point_str;
  switch (entry_point) {
    case SafariDataImportEntryPoint::kFirstRun:
      entry_point_str = "FRE";
      break;
    case SafariDataImportEntryPoint::kReminder:
      entry_point_str = "Reminder";
      break;
    case SafariDataImportEntryPoint::kSetting:
      entry_point_str = "Setting";
      break;
  }
  base::UmaHistogramEnumeration(prefix + entry_point_str + suffix, action);
}

void RecordActionOnSafariExportEducationScreen(
    SafariDataImportExportEducationAction action) {
  base::UmaHistogramEnumeration("IOS.SafariImport.ExportEducationAction",
                                action);
}

void RecordSafariDataImportTapsBackAtImportStage(SafariDataImportStage stage) {
  base::UmaHistogramEnumeration("IOS.SafariImport.Import.TapBackOnStage",
                                stage);
}

void RecordSafariDataImportEndsAtImportStage(SafariDataImportStage stage) {
  base::UmaHistogramEnumeration("IOS.SafariImport.Import.ExitOnStage", stage);
}

void RecordSafariDataImportFailure(bool alert_displayed) {
  base::UmaHistogramBoolean("IOS.SafariImport.Import.FailureAlert",
                            alert_displayed);
}

void RecordSafariDataImportInvalidPasswordDisplay() {
  base::UmaHistogramBoolean("IOS.SafariImport.Import.InvalidPasswords.Display",
                            true);
}

void RecordSafariDataImportFileDeletionDecision(bool delete_file) {
  base::UmaHistogramBoolean("IOS.SafariImport.Import.DeleteFile", delete_file);
}
