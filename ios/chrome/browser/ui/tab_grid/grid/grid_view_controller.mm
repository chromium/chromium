// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid/grid_view_controller.h"

#include "base/ios/block_types.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "base/numerics/safe_conversions.h"
#include "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCellIdentifier = @"GridCellIdentifier";
// Creates an NSIndexPath with |index| in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}
}  // namespace

@interface GridViewController ()<GridCellDelegate,
                                 UICollectionViewDataSource,
                                 UICollectionViewDelegate>
// There is no need to update the collection view when other view controllers
// are obscuring the collection view. Bookkeeping is based on |-viewWillAppear:|
// and |-viewWillDisappear methods. Note that the |Did| methods are not reliably
// called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL updatesCollectionView;
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<GridItem*>* items;
// Identifier of the selected item. This value is disregarded if |self.items| is
// empty. This bookkeeping is done to set the correct selection on
// |-viewWillAppear:|.
@property(nonatomic, copy) NSString* selectedItemID;
// Index of the selected item in |items|.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// ID of the last item to be inserted. This is used to track if the active tab
// was newly created when building the animation layout for transitions.
@property(nonatomic, copy) NSString* lastInsertedItemID;
// The gesture recognizer used for interactive item reordering.
@property(nonatomic, strong)
    UILongPressGestureRecognizer* itemReorderRecognizer;
// The touch location of an interactively reordering item, expressed as an
// offset from its center. This is subtracted from the location that is passed
// to -updateInteractiveMovementTargetPosition: so that the moving item will
// keep them same relative location to the user's touch.
@property(nonatomic, assign) CGPoint itemReorderTouchPoint;
// Animator to show or hide the empty state.
@property(nonatomic, strong) UIViewPropertyAnimator* emptyStateAnimator;
// The default layout for the tab grid.
@property(nonatomic, strong) GridLayout* defaultLayout;
// The layout used while the grid is being reordered.
@property(nonatomic, strong) UICollectionViewLayout* reorderingLayout;
// YES if, when reordering is enabled, the order of the cells has changed.
@property(nonatomic, assign) BOOL hasChangedOrder;
@end

@implementation GridViewController

- (instancetype)init {
  if (self = [super init]) {
    _items = [[NSMutableArray<GridItem*> alloc] init];
    _showsSelectionUpdates = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.defaultLayout = [[GridLayout alloc] init];
  self.reorderingLayout = [[GridReorderingLayout alloc] init];
  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:self.defaultLayout];
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  collectionView.dataSource = self;
  collectionView.delegate = self;
  collectionView.backgroundView = [[UIView alloc] init];
  collectionView.backgroundView.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];
  // CollectionView, in contrast to TableView, doesn’t inset the
  // cell content to the safe area guide by default. We will just manage the
  // collectionView contentInset manually to fit in the safe area instead.
  collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  self.itemReorderRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleItemReorderingWithGesture:)];
  // The collection view cells will by default get touch events in parallel with
  // the reorder recognizer. When this happens, long-pressing on a non-selected
  // cell will cause the selected cell to briefly become unselected and then
  // selected again. To avoid this, the recognizer delays touchesBegan: calls
  // until it fails to recognize a long-press.
  self.itemReorderRecognizer.delaysTouchesBegan = YES;
  [collectionView addGestureRecognizer:self.itemReorderRecognizer];
  self.collectionView = collectionView;
  self.view = collectionView;

  // A single selection collection view's default behavior is to momentarily
  // deselect the selected cell on touch down then select the new cell on touch
  // up. In this tab grid, the selection ring should stay visible on the
  // selected cell on touch down. Multiple selection disables the deselection
  // behavior. Multiple selection will not actually be possible since
  // |-collectionView:shouldSelectItemAtIndexPath:| returns NO.
  collectionView.allowsMultipleSelection = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.updatesCollectionView = YES;
  self.defaultLayout.animatesItemUpdates = YES;
  [self.collectionView reloadData];
  // Selection is invalid if there are no items.
  if (self.items.count == 0) {
    [self animateEmptyStateIn];
    return;
  }
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                    animated:animated
                              scrollPosition:UICollectionViewScrollPositionTop];
  // Update the delegate, in case it wasn't set when |items| was populated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self removeEmptyStateAnimated:NO];
  self.lastInsertedItemID = nil;
}

