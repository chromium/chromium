// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_plus_button_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_action_tile_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_tile_constants.h"

/// The cell content view of the add pinned site button. It is subclassed from
/// the most-visited-action button so it shares the same color theme with the
/// shortcuts tiles.
@interface ContentSuggestionsPlusButtonTileView
    : ContentSuggestionsActionTileView <UIContentView>

@end

@implementation ContentSuggestionsPlusButtonTileView

- (instancetype)initWithConfiguration:(ContentSuggestionsActionItem*)config {
  self = [super initWithConfiguration:config];
  if (self) {
    self.imageBackgroundView.layer.cornerRadius =
        kMagicStackImageContainerWidth / 2;
    self.imageBackgroundView.clipsToBounds = YES;
    [self addGestureRecognizer:[[UITapGestureRecognizer alloc]
                                   initWithTarget:self
                                           action:@selector(handleTap)]];
  }
  return self;
}

- (id<UIContentConfiguration>)configuration {
  return base::apple::ObjCCastStrict<MostVisitedTilesPlusButtonItem>(
      self.config);
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  if ([configuration isKindOfClass:MostVisitedTilesPlusButtonItem.class]) {
    MostVisitedTilesPlusButtonItem* item =
        base::apple::ObjCCastStrict<MostVisitedTilesPlusButtonItem>(
            configuration);
    [self updateConfiguration:[item copy]];
  }
}

/// Handles user taps on the plus button.
- (void)handleTap {
  MostVisitedTilesPlusButtonItem* configuration =
      base::apple::ObjCCastStrict<MostVisitedTilesPlusButtonItem>(self.config);
  [configuration.mostVisitedTilesHandler openModalToAddPinnedSite];
}

@end

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
      [[ContentSuggestionsPlusButtonTileView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  /// Plus button in most visited tile looks the same across different states.
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  MostVisitedTilesPlusButtonItem* item =
      [[MostVisitedTilesPlusButtonItem alloc] init];
  item.mostVisitedTilesHandler = self.mostVisitedTilesHandler;
  return item;
}

@end
