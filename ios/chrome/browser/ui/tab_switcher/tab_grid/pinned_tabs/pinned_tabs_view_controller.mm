// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/dcheck_is_on.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/modals/modals_api.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The number of sections for the pinned collection view.
NSInteger kNumberOfSectionsInPinnedCollection = 1;

// Pinned cell identifier.
NSString* const kCellIdentifier = @"PinnedCellIdentifier";

// Creates an NSIndexPath with `index` in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}

}  // namespace

@interface PinnedTabsViewController () <UICollectionViewDragDelegate,
                                        UICollectionViewDropDelegate>

// Index of the selected item in `_items`.
@property(nonatomic, readonly) NSUInteger selectedIndex;

@end

@implementation PinnedTabsViewController {
  // The local model backing the collection view.
  NSMutableArray<TabSwitcherItem*>* _items;

  // Identifier of the selected item.
  web::WebStateID _selectedItemID;

  // Latest dragged item. This property is set when the item
  // is long pressed which does not always result in a drag action.
  TabSwitcherItem* _draggedItem;

  // Identifier of the last item to be inserted. This is used to track if the
  // active tab was newly created when building the animation layout for
  // transitions.
  web::WebStateID _lastInsertedItemID;

  // Constraints used to update the view during drag and drop actions.
  NSLayoutConstraint* _heightConstraint;

  // Background color of the view.
  UIColor* _backgroundColor;

  // View displayed during an external drag action.
  UIView* _dropOverlayView;

  // Tracks if the view is available.
  BOOL _available;

  // Tracks if a drag action is in progress.
  BOOL _dragSessionEnabled;
  BOOL _localDragActionInProgress;

  // YES if the dragged tab moved to a new index.
  BOOL _dragEndAtNewIndex;

  // YES if view controller's content has appeared.
  BOOL _contentAppeared;

  // Tracks if there is a scroll in progress.
  BOOL _scrollInProgress;
}

- (instancetype)init {
  PinnedTabsLayout* layout = [[PinnedTabsLayout alloc] init];
  if ((self = [super initWithCollectionViewLayout:layout])) {
  }
  return self;
}

#pragma mark - UICollectionViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _available = YES;
  _visible = YES;
  _dragSessionEnabled = NO;
  _localDragActionInProgress = NO;
  _dropAnimationInProgress = NO;
  _contentAppeared = NO;
  _scrollInProgress = NO;

  [self configureCollectionView];
  [self configureDropOverlayView];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

#pragma mark - Public

- (void)contentWillAppearAnimated:(BOOL)animated {
  [self.collectionView reloadData];

  [self deselectAllCollectionViewItemsAnimated:NO];
  [self selectCollectionViewItemWithID:_selectedItemID animated:NO];
  [self scrollCollectionViewToSelectedItemAnimated:NO];

  _lastInsertedItemID = web::WebStateID();
  _contentAppeared = YES;
}

- (void)contentWillDisappear {
  _contentAppeared = NO;
}

- (void)dragSessionEnabled:(BOOL)enabled {
  if (_dropAnimationInProgress || (_dragSessionEnabled == enabled)) {
    return;
  }

  _dragSessionEnabled = enabled;

  if (!_available) {
    // If not available, return early to avoid a visual glitch, see
    // crbug.com/328019332.
    return;
  }

  [self updateForDragInProgress:enabled];
}

- (void)pinnedTabsAvailable:(BOOL)available {
  _available = available;

  // The view is visible if `_items` is not empty or if a drag action is in
  // progress.
  bool visible = _available && (_items.count || _dragSessionEnabled);
  if (visible == _visible) {
    return;
  }
  _visible = visible;

  // Show the view if `visible` is true to ensure smooth animation.
  if (visible) {
    if (_dragSessionEnabled) {
      // The update has been canceled to avoid glitch, see crbug.com/328019332,
      // restart it here.
      [self updateForDragInProgress:_dragSessionEnabled];
    }
    [self updateDropOverlayViewVisibility];
    self.view.hidden = NO;
  }

  // Tell the delegate that the visibility has changed in order to update the
  // tab grid view inset before hiding the pinned view.
  [self.delegate pinnedTabsViewControllerVisibilityDidChange:self];

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kPinnedViewFadeInTime
      animations:^{
        self.view.alpha = visible ? 1.0 : 0.0;
      }
      completion:^(BOOL finished) {
        [weakSelf updatePinnedTabsVisibilityAfterAnimation];
      }];
}