- (void)viewWillDisappear:(BOOL)animated {
  self.updatesCollectionView = NO;
  [super viewWillDisappear:animated];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - Public

- (UIScrollView*)gridView {
  return self.collectionView;
}

- (void)setEmptyStateView:(UIView<GridEmptyView>*)emptyStateView {
  if (_emptyStateView)
    [_emptyStateView removeFromSuperview];
  _emptyStateView = emptyStateView;
  emptyStateView.scrollViewContentInsets =
      self.collectionView.adjustedContentInset;
  emptyStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.collectionView.backgroundView addSubview:emptyStateView];
  id<LayoutGuideProvider> safeAreaGuide =
      self.collectionView.backgroundView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [self.collectionView.backgroundView.centerYAnchor
        constraintEqualToAnchor:emptyStateView.centerYAnchor],
    [safeAreaGuide.leadingAnchor
        constraintEqualToAnchor:emptyStateView.leadingAnchor],
    [safeAreaGuide.trailingAnchor
        constraintEqualToAnchor:emptyStateView.trailingAnchor],
    [emptyStateView.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeAreaGuide.topAnchor],
    [emptyStateView.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeAreaGuide.bottomAnchor],
  ]];
}

- (BOOL)isGridEmpty {
  return self.items.count == 0;
}

- (BOOL)isSelectedCellVisible {
  // The collection view's selected item may not have updated yet, so use the
  // selected index.
  NSUInteger selectedIndex = self.selectedIndex;
  if (selectedIndex == NSNotFound)
    return NO;
  NSIndexPath* selectedIndexPath = CreateIndexPath(selectedIndex);
  return [self.collectionView.indexPathsForVisibleItems
      containsObject:selectedIndexPath];
}

- (GridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<GridTransitionItem*>* items = [[NSMutableArray alloc] init];
  GridTransitionActiveItem* activeItem;
  GridTransitionItem* selectionItem;
  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
        [self.collectionView cellForItemAtIndexPath:path]);
    UICollectionViewLayoutAttributes* attributes =
        [self.collectionView layoutAttributesForItemAtIndexPath:path];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame =
        [self.collectionView convertRect:attributes.frame toView:nil];
    if ([cell.itemIdentifier isEqualToString:self.selectedItemID]) {
      GridTransitionCell* activeCell =
          [GridTransitionCell transitionCellFromCell:cell];
      activeItem = [GridTransitionActiveItem itemWithCell:activeCell
                                                   center:attributes.center
                                                     size:attributes.size];
      // If the active item is the last inserted item, it needs to be animated
      // differently.
      if ([cell.itemIdentifier isEqualToString:self.lastInsertedItemID])
        activeItem.isAppearing = YES;
      selectionItem = [GridTransitionItem
          itemWithCell:[GridTransitionSelectionCell transitionCellFromCell:cell]
                center:attributes.center];
    } else {
      UIView* cellSnapshot = [cell snapshotViewAfterScreenUpdates:YES];
      GridTransitionItem* item =
          [GridTransitionItem itemWithCell:cellSnapshot
                                    center:attributes.center];
      [items addObject:item];
    }
  }
  return [GridTransitionLayout layoutWithInactiveItems:items
                                            activeItem:activeItem
                                         selectionItem:selectionItem];
}

- (void)prepareForDismissal {
  // Stop animating the collection view to prevent the insertion animation from
  // interfering with the tab presentation animation.
  self.defaultLayout.animatesItemUpdates = NO;
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return base::checked_cast<NSInteger>(self.items.count);
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                forIndexPath:indexPath]);
  cell.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix,
                                 base::checked_cast<long>(indexPath.item)];
  GridItem* item = self.items[indexPath.item];
  [self configureCell:cell withItem:item];
  return cell;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canMoveItemAtIndexPath:(NSIndexPath*)indexPath {
  return indexPath && self.items.count > 1;
}

