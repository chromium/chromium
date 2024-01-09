// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_contents_factory.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/most_visited_tiles_config.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "url/gurl.h"

@implementation MagicStackModuleContentsFactory

- (UIView*)contentViewForConfig:(MagicStackModule*)config
                traitCollection:(UITraitCollection*)traitCollection {
  switch (config.type) {
    case ContentSuggestionsModuleType::kMostVisited: {
      MostVisitedTilesConfig* mvtConfig =
          static_cast<MostVisitedTilesConfig*>(config);
      return [self
          mostVisitedTilesStackViewForConfig:mvtConfig
                                 tileSpacing:
                                     ContentSuggestionsTilesHorizontalSpacing(
                                         traitCollection)];
    }
    default:
      NOTREACHED_NORETURN();
  }
}

#pragma mark - Private

// Returns the Most Visited Tile content view configured with `config` and
// `spacing` between the tiles.
- (UIView*)mostVisitedTilesStackViewForConfig:(MostVisitedTilesConfig*)config
                                  tileSpacing:(CGFloat)spacing {
  UIStackView* mostVisitedStackView = [[UIStackView alloc] init];
  mostVisitedStackView.axis = UILayoutConstraintAxisHorizontal;
  mostVisitedStackView.distribution = UIStackViewDistributionFillEqually;
  mostVisitedStackView.spacing = spacing;
  mostVisitedStackView.alignment = UIStackViewAlignmentTop;

  NSInteger index = 0;
  for (ContentSuggestionsMostVisitedItem* item in config.mostVisitedItems) {
    ContentSuggestionsMostVisitedTileView* view =
        [[ContentSuggestionsMostVisitedTileView alloc]
            initWithConfiguration:item];
    view.menuProvider = item.menuProvider;
    view.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li",
            kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix, index];

    __weak ContentSuggestionsMostVisitedItem* weakItem = item;
    __weak ContentSuggestionsMostVisitedTileView* weakView = view;
    void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
      ContentSuggestionsMostVisitedTileView* strongView = weakView;
      ContentSuggestionsMostVisitedItem* strongItem = weakItem;
      if (!strongView || !strongItem) {
        return;
      }

      strongItem.attributes = attributes;
      [strongView.faviconView configureWithAttributes:attributes];
    };
    [config.imageDataSource fetchFaviconForURL:item.URL completion:completion];
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:config.commandHandler
                action:@selector(mostVisitedTileTapped:)];
    view.tapRecognizer = tapRecognizer;
    [view addGestureRecognizer:tapRecognizer];
    tapRecognizer.enabled = YES;
    [mostVisitedStackView addArrangedSubview:view];
    index++;
  }

  return mostVisitedStackView;
}

@end