- (void)dropAnimationDidEnd {
  // If a local drag action is in progress, `dragSessionDidEnd:` will end the
  // drag session.
  if (_localDragActionInProgress) {
    return;
  }

  _dropAnimationInProgress = NO;
  [self dragSessionEnabled:NO];
}

- (LegacyGridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];

  LegacyGridTransitionActiveItem* activeItem;
  LegacyGridTransitionItem* selectionItem;

  NSIndexPath* selectedItemIndexPath =
      self.collectionView.indexPathsForSelectedItems.firstObject;
  PinnedCell* selectedCell = base::apple::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:selectedItemIndexPath]);
  if (!selectedCell) {
    return nil;
  }

  if (selectedCell.pinnedItemIdentifier == _selectedItemID) {
    UICollectionViewLayoutAttributes* attributes = [self.collectionView
        layoutAttributesForItemAtIndexPath:selectedItemIndexPath];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame = [self.collectionView convertRect:attributes.frame
                                                 toView:nil];

    PinnedTransitionCell* activeCell =
        [PinnedTransitionCell transitionCellFromCell:selectedCell];
    activeItem = [LegacyGridTransitionActiveItem itemWithCell:activeCell
                                                       center:attributes.center
                                                         size:attributes.size];
    // If the active item is the last inserted item, it needs to be animated
    // differently.
    if (selectedCell.pinnedItemIdentifier == _lastInsertedItemID) {
      activeItem.isAppearing = YES;
    }

    selectionItem = [LegacyGridTransitionItem
        itemWithCell:[PinnedCell transitionSelectionCellFromCell:selectedCell]
              center:attributes.center];
  }

  return [LegacyGridTransitionLayout layoutWithInactiveItems:@[]
                                                  activeItem:activeItem
                                               selectionItem:selectionItem];
}

- (TabGridTransitionItem*)transitionItemForActiveCell {
  [self.collectionView layoutIfNeeded];

  NSIndexPath* selectedItemIndexPath =
      self.collectionView.indexPathsForSelectedItems.firstObject;

  if (![self.collectionView.indexPathsForVisibleItems
          containsObject:selectedItemIndexPath]) {
    return nil;
  }

  PinnedCell* cell = base::apple::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:selectedItemIndexPath]);

  UICollectionViewLayoutAttributes* attributes = [self.collectionView
      layoutAttributesForItemAtIndexPath:selectedItemIndexPath];

  // Normalize frame to window coordinates. The attributes class applies this
  // change to the other properties such as center, bounds, etc.
  attributes.frame = [self.collectionView convertRect:attributes.frame
                                               toView:nil];

  return [TabGridTransitionItem itemWithView:cell
                               originalFrame:attributes.frame];
}

- (BOOL)isCollectionEmpty {
  return _items.count == 0;
}

- (BOOL)isSelectedCellVisible {
  // The collection view's selected item may not have updated yet, so use the
  // selected index.
  NSUInteger selectedIndex = self.selectedIndex;
  if (selectedIndex == NSNotFound) {
    return NO;
  }

  NSIndexPath* selectedIndexPath = CreateIndexPath(selectedIndex);
  return [self.collectionView.indexPathsForVisibleItems
      containsObject:selectedIndexPath];
}

- (BOOL)hasSelectedCell {
  return self.selectedIndex != NSNotFound;
}

