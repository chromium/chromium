// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedActionItem
@synthesize metricsRecorded;
@synthesize suggestionIdentifier;

- (instancetype)initWithCollectionShortcutType:(NTPCollectionShortcutType)type {
  self = [super initWithType:0];
  if (self) {
    _collectionShortcutType = type;
    self.cellClass = [ContentSuggestionsMostVisitedActionCell class];
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

#pragma mark - AccessibilityCustomAction

- (void)configureCell:(ContentSuggestionsMostVisitedActionCell*)cell {
  [super configureCell:cell];
  cell.accessibilityCustomActions = nil;
  cell.titleLabel.text = self.title;
  cell.accessibilityLabel =
      self.accessibilityLabel.length ? self.accessibilityLabel : self.title;
  if (@available(iOS 13, *)) {
    // The accessibilityUserInputLabel should just be the title, with nothing
    // extra from the accessibilityLabel.
    cell.accessibilityUserInputLabels = @[ self.title ];
  }
  cell.iconView.image = ImageForCollectionShortcutType(_collectionShortcutType);
  if (self.count != 0) {
    cell.countLabel.text = [@(self.count) stringValue];
    cell.countContainer.hidden = NO;
  } else {
    cell.countContainer.hidden = YES;
  }
}

#pragma mark - ContentSuggestionsItem

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [ContentSuggestionsMostVisitedActionCell defaultSize].height;
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
