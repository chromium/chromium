// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_data_source.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_item.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator_items_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+private.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/legacy_grid_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/modals/modals_api.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"

using base::apple::ObjCCast;
using base::apple::ObjCCastStrict;

class ScopedScrollingTimeLogger {
 public:
  ScopedScrollingTimeLogger() : start_(base::TimeTicks::Now()) {}
  ~ScopedScrollingTimeLogger() {
    base::TimeDelta duration = base::TimeTicks::Now() - start_;
    base::UmaHistogramTimes("IOS.TabSwitcher.TimeSpentScrolling", duration);
  }

 private:
  base::TimeTicks start_;
};

// Needed for subclassing.
constexpr int kGridOpenTabsSectionIndex = 0;
NSString* const kGridOpenTabsSectionIdentifier = @"OpenTabsSectionIdentifier";

namespace {
// TODO(crbug.com/1466000): Remove hard-coding of sections.
constexpr int kSuggestedActionsSectionIndex = 1;

NSString* const kSuggestedActionsSectionIdentifier =
    @"SuggestedActionsSectionIdentifier";
NSString* const kCellIdentifier = @"GridCellIdentifier";

// Creates an NSIndexPath with `index` in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}

// Returns the accessibility identifier to set on a GridCell when positioned at
// the given index.
NSString* GridCellAccessibilityIdentifier(NSUInteger index) {
  return [NSString stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, index];
}

}  // namespace

@interface BaseGridViewController () <GridCellDelegate,
                                      SuggestedActionsViewControllerDelegate,
                                      UICollectionViewDragDelegate,
                                      UICollectionViewDropDelegate,
                                      UIPointerInteractionDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The collection view's data source.
@property(nonatomic, strong) GridDiffableDataSource* diffableDataSource;
// The cell registration for grid cells.
@property(nonatomic, strong)
    UICollectionViewCellRegistration* gridCellRegistration;
// The cell registration for the Suggested Actions cell.
@property(nonatomic, strong)
    UICollectionViewCellRegistration* suggestedActionsCellRegistration;
// The supplementary view registration for the grid header.
@property(nonatomic, strong)
    UICollectionViewSupplementaryRegistration* gridHeaderRegistration;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<TabSwitcherItem*>* items;
// Identifier of the selected item. This value is disregarded if `self.items` is
// empty. This bookkeeping is done to set the correct selection on
// `-viewWillAppear:`.
@property(nonatomic, assign) web::WebStateID selectedItemID;
// Index of the selected item in `items`.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// ID of the last item to be inserted. This is used to track if the active tab
// was newly created when building the animation layout for transitions.
@property(nonatomic, assign) web::WebStateID lastInsertedItemID;
// Identifier of the latest dragged item. This property is set when the item is
// long pressed which does not always result in a drag action.
@property(nonatomic, assign) web::WebStateID draggedItemID;
// Animator to show or hide the empty state.
@property(nonatomic, strong) UIViewPropertyAnimator* emptyStateAnimator;
// The layout for the tab grid.
@property(nonatomic, strong) UICollectionViewLayout* gridLayout;
// The view controller that holds the view of the suggested search actions.
@property(nonatomic, strong)
    SuggestedActionsViewController* suggestedActionsViewController;
// Grid cells for which pointer interactions have been added. Pointer
// interactions should only be added to displayed cells (not transition cells).
// This is only expected to get as large as the number of reusable grid cells in
// memory.
@property(nonatomic, strong) NSHashTable<GridCell*>* pointerInteractionCells;
// YES while batch updates and the batch update completion are being performed.
@property(nonatomic) BOOL updating;
// YES while the grid has the suggested actions section.
@property(nonatomic) BOOL showingSuggestedActions;
// YES if the dragged tab moved to a new index.
@property(nonatomic, assign) BOOL dragEndAtNewIndex;
// Tracks if a drop action initiated in this grid is in progress.
@property(nonatomic) BOOL localDragActionInProgress;
// Tracks if the items are in a batch action, which are the "Close All" or
// "Undo" the close all.
@property(nonatomic) BOOL isClosingAllOrUndoRunning;
@end

@implementation BaseGridViewController {
  // Tracks when the grid view is scrolling. Create a new instance to start
  // timing and reset to stop and log the associated time histogram.
  std::optional<ScopedScrollingTimeLogger> _scopedScrollingTimeLogger;
}

- (instancetype)init {
  if (self = [super init]) {
    _items = [[NSMutableArray<TabSwitcherItem*> alloc] init];
    _dropAnimationInProgress = NO;
    _localDragActionInProgress = NO;
    _notSelectedTabCellOpacity = 1.0;
    _mode = TabGridModeNormal;

    // Register for VoiceOver notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverStatusDidChange)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];

    // Register for Dynamic Type notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(preferredContentSizeCategoryDidChange)
               name:UIContentSizeCategoryDidChangeNotification
             object:nil];
  }

  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  if (IsTabGridCompositionalLayoutEnabled()) {
    self.gridLayout = [[GridLayout alloc] init];
  } else {
    self.gridLayout = [[LegacyGridLayout alloc] init];
  }

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:self.gridLayout];
  // During deletion (in horizontal layout) the backgroundView can resize,
  // revealing temporarily the collectionView background. This makes sure
  // both are the same color.
  collectionView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  // If this stays as the default `YES`, then cells aren't highlighted
  // immediately on touch, but after a short delay.
  collectionView.delaysContentTouches = NO;

  [self createRegistrations];

  __weak __typeof(self) weakSelf = self;
  self.diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* innerCollectionView,
                    NSIndexPath* indexPath,
                    GridItemIdentifier* itemIdentifier) {
                  return [weakSelf cellForItemAtIndexPath:indexPath
                                           itemIdentifier:itemIdentifier];
                }];
  self.diffableDataSource.supplementaryViewProvider =
      ^UICollectionReusableView*(UICollectionView* innerCollectionView,
                                 NSString* elementKind,
                                 NSIndexPath* indexPath) {
        return [weakSelf headerForSectionAtIndexPath:indexPath];
      };
  collectionView.dataSource = self.diffableDataSource;

  GridSnapshot* snapshot = [[GridSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kGridOpenTabsSectionIdentifier ]];
  [self.diffableDataSource applySnapshotUsingReloadData:snapshot];

  // UICollectionViewDropPlaceholder uses a GridCell and needs the class to be
  // registered.
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];

  collectionView.delegate = self;
  collectionView.backgroundView = [[UIView alloc] init];
  collectionView.backgroundView.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];
  collectionView.backgroundView.accessibilityIdentifier =
      kGridBackgroundIdentifier;

  // CollectionView, in contrast to TableView, doesn’t inset the
  // cell content to the safe area guide by default. We will just manage the
  // collectionView contentInset manually to fit in the safe area instead.
  collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  self.collectionView = collectionView;
  self.view = collectionView;

  // A single selection collection view's default behavior is to momentarily
  // deselect the selected cell on touch down then select the new cell on touch
  // up. In this tab grid, the selection ring should stay visible on the
  // selected cell on touch down. Multiple selection disables the deselection
  // behavior. Multiple selection will not actually be possible since
  // `-collectionView:shouldSelectItemAtIndexPath:` returns NO.
  collectionView.allowsMultipleSelection = YES;
  collectionView.dragDelegate = self;
  collectionView.dropDelegate = self;
  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];

  self.pointerInteractionCells = [NSHashTable<GridCell*> weakObjectsHashTable];

  [self updateTabsSectionHeaderType];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self contentWillDisappear];
  [super viewWillDisappear:animated];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.collectionView.collectionViewLayout invalidateLayout];
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.collectionView setNeedsLayout];
        [self.collectionView layoutIfNeeded];
      }];
}

