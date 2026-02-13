// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation MostVisitedTilesConfig

@synthesize layoutGuideCenter = _layoutGuideCenter;

- (instancetype)initWithLayoutGuideCenter:
    (LayoutGuideCenter*)layoutGuideCenter {
  self = [super init];
  if (self) {
    _layoutGuideCenter = layoutGuideCenter;
  }
  return self;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kMostVisited;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  MostVisitedTilesConfig* copy =
      [[super copyWithZone:zone] initWithLayoutGuideCenter:_layoutGuideCenter];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.mostVisitedItems = self.mostVisitedItems;
  copy.imageDataSource = self.imageDataSource;
  copy.commandHandler = self.commandHandler;
  // LINT.ThenChange(most_visited_tiles_config.h:Copy)
  return copy;
}

@end