- (void)collectionView:(UICollectionView*)collectionView
    moveItemAtIndexPath:(NSIndexPath*)sourceIndexPath
            toIndexPath:(NSIndexPath*)destinationIndexPath {
  NSUInteger source = base::checked_cast<NSUInteger>(sourceIndexPath.item);
  NSUInteger destination =
      base::checked_cast<NSUInteger>(destinationIndexPath.item);
  // Update |items| before informing the delegate, so the state of the UI
  // is correctly represented before any updates occur.
  GridItem* item = self.items[source];
  [self.items removeObjectAtIndex:source];
  [self.items insertObject:item atIndex:destination];
  self.hasChangedOrder = YES;
  [self.delegate gridViewController:self
                  didMoveItemWithID:item.identifier
                            toIndex:destination];
}

#pragma mark - UICollectionViewDelegate

// This method is used instead of -didSelectItemAtIndexPath, because any
// selection events will be signalled through the model layer and handled in
// the GridConsumer -selectItemWithID: method.
- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
  // Tapping on a non-selected cell should not select it immediately. The
  // delegate will trigger a transition to show the item.
  return NO;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
  // Tapping on the current selected cell should not deselect it.
  return NO;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  self.emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

#pragma mark - GridCellDelegate

- (void)closeButtonTappedForCell:(GridCell*)cell {
  // Disable the reordering recognizer to cancel any in-flight reordering.  The
  // DCHECK below ensures that the gesture is re-enabled after being cancelled
  // in |-handleItemReorderingWithGesture:|.
  if (self.itemReorderRecognizer.state != UIGestureRecognizerStatePossible) {
    self.itemReorderRecognizer.enabled = NO;
    DCHECK(self.itemReorderRecognizer.enabled);
  }

  [self.delegate gridViewController:self
                 didCloseItemWithID:cell.itemIdentifier];
  // Record when a tab is closed via the X.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseControlTapped"));
}

#pragma mark - GridConsumer

- (void)populateItems:(NSArray<GridItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
#ifndef NDEBUG
  // Consistency check: ensure no IDs are duplicated.
  NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
  for (GridItem* item in items) {
    [identifiers addObject:item.identifier];
  }
  CHECK_EQ(identifiers.count, items.count);
#endif

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;
  if ([self updatesCollectionView]) {
    [self.collectionView reloadData];
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionTop];
  }
  // Whether the view is visible or not, the delegate must be updated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
}

- (void)insertItem:(GridItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  // Consistency check: |item|'s ID is not in |items|.
  // (using DCHECK rather than DCHECK_EQ to avoid a checked_cast on NSNotFound).
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);
  auto modelUpdates = ^{
    [self.items insertObject:item atIndex:index];
    self.selectedItemID = selectedItemID;
    self.lastInsertedItemID = item.identifier;
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  auto collectionViewUpdates = ^{
    [self removeEmptyStateAnimated:YES];
    [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  };
  auto completion = ^(BOOL finished) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionNone];
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
      collectionViewUpdatesCompletion:completion];
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  // Disable the reordering recognizer to cancel any in-flight reordering.  The
  // DCHECK below ensures that the gesture is re-enabled after being cancelled
  // in |-handleItemReorderingWithGesture:|.
  if (self.itemReorderRecognizer.state != UIGestureRecognizerStatePossible &&
      self.itemReorderRecognizer.state != UIGestureRecognizerStateCancelled) {
    self.itemReorderRecognizer.enabled = NO;
    DCHECK(self.itemReorderRecognizer.enabled);
  }

  NSUInteger index = [self indexOfItemWithID:removedItemID];
  auto modelUpdates = ^{
    [self.items removeObjectAtIndex:index];
    self.selectedItemID = selectedItemID;
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  auto collectionViewUpdates = ^{
    [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
    if (self.items.count == 0) {
      [self animateEmptyStateIn];
    }
  };
  auto completion = ^(BOOL finished) {
    if (self.items.count > 0) {
      [self.collectionView
          selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                       animated:YES
                 scrollPosition:UICollectionViewScrollPositionNone];
    }
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
      collectionViewUpdatesCompletion:completion];
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  self.selectedItemID = selectedItemID;
  if (!([self updatesCollectionView] && self.showsSelectionUpdates))
    return;
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)replaceItemID:(NSString*)itemID withItem:(GridItem*)item {
  if ([self indexOfItemWithID:itemID] == NSNotFound)
    return;
  // Consistency check: |item|'s ID is either |itemID| or not in |items|.
  DCHECK([item.identifier isEqualToString:itemID] ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);
  NSUInteger index = [self indexOfItemWithID:itemID];
  self.items[index] = item;
  if (![self updatesCollectionView])
    return;
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)]);
  // |cell| may be nil if it is scrolled offscreen.
  if (cell)
    [self configureCell:cell withItem:item];
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
  NSUInteger fromIndex = [self indexOfItemWithID:itemID];
  // If this move would be a no-op, early return and avoid spurious UI updates.
  if (fromIndex == toIndex)
    return;
  auto modelUpdates = ^{
    GridItem* item = self.items[fromIndex];
    [self.items removeObjectAtIndex:fromIndex];
    [self.items insertObject:item atIndex:toIndex];
  };
  auto collectionViewUpdates = ^{
    [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                                 toIndexPath:CreateIndexPath(toIndex)];
  };
  auto completion = ^(BOOL finished) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionNone];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
      collectionViewUpdatesCompletion:completion];
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:self.selectedItemID];
}