#pragma mark - Public

- (BOOL)isScrolledToTop {
  return IsScrollViewScrolledToTop(self.collectionView);
}

- (BOOL)isScrolledToBottom {
  return IsScrollViewScrolledToBottom(self.collectionView);
}

- (void)setEmptyStateView:(UIView<GridEmptyView>*)emptyStateView {
  if (_emptyStateView) {
    [_emptyStateView removeFromSuperview];
  }
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

- (BOOL)isContainedGridEmpty {
  return YES;
}

// Returns the items whose associated cell is visible.
- (NSSet<TabSwitcherItem*>*)visibleGridItems {
  NSArray<NSIndexPath*>* visibleItemsIndexPaths =
      [self.collectionView indexPathsForVisibleItems];
  return [self itemsFromIndexPaths:visibleItemsIndexPaths];
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }

  TabGridMode previousMode = _mode;
  _mode = mode;

  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];
  self.emptyStateView.tabGridMode = _mode;

  if (mode == TabGridModeSearch && self.suggestedActionsDelegate) {
    if (!self.suggestedActionsViewController) {
      self.suggestedActionsViewController =
          [[SuggestedActionsViewController alloc] initWithDelegate:self];
    }
  }
  [self updateTabsSectionHeaderType];
  [self updateSuggestedActionsSection];

  // Reconfigure all tabs.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  [snapshot reconfigureItemsWithIdentifiers:snapshot.itemIdentifiers];
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  [self.gridLayout invalidateLayout];

  NSUInteger selectedIndex = self.selectedIndex;
  if (previousMode != TabGridModeSelection && mode == TabGridModeNormal &&
      selectedIndex != NSNotFound &&
      static_cast<NSInteger>(selectedIndex) <
          [self.collectionView
              numberOfItemsInSection:kGridOpenTabsSectionIndex]) {
    // Scroll to the selected item here, so the action of reloading and
    // scrolling happens at once.
    [self.collectionView
        scrollToItemAtIndexPath:CreateIndexPath(selectedIndex)
               atScrollPosition:UICollectionViewScrollPositionTop
                       animated:NO];
  }

  if (mode == TabGridModeNormal) {
    // After transition from other modes to the normal mode, the selection
    // border doesn't show around the selected item, because reloading
    // operations lose the selected items. The collection view needs to be
    // updated with the selected item again for it to appear correctly.
    [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];

    self.searchText = nil;
  } else if (mode == TabGridModeSelection) {
    // The selected state is not visible in TabGridModeSelection mode, but
    // VoiceOver surfaces it. Deselects all the collection view items.
    // The selection will be reinstated when moving off of TabGridModeSelection.
    NSArray<NSIndexPath*>* indexPathsForSelectedItems =
        [self.collectionView indexPathsForSelectedItems];
    for (NSIndexPath* itemIndexPath in indexPathsForSelectedItems) {
      [self.collectionView deselectItemAtIndexPath:itemIndexPath animated:NO];
    }
  }
}

- (void)setSearchText:(NSString*)searchText {
  _searchText = searchText;
  _suggestedActionsViewController.searchText = searchText;
  [self updateTabsSectionHeaderType];
  [self updateSuggestedActionsSection];
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

- (void)setContentInsets:(UIEdgeInsets)contentInsets {
  if (IsTabGridCompositionalLayoutEnabled()) {
    // Set the vertical insets on the collection view…
    self.collectionView.contentInset =
        UIEdgeInsetsMake(contentInsets.top, 0, contentInsets.bottom, 0);
    // … and the horizontal insets on the layout sections.
    // This is a workaround, as setting the horizontal insets on the collection
    // view isn't honored by the layout when computing the item sizes (items are
    // too big in landscape iPhones with a notch or Dynamic Island).
    ObjCCastStrict<GridLayout>(self.gridLayout).sectionInsets =
        NSDirectionalEdgeInsetsMake(0, contentInsets.left, 0,
                                    contentInsets.right);
  } else {
    self.collectionView.contentInset = contentInsets;
  }
  _contentInsets = contentInsets;
}

- (LegacyGridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<LegacyGridTransitionItem*>* items =
      [[NSMutableArray alloc] init];
  LegacyGridTransitionActiveItem* activeItem;
  LegacyGridTransitionItem* selectionItem;
  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    if (path.section != kGridOpenTabsSectionIndex) {
      continue;
    }
    GridCell* cell = ObjCCastStrict<GridCell>(
        [self.collectionView cellForItemAtIndexPath:path]);
    UICollectionViewLayoutAttributes* attributes =
        [self.collectionView layoutAttributesForItemAtIndexPath:path];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame = [self.collectionView convertRect:attributes.frame
                                                 toView:nil];
    if (cell.itemIdentifier == self.selectedItemID) {
      GridTransitionCell* activeCell =
          [GridTransitionCell transitionCellFromCell:cell];
      activeItem =
          [LegacyGridTransitionActiveItem itemWithCell:activeCell
                                                center:attributes.center
                                                  size:attributes.size];
      // If the active item is the last inserted item, it needs to be animated
      // differently.
      if (cell.itemIdentifier == self.lastInsertedItemID) {
        activeItem.isAppearing = YES;
      }
      selectionItem = [LegacyGridTransitionItem
          itemWithCell:[GridCell transitionSelectionCellFromCell:cell]
                center:attributes.center];
    } else {
      UIView* cellSnapshot = [cell snapshotViewAfterScreenUpdates:YES];
      LegacyGridTransitionItem* item =
          [LegacyGridTransitionItem itemWithCell:cellSnapshot
                                          center:attributes.center];
      [items addObject:item];
    }
  }
  return [LegacyGridTransitionLayout layoutWithInactiveItems:items
                                                  activeItem:activeItem
                                               selectionItem:selectionItem];
}

