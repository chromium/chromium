// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"

#import "base/dcheck_is_on.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  NSString* _selectedItemID;

  // Constraints used to update the view during drag and drop actions.
  NSLayoutConstraint* _dragEnabledConstraint;
  NSLayoutConstraint* _defaultConstraint;

  // Background color of the view.
  UIColor* _backgroundColor;

  // UILabel displayed when the collection view is empty.
  UILabel* _emptyCollectionViewLabel;

  // Tracks if the view is available.
  BOOL _available;

  // Tracks if the view is visible.
  BOOL _visible;

  // Tracks if a drag action is in progress.
  BOOL _dragActionInProgress;

  // YES if the dragged tab moved to a new index.
  BOOL _dragEndAtNewIndex;
}

- (instancetype)init {
  PinnedTabsLayout* layout = [[PinnedTabsLayout alloc] init];
  if (self = [super initWithCollectionViewLayout:layout]) {
  }
  return self;
}

#pragma mark - UICollectionViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _available = YES;
  _visible = YES;
  _dragActionInProgress = NO;
  _dropAnimationInProgress = NO;

  [self configureCollectionView];
  [self configureEmptyCollectionViewLabel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

#pragma mark - Public

- (void)contentWillAppearAnimated:(BOOL)animated {
  [self.collectionView reloadData];
  [self updateEmptyCollectionViewLabelVisibility];

  [self scrollCollectionViewToSelectedItem];

  // Update the delegate, in case it wasn't set when `items` was populated.
  [self.delegate pinnedTabsViewController:self didChangeItemCount:_items.count];
}

- (void)contentWillDisappear {
}

- (void)dragSessionEnabled:(BOOL)enabled {
  if (_dropAnimationInProgress) {
    return;
  }

  _dragActionInProgress = enabled;

  [UIView animateWithDuration:kPinnedViewDragAnimationTime
                   animations:^{
                     self->_dragEnabledConstraint.active = enabled;
                     self->_defaultConstraint.active = !enabled;

                     [self resetCollectionViewBackground];
                     [self.view.superview layoutIfNeeded];
                   }
                   completion:nil];
}

- (void)pinnedTabsAvailable:(BOOL)available {
  _available = available;

  // The view is visible if `_items` is not empty or if a drag action is in
  // progress.
  bool visible = _available && (_items.count || _dragActionInProgress);
  if (visible == _visible) {
    return;
  }

  // Show the view if `visible` is true to ensure smooth animation.
  if (visible) {
    self.view.hidden = NO;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kPinnedViewFadeInTime
      animations:^{
        self.view.alpha = visible ? 1.0 : 0.0;
      }
      completion:^(BOOL finished) {
        [weakSelf updatePinnedTabsVisibilityAfterAnimation:visible];
      }];
}

- (void)dropAnimationDidEnd {
  _dropAnimationInProgress = NO;
  [self dragSessionEnabled:NO];
}

#pragma mark - TabCollectionConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
#if DCHECK_IS_ON()
  // Consistency check: ensure no IDs are duplicated.
  NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
  for (TabSwitcherItem* item in items) {
    [identifiers addObject:item.identifier];
  }
  DCHECK_EQ(identifiers.count, items.count);
#endif

  _items = [items mutableCopy];
  _selectedItemID = selectedItemID;

  [self updateEmptyCollectionViewLabelVisibility];

  [self.delegate pinnedTabsViewController:self didChangeItemCount:items.count];

  [self.collectionView reloadData];
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  // Consistency check: `item`'s ID is not in `_items`.
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);

  NSString* previousItemID = _selectedItemID;

  __weak __typeof(self) weakSelf = self;
  [self.collectionView
      performBatchUpdates:^{
        [weakSelf performBatchUpdateForInsertingItem:item
                                             atIndex:index
                                      selectedItemID:selectedItemID];
      }
      completion:^(BOOL completed) {
        [weakSelf
            handleItemInsertionCompletionWithPreviousItemID:previousItemID];
      }];
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
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
      }];
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  if ([_selectedItemID isEqualToString:selectedItemID]) {
    return;
  }

  [self.collectionView
      deselectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:NO];
  _selectedItemID = selectedItemID;
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)replaceItemID:(NSString*)itemID withItem:(TabSwitcherItem*)item {
  DCHECK([item.identifier isEqualToString:itemID] ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);

  NSUInteger index = [self indexOfItemWithID:itemID];
  _items[index] = item;
  PinnedCell* cell = base::mac::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)]);
  // `cell` may be nil if it is scrolled offscreen.
  if (cell) {
    [self configureCell:cell withItem:item];
  }
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
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
  // Should never be called for this class.
  NOTREACHED();
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
  // TODO(crbug.com/1068136): Remove this when the issue is closed.
  // This is a preventive fix related to the issue above.
  // Presumably this is a race condition where an item has been deleted at the
  // same time as the collection is doing layout. The assumption is that there
  // will be another, correct layout shortly after the incorrect one.
  if (itemIndex >= _items.count) {
    itemIndex = _items.count - 1;
  }

  TabSwitcherItem* item = _items[itemIndex];
  PinnedCell* cell = base::mac::ObjCCastStrict<PinnedCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kPinnedCellIdentifier
                                forIndexPath:indexPath]);

  [self configureCell:cell withItem:item];
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  if (@available(iOS 16, *)) {
    // This is handled by
    // `collectionView:performPrimaryActionForItemAtIndexPath:` on iOS 16.
  } else {
    [self tappedItemAtIndexPath:indexPath];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    performPrimaryActionForItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  PinnedCell* cell = base::mac::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return [self.menuProvider
      contextMenuConfigurationForTabCell:cell
                            menuScenario:MenuScenarioHistogram::
                                             kPinnedTabsEntry];
}

