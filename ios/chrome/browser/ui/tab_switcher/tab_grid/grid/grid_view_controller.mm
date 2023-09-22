// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"

#import <algorithm>
#import <memory>

#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_data_source.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_item.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_shareable_items_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller+private.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_ui_swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_preamble_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/modals/modals_api.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/l10n/l10n_util.h"

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

namespace {

// TODO(crbug.com/1466000): Remove hard-coding of sections.
constexpr int kOpenTabsSectionIndex = 0;
constexpr int kSuggestedActionsSectionIndex = 1;

NSString* const kOpenTabsSectionIdentifier = @"OpenTabsSectionIdentifier";
NSString* const kSuggestedActionsSectionIdentifier =
    @"SuggestedActionsSectionIdentifier";
NSString* const kCellIdentifier = @"GridCellIdentifier";
NSString* const kSuggestedActionsCellIdentifier =
    @"SuggestedActionsCellIdentifier";
NSString* const kGridHeaderIdentifier = @"GridHeaderIdentifier";
NSString* const kInactiveTabsButtonHeaderIdentifier =
    @"InactiveTabsButtonHeaderIdentifier";
NSString* const kInactiveTabsPreambleHeaderIdentifier =
    @"InactiveTabsPreambleHeaderIdentifier";

constexpr base::TimeDelta kInactiveTabsHeaderAnimationDuration =
    base::Seconds(0.3);

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

@interface BidirectionalCollectionViewTransitionLayout
    : UICollectionViewTransitionLayout
@end

@implementation BidirectionalCollectionViewTransitionLayout
- (BOOL)flipsHorizontallyInOppositeLayoutDirection {
  return UseRTLLayout() ? YES : NO;
}
@end

@interface GridViewController () <GridCellDelegate,
                                  SuggestedActionsViewControllerDelegate,
                                  // TODO(crbug.com/1462907): Remove once the
                                  // diffable data source refactor is validated
                                  // by testers.
                                  UICollectionViewDataSource,
                                  UICollectionViewDelegate,
                                  UICollectionViewDelegateFlowLayout,
                                  UICollectionViewDragDelegate,
                                  UICollectionViewDropDelegate,
                                  UIPointerInteractionDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The collection view's data source.
@property(nonatomic, strong)
    UICollectionViewDiffableDataSource<NSString*, NSString*>*
        diffableDataSource;
// The cell registration for grid cells.
@property(nonatomic, strong)
    UICollectionViewCellRegistration* gridCellRegistration;
// The cell registration for the Suggested Actions cell.
@property(nonatomic, strong)
    UICollectionViewCellRegistration* suggestedActionsCellRegistration;
// The supplementary view registration for the grid header.
@property(nonatomic, strong)
    UICollectionViewSupplementaryRegistration* gridHeaderRegistration;
// The supplementary view registration for the Inactive Tabs button header.
@property(nonatomic, strong) UICollectionViewSupplementaryRegistration*
    inactiveTabsButtonHeaderRegistration;
// The supplementary view registration for the Inactive Tabs preamble header.
@property(nonatomic, strong) UICollectionViewSupplementaryRegistration*
    inactiveTabsPreambleHeaderRegistration;
// A view to obscure incognito content when the user isn't authorized to
// see it.
@property(nonatomic, strong) IncognitoReauthView* blockingView;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<TabSwitcherItem*>* items;
// Identifier of the selected item. This value is disregarded if `self.items` is
// empty. This bookkeeping is done to set the correct selection on
// `-viewWillAppear:`.
@property(nonatomic, copy) NSString* selectedItemID;
// Index of the selected item in `items`.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// Items selected for editing.
@property(nonatomic, strong) NSMutableSet<NSString*>* selectedEditingItemIDs;
// Items selected for editing which are shareable outside of the app.
@property(nonatomic, strong)
    NSMutableSet<NSString*>* selectedSharableEditingItemIDs;
// ID of the last item to be inserted. This is used to track if the active tab
// was newly created when building the animation layout for transitions.
@property(nonatomic, copy) NSString* lastInsertedItemID;
// Identifier of the lastest dragged item. This property is set when the item is
// long pressed which does not always result in a drag action.
@property(nonatomic, copy) NSString* draggedItemID;
// Animator to show or hide the empty state.
@property(nonatomic, strong) UIViewPropertyAnimator* emptyStateAnimator;
// The current layout for the tab switcher.
@property(nonatomic, strong) FlowLayout* currentLayout;
// The layout for the tab grid.
@property(nonatomic, strong) GridLayout* gridLayout;
// By how much the user scrolled past the view's content size. A negative value
// means the user hasn't scrolled past the end of the scroll view.
@property(nonatomic, assign, readonly) CGFloat offsetPastEndOfScrollView;
// The view controller that holds the view of the suggested saerch actions.
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
// The number of currently inactive tabs. If there are (inactiveTabsCount > 0)
// and the grid is in TabGridModeNormal, a button is displayed at the top,
// advertizing them.
@property(nonatomic, assign) NSInteger inactiveTabsCount;
// The number of days after which tabs are considered inactive. This is
// displayed to the user in the Inactive Tabs button when inactiveTabsCount > 0.
@property(nonatomic, assign) NSInteger inactiveTabsDaysThreshold;
// Tracks if a drop action initiated in this grid is in progress.
@property(nonatomic) BOOL localDragActionInProgress;
// Tracks if the Inactive Tabs button is being animated out.
@property(nonatomic) BOOL inactiveTabsHeaderHideAnimationInProgress;
// Tracks if the items are in a batch action, which are the "Close All" or
// "Undo" the close all.
@property(nonatomic) BOOL isClosingAllOrUndoRunning;
@end

@implementation GridViewController {
  // Tracks when the grid view is scrolling. Create a new instance to start
  // timing and reset to stop and log the associated time histogram.
  absl::optional<ScopedScrollingTimeLogger> _scopedScrollingTimeLogger;
}

- (instancetype)init {
  if (self = [super init]) {
    _items = [[NSMutableArray<TabSwitcherItem*> alloc] init];
    _selectedEditingItemIDs = [[NSMutableSet<NSString*> alloc] init];
    _selectedSharableEditingItemIDs = [[NSMutableSet<NSString*> alloc] init];
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
  self.gridLayout = [[GridLayout alloc] init];
  self.currentLayout = self.gridLayout;

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:self.currentLayout];
  // During deletion (in horizontal layout) the backgroundView can resize,
  // revealing temporarily the collectionView background. This makes sure
  // both are the same color.
  collectionView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  // If this stays as the default `YES`, then cells aren't highlighted
  // immediately on touch, but after a short delay.
  collectionView.delaysContentTouches = NO;
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    [self createRegistrations];

    __weak __typeof(self) weakSelf = self;
    self.diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
        initWithCollectionView:collectionView
                  cellProvider:^UICollectionViewCell*(
                      UICollectionView* innerCollectionView,
                      NSIndexPath* indexPath, NSString* itemIdentifier) {
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

    // UICollectionViewDropPlaceholder uses a GridCell and needs the class to be
    // registered.
    [collectionView registerClass:[GridCell class]
        forCellWithReuseIdentifier:kCellIdentifier];
  } else {
    [collectionView registerClass:[GridCell class]
        forCellWithReuseIdentifier:kCellIdentifier];
    [collectionView registerClass:[SuggestedActionsGridCell class]
        forCellWithReuseIdentifier:kSuggestedActionsCellIdentifier];
    [collectionView registerClass:[GridHeader class]
        forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
               withReuseIdentifier:kGridHeaderIdentifier];
    [collectionView registerClass:[InactiveTabsButtonHeader class]
        forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
               withReuseIdentifier:kInactiveTabsButtonHeaderIdentifier];
    [collectionView registerClass:[InactiveTabsPreambleHeader class]
        forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
               withReuseIdentifier:kInactiveTabsPreambleHeaderIdentifier];
    collectionView.dataSource = self;
  }
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

- (BOOL)isInactiveGridEmpty {
  return self.inactiveTabsCount == 0;
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

  // TODO(crbug.com/1300369): Enable dragging items from search results.
  self.collectionView.dragInteractionEnabled = (_mode != TabGridModeSearch);
  self.emptyStateView.tabGridMode = _mode;

  if (mode == TabGridModeSearch && self.suggestedActionsDelegate) {
    if (!self.suggestedActionsViewController) {
      self.suggestedActionsViewController =
          [[SuggestedActionsViewController alloc] initWithDelegate:self];
    }
  }
  [self updateSuggestedActionsSection];

  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    // Reconfigure all items.
    NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
    [snapshot reconfigureItemsWithIdentifiers:snapshot.itemIdentifiers];
    [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];

    NSUInteger selectedIndex = self.selectedIndex;
    if (previousMode != TabGridModeSelection && mode == TabGridModeNormal &&
        selectedIndex != NSNotFound) {
      // Scroll to the selected item here, so the action of reloading and
      // scrolling happens at once.
      [self.collectionView
          scrollToItemAtIndexPath:CreateIndexPath(selectedIndex)
                 atScrollPosition:UICollectionViewScrollPositionTop
                         animated:NO];
    }
  } else {
    // Reloading specific sections in a `performBatchUpdates` fades the changes
    // in rather than reloads the collection view with a harsh flash.
    __weak GridViewController* weakSelf = self;
    [self.collectionView
        performBatchUpdates:^{
          GridViewController* strongSelf = weakSelf;
          if (!strongSelf || !strongSelf.collectionView) {
            return;
          }

          if (mode == TabGridModeSelection &&
              previousMode == TabGridModeNormal) {
            // If the grid is switching from normal to selected state don't
            // reload the whole table view to avoid having a flash, particularly
            // visible when using context menu.
            for (UITableViewCell* cell in strongSelf.collectionView
                     .visibleCells) {
              GridCell* gridCell = base::mac::ObjCCast<GridCell>(cell);
              gridCell.state = mode == TabGridModeSelection
                                   ? GridCellStateEditingUnselected
                                   : GridCellStateNotEditing;
            }
          } else {
            NSRange allSectionsRange = NSMakeRange(
                /*location=*/0, strongSelf.collectionView.numberOfSections);
            NSIndexSet* allSectionsIndexSet =
                [NSIndexSet indexSetWithIndexesInRange:allSectionsRange];
            [strongSelf.collectionView reloadSections:allSectionsIndexSet];
          }
          NSUInteger selectedIndex = strongSelf.selectedIndex;
          if (previousMode != TabGridModeSelection &&
              mode == TabGridModeNormal && selectedIndex != NSNotFound) {
            // Scroll to the selected item here, so the animation of reloading
            // and scrolling happens at once.
            [strongSelf.collectionView
                scrollToItemAtIndexPath:CreateIndexPath(selectedIndex)
                       atScrollPosition:UICollectionViewScrollPositionTop
                               animated:NO];
          }
        }

                 completion:nil];
  }

  if (mode == TabGridModeNormal) {
    // Clear items when exiting selection mode.
    [self.selectedEditingItemIDs removeAllObjects];
    [self.selectedSharableEditingItemIDs removeAllObjects];

    // After transition from other modes to the normal mode, the selection
    // border doesn't show around the selected item, because reloading
    // operations lose the selected items. The collection view needs to be
    // updated with the selected item again for it to appear correctly.
    [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];

    self.searchText = nil;
  }
}

- (void)setSearchText:(NSString*)searchText {
  _searchText = searchText;
  _suggestedActionsViewController.searchText = searchText;
  [self updateSuggestedActionsSection];
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

- (LegacyGridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<LegacyGridTransitionItem*>* items =
      [[NSMutableArray alloc] init];
  LegacyGridTransitionActiveItem* activeItem;
  LegacyGridTransitionItem* selectionItem;
  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    if (path.section != kOpenTabsSectionIndex)
      continue;
    GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
        [self.collectionView cellForItemAtIndexPath:path]);
    UICollectionViewLayoutAttributes* attributes =
        [self.collectionView layoutAttributesForItemAtIndexPath:path];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame = [self.collectionView convertRect:attributes.frame
                                                 toView:nil];
    if ([cell hasIdentifier:self.selectedItemID]) {
      GridTransitionCell* activeCell =
          [GridTransitionCell transitionCellFromCell:cell];
      activeItem =
          [LegacyGridTransitionActiveItem itemWithCell:activeCell
                                                center:attributes.center
                                                  size:attributes.size];
      // If the active item is the last inserted item, it needs to be animated
      // differently.
      if ([cell hasIdentifier:self.lastInsertedItemID])
        activeItem.isAppearing = YES;
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

- (void)prepareForAppearance {
  for (TabSwitcherItem* item in [self visibleGridItems]) {
    [item prefetchSnapshot];
  }
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  self.currentLayout.animatesItemUpdates = YES;
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    [self reloadCollectionViewData];
  } else {
    [self.collectionView reloadData];
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
  self.lastInsertedItemID = nil;
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
  self.currentLayout.animatesItemUpdates = NO;
}

- (void)willCloseAll {
  self.isClosingAllOrUndoRunning = YES;
}

- (void)didCloseAll {
  self.isClosingAllOrUndoRunning = NO;
}

- (void)willUndoCloseAll {
  self.isClosingAllOrUndoRunning = YES;
}

- (void)didUndoCloseAll {
  self.isClosingAllOrUndoRunning = NO;

  // Reload the button and ensure it is not hidden, as this is the only flow
  // where the button can dynamically reappear when the app is running and the
  // reappearance is not managed by default.
  [self reloadInactiveTabsButtonHeader];
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:kOpenTabsSectionIndex];
  InactiveTabsButtonHeader* header =
      base::mac::ObjCCast<InactiveTabsButtonHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  header.hidden = NO;
}

#pragma mark - Public Editing Mode Selection

- (void)selectAllItemsForEditing {
  if (_mode != TabGridModeSelection) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  for (TabSwitcherItem* item in self.items) {
    [self selectItemWithIDForEditing:item.identifier];
  }
  [self.collectionView reloadData];
}

- (void)deselectAllItemsForEditing {
  if (_mode != TabGridModeSelection) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  for (TabSwitcherItem* item in self.items) {
    [self deselectItemWithIDForEditing:item.identifier];
  }
  [self.collectionView reloadData];
}

- (NSArray<NSString*>*)selectedItemIDsForEditing {
  return [self.selectedEditingItemIDs allObjects];
}

- (NSArray<NSString*>*)selectedShareableItemIDsForEditing {
  return [self.selectedSharableEditingItemIDs allObjects];
}

- (BOOL)allItemsSelectedForEditing {
  return _mode == TabGridModeSelection &&
         self.items.count == self.selectedEditingItemIDs.count;
}

#pragma mark - Private Editing Mode Selection

- (BOOL)isItemWithIDSelectedForEditing:(NSString*)identifier {
  return [self.selectedEditingItemIDs containsObject:identifier];
}

- (void)selectItemWithIDForEditing:(NSString*)identifier {
  [self.selectedEditingItemIDs addObject:identifier];
  if ([self.shareableItemsProvider isItemWithIdentifierSharable:identifier]) {
    [self.selectedSharableEditingItemIDs addObject:identifier];
  }
}

- (void)deselectItemWithIDForEditing:(NSString*)identifier {
  [self.selectedEditingItemIDs removeObject:identifier];
  [self.selectedSharableEditingItemIDs removeObject:identifier];
}

// TODO(crbug.com/1462907): Remove the UICollectionViewDataSource methods once
// the diffable data source refactor is validated by testers.
#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  CHECK(!base::FeatureList::IsEnabled(kTabGridRefactoring));
  if (self.showingSuggestedActions) {
    return kSuggestedActionsSectionIndex + 1;
  }
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  CHECK(!base::FeatureList::IsEnabled(kTabGridRefactoring));
  if (section == kSuggestedActionsSectionIndex) {
    // In the search mode there there is only one item in the suggested actions
    // section which contains the table for the suggested actions.
    if (self.showingSuggestedActions) {
      return 1;
    }
    return 0;
  }
  return base::checked_cast<NSInteger>(self.items.count);
}

// TODO(crbug.com/1462907): Remove the UICollectionViewDataSource methods once
// the diffable data source refactor is validated by testers.
- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  CHECK(!base::FeatureList::IsEnabled(kTabGridRefactoring));

  NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
  UICollectionViewCell* cell;

  if (indexPath.section == kSuggestedActionsSectionIndex) {
    DCHECK(self.suggestedActionsViewController);
    cell = [collectionView
        dequeueReusableCellWithReuseIdentifier:kSuggestedActionsCellIdentifier
                                  forIndexPath:indexPath];
    SuggestedActionsGridCell* suggestedActionsCell =
        base::mac::ObjCCastStrict<SuggestedActionsGridCell>(cell);
    suggestedActionsCell.suggestedActionsView =
        self.suggestedActionsViewController.view;
  } else {
    // In some cases this is called with an indexPath.item that's beyond (by 1)
    // the bounds of self.items -- see crbug.com/1068136. Presumably this is a
    // race condition where an item has been deleted at the same time as the
    // collection is doing layout (potentially during rotation?). Fudge by
    // duplicating the last cell. The assumption is that there will be another,
    // correct layout shortly after the incorrect one.
    // Keep array bounds valid.
    if (itemIndex >= self.items.count) {
      itemIndex = self.items.count - 1;
    }

    TabSwitcherItem* item = self.items[itemIndex];
    cell =
        [collectionView dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                                  forIndexPath:indexPath];
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(cell);
    [self configureCell:gridCell withItem:item atIndex:itemIndex];
  }

