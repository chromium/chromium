// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shortcuts/ui/shortcuts_action_item.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_constants.h"

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

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[ShortcutsActionItem class]]) {
    return NO;
  }
  ShortcutsActionItem* other = static_cast<ShortcutsActionItem*>(object);
  return self.collectionShortcutType == other.collectionShortcutType &&
         self.count == other.count && self.disabled == other.disabled &&
         [self.title isEqualToString:other.title];
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(_collectionShortcutType) ^ self.count ^
         self.title.hash;
}

@end