#pragma mark - Private

// Performs model updates and view updates together if the view is appeared, or
// only the model updates if the view is not appeared. |completion| is only run
// if view is appeared.
- (void)performModelUpdates:(ProceduralBlock)modelUpdates
              collectionViewUpdates:(ProceduralBlock)collectionViewUpdates
    collectionViewUpdatesCompletion:
        (ProceduralBlockWithBool)collectionViewUpdatesCompletion {
  // If the view isn't visible, there's no need for the collection view to
  // update.
  if (![self updatesCollectionView]) {
    modelUpdates();
    return;
  }
  [self.collectionView performBatchUpdates:^{
    // Synchronize model and view updates.
    modelUpdates();
    collectionViewUpdates();
  }
                                completion:collectionViewUpdatesCompletion];
}

// Returns the index in |self.items| of the first item whose identifier is
// |identifier|.
- (NSUInteger)indexOfItemWithID:(NSString*)identifier {
  auto selectedTest = ^BOOL(GridItem* item, NSUInteger index, BOOL* stop) {
    return [item.identifier isEqualToString:identifier];
  };
  return [self.items indexOfObjectPassingTest:selectedTest];
}

// Configures |cell|'s title synchronously, and favicon and snapshot
// asynchronously with information from |item|. Updates the |cell|'s theme to
// this view controller's theme. This view controller becomes the delegate for
// the cell.
- (void)configureCell:(GridCell*)cell withItem:(GridItem*)item {
  DCHECK(cell);
  DCHECK(item);
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
  NSString* itemIdentifier = item.identifier;
  [self.imageDataSource faviconForIdentifier:itemIdentifier
                                  completion:^(UIImage* icon) {
                                    // Only update the icon if the cell is not
                                    // already reused for another item.
                                    if (cell.itemIdentifier == itemIdentifier)
                                      cell.icon = icon;
                                  }];
  [self.imageDataSource snapshotForIdentifier:itemIdentifier
                                   completion:^(UIImage* snapshot) {
                                     // Only update the icon if the cell is not
                                     // already reused for another item.
                                     if (cell.itemIdentifier == itemIdentifier)
                                       cell.snapshot = snapshot;
                                   }];
}

// Tells the delegate that the user tapped the item with identifier
// corresponding to |indexPath|.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  DCHECK_LT(index, self.items.count);
  NSString* itemID = self.items[index].identifier;
  [self.delegate gridViewController:self didSelectItemWithID:itemID];
}

// Animates the empty state into view.
- (void)animateEmptyStateIn {
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  [self.emptyStateAnimator stopAnimation:YES];
  self.emptyStateAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0 - self.emptyStateView.alpha
          dampingRatio:1.0
            animations:^{
              self.emptyStateView.alpha = 1.0;
              self.emptyStateView.transform = CGAffineTransformIdentity;
            }];
  [self.emptyStateAnimator startAnimation];
}

