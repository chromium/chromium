// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_plus_button_tile_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_plus_button_item.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation MostVisitedTilesPlusButtonTileView

- (instancetype)initWithConfiguration:(ContentSuggestionsActionItem*)config {
  self = [super initWithConfiguration:config];
  if (self) {
    self.imageBackgroundView.clipsToBounds = YES;
    if (IsNewTabPageUICleanupEnabled()) {
      self.titleLabel.numberOfLines = 1;
      self.imageBackgroundView.layer.cornerRadius =
          kMostVisitedTileImageContainerSquareCornerRadius;
      [self setImageBackgroundSize:kMagicStackImageContainerWidth];
    } else {
      self.imageBackgroundView.layer.cornerRadius =
          kMagicStackImageContainerWidth / 2;
    }
    [self addGestureRecognizer:[[UITapGestureRecognizer alloc]
                                   initWithTarget:self
                                           action:@selector(handleTap)]];
  }
  return self;
}

#pragma mark - ContentSuggestionsActionTileView

- (void)applyBackgroundTheme {
  if (!IsNewTabPageUICleanupEnabled()) {
    [super applyBackgroundTheme];
    return;
  }

  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];

  if (colorPalette) {
    self.imageBackgroundView.tintColor = colorPalette.primaryColor;
    self.iconView.tintColor = colorPalette.monogramColor;
  } else {
    self.imageBackgroundView.tintColor =
        [UIColor colorNamed:kNewTabPageBackgroundColor];
    self.iconView.tintColor = [UIColor colorNamed:kGrey700Color];
  }
}

#pragma mark - UIContentConfiguration

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

#pragma mark - Private

/// Handles user taps on the plus button.
- (void)handleTap {
  MostVisitedTilesPlusButtonItem* configuration =
      base::apple::ObjCCastStrict<MostVisitedTilesPlusButtonItem>(self.config);
  [configuration.mostVisitedTilesHandler openModalToAddPinnedSite];
}

@end