  return cell;
}

// TODO(crbug.com/1462907): Remove the UICollectionViewDataSource methods once
// the diffable data source refactor is validated by testers.
- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  CHECK(!base::FeatureList::IsEnabled(kTabGridRefactoring));
  switch (_mode) {
    case TabGridModeNormal: {
      // The Regular Tabs grid has a button to inform about the hidden inactive
      // tabs.
      CHECK(IsInactiveTabsAvailable());
      if (self.inactiveTabsCount == 0 &&
          !self.inactiveTabsHeaderHideAnimationInProgress) {
        base::debug::DumpWithoutCrashing();
      }
      InactiveTabsButtonHeader* header = [collectionView
          dequeueReusableSupplementaryViewOfKind:kind
                             withReuseIdentifier:
                                 kInactiveTabsButtonHeaderIdentifier
                                    forIndexPath:indexPath];
      [self configureInactiveTabsButtonHeader:header];
      return header;
    }
    case TabGridModeSelection:
      NOTREACHED();
      return nil;
    case TabGridModeSearch: {
      GridHeader* headerView = [collectionView
          dequeueReusableSupplementaryViewOfKind:kind
                             withReuseIdentifier:kGridHeaderIdentifier
                                    forIndexPath:indexPath];
      switch (indexPath.section) {
        case kOpenTabsSectionIndex: {
          headerView.title = l10n_util::GetNSString(
              IDS_IOS_TABS_SEARCH_OPEN_TABS_SECTION_HEADER_TITLE);
          NSString* resultsCount = [@(self.items.count) stringValue];
          headerView.value =
              l10n_util::GetNSStringF(IDS_IOS_TABS_SEARCH_OPEN_TABS_COUNT,
                                      base::SysNSStringToUTF16(resultsCount));
          break;
        }
        case kSuggestedActionsSectionIndex: {
          headerView.title =
              l10n_util::GetNSString(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTIONS);
          break;
        }
      }
      return headerView;
    }
    case TabGridModeInactive:
      // The Inactive Tabs grid has a header to inform about the feature and a
      // link to its settings.
      CHECK(IsInactiveTabsEnabled());
      InactiveTabsPreambleHeader* header = [collectionView
          dequeueReusableSupplementaryViewOfKind:kind
                             withReuseIdentifier:
                                 kInactiveTabsPreambleHeaderIdentifier
                                    forIndexPath:indexPath];
      [self configureInactiveTabsPreambleHeader:header];
      return header;
  }
}

#pragma mark - UICollectionView Diffable Data Source Helpers

- (void)reloadCollectionViewData {
  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kOpenTabsSectionIdentifier ]];
  NSString* identifierKey = NSStringFromSelector(@selector(identifier));
  [snapshot appendItemsWithIdentifiers:[self.items valueForKey:identifierKey]];
  if (self.showingSuggestedActions) {
    [snapshot
        appendSectionsWithIdentifiers:@[ kSuggestedActionsSectionIdentifier ]];
    [snapshot appendItemsWithIdentifiers:@[ kSuggestedActionsCellIdentifier ]];
  }
  [self.diffableDataSource applySnapshotUsingReloadData:snapshot];
}