- (TabGridTransitionItem*)transitionItemForActiveCell {
  [self.collectionView layoutIfNeeded];

  NSIndexPath* selectedItemIndexPath = CreateIndexPath(self.selectedIndex);
  if (![self.collectionView.indexPathsForVisibleItems
          containsObject:selectedItemIndexPath]) {
    return nil;
  }

  GridCell* cell = ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:selectedItemIndexPath]);

  UICollectionViewLayoutAttributes* attributes = [self.collectionView
      layoutAttributesForItemAtIndexPath:selectedItemIndexPath];

  // Removes the cell header height from the orignal frame.
  CGRect attributesFrame = attributes.frame;
  attributesFrame.origin.y += kGridCellHeaderHeight;
  attributesFrame.size.height -= kGridCellHeaderHeight;

  // Normalize frame to window coordinates. The attributes class applies this
  // change to the other properties such as center, bounds, etc.
  CGRect frameInWindow = [self.collectionView convertRect:attributesFrame
                                                   toView:nil];

  return [TabGridTransitionItem itemWithView:cell originalFrame:frameInWindow];
}

- (void)prepareForAppearance {
  for (TabSwitcherItem* item in [self visibleGridItems]) {
    [item prefetchSnapshot];
  }
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  if (IsTabGridCompositionalLayoutEnabled()) {
    ObjCCastStrict<GridLayout>(self.gridLayout).animatesItemUpdates = YES;
  } else {
    ObjCCastStrict<LegacyGridLayout>(self.gridLayout).animatesItemUpdates = YES;
  }
  // Selection is invalid if there are no items.
  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
    return;
  }

  [self updateSelectedCollectionViewItemRingAndBringIntoView:YES];

  // Update the delegate, in case it wasn't set when `items` was populated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self removeEmptyStateAnimated:NO];
  self.lastInsertedItemID = web::WebStateID();
}

- (void)contentDidAppear {
  for (TabSwitcherItem* item in self.items) {
    [item clearPrefetchedSnapshot];
  }
}

- (void)contentWillDisappear {
}

- (void)prepareForDismissal {
  // Stop animating the collection view to prevent the insertion animation from
  // interfering with the tab presentation animation.
  if (IsTabGridCompositionalLayoutEnabled()) {
    ObjCCastStrict<GridLayout>(self.gridLayout).animatesItemUpdates = NO;
  } else {
    ObjCCastStrict<LegacyGridLayout>(self.gridLayout).animatesItemUpdates = NO;
  }
}

#pragma mark - UICollectionView Diffable Data Source Helpers

// Configures the grid header for the given section.
- (void)configureGridHeader:(GridHeader*)gridHeader
       forSectionIdentifier:(NSString*)sectionIdentifier {
  if ([sectionIdentifier isEqualToString:kGridOpenTabsSectionIdentifier]) {
    gridHeader.title = l10n_util::GetNSString(
        IDS_IOS_TABS_SEARCH_OPEN_TABS_SECTION_HEADER_TITLE);
    NSString* resultsCount = [@(self.items.count) stringValue];
    gridHeader.value =
        l10n_util::GetNSStringF(IDS_IOS_TABS_SEARCH_OPEN_TABS_COUNT,
                                base::SysNSStringToUTF16(resultsCount));
  } else if ([sectionIdentifier
                 isEqualToString:kSuggestedActionsSectionIdentifier]) {
    gridHeader.title =
        l10n_util::GetNSString(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTIONS);
  }
}

// Returns a configured cell for the given index path and item identifier.
- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:
                                     (GridItemIdentifier*)itemIdentifier {
  switch (itemIdentifier.type) {
    case GridItemType::Tab: {
      // Handle GridCell-s.
      NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
      // In some cases, this is called with an index path that doesn't match the
      // data source -- see crbug.com/1068136. Presumably this is a race
      // condition where an item has been deleted at the same time as the
      // collection is doing layout (potentially during rotation?). Fudge by
      // returning an unconfigured cell. The assumption is that there will be
      // another – correct – layout shortly after the incorrect one.
      // Keep `items`' bounds valid.
      if (self.items.count == 0 || itemIndex >= self.items.count) {
        // Dequeue using the reuse identifier, not the registration, as there is
        // no valid `item` that could be passed in for the configuration.
        return [self.collectionView
            dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                      forIndexPath:indexPath];
      }
      TabSwitcherItem* item = self.items[itemIndex];
      UICollectionViewCellRegistration* registration =
          self.gridCellRegistration;
      return [self.collectionView
          dequeueConfiguredReusableCellWithRegistration:registration
                                           forIndexPath:indexPath
                                                   item:item];
    }
    case GridItemType::SuggestedActions:
      UICollectionViewCellRegistration* registration =
          self.suggestedActionsCellRegistration;
      return [self.collectionView
          dequeueConfiguredReusableCellWithRegistration:registration
                                           forIndexPath:indexPath
                                                   item:itemIdentifier];
  }
}

#pragma mark - UICollectionViewDelegate

// This method is used instead of -didSelectItemAtIndexPath, because any
// selection events will be signaled through the model layer and handled in
// the TabCollectionConsumer -selectItemWithID: method.
- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  if (@available(iOS 16, *)) {
    // This is handled by
    // `collectionView:performPrimaryActionForItemAtIndexPath:` on iOS 16. The
    // method comment should be updated once iOS 15 is dropped.
    return YES;
  } else {
    [self tappedItemAtIndexPath:indexPath];
  }
  // Tapping on a non-selected cell should not select it immediately. The
  // delegate will trigger a transition to show the item.
  return NO;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  if (@available(iOS 16, *)) {
    // This is handled by
    // `collectionView:performPrimaryActionForItemAtIndexPath:` on iOS 16.
  } else {
    [self tappedItemAtIndexPath:indexPath];
  }
  // Tapping on the current selected cell should not deselect it.
  return NO;
}

- (void)collectionView:(UICollectionView*)collectionView
    performPrimaryActionForItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  // Context menu shouldn't appear in the selection mode.
  if (_mode == TabGridModeSelection) {
    return nil;
  }

  // No context menu on suggested actions section.
  if (indexPath.section == kSuggestedActionsSectionIndex) {
    return nil;
  }

  GridCell* cell = ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);

  MenuScenarioHistogram scenario;
  if (_mode == TabGridModeSearch) {
    scenario = kMenuScenarioHistogramTabGridSearchResult;
  } else if (_mode == TabGridModeInactive) {
    scenario = kMenuScenarioHistogramInactiveTabsEntry;
  } else {
    scenario = kMenuScenarioHistogramTabGridEntry;
  }

  return [self.menuProvider contextMenuConfigurationForTabCell:cell
                                                  menuScenario:scenario];
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[GridCell class]]) {
    // Stop animation of GridCells when removing them from the collection view.
    // This is important to prevent cells from animating indefinitely. This is
    // safe because the animation state of GridCells is set in
    // `configureCell:withItem:atIndex:` whenever a cell is used.
    [ObjCCastStrict<GridCell>(cell) hideActivityIndicator];
  }
}