#pragma mark - PinnedTabCollectionConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(web::WebStateID)selectedItemID {
  // Note: Keep as a DCHECK, as this can be costly.
  DCHECK(!HasDuplicateIdentifiers(items));

  _items = [items mutableCopy];
  _selectedItemID = selectedItemID;

  [self updatePinnedTabsVisibility];

  [self.collectionView reloadData];

  [self deselectAllCollectionViewItemsAnimated:YES];
  [self selectCollectionViewItemWithID:_selectedItemID animated:YES];
  [self scrollCollectionViewToSelectedItemAnimated:YES];
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(web::WebStateID)selectedItemID {
  // Consistency check: `item`'s ID is not in `_items`.
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);

  __weak __typeof(self) weakSelf = self;
  [self.collectionView
      performBatchUpdates:^{
        [weakSelf performBatchUpdateForInsertingItem:item
                                             atIndex:index
                                      selectedItemID:selectedItemID];
      }
      completion:^(BOOL completed) {
        [weakSelf handleItemInsertionCompletion];
      }];
}

- (void)removeItemWithID:(web::WebStateID)removedItemID
          selectedItemID:(web::WebStateID)selectedItemID {
  NSUInteger index = [self indexOfItemWithID:removedItemID];
  if (index == NSNotFound) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self.collectionView
      performBatchUpdates:^{
        [weakSelf performBatchUpdateForRemovingItemAtIndex:index
                                            selectedItemID:selectedItemID];
      }
      completion:^(BOOL completed) {
        [weakSelf handleItemRemovalCompletion];
        [weakSelf.delegate pinnedTabsViewController:weakSelf
                                didRemoveItemWithID:removedItemID];
      }];
}

- (void)selectItemWithID:(web::WebStateID)selectedItemID {
  if (_selectedItemID == selectedItemID) {
    return;
  }

  [self deselectAllCollectionViewItemsAnimated:NO];

  _selectedItemID = selectedItemID;
  [self selectCollectionViewItemWithID:_selectedItemID animated:NO];
  [self scrollCollectionViewToSelectedItemAnimated:NO];
}

- (void)replaceItemID:(web::WebStateID)itemID withItem:(TabSwitcherItem*)item {
  DCHECK(item.identifier == itemID ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);

  NSUInteger index = [self indexOfItemWithID:itemID];
  _items[index] = item;
  PinnedCell* cell = base::apple::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)]);
  // `cell` may be nil if it is scrolled offscreen.
  if (cell) {
    [self configureCell:cell withItem:item];
  }
}

- (void)moveItemWithID:(web::WebStateID)itemID toIndex:(NSUInteger)toIndex {
  NSUInteger fromIndex = [self indexOfItemWithID:itemID];
  if (fromIndex == toIndex || toIndex == NSNotFound ||
      fromIndex == NSNotFound) {
    return;
  }

  ProceduralBlock modelUpdates = ^{
    TabSwitcherItem* item = self->_items[fromIndex];
    [self->_items removeObjectAtIndex:fromIndex];
    [self->_items insertObject:item atIndex:toIndex];
  };
  ProceduralBlock collectionViewUpdates = ^{
    [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                                 toIndexPath:CreateIndexPath(toIndex)];
  };

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock collectionViewUpdatesCompletion = ^{
    [weakSelf updateCollectionViewAfterMovingItemToIndex:toIndex];
    [weakSelf.delegate pinnedTabsViewControllerDidMoveItem:weakSelf];
  };

  [self.collectionView
      performBatchUpdates:^{
        modelUpdates();
        collectionViewUpdates();
      }
      completion:^(BOOL completed) {
        collectionViewUpdatesCompletion();
      }];
}