// Creates the cell and supplementary view registrations and assigns them to the
// appropriate properties.
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

  // Register InactiveTabsButtonHeader.
  auto configureInactiveTabsButtonHeader =
      ^(InactiveTabsButtonHeader* header, NSString* elementKind,
        NSIndexPath* indexPath) {
        [weakSelf configureInactiveTabsButtonHeader:header];
      };
  self.inactiveTabsButtonHeaderRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[InactiveTabsButtonHeader class]
                                 elementKind:
                                     UICollectionElementKindSectionHeader
                        configurationHandler:configureInactiveTabsButtonHeader];

  // Register InactiveTabsPreambleHeader.
  auto configureInactiveTabsPreambleHeader =
      ^(InactiveTabsPreambleHeader* header, NSString* elementKind,
        NSIndexPath* indexPath) {
        [weakSelf configureInactiveTabsPreambleHeader:header];
      };
  self.inactiveTabsPreambleHeaderRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[InactiveTabsPreambleHeader class]
                                 elementKind:
                                     UICollectionElementKindSectionHeader
                        configurationHandler:
                            configureInactiveTabsPreambleHeader];
}

// Configures the grid header for the given section.
- (void)configureGridHeader:(GridHeader*)gridHeader
       forSectionIdentifier:(NSString*)sectionIdentifier {
  CHECK(base::FeatureList::IsEnabled(kTabGridRefactoring));
  if ([sectionIdentifier isEqualToString:kOpenTabsSectionIdentifier]) {
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

// Configures the Inactive Tabs Button header according to the current state.
- (void)configureInactiveTabsButtonHeader:(InactiveTabsButtonHeader*)header {
  header.parent = self;
  __weak __typeof(self) weakSelf = self;
  header.buttonAction = ^{
    [weakSelf didTapInactiveTabsButton];
  };
  [header configureWithDaysThreshold:self.inactiveTabsDaysThreshold];
  if (IsShowInactiveTabsCountEnabled()) {
    [header configureWithCount:self.inactiveTabsCount];
  }
}

// Configures the Inactive Tabs Preamble header according to the current state.
- (void)configureInactiveTabsPreambleHeader:
    (InactiveTabsPreambleHeader*)header {
  __weak __typeof(self) weakSelf = self;
  header.settingsLinkAction = ^{
    [weakSelf didTapInactiveTabsSettingsLink];
  };
  header.daysThreshold = self.inactiveTabsDaysThreshold;
}

// Returns a configured header for the given index path.
- (UICollectionReusableView*)headerForSectionAtIndexPath:
    (NSIndexPath*)indexPath {
  UICollectionViewSupplementaryRegistration* registration;
  switch (_mode) {
    case TabGridModeNormal:
      // The Regular Tabs grid has a button to inform about the hidden inactive
      // tabs.
      CHECK(IsInactiveTabsAvailable());
      if (self.inactiveTabsCount == 0 &&
          !self.inactiveTabsHeaderHideAnimationInProgress) {
        base::debug::DumpWithoutCrashing();
      }
      registration = self.inactiveTabsButtonHeaderRegistration;
      break;
    case TabGridModeSelection:
      NOTREACHED();
      break;
    case TabGridModeSearch:
      registration = self.gridHeaderRegistration;
      break;
    case TabGridModeInactive:
      // The Inactive Tabs grid has a header to inform about the feature and a
      // link to its settings.
      CHECK(IsInactiveTabsEnabled());
      registration = self.inactiveTabsPreambleHeaderRegistration;
      break;
  }
  return [self.collectionView
      dequeueConfiguredReusableSupplementaryViewWithRegistration:registration
                                                    forIndexPath:indexPath];
}

// Returns a configured cell for the given index path and item identifier.
- (UICollectionViewCell*)cellForItemAtIndexPath:(NSIndexPath*)indexPath
                                 itemIdentifier:(NSString*)itemIdentifier {
  CHECK(base::FeatureList::IsEnabled(kTabGridRefactoring));

  // Handle the SuggestedActionsGridCell.
  if ([itemIdentifier isEqualToString:kSuggestedActionsCellIdentifier]) {
    return [self.collectionView
        dequeueConfiguredReusableCellWithRegistration:
            self.suggestedActionsCellRegistration
                                         forIndexPath:indexPath
                                                 item:itemIdentifier];
  }

  // Handle GridCell-s.
  NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
  // In some cases this is called with an indexPath.item that's beyond (by 1)
  // the bounds of self.items -- see crbug.com/1068136. Presumably this is a
  // race condition where an item has been deleted at the same time as the
  // collection is doing layout (potentially during rotation?). Fudge by
  // duplicating the last cell. The assumption is that there will be another,
  // correct layout shortly after the incorrect one.
  // Keep array bounds valid.
  if (itemIndex >= self.items.count) {
    itemIndex = self.items.count - 1;
  }

  TabSwitcherItem* item = self.items[itemIndex];
  return [self.collectionView
      dequeueConfiguredReusableCellWithRegistration:self.gridCellRegistration
                                       forIndexPath:indexPath
                                               item:item];
}

#pragma mark - UICollectionViewDelegate

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  // `collectionViewLayout` should always be a flow layout.
  DCHECK(
      [collectionViewLayout isKindOfClass:[UICollectionViewFlowLayout class]]);
  if (self.isClosingAllOrUndoRunning) {
    return CGSizeZero;
  }
  UICollectionViewFlowLayout* layout =
      (UICollectionViewFlowLayout*)collectionViewLayout;
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
  switch (_mode) {
    case TabGridModeNormal:
      if (!IsInactiveTabsAvailable()) {
        return CGSizeZero;
      }
      if (self.isClosingAllOrUndoRunning) {
        return CGSizeZero;
      }
      if (self.inactiveTabsHeaderHideAnimationInProgress) {
        // The header is animated out to a height of 0.1.
        return CGSizeMake(collectionView.bounds.size.width, 0.1);
      }
      if (self.inactiveTabsCount == 0) {
        return CGSizeZero;
      }
      // The Regular Tabs grid has a button to inform about the hidden inactive
      // tabs.
      return [self inactiveTabsButtonHeaderSize];
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
      if (!IsInactiveTabsEnabled()) {
        return CGSizeZero;
      }
      // The Inactive Tabs grid has a header to inform about the feature and a
      // link to its settings.
      return [self inactiveTabsPreambleHeaderSize];
  }
}

// This prevents the user from dragging a cell past the plus sign cell (the last
// cell in the collection view).
- (NSIndexPath*)collectionView:(UICollectionView*)collectionView
    targetIndexPathForMoveFromItemAtIndexPath:(NSIndexPath*)originalIndexPath
                          toProposedIndexPath:(NSIndexPath*)proposedIndexPath {
  return proposedIndexPath;
}

// This method is used instead of -didSelectItemAtIndexPath, because any
// selection events will be signalled through the model layer and handled in
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
  // Don't allow long-press previews when the incognito reauth view is blocking
  // the content.
  if (self.contentNeedsAuthentication) {
    return nil;
  }

  // Context menu shouldn't appear in the selection mode.
  if (_mode == TabGridModeSelection) {
    return nil;
  }

  // No context menu on suggested actions section.
  if (indexPath.section == kSuggestedActionsSectionIndex) {
    return nil;
  }

  GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);

  MenuScenarioHistogram scenario;
  if (_mode == TabGridModeSearch) {
    scenario = MenuScenarioHistogram::kTabGridSearchResult;
  } else if (_mode == TabGridModeInactive) {
    scenario = MenuScenarioHistogram::kInactiveTabsEntry;
  } else {
    scenario = MenuScenarioHistogram::kTabGridEntry;
  }

  return [self.menuProvider contextMenuConfigurationForTabCell:cell
                                                  menuScenario:scenario];
}