// TODO(crbug.com/1504112): Remove the entire code section when the
// compositional layout is fully landed.
#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/1504112): Remove the entire method when the compositional
  // layout is fully landed.
  CHECK(!IsTabGridCompositionalLayoutEnabled());
  if (self.isClosingAllOrUndoRunning) {
    return CGSizeZero;
  }
  // `collectionViewLayout` should always be a flow layout.
  UICollectionViewFlowLayout* layout =
      ObjCCastStrict<UICollectionViewFlowLayout>(collectionViewLayout);
  CGSize itemSize = layout.itemSize;
  // The SuggestedActions cell can't use the item size that is set in
  // `prepareLayout` of the layout class. For that specific cell calculate the
  // anticipated size from the layout section insets and the content view insets
  // and return it.
  if (indexPath.section == kSuggestedActionsSectionIndex) {
    UIEdgeInsets sectionInset = layout.sectionInset;
    UIEdgeInsets contentInset = layout.collectionView.adjustedContentInset;
    CGFloat width = layout.collectionView.frame.size.width - sectionInset.left -
                    sectionInset.right - contentInset.left - contentInset.right;
    CGFloat height = self.suggestedActionsViewController.contentHeight;
    return CGSizeMake(width, height);
  }
  return itemSize;
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  // TODO(crbug.com/1504112): Remove the entire method when the compositional
  // layout is fully landed.
  CHECK(!IsTabGridCompositionalLayoutEnabled());
  switch (_mode) {
    case TabGridModeNormal:
      return CGSizeZero;
    case TabGridModeSelection:
      return CGSizeZero;
    case TabGridModeSearch: {
      if (_searchText.length == 0) {
        return CGSizeZero;
      }

      CGFloat height = UIContentSizeCategoryIsAccessibilityCategory(
                           self.traitCollection.preferredContentSizeCategory)
                           ? kGridHeaderAccessibilityHeight
                           : kGridHeaderHeight;
      return CGSizeMake(collectionView.bounds.size.width, height);
    }
    case TabGridModeInactive:
      NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
  }
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  UIPointerLiftEffect* effect = [UIPointerLiftEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:interaction.view]];
  return [UIPointerStyle styleWithEffect:effect shape:nil];
}

#pragma mark - UICollectionViewDragDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dragSessionWillBegin:(id<UIDragSession>)session {
  [self.dragDropHandler dragWillBeginForItemWithID:_draggedItemID];
  self.dragEndAtNewIndex = NO;
  self.localDragActionInProgress = YES;
  base::UmaHistogramEnumeration(kUmaGridViewDragDropTabs,
                                DragDropTabs::kDragBegin);

  [self.delegate gridViewControllerDragSessionWillBegin:self];
}

- (void)collectionView:(UICollectionView*)collectionView
     dragSessionDidEnd:(id<UIDragSession>)session {
  self.localDragActionInProgress = NO;

  DragDropTabs dragEvent = self.dragEndAtNewIndex
                               ? DragDropTabs::kDragEndAtNewIndex
                               : DragDropTabs::kDragEndAtSameIndex;
  // If a drop animation is in progress and the drag didn't end at a new index,
  // that means the item has been dropped outside of its collection view.
  if (_dropAnimationInProgress && !_dragEndAtNewIndex) {
    dragEvent = DragDropTabs::kDragEndInOtherCollection;
  }
  base::UmaHistogramEnumeration(kUmaGridViewDragDropTabs, dragEvent);

  // Used to let the Taptic Engine return to its idle state.
  // To preserve power, the Taptic Engine remains in a prepared state for only a
  // short period of time (on the order of seconds). If for some reason the
  // interactive move / reordering session is not completely finished, the
  // unfinished `UIFeedbackGenerator` may result in a crash.
  [self.collectionView endInteractiveMovement];

  [self.delegate gridViewControllerDragSessionDidEnd:self];
  [self.dragDropHandler dragSessionDidEnd];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  if (self.dragDropHandler == nil) {
    // Don't support dragging items if the drag&drop handler is not set.
    return @[];
  }
  if (_mode == TabGridModeSearch) {
    // TODO(crbug.com/1300369): Enable dragging items from search results.
    return @[];
  }
  if (indexPath.section == kSuggestedActionsSectionIndex) {
    // Return an empty array because ther suggested actions cell should not be
    // dragged.
    return @[];
  }
  if (_mode != TabGridModeSelection) {
    TabSwitcherItem* item = self.items[indexPath.item];
    _draggedItemID = item.identifier;
    return @[ [self.dragDropHandler dragItemForItemWithID:_draggedItemID] ];
  }

  // Make sure that the long pressed cell is selected before initiating a drag
  // from it.
  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  web::WebStateID pressedItemID = self.items[index].identifier;
  [self.mutator addToSelectionItemID:pressedItemID];
  return [self.dragDropHandler allSelectedDragItems];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
            itemsForAddingToDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath
                                  point:(CGPoint)point {
  // TODO(crbug.com/1087848): Allow multi-select.
  // Prevent more items from getting added to the drag session.
  return @[];
}

- (UIDragPreviewParameters*)collectionView:(UICollectionView*)collectionView
    dragPreviewParametersForItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == kSuggestedActionsSectionIndex) {
    // Return nil so that the suggested actions cell doesn't superpose the
    // dragged cell.
    return nil;
  }

  GridCell* gridCell = ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return gridCell.dragPreviewParameters;
}

