// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_collection_view.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_utils.h"
#import "url/gurl.h"

namespace {

/// Section identifier for the only section in`MostVisitedTilesCollectionView`.
NSString* const kSectionIdentifier =
    @"MostVisitedTilesCollectionViewSectionIdentifier";

/// Cell identifier in the `MostVisitedTilesCollectionView`.
NSString* const kCellReuseIdentifier =
    @"MostVisitedTilesCollectionViewCellIdentifier";

/// Maximum number of items that should be added to the collection view..
NSUInteger const kMaximumItems = 8;

/// Maximum number of items that should be fully visible on the screen.
const NSUInteger kMaximumVisibleItemsOnScreen = 4;

/// Item identifier for the plus button.
NSNumber* const kPlusButtonIdentifier = @(-1);

/// Creates a section in the collection view layout.
NSCollectionLayoutSection* GetSectionForMostVisitedTilesCollectionView(
    NSUInteger item_count,
    CGFloat container_width,
    UITraitCollection* trait_collection) {
  CGFloat items_per_group = MIN(item_count, kMaximumVisibleItemsOnScreen);
  NSCollectionLayoutDimension* estimated_height_dimension =
      [NSCollectionLayoutDimension estimatedDimension:kIconSize];

  NSCollectionLayoutDimension* item_width_dimension =
      [NSCollectionLayoutDimension
          fractionalWidthDimension:1 / items_per_group];
  NSCollectionLayoutItem* item = [NSCollectionLayoutItem
      itemWithLayoutSize:
          [NSCollectionLayoutSize
              sizeWithWidthDimension:item_width_dimension
                     heightDimension:estimated_height_dimension]];
  /// Group configuration.
  CGFloat group_width = container_width;
  if (item_count > kMaximumVisibleItemsOnScreen) {
    group_width -=
        ModuleNarrowerWidthToAllowPeekingForTraitCollection(trait_collection);
  }
  NSCollectionLayoutDimension* group_width_dimension =
      [NSCollectionLayoutDimension absoluteDimension:group_width];
  NSCollectionLayoutGroup* group = [NSCollectionLayoutGroup
      horizontalGroupWithLayoutSize:
          [NSCollectionLayoutSize
              sizeWithWidthDimension:group_width_dimension
                     heightDimension:estimated_height_dimension]
                   repeatingSubitem:item
                              count:items_per_group];
  CGFloat spacing = ContentSuggestionsTilesHorizontalSpacing(trait_collection);
  group.interItemSpacing = [NSCollectionLayoutSpacing fixedSpacing:spacing];
  /// Section configuration.
  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = spacing;
  return section;
}

/// Create the collection view layout.
UICollectionViewCompositionalLayout* GetLayoutForMostVisitedTilesCollectionView(
    NSUInteger item_count) {
  UICollectionViewCompositionalLayoutConfiguration* config =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  [config setScrollDirection:UICollectionViewScrollDirectionHorizontal];
  UICollectionViewCompositionalLayoutSectionProvider section_provider =
      ^NSCollectionLayoutSection*(
          NSInteger section_index,
          id<NSCollectionLayoutEnvironment> layout_environment) {
        return GetSectionForMostVisitedTilesCollectionView(
            item_count, layout_environment.container.contentSize.width,
            layout_environment.traitCollection);
      };
  return [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:section_provider
                configuration:config];
}

}  // namespace

@implementation MostVisitedTilesCollectionView {
  /// Current most visited tiles items being displayed.
  NSArray<ContentSuggestionsMostVisitedItem*>* _items;
  /// Data source for favicons of each site.
  id<ContentSuggestionsImageDataSource> _imageDataSource;
  /// Data source object powering the display of the collection view.
  UICollectionViewDiffableDataSource* _diffableDataSource;
}

- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config {
  self = [super initWithFrame:CGRectZero
         collectionViewLayout:GetLayoutForMostVisitedTilesCollectionView(
                                  /*item_count=*/config.mostVisitedItems.count +
                                  1)];
  if (self) {
    _items = config.mostVisitedItems;
    _imageDataSource = config.imageDataSource;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundColor = UIColor.clearColor;
    self.showsHorizontalScrollIndicator = NO;
    [self registerClass:[UICollectionViewCell class]
        forCellWithReuseIdentifier:kCellReuseIdentifier];
    [self initializeDataSource];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (!CGSizeEqualToSize(self.bounds.size, self.intrinsicContentSize)) {
    [self invalidateIntrinsicContentSize];
  }
}

- (CGSize)intrinsicContentSize {
  /// Until content in the collection view is loaded, returns an estimate of the
  /// initial size so its superclass (a UIStackView) would allocate space to
  /// perform the first layout pass.
  return CGSizeMake(UIViewNoIntrinsicMetric,
                    MAX(self.contentSize.height, kIconSize));
}

#pragma mark - Private

/// Initializes and displays the data source.
- (void)initializeDataSource {
  __weak __typeof(self) weakSelf = self;
  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:self
                cellProvider:^(UICollectionView* collectionView,
                               NSIndexPath* indexPath, NSNumber* identifier) {
                  CHECK_EQ(collectionView, weakSelf);
                  return [weakSelf cellForIndexPath:indexPath
                                     itemIdentifier:identifier];
                }];
  self.dataSource = _diffableDataSource;
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kSectionIdentifier ]];
  NSMutableArray* indices = [NSMutableArray array];
  NSUInteger count = _items.count;
  for (NSUInteger i = 0; i < count; i++) {
    [indices addObject:@(i)];
  }
  /// Add the "+" button.
  if (count <= kMaximumItems) {
    [indices addObject:kPlusButtonIdentifier];
  }
  [snapshot appendItemsWithIdentifiers:indices
             intoSectionWithIdentifier:kSectionIdentifier];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

/// Returns the cell with the properties of the `item` displayed.
- (UICollectionViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                           itemIdentifier:(NSNumber*)identifier {
  UICollectionViewCell* cell =
      [self dequeueReusableCellWithReuseIdentifier:kCellReuseIdentifier
                                      forIndexPath:indexPath];
  cell.accessibilityIdentifier = [NSString
      stringWithFormat:
          @"%@%li", kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
          identifier.longValue];
  if (identifier == kPlusButtonIdentifier) {
    /// TODO(crbug.com/462459633): The following code is used for place-holding
    /// purpose. Use the correct plus button UI.
    UILabel* plusButton =
        [[UILabel alloc] initWithFrame:CGRectMake(0, 0, kIconSize, kIconSize)];
    plusButton.text = @"+";
    plusButton.font = [UIFont systemFontOfSize:70];
    [cell.contentView addSubview:plusButton];
  } else {
    [self loadFaviconIfNeeded:identifier];
    cell.contentConfiguration = _items[identifier.unsignedIntValue];
  }
  return cell;
}

/// Loads the favicon for item with `identifier`.
- (void)loadFaviconIfNeeded:(NSNumber*)identifier {
  ContentSuggestionsMostVisitedItem* item = _items[identifier.unsignedIntValue];
  if (!item.attributes) {
    __weak MostVisitedTilesCollectionView* weakSelf = self;
    void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
      [weakSelf updateFaviconWithAttributes:attributes
                      forItemWithIdentifier:identifier];
    };
    [_imageDataSource fetchFaviconForURL:item.URL completion:completion];
  }
}

/// Updates the favicon for item.
- (void)updateFaviconWithAttributes:(FaviconAttributes*)attributes
              forItemWithIdentifier:(NSNumber*)identifier {
  _items[identifier.longValue].attributes = attributes;
  NSDiffableDataSourceSnapshot* snapshot = [_diffableDataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:@[ identifier ]];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