- (UICollectionViewTransitionLayout*)
                  collectionView:(UICollectionView*)collectionView
    transitionLayoutForOldLayout:(UICollectionViewLayout*)fromLayout
                       newLayout:(UICollectionViewLayout*)toLayout {
  return [[BidirectionalCollectionViewTransitionLayout alloc]
      initWithCurrentLayout:fromLayout
                 nextLayout:toLayout];
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    return;
  }

  // Checking and updating the GridCell's `state` if needed.
  //
  // For the context: `setMode:` method updates the `state` property of the
  // visible GridCells only. However, there are might be some cells that were
  // dequeued already but haven't been displayed yet. Those will never update
  // their `state` unless we forcely handle it here.
  //
  // See crbug.com//1427278
  if ([cell isKindOfClass:[GridCell class]]) {
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(cell);

    BOOL isTabGridInSelectionMode = _mode == TabGridModeSelection;
    BOOL isGridCellInSelectionMode = gridCell.state != GridCellStateNotEditing;

    if (isTabGridInSelectionMode != isGridCellInSelectionMode) {
      gridCell.state = isTabGridInSelectionMode ? GridCellStateEditingUnselected
                                                : GridCellStateNotEditing;
    }
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[GridCell class]]) {
    // Stop animation of GridCells when removing them from the collection view.
    // This is important to prevent cells from animating indefinitely. This is
    // safe because the animation state of GridCells is set in
    // `configureCell:withItem:atIndex:` whenever a cell is used.
    [base::mac::ObjCCastStrict<GridCell>(cell) hideActivityIndicator];
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

  [self.dragDropHandler dragSessionDidEnd];
  [self.delegate gridViewControllerDragSessionDidEnd:self];
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
  NSString* pressedItemID = self.items[index].identifier;
  if (![self isItemWithIDSelectedForEditing:pressedItemID]) {
    [self tappedItemAtIndexPath:indexPath];
  }

  NSMutableArray<UIDragItem*>* dragItems = [[NSMutableArray alloc] init];
  for (NSString* itemID in self.selectedEditingItemIDs) {
    [dragItems addObject:[self.dragDropHandler dragItemForItemWithID:itemID]];
  }
  return dragItems;
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

  GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(
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
        GridCell* gridCell =
            base::mac::ObjCCastStrict<GridCell>(placeholderCell);
        gridCell.theme = self.theme;
      };
      placeholder.previewParametersProvider =
          ^UIDragPreviewParameters*(UICollectionViewCell* placeholderCell) {
        GridCell* gridCell =
            base::mac::ObjCCastStrict<GridCell>(placeholderCell);
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

#pragma mark - IncognitoReauthConsumer

- (void)setItemsRequireAuthentication:(BOOL)require {
  self.contentNeedsAuthentication = require;
  [self.delegate gridViewController:self
      contentNeedsAuthenticationChanged:require];

  if (require) {
    if (!self.blockingView) {
      self.blockingView = [[IncognitoReauthView alloc] init];
      self.blockingView.translatesAutoresizingMaskIntoConstraints = NO;
      self.blockingView.layer.zPosition = FLT_MAX;
      // No need to show tab switcher button when already in the tab switcher.
      self.blockingView.tabSwitcherButton.hidden = YES;
      // Hide the logo.
      self.blockingView.logoView.hidden = YES;

      [self.blockingView.authenticateButton
                 addTarget:self.reauthHandler
                    action:@selector(authenticateIncognitoContent)
          forControlEvents:UIControlEventTouchUpInside];
    }

    [self.view addSubview:self.blockingView];
    self.blockingView.alpha = 1;
    AddSameConstraints(self.collectionView.frameLayoutGuide, self.blockingView);
  } else {
    [UIView animateWithDuration:0.2
        animations:^{
          self.blockingView.alpha = 0;
        }
        completion:^(BOOL finished) {
          if (self.contentNeedsAuthentication) {
            self.blockingView.alpha = 1;
          } else {
            [self.blockingView removeFromSuperview];
          }
        }];
  }
}

#pragma mark - TabCollectionConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
#ifndef NDEBUG
  // Consistency check: ensure no IDs are duplicated.
  NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
  for (TabSwitcherItem* item in items) {
    [identifiers addObject:item.identifier];
  }
  CHECK_EQ(identifiers.count, items.count);
#endif

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;
  [self.selectedEditingItemIDs removeAllObjects];
  [self.selectedSharableEditingItemIDs removeAllObjects];

  [self reloadTabs];

  [self updateSelectedCollectionViewItemRingAndBringIntoView:YES];

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }
  // Whether the view is visible or not, the delegate must be updated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  if (_mode == TabGridModeSearch) {
    if (_searchText.length)
      [self updateSearchResultsHeader];
    [self.collectionView
        setContentOffset:CGPointMake(
                             -self.collectionView.adjustedContentInset.left,
                             -self.collectionView.adjustedContentInset.top)
                animated:NO];
  }
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  if (_mode == TabGridModeSearch) {
    // Prevent inserting items while viewing search results.
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(NSDiffableDataSourceSnapshot* snapshot) {
        [weakSelf applyModelAndViewUpdatesForInsertionOfItem:item
                                                     atIndex:index
                                              selectedItemID:selectedItemID
                                                    snapshot:snapshot];
      }
      completion:^{
        [weakSelf modelAndViewUpdatesForInsertionDidCompleteAtIndex:index];
      }];
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  NSUInteger index = [self indexOfItemWithID:removedItemID];

  // Do not remove if not showing the item (i.e. showing search results).
  if (index == NSNotFound) {
    [self selectItemWithID:selectedItemID];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self
      performModelAndViewUpdates:^(NSDiffableDataSourceSnapshot* snapshot) {
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

- (void)selectItemWithID:(NSString*)selectedItemID {
  if ([self.selectedItemID isEqualToString:selectedItemID])
    return;

  self.selectedItemID = selectedItemID;
  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
}

- (void)replaceItemID:(NSString*)existingItemID
             withItem:(TabSwitcherItem*)newItem {
  NSUInteger index = [self indexOfItemWithID:existingItemID];
  if (index == NSNotFound) {
    return;
  }
  // Consistency check: `newItem`'s ID is either `existingItemID` or not in
  // `self.items`.
  DCHECK([newItem.identifier isEqualToString:existingItemID] ||
         [self indexOfItemWithID:newItem.identifier] == NSNotFound);
  self.items[index] = newItem;

  if (base::FeatureList::IsEnabled(kTabGridRefactoring) &&
      base::FeatureList::IsEnabled(kTabGridRefactoringFix)) {
    NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
    if ([existingItemID isEqualToString:newItem.identifier]) {
      [snapshot reconfigureItemsWithIdentifiers:@[ existingItemID ]];
    } else {
      // Add the new item before the existing item.
      [snapshot insertItemsWithIdentifiers:@[ newItem.identifier ]
                  beforeItemWithIdentifier:existingItemID];
      [snapshot deleteItemsWithIdentifiers:@[ existingItemID ]];
    }
    [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  } else {
    GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
        [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)]);
    // `cell` may be nil if it is scrolled offscreen.
    if (cell) {
      [self configureCell:cell withItem:newItem atIndex:index];
    }
  }
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
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
      performModelAndViewUpdates:^(NSDiffableDataSourceSnapshot* snapshot) {
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

#pragma mark - InactiveTabsInfoConsumer

- (void)updateInactiveTabsCount:(NSInteger)count {
  if (self.inactiveTabsCount == count) {
    return;
  }
  NSInteger oldCount = self.inactiveTabsCount;
  self.inactiveTabsCount = count;

  // Update the header.
  if (oldCount == 0) {
    [self showInactiveTabsButtonHeader];
  } else if (count == 0) {
    [self hideInactiveTabsButtonHeader];
  } else {
    // The header just needs to be updated with the new count.
    [self updateInactiveTabsButtonHeader];
  }
}

- (void)updateInactiveTabsDaysThreshold:(NSInteger)daysThreshold {
  if (self.inactiveTabsDaysThreshold == daysThreshold) {
    return;
  }
  NSInteger oldDaysThreshold = self.inactiveTabsDaysThreshold;
  self.inactiveTabsDaysThreshold = daysThreshold;

  // Update the header.
  if (oldDaysThreshold == kInactiveTabsDisabledByUser ||
      daysThreshold == kInactiveTabsDisabledByUser) {
    // The header should appear or disappear. Reload the section.
    [self reloadInactiveTabsButtonHeader];
  } else {
    // The header just needs to be updated with the new days threshold.
    [self updateInactiveTabsButtonHeader];
  }

  // Update the preamble.
  [self updateInactiveTabsPreambleHeader];
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
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
    if (self.mode == TabGridModeSearch && self.searchText.length) {
      if (!self.showingSuggestedActions) {
        [snapshot appendSectionsWithIdentifiers:@[
          kSuggestedActionsSectionIdentifier
        ]];
        [snapshot
            appendItemsWithIdentifiers:@[ kSuggestedActionsCellIdentifier ]];
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
  } else {
    auto updateSectionBlock = ^{
      NSIndexSet* sections =
          [NSIndexSet indexSetWithIndex:kSuggestedActionsSectionIndex];
      if (self.mode == TabGridModeSearch && self.searchText.length) {
        if (!self.showingSuggestedActions) {
          [self.collectionView insertSections:sections];
          self.showingSuggestedActions = YES;
        }
      } else {
        if (self.showingSuggestedActions) {
          [self.collectionView deleteSections:sections];
          self.showingSuggestedActions = NO;
        }
      }
    };
    [UIView performWithoutAnimation:^{
      [self.collectionView performBatchUpdates:updateSectionBlock
                                    completion:nil];
    }];
  }
}

#pragma mark - Private helpers for joint model and view updates

// Performs model and view updates together.
- (void)performModelAndViewUpdates:
            (void (^)(NSDiffableDataSourceSnapshot* snapshot))
                modelAndViewUpdates
                        completion:(ProceduralBlock)completion {
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    self.updating = YES;
    // Synchronize model and diffable snapshot updates.
    NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
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
  } else {
    __weak __typeof(self) weakSelf = self;
    [self.collectionView
        performBatchUpdates:^{
          weakSelf.updating = YES;
          modelAndViewUpdates(nil);
        }
        completion:^(BOOL completed) {
          if (weakSelf) {
            completion();
            weakSelf.updating = NO;
          }
        }];
  }

  [self updateVisibleCellIdentifiers];
}

// Makes the required changes to `items` and `collectionView` when a new item is
// inserted.
- (void)applyModelAndViewUpdatesForInsertionOfItem:(TabSwitcherItem*)item
                                           atIndex:(NSUInteger)index
                                    selectedItemID:(NSString*)selectedItemID
                                          snapshot:
                                              (NSDiffableDataSourceSnapshot*)
                                                  snapshot {
  // Consistency check: `item`'s ID is not in `items`.
  // (using DCHECK rather than DCHECK_EQ to avoid a checked_cast on NSNotFound).
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);

  // Store the identifier of the current item at the given index, if any, prior
  // to model updates.
  NSString* previousItemIdentifierAtIndex;
  if (index < self.items.count) {
    previousItemIdentifierAtIndex = self.items[index].identifier;
  }

  [self.items insertObject:item atIndex:index];
  self.selectedItemID = selectedItemID;
  self.lastInsertedItemID = item.identifier;
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];

  [self removeEmptyStateAnimated:YES];
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    // TODO(crbug.com/1473625): There are crash reports that show there could be
    // cases where the open tabs section is not present in the snapshot. If so,
    // don't perform the update.
    if ([snapshot indexOfSectionIdentifier:kOpenTabsSectionIdentifier] ==
        NSNotFound) {
      return;
    }
    // The snapshot API doesn't provide a way to insert at a given index (that's
    // its purpose actually), only before/after an existing item, or by
    // appending to an existing section.
    // If the new item is taking the spot of an existing item, insert the new
    // one before it. Otherwise (if the section is empty, or the new index is
    // the new last position), append at the end of the section.
    if (previousItemIdentifierAtIndex) {
      [snapshot insertItemsWithIdentifiers:@[ item.identifier ]
                  beforeItemWithIdentifier:previousItemIdentifierAtIndex];
    } else {
      [snapshot appendItemsWithIdentifiers:@[ item.identifier ]
                 intoSectionWithIdentifier:kOpenTabsSectionIdentifier];
    }
  } else {
    [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  }
}

// Makes the required changes when a new item has been inserted.
- (void)modelAndViewUpdatesForInsertionDidCompleteAtIndex:(NSUInteger)index {
  [self updateSelectedCollectionViewItemRingAndBringIntoView:YES];

  [self.delegate gridViewController:self didChangeItemCount:self.items.count];

  // Check `index` boundaries in order to filter out possible race conditions
  // while mutating the collection.
  if (index == NSNotFound || index >= self.items.count) {
    return;
  }

  [self.collectionView
      scrollToItemAtIndexPath:CreateIndexPath(index)
             atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                     animated:YES];
}

// Makes the required changes to `items` and `collectionView` when an existing
// item is removed.
- (void)
    applyModelAndViewUpdatesForRemovalOfItemWithID:(NSString*)removedItemID
                                    selectedItemID:(NSString*)selectedItemID
                                          snapshot:
                                              (NSDiffableDataSourceSnapshot*)
                                                  snapshot {
  NSUInteger index = [self indexOfItemWithID:removedItemID];
  [self.items removeObjectAtIndex:index];
  self.selectedItemID = selectedItemID;
  [self deselectItemWithIDForEditing:removedItemID];
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];

  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    [snapshot deleteItemsWithIdentifiers:@[ removedItemID ]];
    if ([self shouldShowEmptyState]) {
      [self animateEmptyStateIn];
    }
  } else {
    [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  }
  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  }
}

