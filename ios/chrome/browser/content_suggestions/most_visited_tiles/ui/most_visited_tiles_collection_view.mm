// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_utils.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view_layout.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_plus_button_item.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "url/gurl.h"

namespace {

/// Section identifier for the only section
/// in`MostVisitedTilesCollectionView`.
NSString* const kSectionIdentifier =
    @"MostVisitedTilesCollectionViewSectionIdentifier";

/// Cell identifier in the `MostVisitedTilesCollectionView`.
NSString* const kCellReuseIdentifier =
    @"MostVisitedTilesCollectionViewCellIdentifier";

/// Maximum number of items that should be added to the collection view..
NSUInteger const kMaximumItems = 8;

/// Item identifier for the plus button.
const int kPlusButtonIdentifier = -1;

}  // namespace

@interface MostVisitedTilesCollectionView () <UICollectionViewDragDelegate,
                                              UICollectionViewDropDelegate>
@end

@implementation MostVisitedTilesCollectionView {
  /// Current most visited tiles items being displayed.
  NSArray<MostVisitedItem*>* _items;
  /// Data source for favicons of each site.
  id<ContentSuggestionsImageDataSource> _imageDataSource;
  /// The layout guide center for the first cell in the collection.
  LayoutGuideCenter* _layoutGuideCenter;
  /// Command handler for each tile.
  id<MostVisitedTilesCommands> _mostVisitedTilesHandler;
  /// Data source object powering the display of the collection view.
  UICollectionViewDiffableDataSource* _diffableDataSource;
}

- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config {
  MostVisitedTilesCollectionViewLayout* layout =
      [[MostVisitedTilesCollectionViewLayout alloc]
          initWithItemCount:config.mostVisitedItems.count + 1];
  self = [super initWithFrame:CGRectZero collectionViewLayout:layout];
  if (self) {
    _items = config.mostVisitedItems;
    _imageDataSource = config.imageDataSource;
    _layoutGuideCenter = config.layoutGuideCenter;
    _mostVisitedTilesHandler = config.commandHandler;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundColor = UIColor.clearColor;
    self.showsHorizontalScrollIndicator = NO;
    self.scrollEnabled = NO;  /// Disables vertical scrolling.
    self.dragDelegate = self;
    self.dropDelegate = self;
    self.dragInteractionEnabled = YES;
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
                    MAX(self.contentSize.height, kMostVisitedTileIconSize));
}

#pragma mark - UICollectionViewDragDelegate

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  NSNumber* identifier =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];
  if (identifier.intValue == kPlusButtonIdentifier) {
    return @[];
  }
  MostVisitedItem* item = _items[identifier.unsignedIntValue];
  if (!item.isPinned) {
    return @[];
  }
  NSItemProvider* itemProvider =
      [[NSItemProvider alloc] initWithObject:item.title];
  UIDragItem* dragItem = [[UIDragItem alloc] initWithItemProvider:itemProvider];
  dragItem.localObject = item;
  return @[ dragItem ];
}

#pragma mark - UICollectionViewDropDelegate

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  BOOL allowDrop = NO;
  if (self == collectionView && self.hasActiveDrag) {
    NSUInteger destinationIndex = destinationIndexPath.item;
    /// Only allow dropping at the "pinned sites" area.
    if (destinationIndex < _items.count && _items[destinationIndex].isPinned) {
      allowDrop = YES;
    }
  }
  return [[UICollectionViewDropProposal alloc]
      initWithDropOperation:allowDrop ? UIDropOperationMove
                                      : UIDropOperationForbidden
                     intent:
                         UICollectionViewDropIntentInsertAtDestinationIndexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    performDropWithCoordinator:
        (id<UICollectionViewDropCoordinator>)coordinator {
  NSIndexPath* destinationIndexPath = coordinator.destinationIndexPath;
  if (!destinationIndexPath) {
    return;
  }
  id<UICollectionViewDropItem> dragItem = coordinator.items.firstObject;
  if (!(dragItem && dragItem.sourceIndexPath)) {
    return;
  }
  MostVisitedItem* item = base::apple::ObjCCastStrict<MostVisitedItem>(
      dragItem.dragItem.localObject);
  [_mostVisitedTilesHandler moveMostVisitedItem:item
                                        toIndex:destinationIndexPath.item];
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
  if (!_items.lastObject.isPinned || count < kMaximumItems) {
    [indices addObject:@(kPlusButtonIdentifier)];
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
  if (identifier.intValue == kPlusButtonIdentifier) {
    MostVisitedTilesPlusButtonItem* plusButtonItem =
        [[MostVisitedTilesPlusButtonItem alloc] init];
    plusButtonItem.mostVisitedTilesHandler = _mostVisitedTilesHandler;
    cell.contentConfiguration = plusButtonItem;
  } else {
    [self loadFaviconIfNeeded:identifier];
    cell.contentConfiguration = _items[identifier.unsignedIntValue];
  }
  /// Mark the first item in the tiles for layout guide
  /// `kNTPFirstMostVisitedTileGuide`.
  if (identifier.intValue == 0) {
    [_layoutGuideCenter referenceView:cell
                            underName:kNTPFirstMostVisitedTileGuide];
  } else if ([_layoutGuideCenter
                 referencedViewUnderName:kNTPFirstMostVisitedTileGuide] ==
             cell) {
    [_layoutGuideCenter referenceView:nil
                            underName:kNTPFirstMostVisitedTileGuide];
  }
  return cell;
}

/// Loads the favicon for item with `identifier`.
- (void)loadFaviconIfNeeded:(NSNumber*)identifier {
  MostVisitedItem* item = _items[identifier.unsignedIntValue];
  if (!item.attributes) {
    __weak MostVisitedTilesCollectionView* weakSelf = self;
    void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
      /// Execute block on the main queue.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^{
            [weakSelf updateFaviconWithAttributes:attributes
                            forItemWithIdentifier:identifier];
          }));
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