// Removes the empty state out of view, with animation if |animated| is YES.
- (void)removeEmptyStateAnimated:(BOOL)animated {
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  [self.emptyStateAnimator stopAnimation:YES];
  auto removeEmptyState = ^{
    self.emptyStateView.alpha = 0.0;
    self.emptyStateView.transform = CGAffineTransformScale(
        CGAffineTransformIdentity, /*sx=*/0.9, /*sy=*/0.9);
  };
  if (animated) {
    self.emptyStateAnimator = [[UIViewPropertyAnimator alloc]
        initWithDuration:self.emptyStateView.alpha
            dampingRatio:1.0
              animations:removeEmptyState];
    [self.emptyStateAnimator startAnimation];
  } else {
    removeEmptyState();
  }
}

// Handle the long-press gesture used to reorder cells in the collection view.
- (void)handleItemReorderingWithGesture:(UIGestureRecognizer*)gesture {
  DCHECK(gesture == self.itemReorderRecognizer);
  CGPoint location = [gesture locationInView:self.collectionView];
  switch (gesture.state) {
    case UIGestureRecognizerStateBegan: {
      NSIndexPath* path =
          [self.collectionView indexPathForItemAtPoint:location];
      BOOL moving =
          [self.collectionView beginInteractiveMovementForItemAtIndexPath:path];
      if (!moving) {
        gesture.enabled = NO;
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileTabGridBeganReordering"));
        CGPoint cellCenter =
            [self.collectionView cellForItemAtIndexPath:path].center;
        self.itemReorderTouchPoint =
            CGPointMake(location.x - cellCenter.x, location.y - cellCenter.y);
        // Switch to the reordering layout.
        [self.collectionView setCollectionViewLayout:self.reorderingLayout
                                            animated:YES];
        // Immediately update the position of the moving item; otherwise, the
        // collection view may relayout its subviews and briefly show the moving
        // item at position (0,0).
        [self updateItemReorderingForPosition:location];
      }
      break;
    }
    case UIGestureRecognizerStateChanged:
      // Offset the location so it's the touch point that moves, not the cell
      // center.
      [self updateItemReorderingForPosition:location];
      break;
    case UIGestureRecognizerStateEnded: {
      self.itemReorderTouchPoint = CGPointZero;
      // End the interactive movement and transition the layout back to the
      // defualt layout. These can't be simultaneous, because that will produce
      // a copy of the moving cell in its final position while it animates from
      // under thr user's touch. In order to fire the animated switch to the
      // defualt layout at the correct time, a CATransaction is used to wrap the
      // -endInteractiveMovement call which will generate the animations on the
      // moving cell. The -setCollectionViewLayout: call can then be added as a
      // completion block.
      [CATransaction begin];
      // Note: The completion block must be added *before* any animations are
      // added in the transaction.
      [CATransaction setCompletionBlock:^{
        [self.collectionView setCollectionViewLayout:self.defaultLayout
                                            animated:YES];
      }];
      [self.collectionView endInteractiveMovement];
      [self recordInteractiveReordering];
      [CATransaction commit];
      break;
    }
    case UIGestureRecognizerStateCancelled:
      self.itemReorderTouchPoint = CGPointZero;
      [self.collectionView cancelInteractiveMovement];
      [self recordInteractiveReordering];
      [self.collectionView setCollectionViewLayout:self.defaultLayout
                                          animated:YES];
      // Re-enable cancelled gesture.
      gesture.enabled = YES;
      break;
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateFailed:
      NOTREACHED() << "Unexpected long-press recognizer state";
  }
}

- (void)updateItemReorderingForPosition:(CGPoint)position {
  CGPoint targetLocation =
      CGPointMake(position.x - self.itemReorderTouchPoint.x,
                  position.y - self.itemReorderTouchPoint.y);

  [self.collectionView updateInteractiveMovementTargetPosition:targetLocation];
}

- (void)recordInteractiveReordering {
  if (self.hasChangedOrder) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridReordered"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridEndedWithoutReordering"));
  }
  self.hasChangedOrder = NO;
}

@end