// Makes the required changes when a new item has been removed.
- (void)modelAndViewUpdatesForRemovalDidCompleteForItemWithID:
    (NSString*)removedItemID {
  if (self.items.count > 0) {
    [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
  }
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self.delegate gridViewController:self didRemoveItemWIthID:removedItemID];
}

// Makes the required changes to `items` and `collectionView` when an existing
// item is moved.
- (void)applyModelAndViewUpdatesForMoveOfItemWithID:(NSString*)itemID
                                          fromIndex:(NSUInteger)fromIndex
                                            toIndex:(NSUInteger)toIndex
                                           snapshot:
                                               (NSDiffableDataSourceSnapshot*)
                                                   snapshot {
  NSString* toItemID = self.items[toIndex].identifier;
  TabSwitcherItem* item = self.items[fromIndex];
  [self.items removeObjectAtIndex:fromIndex];
  [self.items insertObject:item atIndex:toIndex];

  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    if (fromIndex < toIndex) {
      [snapshot moveItemWithIdentifier:itemID afterItemWithIdentifier:toItemID];
    } else {
      [snapshot moveItemWithIdentifier:itemID
              beforeItemWithIdentifier:toItemID];
    }
  } else {
    [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                                 toIndexPath:CreateIndexPath(toIndex)];
  }
}

