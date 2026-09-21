// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"

#import "base/check.h"
#import "components/ntp_tiles/constants.h"
#import "components/ntp_tiles/features.h"
#import "components/ntp_tiles/tile_source.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tile_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "url/gurl.h"

@implementation MostVisitedItem

- (BOOL)isPinned {
  return self.source == ntp_tiles::TileSource::CUSTOM_LINKS;
}

#pragma mark - Public

- (BOOL)isAIMTile {
  if (ntp_tiles::GetAimButtonRefactorArm() !=
          ntp_tiles::AimButtonRefactorArm::kAimAsMvt ||
      !IsAimEnabledInNtp()) {
    // No AIM tile exists among the MVTs.
    return NO;
  }
  return self.URL == GURL(ntp_tiles::kAiModeTileUrl);
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return [[MostVisitedTileView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  /// Most visited tile looks the same across different states.
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  MostVisitedItem* newCopy = [[MostVisitedItem alloc] init];
  newCopy.title = self.title;
  newCopy.URL = self.URL;
  newCopy.source = self.source;
  newCopy.titleSource = self.titleSource;
  newCopy.attributes = self.attributes;
  newCopy.commandHandler = self.commandHandler;
  newCopy.incognitoAvailable = self.incognitoAvailable;
  newCopy.index = self.index;
  newCopy.actionsProvider = self.actionsProvider;
  return newCopy;
}

@end