- (void)dismissModals {
  ios::provider::DismissModalsForCollectionView(self.collectionView);
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return kNumberOfSectionsInPinnedCollection;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _items.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
  // TODO(crbug.com/40683330): Remove this when the issue is closed.
  // This is a preventive fix related to the issue above.
  // Presumably this is a race condition where an item has been deleted at the
  // same time as the collection is doing layout. The assumption is that there
  // will be another, correct layout shortly after the incorrect one.
  if (itemIndex >= _items.count) {
    itemIndex = _items.count - 1;
  }

  TabSwitcherItem* item = _items[itemIndex];
  PinnedCell* cell = base::apple::ObjCCastStrict<PinnedCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kPinnedCellIdentifier
                                forIndexPath:indexPath]);

  [self configureCell:cell withItem:item];
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    performPrimaryActionForItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  [self.delegate pinnedViewControllerDidRequestContextMenu:self];

  PinnedCell* cell = base::apple::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return [self.menuProvider
      contextMenuConfigurationForTabCell:cell
                            menuScenario:kMenuScenarioHistogramPinnedTabsEntry];
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[PinnedCell class]]) {
    // Stop animation of PinnedCells when removing them from the collection
    // view. This is important to prevent cells from animating indefinitely.
    // This is safe because the animation state of GridCells is set in
    // `configureCell:withItem:` whenever a cell is used.
    [base::apple::ObjCCastStrict<PinnedCell>(cell) hideActivityIndicator];
  }
}

#pragma mark - UICollectionViewDragDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dragSessionWillBegin:(id<UIDragSession>)session {
  _dragEndAtNewIndex = NO;
  _localDragActionInProgress = YES;
  base::UmaHistogramEnumeration(kUmaPinnedViewDragDropTabsEvent,
                                DragDropItem::kDragBegin);
  [self.delegate pinnedViewControllerDragSessionWillBegin:self];
  [self dragSessionEnabled:YES];
}

