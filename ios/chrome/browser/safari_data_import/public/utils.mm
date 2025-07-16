// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/utils.h"

namespace {
/// Accessibility identifier the table.
NSString* const kSafariDataItemTableViewAXidPrefix =
    @"kSafariDataItemTableView";
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