#pragma mark - UICollectionViewDropDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canHandleDropSession:(id<UIDropSession>)session {
  if (self.dragDropHandler == nil) {
    // Don't support dropping items if the drag&drop handler is not set.
    return NO;
  }
  // Prevent dropping tabs into grid while displaying search results.
  return (_mode != TabGridModeSearch);
}

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  // This is how the explicit forbidden icon or (+) copy icon is shown. Move has
  // no explicit icon.
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
        item.sourceIndexPath ? self.items.count - 1 : self.items.count;
    if (coordinator.destinationIndexPath) {
      destinationIndex =
          base::checked_cast<NSUInteger>(coordinator.destinationIndexPath.item);
    }
    self.dragEndAtNewIndex = YES;

    NSIndexPath* dropIndexPath = CreateIndexPath(destinationIndex);
    // Drop synchronously if local object is available.
    if (item.dragItem.localObject) {
      _dropAnimationInProgress = YES;
      [self.delegate gridViewControllerDropAnimationWillBegin:self];
      __weak __typeof(self) weakSelf = self;
      [[coordinator dropItem:item.dragItem toItemAtIndexPath:dropIndexPath]
          addCompletion:^(UIViewAnimatingPosition finalPosition) {
            [weakSelf.delegate gridViewControllerDropAnimationDidEnd:weakSelf];
            weakSelf.dropAnimationInProgress = NO;
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
      placeholder.cellUpdateHandler = ^(UICollectionViewCell* placeholderCell) {
        GridCell* gridCell = ObjCCastStrict<GridCell>(placeholderCell);
        gridCell.theme = self.theme;
      };
      placeholder.previewParametersProvider =
          ^UIDragPreviewParameters*(UICollectionViewCell* placeholderCell) {
            GridCell* gridCell = ObjCCastStrict<GridCell>(placeholderCell);
            return gridCell.dragPreviewParameters;
          };

      id<UICollectionViewDropPlaceholderContext> context =
          [coordinator dropItem:item.dragItem toPlaceholder:placeholder];
      [self.dragDropHandler dropItemFromProvider:item.dragItem.itemProvider
                                         toIndex:destinationIndex
                              placeholderContext:context];
    }
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    dropSessionDidEnter:(id<UIDropSession>)session {
  if (IsPinnedTabsEnabled()) {
    // Notify the delegate that a drag cames from another app.
    [self.delegate gridViewControllerDragSessionWillBegin:self];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
     dropSessionDidEnd:(id<UIDropSession>)session {
  if (IsPinnedTabsEnabled()) {
    // Notify the delegate that a drag ends from another app.
    [self.delegate gridViewControllerDropAnimationDidEnd:self];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.delegate gridViewControllerScrollViewDidScroll:self];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.delegate gridViewControllerWillBeginDragging:self];
  base::RecordAction(base::UserMetricsAction("MobileTabGridUserScrolled"));
  _scopedScrollingTimeLogger = ScopedScrollingTimeLogger();
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  if (!decelerate) {
    _scopedScrollingTimeLogger.reset();
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  _scopedScrollingTimeLogger.reset();
}

- (void)scrollViewDidScrollToTop:(UIScrollView*)scrollView {
  base::RecordAction(base::UserMetricsAction("MobileTabGridUserScrolledToTop"));
}

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  self.emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

#pragma mark - GridCellDelegate

- (void)closeButtonTappedForCell:(GridCell*)cell {
  [self.delegate gridViewController:self
                 didCloseItemWithID:cell.itemIdentifier];
  // Record when a tab is closed via the X.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseControlTapped"));
  if (_mode == TabGridModeSearch) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCloseControlTappedDuringSearch"));
  }
}

#pragma mark - SuggestedActionsViewControllerDelegate

- (void)suggestedActionsViewController:
            (SuggestedActionsViewController*)viewController
    fetchHistoryResultsCountWithCompletion:(void (^)(size_t))completion {
  [self.suggestedActionsDelegate
      fetchSearchHistoryResultsCountForText:self.searchText
                                 completion:completion];
}

- (void)didSelectSearchHistoryInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController {
  base::RecordAction(
      base::UserMetricsAction("TabsSearch.SuggestedActions.SearchHistory"));
  [self.suggestedActionsDelegate searchHistoryForText:self.searchText];
}

- (void)didSelectSearchRecentTabsInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController {
  base::RecordAction(
      base::UserMetricsAction("TabsSearch.SuggestedActions.RecentTabs"));
  [self.suggestedActionsDelegate searchRecentTabsForText:self.searchText];
}

- (void)didSelectSearchWebInSuggestedActionsViewController:
    (SuggestedActionsViewController*)viewController {
  base::RecordAction(
      base::UserMetricsAction("TabsSearch.SuggestedActions.SearchOnWeb"));
  [self.suggestedActionsDelegate searchWebForText:self.searchText];
}

#pragma mark - TabCollectionConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(web::WebStateID)selectedItemID {
  CHECK(!HasDuplicateIdentifiers(items));
  // Call self.view to ensure that the collection view is created.
  [self view];
  CHECK(self.diffableDataSource);

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;

  GridSnapshot* snapshot = [[GridSnapshot alloc] init];

  // Open Tabs section.
  [snapshot appendSectionsWithIdentifiers:@[ kGridOpenTabsSectionIdentifier ]];
  NSMutableArray<GridItemIdentifier*>* itemIdentifiers =
      [NSMutableArray arrayWithCapacity:items.count];
  for (TabSwitcherItem* item in items) {
    [itemIdentifiers addObject:[GridItemIdentifier tabIdentifier:item]];
  }
  [snapshot appendItemsWithIdentifiers:itemIdentifiers];

  // Optional Suggested Actions section.
  if (self.showingSuggestedActions) {
    [snapshot
        appendSectionsWithIdentifiers:@[ kSuggestedActionsSectionIdentifier ]];
    GridItemIdentifier* itemIdentifier =
        [GridItemIdentifier suggestedActionsIdentifier];
    [snapshot appendItemsWithIdentifiers:@[ itemIdentifier ]];
  }

  [self.diffableDataSource applySnapshotUsingReloadData:snapshot];

  [self updateSelectedCollectionViewItemRingAndBringIntoView:YES];

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }
  // Whether the view is visible or not, the delegate must be updated.
  [self.delegate gridViewController:self
                 didChangeItemCount:self.items.count];
  if (_mode == TabGridModeSearch) {
    if (_searchText.length) {
      [self updateSearchResultsHeader];
    }
    [self.collectionView
        setContentOffset:CGPointMake(
                             -self.collectionView.adjustedContentInset.left,
                             -self.collectionView.adjustedContentInset.top)
                animated:NO];
  }
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(web::WebStateID)selectedItemID {
  if (_mode == TabGridModeSearch) {
    // Prevent inserting items while viewing search results.
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(GridSnapshot* snapshot) {
        [weakSelf applyModelAndViewUpdatesForInsertionOfItem:item
                                                     atIndex:index
                                              selectedItemID:selectedItemID
                                                    snapshot:snapshot];
      }
      completion:^{
        [weakSelf modelAndViewUpdatesForInsertionDidCompleteAtIndex:index];
      }];
}

- (void)removeItemWithID:(web::WebStateID)removedItemID
          selectedItemID:(web::WebStateID)selectedItemID {
  NSUInteger index = [self indexOfItemWithID:removedItemID];

  // Do not remove if not showing the item (i.e. showing search results).
  if (index == NSNotFound) {
    [self selectItemWithID:selectedItemID];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(GridSnapshot* snapshot) {
        [weakSelf applyModelAndViewUpdatesForRemovalOfItemWithID:removedItemID
                                                  selectedItemID:selectedItemID
                                                        snapshot:snapshot];
      }
      completion:^{
        [weakSelf modelAndViewUpdatesForRemovalDidCompleteForItemWithID:
                      removedItemID];
      }];

  if (_searchText.length) {
    [self updateSearchResultsHeader];
  }
}

- (void)selectItemWithID:(web::WebStateID)selectedItemID {
  if (self.selectedItemID == selectedItemID) {
    return;
  }

  self.selectedItemID = selectedItemID;
  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
}

- (void)replaceItemID:(web::WebStateID)existingItemID
             withItem:(TabSwitcherItem*)newItem {
  NSUInteger index = [self indexOfItemWithID:existingItemID];
  if (index == NSNotFound) {
    return;
  }
  // Consistency check: `newItem`'s ID is either `existingItemID` or not in
  // `self.items`.
  CHECK(newItem.identifier == existingItemID ||
        [self indexOfItemWithID:newItem.identifier] == NSNotFound);
  TabSwitcherItem* existingItem = self.items[index];
  self.items[index] = newItem;

  GridItemIdentifier* existingItemIdentifier =
      [GridItemIdentifier tabIdentifier:existingItem];

  if (![self.diffableDataSource
          indexPathForItemIdentifier:existingItemIdentifier]) {
    // It is possible that the item is still/already in self.items but no
    // longer/yet in the data source. No repro steps were found. In that case,
    // ignore the update. See crbug.com/1503139.
    return;
  }

  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  if (existingItemID == newItem.identifier) {
    [snapshot reconfigureItemsWithIdentifiers:@[ existingItemIdentifier ]];
  } else {
    // Add the new item before the existing item.
    GridItemIdentifier* newItemIdentifier =
        [GridItemIdentifier tabIdentifier:newItem];
    [snapshot insertItemsWithIdentifiers:@[ newItemIdentifier ]
                beforeItemWithIdentifier:existingItemIdentifier];
    [snapshot deleteItemsWithIdentifiers:@[ existingItemIdentifier ]];
  }
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)moveItemWithID:(web::WebStateID)itemID toIndex:(NSUInteger)toIndex {
  if (_mode == TabGridModeSearch) {
    // Prevent moving items while viewing search results.
    return;
  }

  NSUInteger fromIndex = [self indexOfItemWithID:itemID];
  // If this move would be a no-op, early return and avoid spurious UI updates.
  if (fromIndex == toIndex || toIndex == NSNotFound ||
      fromIndex == NSNotFound) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(GridSnapshot* snapshot) {
        [weakSelf applyModelAndViewUpdatesForMoveOfItemWithID:itemID
                                                    fromIndex:fromIndex
                                                      toIndex:toIndex
                                                     snapshot:snapshot];
      }
      completion:^{
        [weakSelf modelAndViewUpdatesForMoveDidCompleteForItemWithID:itemID
                                                             toIndex:toIndex];
      }];
}

- (void)dismissModals {
  ios::provider::DismissModalsForCollectionView(self.collectionView);
}

- (void)reload {
  [self.collectionView reloadData];
}

- (void)willCloseAll {
  self.isClosingAllOrUndoRunning = YES;
}

- (void)didCloseAll {
  self.isClosingAllOrUndoRunning = NO;
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)willUndoCloseAll {
  self.isClosingAllOrUndoRunning = YES;
}

- (void)didUndoCloseAll {
  self.isClosingAllOrUndoRunning = NO;
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - Actions

// Called when the Inactive Tabs button is tapped.
- (void)didTapInactiveTabsButton {
  [self.delegate didTapInactiveTabsButtonInGridViewController:self];
}

// Called when the Inactive Tabs settings link is tapped.
- (void)didTapInactiveTabsSettingsLink {
  [self.delegate didTapInactiveTabsSettingsLinkInGridViewController:self];
}

#pragma mark - Suggested Actions Section

- (void)updateSuggestedActionsSection {
  if (!self.suggestedActionsDelegate) {
    return;
  }

  // In search mode if there is already a search query, and the suggested
  // actions section is not yet added, add it. Otherwise remove the section if
  // it exists and the search mode is not active.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  if (self.mode == TabGridModeSearch && self.searchText.length) {
    if (!self.showingSuggestedActions) {
      [snapshot appendSectionsWithIdentifiers:@[
        kSuggestedActionsSectionIdentifier
      ]];
      GridItemIdentifier* itemIdentifier =
          [GridItemIdentifier suggestedActionsIdentifier];
      [snapshot appendItemsWithIdentifiers:@[ itemIdentifier ]];
      self.showingSuggestedActions = YES;
    }
  } else {
    if (self.showingSuggestedActions) {
      [snapshot deleteSectionsWithIdentifiers:@[
        kSuggestedActionsSectionIdentifier
      ]];
      self.showingSuggestedActions = NO;
    }
  }
  [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - Private helpers for joint model and view updates

// Performs model and view updates together.
- (void)performModelAndViewUpdates:
            (void (^)(GridSnapshot* snapshot))modelAndViewUpdates
                        completion:(ProceduralBlock)completion {
  self.updating = YES;
  // Synchronize model and diffable snapshot updates.
  GridSnapshot* snapshot = self.diffableDataSource.snapshot;
  modelAndViewUpdates(snapshot);
  __weak __typeof(self) weakSelf = self;
  [self.diffableDataSource applySnapshot:snapshot
                    animatingDifferences:YES
                              completion:^{
                                if (weakSelf) {
                                  completion();
                                  weakSelf.updating = NO;
                                }
                              }];

  [self updateVisibleCellIdentifiers];
}

// Makes the required changes to `items` and `collectionView` when a new item is
// inserted.
- (void)applyModelAndViewUpdatesForInsertionOfItem:(TabSwitcherItem*)item
                                           atIndex:(NSUInteger)index
                                    selectedItemID:
                                        (web::WebStateID)selectedItemID
                                          snapshot:(GridSnapshot*)snapshot {
  // Consistency check: `item`'s ID is not in `items`.
  // (using DCHECK rather than DCHECK_EQ to avoid a checked_cast on NSNotFound).
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);

  // Store the identifier of the current item at the given index, if any, prior
  // to model updates.
  TabSwitcherItem* previousItemAtIndex;
  if (index < self.items.count) {
    previousItemAtIndex = self.items[index];
  }

  [self.items insertObject:item atIndex:index];
  self.selectedItemID = selectedItemID;
  self.lastInsertedItemID = item.identifier;
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];

  [self removeEmptyStateAnimated:YES];

  // TODO(crbug.com/1473625): There are crash reports that show there could be
  // cases where the open tabs section is not present in the snapshot. If so,
  // don't perform the update.
  if ([snapshot indexOfSectionIdentifier:kGridOpenTabsSectionIdentifier] ==
      NSNotFound) {
    return;
  }
  // The snapshot API doesn't provide a way to insert at a given index (that's
  // its purpose actually), only before/after an existing item, or by
  // appending to an existing section.
  // If the new item is taking the spot of an existing item, insert the new
  // one before it. Otherwise (if the section is empty, or the new index is
  // the new last position), append at the end of the section.
  GridItemIdentifier* itemIdentifier = [GridItemIdentifier tabIdentifier:item];
  if (previousItemAtIndex) {
    GridItemIdentifier* previousItemIdentifier =
        [GridItemIdentifier tabIdentifier:previousItemAtIndex];
    [snapshot insertItemsWithIdentifiers:@[ itemIdentifier ]
                beforeItemWithIdentifier:previousItemIdentifier];
  } else {
    [snapshot appendItemsWithIdentifiers:@[ itemIdentifier ]
               intoSectionWithIdentifier:kGridOpenTabsSectionIdentifier];
  }
}

// Makes the required changes when a new item has been inserted.
- (void)modelAndViewUpdatesForInsertionDidCompleteAtIndex:(NSUInteger)index {
  [self updateSelectedCollectionViewItemRingAndBringIntoView:YES];

  [self.delegate gridViewController:self didChangeItemCount:self.items.count];

  // Check `index` boundaries in order to filter out possible race conditions
  // while mutating the collection.
  if (index == NSNotFound || index >= self.items.count ||
      static_cast<NSInteger>(index) >=
          [self.collectionView
              numberOfItemsInSection:kGridOpenTabsSectionIndex]) {
    return;
  }

  [self.collectionView
      scrollToItemAtIndexPath:CreateIndexPath(index)
             atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                     animated:YES];
}

// Makes the required changes to `items` and `collectionView` when an existing
// item is removed.
- (void)applyModelAndViewUpdatesForRemovalOfItemWithID:
            (web::WebStateID)removedItemID
                                        selectedItemID:
                                            (web::WebStateID)selectedItemID
                                              snapshot:(GridSnapshot*)snapshot {
  NSUInteger index = [self indexOfItemWithID:removedItemID];
  TabSwitcherItem* removedItem = self.items[index];
  [self.items removeObjectAtIndex:index];
  self.selectedItemID = selectedItemID;
  [self.mutator removeFromSelectionItemID:removedItemID];
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];

  GridItemIdentifier* removedItemIdentifier =
      [GridItemIdentifier tabIdentifier:removedItem];
  [snapshot deleteItemsWithIdentifiers:@[ removedItemIdentifier ]];
  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  }
}