- (void)collectionView:(UICollectionView*)collectionView
     dragSessionDidEnd:(id<UIDragSession>)session {
  _localDragActionInProgress = NO;
  DragDropItem dragEvent = _dragEndAtNewIndex
                               ? DragDropItem::kDragEndAtNewIndex
                               : DragDropItem::kDragEndAtSameIndex;
  // If a drop animation is in progress and the drag didn't end at a new index,
  // that means the item has been dropped outside of its collection view.
  if (_dropAnimationInProgress && !_dragEndAtNewIndex) {
    dragEvent = DragDropItem::kDragEndInOtherCollection;
  }
  base::UmaHistogramEnumeration(kUmaPinnedViewDragDropTabsEvent, dragEvent);

  [self.delegate pinnedViewControllerDragSessionDidEnd:self];
  [self dragSessionEnabled:NO];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  _draggedItem = _items[indexPath.item];
  UIDragItem* dragItem = [self.dragDropHandler dragItemForItem:_draggedItem];
  if (!dragItem) {
    return @[];
  }
  return @[ dragItem ];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
            itemsForAddingToDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath
                                  point:(CGPoint)point {
  // Prevent more items from getting added to the drag session.
  return @[];
}

- (UIDragPreviewParameters*)collectionView:(UICollectionView*)collectionView
    dragPreviewParametersForItemAtIndexPath:(NSIndexPath*)indexPath {
  PinnedCell* pinedCell = base::apple::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return pinedCell.dragPreviewParameters;
}

#pragma mark - UICollectionViewDropDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidEnter:(id<UIDropSession>)session {
  if (_dragSessionEnabled) {
    _dropOverlayView.backgroundColor = [UIColor colorNamed:kBlueColor];
    self.collectionView.backgroundColor = [UIColor colorNamed:kBlueColor];
    self.collectionView.backgroundView.hidden = YES;
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidExit:(id<UIDropSession>)session {
  [self resetViewBackgrounds];
}

- (void)collectionView:(UICollectionView*)collectionView
     dropSessionDidEnd:(id<UIDropSession>)session {
  [self.delegate pinnedViewControllerDropAnimationDidEnd:self];
  [self dropAnimationDidEnd];
}

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  UIDropOperation dropOperation = [self.dragDropHandler
      dropOperationForDropSession:session
                          toIndex:destinationIndexPath.item];

  UICollectionViewDropIntent intent =
      _localDragActionInProgress
          ? UICollectionViewDropIntentInsertAtDestinationIndexPath
          : UICollectionViewDropIntentUnspecified;
  return
      [[UICollectionViewDropProposal alloc] initWithDropOperation:dropOperation
                                                           intent:intent];
}

- (void)collectionView:(UICollectionView*)collectionView
    performDropWithCoordinator:
        (id<UICollectionViewDropCoordinator>)coordinator {
  NSArray<id<UICollectionViewDropItem>>* items = coordinator.items;
  for (id<UICollectionViewDropItem> item in items) {
    // Append to the end of the collection, unless drop is from the same
    // collection view and its index is specified.
    // The sourceIndexPath is nil if the drop item is not from the same
    // collection view. Set the destinationIndex to reflect the addition of an
    // item.
    NSUInteger destinationIndex =
        item.sourceIndexPath ? _items.count - 1 : _items.count;
    if (coordinator.destinationIndexPath && item.sourceIndexPath) {
      destinationIndex =
          base::checked_cast<NSUInteger>(coordinator.destinationIndexPath.item);
    }
    _dragEndAtNewIndex = YES;

    NSIndexPath* dropIndexPath = CreateIndexPath(destinationIndex);
    // Drop synchronously if local object is available.
    if (item.dragItem.localObject) {
      _dropAnimationInProgress = YES;
      [self.delegate pinnedViewControllerDropAnimationWillBegin:self];
      if (_localDragActionInProgress) {
        __weak __typeof(self) weakSelf = self;
        [[coordinator dropItem:item.dragItem toItemAtIndexPath:dropIndexPath]
            addCompletion:^(UIViewAnimatingPosition finalPosition) {
              [weakSelf dropAnimationDidEnd];
            }];
      }
      // The sourceIndexPath is non-nil if the drop item is from this same
      // collection view.
      [self.dragDropHandler dropItem:item.dragItem
                             toIndex:destinationIndex
                  fromSameCollection:(item.sourceIndexPath != nil)];
    } else {
      // Drop asynchronously if local object is not available.
      UICollectionViewDropPlaceholder* placeholder =
          [[UICollectionViewDropPlaceholder alloc]
              initWithInsertionIndexPath:dropIndexPath
                         reuseIdentifier:kCellIdentifier];
      placeholder.previewParametersProvider =
          ^UIDragPreviewParameters*(UICollectionViewCell* placeholderCell) {
            PinnedCell* pinnedCell =
                base::apple::ObjCCastStrict<PinnedCell>(placeholderCell);
            return pinnedCell.dragPreviewParameters;
          };

      id<UICollectionViewDropPlaceholderContext> context =
          [coordinator dropItem:item.dragItem toPlaceholder:placeholder];
      [self.dragDropHandler dropItemFromProvider:item.dragItem.itemProvider
                                         toIndex:destinationIndex
                              placeholderContext:context];
    }
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canHandleDropSession:(id<UIDropSession>)session {
  return _available;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  _scrollInProgress = YES;
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  _scrollInProgress = NO;
  [self popLastInsertedItem];
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:_selectedItemID];
}

#pragma mark - Private

// Animates the latest inserted item (if any) with a pop animation.
// This method is called when :
// - The pinned overlay is hidden.
// - A scroll animation ends.
- (void)popLastInsertedItem {
  if (_dragSessionEnabled || !_lastInsertedItemID.valid()) {
    return;
  }

  NSUInteger itemIndex = [self indexOfItemWithID:_lastInsertedItemID];

  // Check `itemIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (itemIndex == NSNotFound || itemIndex >= _items.count) {
    return;
  }

  PinnedCell* pinnedCell = base::apple::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:CreateIndexPath(itemIndex)]);
  CGAffineTransform originalTransform = pinnedCell.transform;

  // Initial attributes.
  pinnedCell.alpha = 0;
  pinnedCell.hidden = NO;
  pinnedCell.transform =
      CGAffineTransformScale(pinnedCell.transform, kPinnedCellPopInitialScale,
                             kPinnedCellPopInitialScale);

  const BOOL isSelectedItem = _lastInsertedItemID == _selectedItemID;
  _lastInsertedItemID = web::WebStateID();

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kPinnedViewPopAnimationTime
      animations:^{
        pinnedCell.alpha = 1;
        pinnedCell.transform = originalTransform;
        [self.view layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (isSelectedItem) {
          [weakSelf refreshSelectedItem];
        }
      }];
}

