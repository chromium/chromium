// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"

#import "base/dcheck_is_on.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/features.h"
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
  BOOL _isDragActionInProgress;
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
  _isDragActionInProgress = NO;

  [self configureCollectionView];
  [self configureEmptyCollectionViewLabel];
}

#pragma mark - Public

- (void)dragSessionEnabled:(BOOL)enabled {
  _isDragActionInProgress = enabled;

  [UIView animateWithDuration:kPinnedViewDragAnimationTime
                   animations:^{
                     self->_dragEnabledConstraint.active = enabled;
                     self->_defaultConstraint.active = !enabled;
                     [self.view.superview layoutIfNeeded];
                   }
                   completion:nil];

  self.collectionView.backgroundColor = _backgroundColor;
  self.collectionView.backgroundView.hidden = NO;
}

- (void)pinnedTabsAvailable:(BOOL)available {
  _available = available;

  // The view is visible if `_items` is not empty or if a drag action is in
  // progress.
  bool visible = _available && (_items.count || _isDragActionInProgress);
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

  ProceduralBlock modelUpdates = ^{
    [self->_items insertObject:item atIndex:index];
    self->_selectedItemID = selectedItemID;
  };

  ProceduralBlock collectionViewUpdates = ^{
    [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
    [self updateEmptyCollectionViewLabelVisibility];
  };

  __weak __typeof(self) weakSelf = self;
  NSString* previousItemID = _selectedItemID;
  ProceduralBlock collectionViewUpdatesCompletion = ^{
    [weakSelf updateCollectionViewAfterItemInsertionWithPreviousItemID:
                  previousItemID];
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

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  NSUInteger index = [self indexOfItemWithID:removedItemID];
  if (index == NSNotFound) {
    return;
  }

  ProceduralBlock modelUpdates = ^{
    [self->_items removeObjectAtIndex:index];
    self->_selectedItemID = selectedItemID;
  };

  ProceduralBlock collectionViewUpdates = ^{
    [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
    [self updateEmptyCollectionViewLabelVisibility];
  };

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock collectionViewUpdatesCompletion = ^{
    [weakSelf updateCollectionViewAfterItemDeletion];
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
  if (fromIndex == toIndex || fromIndex == NSNotFound) {
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
  [self dragSessionEnabled:YES];
}

- (void)collectionView:(UICollectionView*)collectionView
     dragSessionDidEnd:(id<UIDragSession>)session {
  [self dragSessionEnabled:NO];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/1382015): Implement this.
  return @[ [[UIDragItem alloc]
      initWithItemProvider:[[NSItemProvider alloc] initWithObject:@""]] ];
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
  self.collectionView.backgroundColor = _backgroundColor;
  self.collectionView.backgroundView.hidden = NO;
}

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  return [[UICollectionViewDropProposal alloc]
      initWithDropOperation:UIDropOperationMove
                     intent:
                         UICollectionViewDropIntentInsertAtDestinationIndexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    performDropWithCoordinator:
        (id<UICollectionViewDropCoordinator>)coordinator {
  // TODO(crbug.com/1382015): Implement this.
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
// information from `item`. Updates the `cell`'s theme to this view controller's
// theme.
- (void)configureCell:(PinnedCell*)cell withItem:(TabSwitcherItem*)item {
  if (item) {
    cell.itemIdentifier = item.identifier;
    cell.titleLabel.text = item.title;
    NSString* itemIdentifier = item.identifier;
    [self.imageDataSource faviconForIdentifier:itemIdentifier
                                    completion:^(UIImage* icon) {
                                      // Only update the icon if the cell is not
                                      // already reused for another item.
                                      if ([cell hasIdentifier:itemIdentifier]) {
                                        cell.faviconView.image = icon;
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
  // TODO(crbug.com/1382015): Implement selected halo.
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
  [self.delegate didSelectItemWithID:itemID];
}

@end
