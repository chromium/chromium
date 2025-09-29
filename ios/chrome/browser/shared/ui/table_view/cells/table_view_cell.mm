// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@implementation TableViewCell {
  NSString* _accessibilityLabel;
  NSArray<NSString*>* _accessibilityUserInputLabels;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessoryType = UITableViewCellAccessoryNone;
  self.accessibilityLabel = nil;
  self.accessibilityUserInputLabels = nil;
}

#pragma mark - Accessibility

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

- (void)setAccessibilityUserInputLabels:
    (NSArray<NSString*>*)accessibilityUserInputLabels {
  _accessibilityUserInputLabels = accessibilityUserInputLabels;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityUserInputLabels) {
    return contentConfiguration.accessibilityUserInputLabels;
  }
  return _accessibilityUserInputLabels;
}

@end
