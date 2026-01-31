// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_stack_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_utils.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tile_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "url/gurl.h"

@implementation MostVisitedTilesStackView

- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config
                       spacing:(CGFloat)spacing {
  if ((self = [super init])) {
    self.axis = UILayoutConstraintAxisHorizontal;
    self.distribution = UIStackViewDistributionFillEqually;
    self.spacing = spacing;
    self.alignment = UIStackViewAlignmentTop;
    [self populateStackViewWithTiles:config];
  }
  return self;
}

#pragma mark - Private

- (void)populateStackViewWithTiles:(MostVisitedTilesConfig*)config {
  NSInteger index = 0;
  for (MostVisitedItem* item in config.mostVisitedItems) {
    MostVisitedTileView* view =
        base::apple::ObjCCastStrict<MostVisitedTileView>(
            [item makeContentView]);
    view.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li",
            kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix, index];

    __weak MostVisitedItem* weakItem = item;
    __weak MostVisitedTileView* weakView = view;
    void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
      MostVisitedTileView* strongView = weakView;
      MostVisitedItem* strongItem = weakItem;
      if (!strongView || !strongItem) {
        return;
      }
      strongItem.attributes = attributes;
      strongView.configuration = strongItem;
    };
    [config.imageDataSource fetchFaviconForURL:item.URL completion:completion];
    [self addArrangedSubview:view];
    index++;
  }
}

@end
