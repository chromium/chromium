// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of sections for the pinned collection view.
NSInteger kNumberOfSectionsInPinnedCollection = 1;

}  // namespace

@interface PinnedTabsViewController () <UICollectionViewDragDelegate,
                                        UICollectionViewDropDelegate>
@end

@implementation PinnedTabsViewController {
  // The local model backing the collection view.
  NSMutableArray<TabSwitcherItem*>* _items;

  // Constraints used to update the view during drag and drop actions.
  NSLayoutConstraint* _dragEnabledConstraint;
  NSLayoutConstraint* _defaultConstraint;

  // Background color of the view.
  UIColor* _backgroundColor;

  // Tracks if the view is available.
  BOOL _available;

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
  _isDragActionInProgress = NO;

  [self configureCollectionView];
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
  // The view is available if  `_items` is not empty or if a drag action is in
  // progress.
  available = available && (_items.count || _isDragActionInProgress);
  if (available == _available)
    return;

  // Show the view if `available` is true to ensure smooth animation.
  if (available) {
    self.view.hidden = NO;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kPinnedViewFadeInTime
      animations:^{
        self.view.alpha = available ? 1.0 : 0.0;
      }
      completion:^(BOOL finished) {
        [weakSelf updatePinnedTabsAvailabilityAfterAnimation:available];
      }];
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
  if (itemIndex >= _items.count)
    itemIndex = _items.count - 1;

  TabSwitcherItem* item = _items[itemIndex];
  PinnedCell* cell = base::mac::ObjCCastStrict<PinnedCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kPinnedCellIdentifier
                                forIndexPath:indexPath]);

  [self configureCell:cell withItem:item];
  return cell;
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
  [self populateFakeItems];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canHandleDropSession:(id<UIDropSession>)session {
  return _available;
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
                                      if ([cell hasIdentifier:itemIdentifier])
                                        cell.faviconView.image = icon;
                                    }];
  }
}

// Updates the pinned tabs view availability after an animation.
- (void)updatePinnedTabsAvailabilityAfterAnimation:(BOOL)available {
  _available = available;
  if (!_available) {
    self.view.hidden = YES;
  }
}

// Adds fake items to the collection view.
// TODO(crbug.com/1382015): Remove this when `_items` are correctly populated.
- (void)populateFakeItems {
  DCHECK(IsPinnedTabsEnabled());
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < 5; i++) {
    TabSwitcherItem* item = [[TabSwitcherItem alloc]
        initWithIdentifier:[NSString stringWithFormat:@"item%d", i]];
    item.title = @"The New York Times - Breaking News";
    [items addObject:item];
  }

  _items = [items mutableCopy];
  [self.collectionView reloadData];
}

@end
