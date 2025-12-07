// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/accessibility_utils.h"

namespace {

/// Accessibility identifier prefixes.
NSString* const kImportDataItemTableViewAXidPrefix = @"ImportDataItemTableView";
NSString* const kSafariDataImportPasswordConflictResolutionAXidPrefix =
    @"SafariDataImportPasswordConflictResolution";
NSString* const kSafariDataImportInvalidPasswordsAXidPrefix =
    @"SafariDataImportInvalidPasswords";

}  // namespace

NSString* GetImportDataItemTableViewAccessibilityIdentifier() {
  return [NSString stringWithFormat:@"%@%@", kImportDataItemTableViewAXidPrefix,
                                    @"AccessibilityIdentifier"];
}

NSString* GetImportDataItemTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index) {
  return
      [NSString stringWithFormat:@"%@-%ld", kImportDataItemTableViewAXidPrefix,
                                 cell_index];
}

NSString* GetPasswordConflictResolutionTableViewAccessibilityIdentifier() {
  return [NSString
      stringWithFormat:@"%@%@",
                       kSafariDataImportPasswordConflictResolutionAXidPrefix,
                       @"AccessibilityIdentifier"];
}

/// Returns the accessibility identifier to set on a cell in the table view for
/// password conflict resolution.
NSString* GetPasswordConflictResolutionTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index) {
  return [NSString
      stringWithFormat:@"%@-%ld",
                       kSafariDataImportPasswordConflictResolutionAXidPrefix,
                       cell_index];
}

NSString* GetInvalidPasswordsTableViewAccessibilityIdentifier() {
  return [NSString stringWithFormat:@"%@%@",
                                    kSafariDataImportInvalidPasswordsAXidPrefix,
                                    @"AccessibilityIdentifier"];
}

/// Returns the accessibility identifier to set on a cell in the table view for
/// the list of invalid passwords.
NSString* GetInvalidPasswordsTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index) {
  return [NSString stringWithFormat:@"%@-%ld",
                                    kSafariDataImportInvalidPasswordsAXidPrefix,
                                    cell_index];
}