// Makes the required changes when a new item has been removed.
- (void)modelAndViewUpdatesForRemovalDidCompleteForItemWithID:
    (web::WebStateID)removedItemID {
  if (self.items.count > 0) {
    [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
  }
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self.delegate gridViewController:self didRemoveItemWIthID:removedItemID];
}

// Makes the required changes to `items` and `collectionView` when an existing
// item is moved.
- (void)applyModelAndViewUpdatesForMoveOfItemWithID:(web::WebStateID)itemID
                                          fromIndex:(NSUInteger)fromIndex
                                            toIndex:(NSUInteger)toIndex
                                           snapshot:(GridSnapshot*)snapshot {
  TabSwitcherItem* toItem = self.items[toIndex];
  TabSwitcherItem* item = self.items[fromIndex];
  [self.items removeObjectAtIndex:fromIndex];
  [self.items insertObject:item atIndex:toIndex];

  GridItemIdentifier* itemIdentifier = [GridItemIdentifier tabIdentifier:item];
  GridItemIdentifier* toItemIdentifier =
      [GridItemIdentifier tabIdentifier:toItem];
  if (fromIndex < toIndex) {
    [snapshot moveItemWithIdentifier:itemIdentifier
             afterItemWithIdentifier:toItemIdentifier];
  } else {
    [snapshot moveItemWithIdentifier:itemIdentifier
            beforeItemWithIdentifier:toItemIdentifier];
  }
}