// Refreshes the selected item when the last popped item was selected.
- (void)refreshSelectedItem {
  [self selectCollectionViewItemWithID:_selectedItemID animated:NO];
}

// Updates the visibility of the pinned view.
- (void)updatePinnedTabsVisibility {
  [self pinnedTabsAvailable:_available];
}

// Performs (in batch) all the actions needed to insert an `item` at the
// specified `index` into the collection view and updates its appearance.
// `selectedItemID` is saved to an instance variable.
- (void)performBatchUpdateForInsertingItem:(TabSwitcherItem*)item
                                   atIndex:(NSUInteger)index
                            selectedItemID:(web::WebStateID)selectedItemID {
  [_items insertObject:item atIndex:index];
  _selectedItemID = selectedItemID;
  _lastInsertedItemID = item.identifier;

  [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
}

// Performs (in batch) all the actions needed to remove an item at the
// specified `index` from the collection view and updates its appearance.
// `selectedItemID` is saved to an instance variable.
- (void)performBatchUpdateForRemovingItemAtIndex:(NSUInteger)index
                                  selectedItemID:
                                      (web::WebStateID)selectedItemID {
  [_items removeObjectAtIndex:index];
  _selectedItemID = selectedItemID;

  [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
}

// Handles the completion of item insertion into the collection view.
- (void)handleItemInsertionCompletion {
  [self updateCollectionViewAfterItemInsertion];
}

// Handles the completion of item removal into the collection view.
- (void)handleItemRemovalCompletion {
  [self updateCollectionViewAfterItemDeletion];
}

// Configures the collectionView.
- (void)configureCollectionView {
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

  UICollectionView* collectionView = self.collectionView;
  [collectionView registerClass:[PinnedCell class]
      forCellWithReuseIdentifier:kPinnedCellIdentifier];
  collectionView.layer.cornerRadius = kPinnedViewCornerRadius;
  collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  collectionView.delegate = self;
  collectionView.dragDelegate = self;
  collectionView.dropDelegate = self;
  collectionView.dragInteractionEnabled = YES;
  collectionView.showsHorizontalScrollIndicator = NO;
  collectionView.accessibilityIdentifier = kPinnedViewIdentifier;

  self.view = collectionView;

  UIView* backgroundView;

  // Only apply the blur if transparency effects are not disabled.
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    _backgroundColor = [UIColor clearColor];

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterialDark];
    backgroundView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  } else {
    _backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

    backgroundView = [[UIView alloc] init];
  }

  backgroundView.frame = collectionView.bounds;
  backgroundView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  collectionView.backgroundView = backgroundView;
  collectionView.backgroundColor = _backgroundColor;

  _heightConstraint = [collectionView.heightAnchor
      constraintEqualToConstant:kPinnedViewDefaultHeight];
  _heightConstraint.active = YES;
}

// Configures `dropOverlayView`.
- (void)configureDropOverlayView {
  _dropOverlayView = [[UIView alloc] init];
  _dropOverlayView.translatesAutoresizingMaskIntoConstraints = NO;
  _dropOverlayView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  [self.view addSubview:_dropOverlayView];

  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.adjustsFontForContentSizeCategory = YES;
  label.adjustsFontSizeToFitWidth = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.text = l10n_util::GetNSString(IDS_IOS_PINNED_TABS_DRAG_TO_PIN_LABEL);
  label.translatesAutoresizingMaskIntoConstraints = NO;

  // Mirror the label for RTL (see crbug.com/1426256).
  if (base::i18n::IsRTL()) {
    label.transform = CGAffineTransformScale(label.transform, -1, 1);
  }

  [_dropOverlayView addSubview:label];

  AddSameConstraints(_dropOverlayView, self.collectionView.backgroundView);
  [NSLayoutConstraint activateConstraints:@[
    [label.centerYAnchor
        constraintEqualToAnchor:_dropOverlayView.centerYAnchor],
    [label.centerXAnchor
        constraintEqualToAnchor:_dropOverlayView.centerXAnchor],
    [label.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_dropOverlayView.leadingAnchor
                                    constant:kPinnedViewHorizontalPadding],
    [label.trailingAnchor
        constraintLessThanOrEqualToAnchor:_dropOverlayView.trailingAnchor
                                 constant:-kPinnedViewHorizontalPadding],
    [label.bottomAnchor constraintEqualToAnchor:_dropOverlayView.bottomAnchor],
    [label.topAnchor constraintEqualToAnchor:_dropOverlayView.topAnchor],
  ]];

  [self updateDropOverlayViewVisibility];
}

