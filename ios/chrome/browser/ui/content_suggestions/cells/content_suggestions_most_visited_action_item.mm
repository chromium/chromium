// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/icons/symbols.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedActionItem

- (instancetype)initWithCollectionShortcutType:(NTPCollectionShortcutType)type {
  self = [super init];
  if (self) {
    _collectionShortcutType = type;
    switch (_collectionShortcutType) {
      case NTPCollectionShortcutTypeBookmark:
        _index = NTPCollectionShortcutTypeBookmark;
        break;
      case NTPCollectionShortcutTypeReadingList:
        _index = NTPCollectionShortcutTypeReadingList;
        break;
      case NTPCollectionShortcutTypeRecentTabs:
        _index = NTPCollectionShortcutTypeRecentTabs;
        break;
      case NTPCollectionShortcutTypeHistory:
        _index = NTPCollectionShortcutTypeHistory;
        break;
      case NTPCollectionShortcutTypeWhatsNew:
        _index = NTPCollectionShortcutTypeWhatsNew;
        break;
      default:
        break;
    }
    self.title = TitleForCollectionShortcutType(_collectionShortcutType);
  }
  return self;
}

#pragma mark - Accessors

- (void)setTitle:(NSString*)title {
  if ([_title isEqualToString:title])
    return;
  _title = title;
  [self updateAccessibilityLabel];
}

- (void)setCount:(NSInteger)count {
  if (_count == count)
    return;
  _count = count;
  [self updateAccessibilityLabel];
}

#pragma mark - Private

// Updates self.accessibilityLabel based on the current property values.
- (void)updateAccessibilityLabel {
  // Resetting self.accessibilityLabel to nil will prompt self.title to be used
  // as the default label.  This default value should be used if:
  // - the cell is not for Reading List,
  // - there are no unread articles in the reading list.
  if (self.collectionShortcutType != NTPCollectionShortcutTypeReadingList ||
      self.count <= 0) {
    self.accessibilityLabel = nil;
    return;
  }

  self.accessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", self.title,
                                 AccessibilityLabelForReadingListCellWithCount(
                                     self.count)];
  DCHECK(self.accessibilityLabel.length);
}

@end
