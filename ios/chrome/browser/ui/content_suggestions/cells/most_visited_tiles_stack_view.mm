// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_stack_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_stack_view_consumer_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "url/gurl.h"

@implementation MostVisitedTilesStackView {
  NSLayoutConstraint* _heightConstraint;
  BOOL _inMagicStack;
  id<MagicStackModuleContentViewDelegate> _contentViewDelegate;
}

- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config
           contentViewDelegate:
               (id<MagicStackModuleContentViewDelegate>)contentViewDelegate
                       spacing:(CGFloat)spacing {
  if ((self = [super init])) {
    if (config.inMagicStack) {
      _inMagicStack = YES;
      [config.consumerSource addConsumer:self];

      _contentViewDelegate = contentViewDelegate;
      _heightConstraint = [self.heightAnchor
          constraintLessThanOrEqualToConstant:GetMagicStackHeight(self) - 10];
      _heightConstraint.active = YES;

      if (@available(iOS 17, *)) {
        NSArray<UITrait>* traits = TraitCollectionSetForTraits(
            @[ UITraitPreferredContentSizeCategory.class ]);
        [self registerForTraitChanges:traits
                           withAction:@selector(updateHeight)];
      }
    } else {
      _inMagicStack = NO;
    }
    self.axis = UILayoutConstraintAxisHorizontal;
    self.distribution = UIStackViewDistributionFillEqually;
    self.spacing = spacing;
    self.alignment = UIStackViewAlignmentTop;
    [self populateStackViewWithTiles:config];
  }
  return self;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
          self.traitCollection.preferredContentSizeCategory &&
      _inMagicStack) {
    [self updateHeight];
  }
}
#endif

#pragma mark - MostVisitedTilesStackViewConsumer

- (void)updateWithConfig:(MostVisitedTilesConfig*)config {
  for (UIView* subview in self.arrangedSubviews) {
    [subview removeFromSuperview];
  }
  [self populateStackViewWithTiles:config];
}

#pragma mark - Private

- (void)populateStackViewWithTiles:(MostVisitedTilesConfig*)config {
  NSInteger index = 0;
  for (ContentSuggestionsMostVisitedItem* item in config.mostVisitedItems) {
    ContentSuggestionsMostVisitedTileView* view =
        [[ContentSuggestionsMostVisitedTileView alloc]
             initInMagicStack:config.inMagicStack
            withConfiguration:item];
    view.menuElementsProvider = item.menuElementsProvider;
    view.magicStackModuleDelegate = _contentViewDelegate;
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
    [self addArrangedSubview:view];
    index++;
  }
}

// Resizes the Most Visted card height.
- (void)updateHeight {
  _heightConstraint.constant = GetMagicStackHeight(self) - 10;
}

@end
