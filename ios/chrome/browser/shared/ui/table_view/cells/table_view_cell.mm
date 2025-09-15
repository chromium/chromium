// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@implementation TableViewCell {
  NSString* _accessibilityLabel;
}

- (void)setAccessibilityLabel:(NSString*)accessibilityLabel {
  _accessibilityLabel = accessibilityLabel;
}

- (NSString*)accessibilityLabel {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityLabel) {
    return contentConfiguration.accessibilityLabel;
  }
  return _accessibilityLabel;
}

@end
