// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shortcuts/ui/shortcuts_action_item.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_constants.h"

@implementation ShortcutsActionItem

- (instancetype)initWithCollectionShortcutType:(NTPCollectionShortcutType)type {
  self = [super init];
  if (self) {
    _collectionShortcutType = type;
    self.title = TitleForCollectionShortcutType(_collectionShortcutType);
    self.icon = SymbolForCollectionShortcutType(_collectionShortcutType);
  }
  return self;
}

- (NSString*)accessibilityLabel {
  if (_collectionShortcutType == NTPCollectionShortcutTypeReadingList &&
      self.count > 0) {
    NSString* accessibilityLabel = [NSString
        stringWithFormat:@"%@, %@", self.title,
                         AccessibilityLabelForReadingListCellWithCount(
                             self.count)];
    CHECK(accessibilityLabel.length);
    return accessibilityLabel;
  }
  return [super accessibilityLabel];
}

@end
