// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/accessibility_utils.h"

namespace {

/// Accessibility identifier prefix.
NSString* const kImportDataItemTableViewAXidPrefix = @"ImportDataItemTableView";

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