// Makes the required changes when a new item has been moved.
- (void)modelAndViewUpdatesForMoveDidCompleteForItemWithID:
            (web::WebStateID)itemID
                                                   toIndex:(NSUInteger)toIndex {
  [self.delegate gridViewController:self
                  didMoveItemWithID:itemID
                            toIndex:toIndex];
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:self.selectedItemID];
}

#pragma mark - Protected

- (void)createRegistrations {
  __weak __typeof(self) weakSelf = self;

  // Register GridCell.
  auto configureGridCell =
      ^(GridCell* cell, NSIndexPath* indexPath, TabSwitcherItem* item) {
        [weakSelf configureCell:cell withItem:item atIndex:indexPath.item];
      };
  self.gridCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[GridCell class]
           configurationHandler:configureGridCell];

  // Register SuggestedActionsGridCell.
  auto configureSuggestedActionsCell =
      ^(SuggestedActionsGridCell* suggestedActionsCell, NSIndexPath* indexPath,
        id item) {
        CHECK(weakSelf.suggestedActionsViewController);
        suggestedActionsCell.suggestedActionsView =
            weakSelf.suggestedActionsViewController.view;
      };
  self.suggestedActionsCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[SuggestedActionsGridCell class]
           configurationHandler:configureSuggestedActionsCell];

  // Register GridHeader.
  auto configureGridHeader =
      ^(GridHeader* gridHeader, NSString* elementKind, NSIndexPath* indexPath) {
        NSString* sectionIdentifier = [weakSelf.diffableDataSource
            sectionIdentifierForIndex:indexPath.section];
        [weakSelf configureGridHeader:gridHeader
                 forSectionIdentifier:sectionIdentifier];
      };
  self.gridHeaderRegistration = [UICollectionViewSupplementaryRegistration
      registrationWithSupplementaryClass:[GridHeader class]
                             elementKind:UICollectionElementKindSectionHeader
                    configurationHandler:configureGridHeader];
}

- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionViewSupplementaryRegistration* registration;
  switch (_mode) {
    case TabGridModeNormal:
    case TabGridModeInactive:
      NOTREACHED_NORETURN() << "Should be implemented in a subclass.";
    case TabGridModeSelection:
      NOTREACHED();
      break;
    case TabGridModeSearch:
      registration = self.gridHeaderRegistration;
      break;
  }
  return [self.collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:registration
                                                    forIndexPath:indexPath];
}

- (void)updateSelectedCollectionViewItemRingAndBringIntoView:
    (BOOL)shouldBringItemIntoView {
  // Deselects all the collection view items.
  NSArray<NSIndexPath*>* indexPathsForSelectedItems =
      [self.collectionView indexPathsForSelectedItems];
  for (NSIndexPath* itemIndexPath in indexPathsForSelectedItems) {
    [self.collectionView deselectItemAtIndexPath:itemIndexPath animated:NO];
  }

  // Select the collection view item for the selected index.
  NSInteger selectedIndex = self.selectedIndex;
  CHECK(selectedIndex >= 0);
  // Check `selectedIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (selectedIndex == NSNotFound ||
      selectedIndex >= static_cast<NSInteger>(self.items.count) ||
      selectedIndex >= [self.collectionView
                           numberOfItemsInSection:kGridOpenTabsSectionIndex]) {
    return;
  }
  NSIndexPath* selectedIndexPath = CreateIndexPath(selectedIndex);
  UICollectionViewScrollPosition scrollPosition =
      shouldBringItemIntoView ? UICollectionViewScrollPositionTop
                              : UICollectionViewScrollPositionNone;
  [self.collectionView selectItemAtIndexPath:selectedIndexPath
                                    animated:NO
                              scrollPosition:scrollPosition];
}

- (void)updateTabsSectionHeaderType {
  if (IsTabGridCompositionalLayoutEnabled()) {
    ObjCCastStrict<GridLayout>(self.gridLayout).tabsSectionHeaderType =
        [self tabsSectionHeaderTypeForMode:_mode];
  }
}

