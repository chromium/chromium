// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"

#include "base/check.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsMostVisitedItem

@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize attributes = _attributes;
@synthesize title = _title;
@synthesize URL = _URL;
@synthesize titleSource = _titleSource;
@synthesize source = _source;
@synthesize commandHandler = _commandHandler;
@synthesize metricsRecorded = _metricsRecorded;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsMostVisitedCell class];
  }
  return self;
}

- (void)configureCell:(MDCCollectionViewCell*)cell {
  [super configureCell:cell];
  if (![cell isKindOfClass:[ContentSuggestionsMostVisitedCell class]]) {
    // Do not attempt to configure cell if it is not the correct class
    // (crbug.com/1276562).
    return;
  }
  ContentSuggestionsMostVisitedCell* mostVisitedCell =
      static_cast<ContentSuggestionsMostVisitedCell*>(cell);
  mostVisitedCell.titleLabel.text = self.title;
  mostVisitedCell.accessibilityLabel = self.title;
  [mostVisitedCell.faviconView configureWithAttributes:self.attributes];
  mostVisitedCell.accessibilityCustomActions = [self customActions];
}

- (CGFloat)cellHeightForWidth:(CGFloat)width {
  return [ContentSuggestionsMostVisitedCell defaultSize].height;
}

#pragma mark - AccessibilityCustomAction

// Custom action for a cell configured with this item.
- (NSArray<UIAccessibilityCustomAction*>*)customActions {
  UIAccessibilityCustomAction* openInNewTab =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                target:self
              selector:@selector(openInNewTab)];
  UIAccessibilityCustomAction* openInNewIncognitoTab =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                target:self
              selector:@selector(openInNewIncognitoTab)];
  UIAccessibilityCustomAction* removeMostVisited = [
      [UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
            target:self
          selector:@selector(removeMostVisited)];

  if (self.incognitoAvailable) {
    return [NSArray arrayWithObjects:openInNewTab, openInNewIncognitoTab,
                                     removeMostVisited, nil];
  } else {
    return [NSArray arrayWithObjects:openInNewTab, removeMostVisited, nil];
  }
}

// Target for custom action.
- (BOOL)openInNewTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:self incognito:NO];
  return YES;
}

// Target for custom action.
- (BOOL)openInNewIncognitoTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:self incognito:YES];
  return YES;
}

// Target for custom action.
- (BOOL)removeMostVisited {
  DCHECK(self.commandHandler);
  [self.commandHandler removeMostVisited:self];
  return YES;
}

@end