#pragma mark - UICollectionViewDragDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dragSessionWillBegin:(id<UIDragSession>)session {
  _dragEndAtNewIndex = NO;
  base::UmaHistogramEnumeration(kUmaPinnedViewDragDropTabs,
                                DragDropTabs::kDragBegin);

  [self dragSessionEnabled:YES];
}

- (void)collectionView:(UICollectionView*)collectionView
     dragSessionDidEnd:(id<UIDragSession>)session {
  DragDropTabs dragEvent = _dragEndAtNewIndex
                               ? DragDropTabs::kDragEndAtNewIndex
                               : DragDropTabs::kDragEndAtSameIndex;
  base::UmaHistogramEnumeration(kUmaPinnedViewDragDropTabs, dragEvent);

  [self.dragDropHandler dragSessionDidEnd];
  [self dragSessionEnabled:NO];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  TabSwitcherItem* item = _items[indexPath.item];
  UIDragItem* dragItem =
      [self.dragDropHandler dragItemForItemWithID:item.identifier];
  return [NSArray arrayWithObjects:dragItem, nil];
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
  PinnedCell* pinedCell = base::mac::ObjCCastStrict<PinnedCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return pinedCell.dragPreviewParameters;
}

#pragma mark - UICollectionViewDropDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidEnter:(id<UIDropSession>)session {
  self.collectionView.backgroundColor = [UIColor colorNamed:kBlueColor];
  self.collectionView.backgroundView.hidden = YES;
}

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidExit:(id<UIDropSession>)session {
  [self resetCollectionViewBackground];
}