// Configures `cell`'s identifier and title synchronously, and favicon and
// snapshot asynchronously from `item`.
- (void)configureCell:(PinnedCell*)cell withItem:(TabSwitcherItem*)item {
  CHECK(cell);
  if (item) {
    cell.pinnedItemIdentifier = item.identifier;
    cell.title = item.title;
    [item fetchFavicon:^(TabSwitcherItem* innerItem, UIImage* icon) {
      // Only update the icon if the cell is not already reused for another
      // item.
      if (cell.pinnedItemIdentifier == innerItem.identifier) {
        cell.icon = icon;
      }
    }];
    [item fetchSnapshot:^(TabSwitcherItem* innerItem, UIImage* snapshot) {
      // Only update the icon if the cell is not already reused for another
      // item.
      if (cell.pinnedItemIdentifier == innerItem.identifier) {
        cell.snapshot = snapshot;
      }
    }];
  }

  cell.accessibilityIdentifier = [NSString
      stringWithFormat:@"%@%ld", kPinnedCellIdentifier,
                       [self indexOfItemWithID:cell.pinnedItemIdentifier]];

  if (item.showsActivity) {
    [cell showActivityIndicator];
  } else {
    [cell hideActivityIndicator];
  }
  if (_contentAppeared && cell.pinnedItemIdentifier == _lastInsertedItemID) {
    cell.hidden = YES;
  }
}

// Returns the index in `_items` of the first item whose identifier is
// `identifier`.
- (NSUInteger)indexOfItemWithID:(web::WebStateID)identifier {
  // Check that identifier is valid.
  if (!identifier.valid()) {
    return NSNotFound;
  }

  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return item.identifier == identifier;
      };
  return [_items indexOfObjectPassingTest:selectedTest];
}

// Updates the pinned tabs view visibility after an animation.
- (void)updatePinnedTabsVisibilityAfterAnimation {
  if (!_visible) {
    self.view.hidden = YES;
  }

  // Don't call the delegate if the pinned view is hidden after a tab grid page
  // change.
  if (!_visible && _items.count > 0) {
    return;
  }

  if (_visible && _items.count == 1) {
    [self popLastInsertedItem];
  }
}

// Shows `_dropOverlayView` when a external drag action is in progress.
- (void)updateDropOverlayViewVisibility {
  BOOL visible = _dragSessionEnabled && !_localDragActionInProgress;
  _dropOverlayView.alpha = visible ? 1 : 0;
}

// Updates the collection view after an item insertion.
- (void)updateCollectionViewAfterItemInsertion {
  [self deselectAllCollectionViewItemsAnimated:NO];
  [self selectCollectionViewItemWithID:_selectedItemID animated:NO];

  // Scroll the collection view to the newly added item, so it doesn't
  // disappear from the user's sight.
  [self scrollCollectionViewToLastItemAnimated:YES];

  [self updatePinnedTabsVisibility];
}

// Updates the collection view after an item deletion.
- (void)updateCollectionViewAfterItemDeletion {
  if (_items.count > 0) {
    [self deselectAllCollectionViewItemsAnimated:NO];
    [self selectCollectionViewItemWithID:_selectedItemID animated:NO];
  } else {
    [self pinnedTabsAvailable:_available];
  }
}