// Makes the required changes when a new item has been moved.
- (void)modelAndViewUpdatesForMoveDidCompleteForItemWithID:(NSString*)itemID
                                                   toIndex:(NSUInteger)toIndex {
  [self.delegate gridViewController:self
                  didMoveItemWithID:itemID
                            toIndex:toIndex];
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:self.selectedItemID];
}

- (CGFloat)offsetPastEndOfScrollView {
  // Use collectionViewLayout.collectionViewContentSize because it has the
  // correct size during a batch update.
  return self.collectionView.contentOffset.x +
         self.collectionView.frame.size.width -
         self.collectionView.collectionViewLayout.collectionViewContentSize
             .width;
}

- (void)setCurrentLayout:(FlowLayout*)currentLayout {
  _currentLayout = currentLayout;
}

#pragma mark - Private

// Updates the ring to be around the currently selected item. If
// `shouldBringItemIntoView` is true, the collection view scrolls to present the
// selected item at the top.
- (void)updateSelectedCollectionViewItemRingAndBringIntoView:
    (BOOL)shouldBringItemIntoView {
  // Deselects all the collection view items.
  NSArray<NSIndexPath*>* indexPathsForSelectedItems =
      [self.collectionView indexPathsForSelectedItems];
  for (NSIndexPath* itemIndexPath in indexPathsForSelectedItems) {
    [self.collectionView deselectItemAtIndexPath:itemIndexPath animated:NO];
  }

  // Select the collection view item for the selected index.
  NSUInteger selectedIndex = self.selectedIndex;
  // Check `selectedIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (selectedIndex == NSNotFound || selectedIndex >= self.items.count) {
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

- (void)voiceOverStatusDidChange {
  self.collectionView.dragInteractionEnabled =
      [self shouldEnableDrapAndDropInteraction];
}

- (void)preferredContentSizeCategoryDidChange {
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (BOOL)shouldEnableDrapAndDropInteraction {
  // Don't enable drag and drop when voice over is enabled.
  return !UIAccessibilityIsVoiceOverRunning();
}

// Returns the index in `self.items` of the first item whose identifier is
// `identifier`.
- (NSUInteger)indexOfItemWithID:(NSString*)identifier {
  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return [item.identifier isEqualToString:identifier];
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
  DCHECK(cell);
  DCHECK(item);
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
  cell.accessibilityIdentifier = GridCellAccessibilityIdentifier(index);
  if (self.mode == TabGridModeSelection) {
    if ([self isItemWithIDSelectedForEditing:item.identifier]) {
      cell.state = GridCellStateEditingSelected;
    } else {
      cell.state = GridCellStateEditingUnselected;
    }
  } else {
    cell.state = GridCellStateNotEditing;
  }
  [item fetchFavicon:^(TabSwitcherItem* innerItem, UIImage* icon) {
    // Only update the icon if the cell is not already reused for another item.
    if ([cell hasIdentifier:innerItem.identifier]) {
      cell.icon = icon;
    }
  }];

  [item fetchSnapshot:^(TabSwitcherItem* innerItem, UIImage* snapshot) {
    // Only update the icon if the cell is not already reused for another item.
    if ([cell hasIdentifier:innerItem.identifier]) {
      cell.snapshot = snapshot;
    }
  }];

  NSString* itemIdentifier = item.identifier;
  [self.priceCardDataSource
      priceCardForIdentifier:itemIdentifier
                  completion:^(PriceCardItem* priceCardItem) {
                    if (priceCardItem && [cell hasIdentifier:itemIdentifier])
                      [cell setPriceDrop:priceCardItem.price
                           previousPrice:priceCardItem.previousPrice];
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
// TODO(crbug.com/1350453): Use the "Primary Action" APIs for collection views
// when running under iOS16 instead of overloading selection changes to handle
// cell taps.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  // Speculative fix for crbug.com/1134663, where this method is called while
  // updates from a tab insertion are processing.
  // *** Do not add any code before this check. ***
  if (self.updating)
    return;

  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  DCHECK_LT(index, self.items.count);

  // crbug.com/1163238: In the wild, the above DCHECK condition is hit. This
  // might be a case of the UI sending touch events after the model has updated.
  // In this case, just no-op; if the user has to tap again to activate a tab,
  // that's better than a crash.
  if (index >= self.items.count)
    return;

  NSString* itemID = self.items[index].identifier;
  if (_mode == TabGridModeSelection) {
    if ([self isItemWithIDSelectedForEditing:itemID]) {
      [self deselectItemWithIDForEditing:itemID];
    } else {
      [self selectItemWithIDForEditing:itemID];
    }
    // Dragging multiple tabs to reorder them is not supported. So there is no
    // need to enable dragging when multiple items are selected in devices that
    // don't support multiple windows.
    if (self.selectedItemIDsForEditing.count > 1 &&
        !base::ios::IsMultipleScenesSupported()) {
      self.collectionView.dragInteractionEnabled = NO;
    } else {
      self.collectionView.dragInteractionEnabled = YES;
    }
    // Reconfigure the item.
    if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
      NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
      [snapshot reconfigureItemsWithIdentifiers:@[ itemID ]];
      [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
    } else {
      [UIView performWithoutAnimation:^{
        [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
      }];
    }
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
    if (![cell isKindOfClass:[GridCell class]])
      continue;
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

// Reloads the tabs section of the grid view.
- (void)reloadTabs {
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
    if (NSNotFound ==
        [snapshot indexOfSectionIdentifier:kOpenTabsSectionIdentifier]) {
      return;
    }

    // Delete old tabs if the section is present.
    auto oldTabsIdentifiers = [snapshot
        itemIdentifiersInSectionWithIdentifier:kOpenTabsSectionIdentifier];
    [snapshot deleteItemsWithIdentifiers:oldTabsIdentifiers];
    // Add new tabs.
    NSString* identifierKey = NSStringFromSelector(@selector(identifier));
    [snapshot appendItemsWithIdentifiers:[self.items valueForKey:identifierKey]
               intoSectionWithIdentifier:kOpenTabsSectionIdentifier];
    [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];

    [self updateVisibleCellIdentifiers];
  } else {
    NSIndexSet* targetSections =
        [NSIndexSet indexSetWithIndex:kOpenTabsSectionIndex];
    [UIView performWithoutAnimation:^{
      // There is a collection view bug (crbug.com/1300733) that prevents
      // CollectionView's `reloadData` from working properly if its preceded by
      // CollectionView's `performBatchUpdates:` in the same UI cycle. To avoid
      // this bug, `reloadSections:` method is used instead to reload the items
      // in the tab grid.
      [self.collectionView reloadSections:targetSections];
    }];
  }
}

// Updates the number of results found on the search open tabs section header.
- (void)updateSearchResultsHeader {
  GridHeader* headerView = (GridHeader*)[self.collectionView
      supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                          atIndexPath:
                              [NSIndexPath
                                  indexPathForRow:0
                                        inSection:kOpenTabsSectionIndex]];
  if (!headerView)
    return;
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

// Returns the size that should be dedicated the the Inactive Tabs button
// header.
- (CGSize)inactiveTabsButtonHeaderSize {
  // Keep a sizing header.
  static InactiveTabsButtonHeader* gHeader =
      [[InactiveTabsButtonHeader alloc] init];

  // Configure it.
  [gHeader configureWithDaysThreshold:self.inactiveTabsDaysThreshold];
  if (IsShowInactiveTabsCountEnabled()) {
    [gHeader configureWithCount:self.inactiveTabsCount];
  }

  // Get its fitting size.
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  CGSize targetSize = CGSize(width, UILayoutFittingExpandedSize.height);
  // Host the view in the hierarchy for it to get the appropriate trait
  // collection. This might be due a UIKit/SwiftUI interaction bug, as this is
  // not necessary for `InactiveTabsPreambleHeader` below for example.
  gHeader.parent = self;
  [self.view addSubview:gHeader];

  CGSize size =
      [gHeader systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel];

  // De-parent the header.
  [gHeader removeFromSuperview];
  gHeader.parent = nil;

  return CGSizeMake(width, size.height);
}

// Returns the size that should be dedicated the the Inactive Tabs preamble
// header.
- (CGSize)inactiveTabsPreambleHeaderSize {
  // Keep a sizing header.
  static InactiveTabsPreambleHeader* gHeader =
      [[InactiveTabsPreambleHeader alloc] init];

  // Configure it.
  gHeader.daysThreshold = self.inactiveTabsDaysThreshold;

  // Get its fitting size.
  CGFloat width = CGRectGetWidth(self.collectionView.bounds);
  CGSize targetSize = CGSize(width, UILayoutFittingExpandedSize.height);
  CGSize size =
      [gHeader systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel];

  return CGSizeMake(width, size.height);
}

- (void)showInactiveTabsButtonHeader {
  // Contrary to `hideInactiveTabsButtonHeader`, this doesn't need to be
  // animated.
  [self reloadInactiveTabsButtonHeader];
}

- (void)hideInactiveTabsButtonHeader {
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:kOpenTabsSectionIndex];
  InactiveTabsButtonHeader* header =
      base::mac::ObjCCast<InactiveTabsButtonHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  if (!header) {
    return;
  }

  self.inactiveTabsHeaderHideAnimationInProgress = YES;
  [UIView animateWithDuration:kInactiveTabsHeaderAnimationDuration.InSecondsF()
      animations:^{
        header.alpha = 0;
        [self.collectionView.collectionViewLayout invalidateLayout];
      }
      completion:^(BOOL finished) {
        header.hidden = YES;
        self.inactiveTabsHeaderHideAnimationInProgress = NO;
        // Update the header to make it entirely disappear once the animation is
        // done. This is done after a delay because the completion can be called
        // before the animation ended, causing a visual glitch.
        __weak __typeof(self) weakSelf = self;
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, base::BindOnce(^{
              [weakSelf reloadInactiveTabsButtonHeader];
            }),
            kInactiveTabsHeaderAnimationDuration);
      }];
}

// Reloads the section containing the Inactive Tabs button header.
- (void)reloadInactiveTabsButtonHeader {
  // Prevent the animation, as it leads to a jarrying effect when closing all
  // inactive tabs: the inactive tabs view controller gets popped, and the
  // underlying regular Tab Grid moves tabs up.
  // Note: this could be revisited when supporting iPad, as the user could
  // have closed all inactive tabs in a different window.
  if (base::FeatureList::IsEnabled(kTabGridRefactoring)) {
    NSDiffableDataSourceSnapshot* snapshot = self.diffableDataSource.snapshot;
    [snapshot reloadSectionsWithIdentifiers:@[ kOpenTabsSectionIdentifier ]];
    [self.diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  } else {
    NSIndexSet* openTabsSection =
        [NSIndexSet indexSetWithIndex:kOpenTabsSectionIndex];
    [UIView performWithoutAnimation:^{
      [self.collectionView reloadSections:openTabsSection];
    }];
  }

  // Make sure to restore the selection. Reloading the section cleared it.
  // https://developer.apple.com/forums/thread/656529
  [self updateSelectedCollectionViewItemRingAndBringIntoView:NO];
}

// Reconfigures the Inactive Tabs button header.
- (void)updateInactiveTabsButtonHeader {
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:kOpenTabsSectionIndex];
  InactiveTabsButtonHeader* header =
      base::mac::ObjCCast<InactiveTabsButtonHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  // Note: At this point, `header` could be nil if not visible, or if the
  // supplementary view is not an InactiveTabsButtonHeader.
  [self configureInactiveTabsButtonHeader:header];
}

// Reconfigures the Inactive Tabs preamble header.
- (void)updateInactiveTabsPreambleHeader {
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0
                                               inSection:kOpenTabsSectionIndex];
  InactiveTabsPreambleHeader* header =
      base::mac::ObjCCast<InactiveTabsPreambleHeader>([self.collectionView
          supplementaryViewForElementKind:UICollectionElementKindSectionHeader
                              atIndexPath:indexPath]);
  // Note: At this point, `header` could be nil if not visible, or if the
  // supplementary view is not an InactiveTabsPreambleHeader.
  header.daysThreshold = self.inactiveTabsDaysThreshold;
}

@end
