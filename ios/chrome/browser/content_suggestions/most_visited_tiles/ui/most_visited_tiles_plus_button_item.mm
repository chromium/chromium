// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_plus_button_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_plus_button_tile_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_action_tile_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_constants.h"

@implementation MostVisitedTilesPlusButtonItem

- (instancetype)init {
  self = [super init];
  if (self) {
    self.title = TitleForMostVisitedTilePlusButton();
    self.icon = SymbolForMostVisitedTilePlusButton();
  }
  return self;
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return
      [[MostVisitedTilesPlusButtonTileView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  /// Plus button in most visited tile looks the same across different states.
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  MostVisitedTilesPlusButtonItem* item =
      [[MostVisitedTilesPlusButtonItem alloc] init];
  item.mostVisitedTilesHandler = self.mostVisitedTilesHandler;
  return item;
}

@end