- (void)collectionView:(UICollectionView*)collectionView
     dropSessionDidEnd:(id<UIDropSession>)session {
  // Reset the background if the drop cames from another app.
  [self resetCollectionViewBackground];
}

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  UIDropOperation dropOperation =
      [self.dragDropHandler dropOperationForDropSession:session];
  return [[UICollectionViewDropProposal alloc]
      initWithDropOperation:dropOperation
                     intent:
                         UICollectionViewDropIntentInsertAtDestinationIndexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    performDropWithCoordinator:
        (id<UICollectionViewDropCoordinator>)coordinator {
  NSArray<id<UICollectionViewDropItem>>* items = coordinator.items;
  for (id<UICollectionViewDropItem> item in items) {
    // Append to the end of the collection, unless drop index is specified.
    // The sourceIndexPath is nil if the drop item is not from the same
    // collection view. Set the destinationIndex to reflect the addition of an
    // item.
    NSUInteger destinationIndex =
        item.sourceIndexPath ? _items.count - 1 : _items.count;
    if (coordinator.destinationIndexPath) {
      destinationIndex =
          base::checked_cast<NSUInteger>(coordinator.destinationIndexPath.item);
    }
    _dragEndAtNewIndex = YES;

    NSIndexPath* dropIndexPath = CreateIndexPath(destinationIndex);
    // Drop synchronously if local object is available.
    __weak __typeof(self) weakSelf = self;
    if (item.dragItem.localObject) {
      _dropAnimationInProgress = YES;
      [[coordinator dropItem:item.dragItem toItemAtIndexPath:dropIndexPath]
          addCompletion:^(UIViewAnimatingPosition finalPosition) {
            [weakSelf dropAnimationDidEnd];
          }];

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
            base::mac::ObjCCastStrict<PinnedCell>(placeholderCell);
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

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:_selectedItemID];
}

#pragma mark - Private

// Performs (in batch) all the actions needed to insert an `item` at the
// specified `index` into the collection view and updates its appearance.
// `selectedItemID` is saved to an instance variable.
- (void)performBatchUpdateForInsertingItem:(TabSwitcherItem*)item
                                   atIndex:(NSUInteger)index
                            selectedItemID:(NSString*)selectedItemID {
  [_items insertObject:item atIndex:index];
  _selectedItemID = selectedItemID;
  [self.delegate pinnedTabsViewController:self didChangeItemCount:_items.count];

  [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  [self updateEmptyCollectionViewLabelVisibility];
}

// Performs (in batch) all the actions needed to remove an item at the
// specified `index` from the collection view and updates its appearance.
// `selectedItemID` is saved to an instance variable.
- (void)performBatchUpdateForRemovingItemAtIndex:(NSUInteger)index
                                  selectedItemID:(NSString*)selectedItemID {
  [_items removeObjectAtIndex:index];
  _selectedItemID = selectedItemID;
  [self.delegate pinnedTabsViewController:self didChangeItemCount:_items.count];

  [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  [self updateEmptyCollectionViewLabelVisibility];
}

// Handles the completion of item insertion into the collection view.
- (void)handleItemInsertionCompletionWithPreviousItemID:
    (NSString*)previousItemID {
  [self
      updateCollectionViewAfterItemInsertionWithPreviousItemID:previousItemID];
  [self.delegate pinnedTabsViewController:self didChangeItemCount:_items.count];
}

// Handles the completion of item removal into the collection view.
- (void)handleItemRemovalCompletion {
  [self updateCollectionViewAfterItemDeletion];
  [self.delegate pinnedTabsViewController:self didChangeItemCount:_items.count];
}

// Scrolls collection view to make the selected item visible.
- (void)scrollCollectionViewToSelectedItem {
  NSUInteger selectedIndex = self.selectedIndex;

  if (selectedIndex != NSNotFound && selectedIndex < _items.count) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(selectedIndex)
                     animated:NO
               scrollPosition:
                   UICollectionViewScrollPositionCenteredHorizontally];
  }
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

  self.view = collectionView;

  // Only apply the blur if transparency effects are not disabled.
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    _backgroundColor = [UIColor clearColor];

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterialDark];
    UIVisualEffectView* blurEffectView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];

    blurEffectView.frame = collectionView.bounds;
    blurEffectView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    collectionView.backgroundView = blurEffectView;
  } else {
    _backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  }
  collectionView.backgroundColor = _backgroundColor;

  _dragEnabledConstraint = [collectionView.heightAnchor
      constraintEqualToConstant:kPinnedViewDragEnabledHeight];
  _defaultConstraint = [collectionView.heightAnchor
      constraintEqualToConstant:kPinnedViewDefaultHeight];
  _defaultConstraint.active = YES;
}