- (TabsSectionHeaderType)tabsSectionHeaderTypeForMode:(TabGridMode)mode {
  CHECK(IsTabGridCompositionalLayoutEnabled());
  switch (mode) {
    case TabGridModeNormal:
    case TabGridModeSelection:
      return TabsSectionHeaderType::kNone;
    case TabGridModeSearch:
      if (_searchText.length == 0) {
        // The Normal mode grid shows behind the search overlay, so use the same
        // header as normal mode. Subclasses can have changed it from
        // TabsSectionHeaderType::kNone, so call this method again with
        // TabGridModeNormal.
        return [self tabsSectionHeaderTypeForMode:TabGridModeNormal];
      }
      return TabsSectionHeaderType::kSearch;
    case TabGridModeInactive:
      if (!IsInactiveTabsEnabled()) {
        return TabsSectionHeaderType::kNone;
      }
      return TabsSectionHeaderType::kInactiveTabs;
  }
}

#pragma mark - Private

- (void)voiceOverStatusDidChange {
  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];
}

- (void)preferredContentSizeCategoryDidChange {
  [self.collectionView.collectionViewLayout invalidateLayout];
}

// Returns YES if drag and drop is enabled.
// TODO(crbug.com/1300369): Enable dragging items from search results.
- (BOOL)shouldEnableDrapAndDropInteraction {
  // Don't enable drag and drop when voice over is enabled.
  return !UIAccessibilityIsVoiceOverRunning()
         // Dragging multiple tabs to reorder them is not supported. So there is
         // no need to enable dragging when multiple items are selected in
         // devices that don't support multiple windows.
         && ((self.mode == TabGridModeSelection &&
              base::ios::IsMultipleScenesSupported()) ||
             self.mode == TabGridModeNormal);
}

// Returns the index in `self.items` of the first item whose identifier is
// `identifier`.
- (NSUInteger)indexOfItemWithID:(web::WebStateID)identifier {
  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return item.identifier == identifier;
      };
  return [self.items indexOfObjectPassingTest:selectedTest];
}

// Configures `cell`'s identifier and title synchronously, and favicon and
// snapshot asynchronously with information from `item`. Updates the `cell`'s
// theme to this view controller's theme. This view controller becomes the
// delegate for the cell.
- (void)configureCell:(GridCell*)cell
             withItem:(TabSwitcherItem*)item
              atIndex:(NSUInteger)index {
  CHECK(cell);
  CHECK(item);
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
  cell.accessibilityIdentifier = GridCellAccessibilityIdentifier(index);
  if (self.mode == TabGridModeSelection) {
    if ([self.gridProvider isItemSelected:item.identifier]) {
      cell.state = GridCellStateEditingSelected;
    } else {
      cell.state = GridCellStateEditingUnselected;
    }
  } else {
    cell.state = GridCellStateNotEditing;
  }
  [item fetchFavicon:^(TabSwitcherItem* innerItem, UIImage* icon) {
    // Only update the icon if the cell is not already reused for another item.
    if (cell.itemIdentifier == innerItem.identifier) {
      cell.icon = icon;
    }
  }];

  [item fetchSnapshot:^(TabSwitcherItem* innerItem, UIImage* snapshot) {
    // Only update the icon if the cell is not already reused for another item.
    if (cell.itemIdentifier == innerItem.identifier) {
      cell.snapshot = snapshot;
    }
  }];

  web::WebStateID itemID = item.identifier;
  [self.priceCardDataSource
      priceCardForIdentifier:itemID
                  completion:^(PriceCardItem* priceCardItem) {
                    if (priceCardItem && cell.itemIdentifier == itemID) {
                      [cell setPriceDrop:priceCardItem.price
                           previousPrice:priceCardItem.previousPrice];
                    }
                  }];
  cell.opacity = 1.0f;
  if (item.showsActivity) {
    [cell showActivityIndicator];
  } else {
    [cell hideActivityIndicator];
  }
  if (![self.pointerInteractionCells containsObject:cell]) {
    [cell addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
    // `self.pointerInteractionCells` is only expected to get as large as the
    // number of reusable grid cells in memory.
    [self.pointerInteractionCells addObject:cell];
  }
}

// Tells the delegate that the user tapped the item with identifier
// corresponding to `indexPath`.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  // Speculative fix for crbug.com/1134663, where this method is called while
  // updates from a tab insertion are processing.
  // *** Do not add any code before this check. ***
  if (self.updating) {
    return;
  }

  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  DCHECK_LT(index, self.items.count);

  // crbug.com/1163238: In the wild, the above DCHECK condition is hit. This
  // might be a case of the UI sending touch events after the model has updated.
  // In this case, just no-op; if the user has to tap again to activate a tab,
  // that's better than a crash.
  if (index >= self.items.count) {
    return;
  }
  web::WebStateID itemID = self.items[index].identifier;
  [self.mutator userTappedOnItemID:itemID];
  if (_mode == TabGridModeSelection) {
    // Reconfigure the item.
    GridSnapshot* snapshot = self.diffableDataSource.snapshot;
    GridItemIdentifier* itemIdentifier =
        [GridItemIdentifier tabIdentifier:self.items[index]];
    [snapshot reconfigureItemsWithIdentifiers:@[ itemIdentifier ]];
    [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  }

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

// Removes the empty state out of view, with animation if `animated` is YES.
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

// Update visible cells identifier, following a reorg of cells.
- (void)updateVisibleCellIdentifiers {
  for (NSIndexPath* indexPath in self.collectionView
           .indexPathsForVisibleItems) {
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    if (![cell isKindOfClass:[GridCell class]]) {
      continue;
    }
    NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
    cell.accessibilityIdentifier = GridCellAccessibilityIdentifier(itemIndex);
  }
}

- (BOOL)shouldShowEmptyState {
  if (self.showingSuggestedActions) {
    return NO;
  }
  return self.items.count == 0;
}

// Updates the number of results found on the search open tabs section header.
- (void)updateSearchResultsHeader {
  GridHeader* headerView = (GridHeader*)[self.collectionView
      supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                          atIndexPath:
                              [NSIndexPath
                                  indexPathForRow:0
                                        inSection:kGridOpenTabsSectionIndex]];
  if (!headerView) {
    return;
  }
  NSString* resultsCount = [@(self.items.count) stringValue];
  headerView.value =
      l10n_util::GetNSStringF(IDS_IOS_TABS_SEARCH_OPEN_TABS_COUNT,
                              base::SysNSStringToUTF16(resultsCount));
}

// Returns the items at the given index paths.
- (NSSet<TabSwitcherItem*>*)itemsFromIndexPaths:
    (NSArray<NSIndexPath*>*)indexPaths {
  NSMutableSet<TabSwitcherItem*>* items = [[NSMutableSet alloc] init];

  [indexPaths enumerateObjectsUsingBlock:^(NSIndexPath* indexPath,
                                           NSUInteger index, BOOL* stop) {
    NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
    if (itemIndex < self.items.count) {
      [items addObject:self.items[itemIndex]];
    }
  }];

  return items;
}

@end
