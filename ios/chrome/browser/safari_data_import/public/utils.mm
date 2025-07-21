// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/utils.h"

namespace {

/// Accessibility identifier prefixes.
NSString* const kSafariDataItemTableViewAXidPrefix =
    @"kSafariDataItemTableView";
NSString* const kSafariDataImportPasswordConflictResolutionAXidPrefix =
    @"SafariDataImportPasswordConflictResolution";

}  // namespace

NSString* GetSafariDataItemTableViewAccessibilityIdentifier() {
  return [NSString stringWithFormat:@"%@%@", kSafariDataItemTableViewAXidPrefix,
                                    @"AccessibilityIdentifier"];
}

NSString* GetSafariDataItemTableViewCellAccessibilityIdentifier(
    NSUInteger cell_index) {
  return
      [NSString stringWithFormat:@"%@-%ld", kSafariDataItemTableViewAXidPrefix,
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