// Updates the collection view after moving an item to the given `index`.
- (void)updateCollectionViewAfterMovingItemToIndex:(NSUInteger)index {
  // Bring back selected halo only for the moved cell, which lost it during
  // the move (drag & drop).
  if (self.selectedIndex != index) {
    [self scrollCollectionViewToItemWithIndex:index animated:YES];
    return;
  }
  // Force reload of the selected cell now to avoid extra delay for the
  // blue halo to appear.
  [UIView
      animateWithDuration:kPinnedViewMoveAnimationTime
               animations:^{
                 [self.collectionView reloadItemsAtIndexPaths:@[
                   CreateIndexPath(self.selectedIndex)
                 ]];
                 [self deselectAllCollectionViewItemsAnimated:NO];
                 [self selectCollectionViewItemWithID:self->_selectedItemID
                                             animated:NO];
               }
               completion:nil];
}

// Updates the visual of the Pinned Tabs to account for whether a drag and drop
// is currently happening or not.
- (void)updateForDragInProgress:(BOOL)dragInProgress {
  __weak __typeof(self) weakSelf = self;
  __weak NSLayoutConstraint* heightConstraint = _heightConstraint;
  [UIView animateWithDuration:kPinnedViewDragAnimationTime
      animations:^{
        heightConstraint.constant = dragInProgress
                                        ? kPinnedViewDragEnabledHeight
                                        : kPinnedViewDefaultHeight;
        [weakSelf updateDropOverlayViewVisibility];
        [weakSelf resetViewBackgrounds];
        [weakSelf.view.superview layoutIfNeeded];
        [weakSelf.view layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        [weakSelf popLastInsertedItem];
      }];
}

// Tells the delegate that the user tapped the item with identifier
// corresponding to `indexPath`.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  // Do not track item taps during tab grid transitions.
  if (!_contentAppeared) {
    return;
  }

  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  DCHECK_LT(index, _items.count);

  const web::WebStateID itemID = _items[index].identifier;
  [self.delegate pinnedTabsViewController:self didSelectItemWithID:itemID];
}

// Resets view backgrounds.
- (void)resetViewBackgrounds {
  _dropOverlayView.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  self.collectionView.backgroundColor = _backgroundColor;
  self.collectionView.backgroundView.hidden = NO;
}

// Selects the collection view's item with `itemID`.
- (void)selectCollectionViewItemWithID:(web::WebStateID)itemID
                              animated:(BOOL)animated {
  NSUInteger itemIndex = [self indexOfItemWithID:itemID];

  // Check `itemIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (itemIndex == NSNotFound || itemIndex >= _items.count) {
    return;
  }

  NSIndexPath* itemIndexPath = CreateIndexPath(itemIndex);

  [self.collectionView
      selectItemAtIndexPath:itemIndexPath
                   animated:animated
             scrollPosition:UICollectionViewScrollPositionNone];
}

// Deselects all the collection view items.
- (void)deselectAllCollectionViewItemsAnimated:(BOOL)animated {
  NSArray<NSIndexPath*>* indexPathsForSelectedItems =
      [self.collectionView indexPathsForSelectedItems];
  for (NSIndexPath* itemIndexPath in indexPathsForSelectedItems) {
    [self.collectionView deselectItemAtIndexPath:itemIndexPath
                                        animated:animated];
  }
}

// Scrolls the collection view to the currently selected item.
- (void)scrollCollectionViewToSelectedItemAnimated:(BOOL)animated {
  [self scrollCollectionViewToItemWithIndex:self.selectedIndex
                                   animated:animated];
}

// Scrolls the collection view to the last item.
- (void)scrollCollectionViewToLastItemAnimated:(BOOL)animated {
  [self scrollCollectionViewToItemWithIndex:_items.count - 1 animated:animated];
}

// Scrolls the collection view to the item with specified `itemIndex`.
- (void)scrollCollectionViewToItemWithIndex:(NSUInteger)itemIndex
                                   animated:(BOOL)animated {
  // Check `itemIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (itemIndex == NSNotFound || itemIndex >= _items.count) {
    return;
  }

  NSIndexPath* itemIndexPath = CreateIndexPath(itemIndex);
  [self.collectionView
      scrollToItemAtIndexPath:itemIndexPath
             atScrollPosition:UICollectionViewScrollPositionCenteredHorizontally
                     animated:animated];
}

@end
