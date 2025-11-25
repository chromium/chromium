// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_plus_button_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_action_tile_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_constants.h"

/// The cell content view of the add pinned site button. It is subclassed from
/// the most-visited-action button so it shares the same color theme with the
/// shortcuts tiles.
@interface ContentSuggestionsPlusButtonTileView
    : ContentSuggestionsMostVisitedActionTileView <UIContentView>

@end

@implementation ContentSuggestionsPlusButtonTileView

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedActionItem*)config {
  self = [super initWithConfiguration:config];
  if (self) {
    self.imageBackgroundView.layer.cornerRadius =
        kMagicStackImageContainerWidth / 2;
    self.imageBackgroundView.clipsToBounds = YES;
  }
  return self;
}

- (id<UIContentConfiguration>)configuration {
  return base::apple::ObjCCastStrict<ContentSuggestionsPlusButtonItem>(
      self.config);
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  if ([configuration isKindOfClass:ContentSuggestionsPlusButtonItem.class]) {
    ContentSuggestionsPlusButtonItem* item =
        base::apple::ObjCCastStrict<ContentSuggestionsPlusButtonItem>(
            configuration);
    [self updateConfiguration:[item copy]];
  }
}

@end

@implementation ContentSuggestionsPlusButtonItem

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
  return [[ContentSuggestionsPlusButtonItem alloc] init];
}

@end