// Configures `_emptyCollectionViewLabel`.
- (void)configureEmptyCollectionViewLabel {
  _emptyCollectionViewLabel = [[UILabel alloc] init];
  _emptyCollectionViewLabel.numberOfLines = 0;
  _emptyCollectionViewLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  _emptyCollectionViewLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _emptyCollectionViewLabel.text =
      l10n_util::GetNSString(IDS_IOS_PINNED_TABS_DRAG_TO_PIN_LABEL);
  _emptyCollectionViewLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_emptyCollectionViewLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_emptyCollectionViewLabel.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [_emptyCollectionViewLabel.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ]];

  [self updateEmptyCollectionViewLabelVisibility];
}

// Configures `cell`'s title synchronously, and favicon asynchronously with
// information from `item`. Updates the `cell`'s theme to this view
// controller's theme.
- (void)configureCell:(PinnedCell*)cell withItem:(TabSwitcherItem*)item {
  if (item) {
    cell.itemIdentifier = item.identifier;
    cell.title = item.title;
    NSString* itemIdentifier = item.identifier;
    [self.imageDataSource faviconForIdentifier:itemIdentifier
                                    completion:^(UIImage* icon) {
                                      // Only update the icon if the cell is not
                                      // already reused for another item.
                                      if ([cell hasIdentifier:itemIdentifier]) {
                                        cell.icon = icon;
                                      }
                                    }];
  }
}

// Returns the index in `_items` of the first item whose identifier is
// `identifier`.
- (NSUInteger)indexOfItemWithID:(NSString*)identifier {
  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return [item.identifier isEqualToString:identifier];
      };
  return [_items indexOfObjectPassingTest:selectedTest];
}

// Updates the pinned tabs view visibility after an animation.
- (void)updatePinnedTabsVisibilityAfterAnimation:(BOOL)visible {
  _visible = visible;
  if (!visible) {
    self.view.hidden = YES;
    [self.delegate pinnedTabsViewControllerDidHide];
  }
}

// Hides `_emptyCollectionViewLabel` when the collection view is not empty.
- (void)updateEmptyCollectionViewLabelVisibility {
  _emptyCollectionViewLabel.hidden = _items.count > 0;
}

// Updates the collection view after an item insertion with the previously
// selected item id.
- (void)updateCollectionViewAfterItemInsertionWithPreviousItemID:
    (NSString*)previousItemID {
  [self.collectionView
      deselectItemAtIndexPath:CreateIndexPath(
                                  [self indexOfItemWithID:previousItemID])
                     animated:NO];

  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];

  [self pinnedTabsAvailable:_available];
}

// Updates the collection view after an item deletion.
- (void)updateCollectionViewAfterItemDeletion {
  if (_items.count > 0) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:NO
               scrollPosition:UICollectionViewScrollPositionNone];
  } else {
    [self pinnedTabsAvailable:_available];
  }
}

// Updates the collection view after moving an item to the given `index`.
- (void)updateCollectionViewAfterMovingItemToIndex:(NSUInteger)index {
  // Bring back selected halo only for the moved cell, which lost it during
  // the move (drag & drop).
  if (self.selectedIndex != index) {
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
                 [self.collectionView
                     selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                  animated:NO
                            scrollPosition:UICollectionViewScrollPositionNone];
               }
               completion:nil];
}

// Tells the delegate that the user tapped the item with identifier
// corresponding to `indexPath`.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  DCHECK_LT(index, _items.count);

  NSString* itemID = _items[index].identifier;
  [self.delegate pinnedTabsViewController:self didSelectItemWithID:itemID];
}

// Resets the `collectionView` background.
- (void)resetCollectionViewBackground {
  self.collectionView.backgroundColor = _backgroundColor;
  self.collectionView.backgroundView.hidden = NO;
}

@end
