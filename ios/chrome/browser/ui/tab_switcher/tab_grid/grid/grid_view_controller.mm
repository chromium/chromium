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
#import "ios/chrome/browser/shared/public/commands/thumb_strip_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_data_source.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_item.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
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
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/horizontal_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/plus_sign_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_ui_swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_preamble_header.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/modals/modals_api.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class ScrollingTimeLogger {
 public:
  ScrollingTimeLogger() : start_(base::TimeTicks::Now()) {}
  ~ScrollingTimeLogger() {
    base::TimeDelta duration = base::TimeTicks::Now() - start_;
    base::UmaHistogramTimes("IOS.TabSwitcher.TimeSpentScrolling", duration);
  }

 private:
  base::TimeTicks start_;
};

namespace {

// Constants used in the spring animation when inserting items in the thumb
// strip.
constexpr CGFloat kSpringAnimationDuration = 0.4;
constexpr CGFloat kSpringAnimationDamping = 0.6;
constexpr CGFloat kSpringAnimationInitialVelocity = 1.0;
constexpr int kOpenTabsSectionIndex = 0;
constexpr int kSuggestedActionsSectionIndex = 1;
constexpr base::TimeDelta kInactiveTabsHeaderAnimationDuration =
    base::Seconds(0.3);

NSString* const kCellIdentifier = @"GridCellIdentifier";
NSString* const kPlusSignCellIdentifier = @"PlusSignCellIdentifier";
NSString* const kSuggestedActionsCellIdentifier =
    @"SuggestedActionsCellIdentifier";
NSString* const kGridHeaderIdentifier = @"GridHeaderIdentifier";
NSString* const kInactiveTabsButtonHeaderIdentifier =
    @"InactiveTabsButtonHeaderIdentifier";
NSString* const kInactiveTabsPreambleHeaderIdentifier =
    @"InactiveTabsPreambleHeaderIdentifier";

// Creates an NSIndexPath with `index` in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
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
                                  UICollectionViewDataSource,
                                  UICollectionViewDelegate,
                                  UICollectionViewDelegateFlowLayout,
                                  UICollectionViewDragDelegate,
                                  UICollectionViewDropDelegate,
                                  UIPointerInteractionDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
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
// The layout for the thumb strip.
@property(nonatomic, strong) HorizontalLayout* horizontalLayout;
// By how much the user scrolled past the view's content size. A negative value
// means the user hasn't scrolled past the end of the scroll view.
@property(nonatomic, assign, readonly) CGFloat offsetPastEndOfScrollView;
// The view controller that holds the view of the suggested saerch actions.
@property(nonatomic, strong)
    SuggestedActionsViewController* suggestedActionsViewController;
// Cells for which pointer interactions have been added. Pointer interactions
// should only be added to displayed cells (not transition cells). This is only
// expected to get as large as the number of reusable cells in memory.
@property(nonatomic, strong)
    NSHashTable<UICollectionViewCell*>* pointerInteractionCells API_AVAILABLE(
        ios(13.4));
// The transition layout either from grid to horizontal layout or from
// horizontal to grid layout.
@property(nonatomic, strong)
    UICollectionViewTransitionLayout* gridHorizontalTransitionLayout;
// YES while `self.gridHorizontalTransitionLayout` is finishing (or cancelling)
// the transition. Is used to avoid cancelling again during enabling/disabling
// of the thumbstrip.
@property(nonatomic, assign) BOOL transitionLayoutIsFinishing;
// Tap gesture recognizer to dismiss the thumb strip.
@property(nonatomic, strong)
    UITapGestureRecognizer* thumbStripDismissRecognizer;
// Swipe up gesture recognizer to dismiss the thumb strip.
@property(nonatomic, strong)
    UISwipeGestureRecognizer* thumbStripSwipeUpDismissRecognizer;
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
@end

@implementation GridViewController {
  // Tracks when the grid view is scrolling. Create a new instance to start
  // timing and reset to stop and log the associated time histogram.
  std::unique_ptr<ScrollingTimeLogger> _scrollingTimeLogger;
}

@synthesize thumbStripEnabled = _thumbStripEnabled;

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
  self.horizontalLayout = [[HorizontalLayout alloc] init];
  self.gridLayout = [[GridLayout alloc] init];
  self.currentLayout = self.gridLayout;

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:self.currentLayout];
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  [collectionView registerClass:[PlusSignCell class]
      forCellWithReuseIdentifier:kPlusSignCellIdentifier];
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

  // During deletion (in horizontal layout) the backgroundView can resize,
  // revealing temporarily the collectionView background. This makes sure
  // both are the same color.
  collectionView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  // If this stays as the default `YES`, then cells aren't highlighted
  // immediately on touch, but after a short delay.
  collectionView.delaysContentTouches = NO;
  collectionView.dataSource = self;
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

  self.pointerInteractionCells =
      [NSHashTable<UICollectionViewCell*> weakObjectsHashTable];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self contentWillDisappear];
  [super viewWillDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateFractionVisibleOfLastItem];
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

  // Reloading specific sections in a `performBatchUpdates` fades the changes in
  // rather than reloads the collection view with a harsh flash.
  __weak GridViewController* weakSelf = self;
  [self.collectionView
      performBatchUpdates:^{
        GridViewController* strongSelf = weakSelf;
        if (!strongSelf || !strongSelf.collectionView) {
          return;
        }

        if (mode == TabGridModeSelection && previousMode == TabGridModeNormal) {
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
        if (previousMode != TabGridModeSelection && mode == TabGridModeNormal &&
            selectedIndex != NSNotFound) {
          // Scroll to the selected item here, so the animation of reloading and
          // scrolling happens at once.
          [strongSelf.collectionView
              scrollToItemAtIndexPath:CreateIndexPath(selectedIndex)
                     atScrollPosition:UICollectionViewScrollPositionTop
                             animated:NO];
        }
      }

               completion:nil];

  if (mode == TabGridModeNormal) {
    // Clear items when exiting selection mode.
    [self.selectedEditingItemIDs removeAllObjects];
    [self.selectedSharableEditingItemIDs removeAllObjects];

    // After transition from other modes to the normal mode, the selection
    // border doesn't show around the selected item, because reloading
    // operations like `reloadSections` loose the selected items. The
    // collection view needs to be updated with the selected item again for it
    // to appear correctly.
    [self deselectAllCollectionViewItemsAnimated:NO];
    [self selectCollectionViewItemWithID:self.selectedItemID
                                animated:NO
                          scrollPosition:UICollectionViewScrollPositionNone];
    [self updateFractionVisibleOfLastItem];

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

- (GridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<GridTransitionItem*>* items = [[NSMutableArray alloc] init];
  GridTransitionActiveItem* activeItem;
  GridTransitionItem* selectionItem;
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
      activeItem = [GridTransitionActiveItem itemWithCell:activeCell
                                                   center:attributes.center
                                                     size:attributes.size];
      // If the active item is the last inserted item, it needs to be animated
      // differently.
      if ([cell hasIdentifier:self.lastInsertedItemID])
        activeItem.isAppearing = YES;
      selectionItem = [GridTransitionItem
          itemWithCell:[GridCell transitionSelectionCellFromCell:cell]
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

- (void)prepareForAppearance {
  for (TabSwitcherItem* item in [self visibleGridItems]) {
    [item prefetchSnapshot];
  }
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  self.currentLayout.animatesItemUpdates = YES;
  [self.collectionView reloadData];
  // Selection is invalid if there are no items.
  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
    return;
  }

  [self deselectAllCollectionViewItemsAnimated:NO];
  [self selectCollectionViewItemWithID:self.selectedItemID animated:NO];

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

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  if (self.showingSuggestedActions) {
    return kSuggestedActionsSectionIndex + 1;
  }
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  if (section == kSuggestedActionsSectionIndex) {
    // In the search mode there there is only one item in the suggested actions
    // section which contains the table for the suggested actions.
    if (self.showingSuggestedActions)
      return 1;
    return 0;
  }
  if (self.thumbStripEnabled) {
    // The PlusSignCell (new item button) is always appended at the end of the
    // collection.
    return base::checked_cast<NSInteger>(self.items.count + 1);
  }
  return base::checked_cast<NSInteger>(self.items.count);
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
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
      header.parent = self;
      __weak __typeof(self) weakSelf = self;
      header.buttonAction = ^{
        [weakSelf didTapInactiveTabsButton];
      };
      [header configureWithDaysThreshold:self.inactiveTabsDaysThreshold];
      if (IsShowInactiveTabsCountEnabled()) {
        [header configureWithCount:self.inactiveTabsCount];
      }
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
          NSString* resultsCount = [NSString
              stringWithFormat:@"%ld",
                               base::checked_cast<NSInteger>(self.items.count)];
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
      __weak __typeof(self) weakSelf = self;
      header.settingsLinkAction = ^{
        [weakSelf didTapInactiveTabsSettingsLink];
      };
      header.daysThreshold = self.inactiveTabsDaysThreshold;
      return header;
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
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
    if ([self isIndexPathForPlusSignCell:indexPath]) {
      cell = [collectionView
          dequeueReusableCellWithReuseIdentifier:kPlusSignCellIdentifier
                                    forIndexPath:indexPath];
      PlusSignCell* plusSignCell =
          base::mac::ObjCCastStrict<PlusSignCell>(cell);
      plusSignCell.theme = self.theme;
    } else {
      // In some cases this is called with an indexPath.item that's beyond (by
      // 1) the bounds of self.items -- see crbug.com/1068136. Presumably this
      // is a race condition where an item has been deleted at the same time as
      // the collection is doing layout (potentially during rotation?). DCHECK
      // to catch this in debug, and then in production fudge by duplicating the
      // last cell. The assumption is that there will be another, correct layout
      // shortly after the incorrect one.
      DCHECK_LT(itemIndex, self.items.count);
      // Outside of debug builds, keep array bounds valid.
      if (itemIndex >= self.items.count)
        itemIndex = self.items.count - 1;

      TabSwitcherItem* item = self.items[itemIndex];
      cell =
          [collectionView dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                                    forIndexPath:indexPath];
      cell.accessibilityIdentifier = [NSString
          stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, itemIndex];
      GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(cell);
      [self configureCell:gridCell withItem:item];
    }
  }
  // Set the z index of cells so that lower rows are moving behind the upper
  // rows during transitions between grid and horizontal layouts.
  cell.layer.zPosition = self.items.count - itemIndex;

  if (![self.pointerInteractionCells containsObject:cell]) {
    [cell addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
    // `self.pointerInteractionCells` is only expected to get as large as
    // the number of reusable cells in memory.
    [self.pointerInteractionCells addObject:cell];
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  // `collectionViewLayout` should always be a flow layout.
  DCHECK(
      [collectionViewLayout isKindOfClass:[UICollectionViewFlowLayout class]]);
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
  if ([self isIndexPathForPlusSignCell:proposedIndexPath]) {
    return CreateIndexPath(proposedIndexPath.item - 1);
  }
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

  // No context menu on plus sign cell.
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    return nil;
  }

  GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);

  MenuScenarioHistogram scenario;
  if (_mode == TabGridModeSearch) {
    scenario = MenuScenarioHistogram::kTabGridSearchResult;
  } else if (_mode == TabGridModeInactive) {
    scenario = MenuScenarioHistogram::kInactiveTabsEntry;
  } else if (self.currentLayout == self.horizontalLayout) {
    scenario = MenuScenarioHistogram::kThumbStrip;
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
    didEndDisplayingCell:(UICollectionViewCell*)cell
      forItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[GridCell class]]) {
    // Stop animation of GridCells when removing them from the collection view.
    // This is important to prevent cells from animating indefinitely. This is
    // safe because the animation state of GridCells is set in
    // `configureCell:withItem:` whenever a cell is used.
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
  if (self.thumbStripEnabled && self.items.count <= 1) {
    // If only one item, don't drag it or this will leave the BVC or the grid
    // empty.
    // TODO(crbug.com/1334474) try to fix better by opening an NTP.
    return @[];
  }
  if (_mode == TabGridModeSearch) {
    // TODO(crbug.com/1300369): Enable dragging items from search results.
    return @[];
  }
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    // Return an empty array because the plus sign cell should not be dragged.
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
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    // Return nil so that the plus sign cell doesn't superpose the dragged cell.
    return nil;
  }
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
  if (IsTabGridSortedByRecency() && self.localDragActionInProgress) {
    // Don't support dropping local items when sorting by recency.
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

    if (self.thumbStripEnabled) {
      // The sourceIndexPath is nil if the drop item is not from the same
      // collection view.
      NSUInteger plusSignCellIndex =
          item.sourceIndexPath ? self.items.count : self.items.count + 1;
      // Can't use [self isIndexPathForPlusSignCell:] here because the index of
      // the plus sign cell in this point in code depends on
      // `item.sourceIndexPath`.
      // I.e., in this point in code, `collectionView.numberOfItemsInSection` is
      // equal to `self.items.count + 1`.
      if (destinationIndex == plusSignCellIndex) {
        // Prevent the cell from being dropped where the plus sign cell is.
        destinationIndex = plusSignCellIndex - 1;
      }
    }
    NSIndexPath* dropIndexPath = CreateIndexPath(destinationIndex);
    // Drop synchronously if local object is available.
    if (item.dragItem.localObject) {
      __weak __typeof(self) weakSelf = self;
      _dropAnimationInProgress = YES;
      [self.delegate gridViewControllerDropAnimationWillBegin:weakSelf];
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
  if (!self.thumbStripEnabled) {
    return;
  }
  [self updateFractionVisibleOfLastItem];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.delegate gridViewControllerWillBeginDragging:self];
  base::RecordAction(base::UserMetricsAction("MobileTabGridUserScrolled"));
  _scrollingTimeLogger = std::make_unique<ScrollingTimeLogger>();
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  if (!decelerate) {
    _scrollingTimeLogger = nullptr;
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  _scrollingTimeLogger = nullptr;
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

  [self deselectAllCollectionViewItemsAnimated:NO];
  [self selectCollectionViewItemWithID:self.selectedItemID animated:NO];

  if ([self shouldShowEmptyState]) {
    [self animateEmptyStateIn];
  } else {
    [self removeEmptyStateAnimated:YES];
  }
  // Whether the view is visible or not, the delegate must be updated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self updateFractionVisibleOfLastItem];
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

  // Consistency check: `item`'s ID is not in `items`.
  // (using DCHECK rather than DCHECK_EQ to avoid a checked_cast on NSNotFound).
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);
  auto modelUpdates = ^{
    [self.items insertObject:item atIndex:index];
    self.selectedItemID = selectedItemID;
    self.lastInsertedItemID = item.identifier;
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    [self updateVisibleCellsOpacity];
  };
  auto collectionViewUpdates = ^{
    [self removeEmptyStateAnimated:YES];
    [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  };

  auto completion = ^(BOOL finished) {
    [self deselectAllCollectionViewItemsAnimated:NO];
    [self selectCollectionViewItemWithID:self.selectedItemID
                                animated:!self.thumbStripEnabled];

    [self.delegate gridViewController:self didChangeItemCount:self.items.count];

    // Check `index` boundaries in order to filter out possible race
    // conditions while mutating the collection.
    if (index == NSNotFound || index >= self.items.count) {
      return;
    }

    [self.collectionView
        scrollToItemAtIndexPath:CreateIndexPath(index)
               atScrollPosition:UICollectionViewScrollPositionCenteredVertically
                       animated:YES];
  };

  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
                   useSpringAnimation:self.currentLayout ==
                                      self.horizontalLayout
      collectionViewUpdatesCompletion:completion];

  [self updateVisibleCellZIndex];
  [self updateVisibleCellIdentifiers];
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  NSUInteger index = [self indexOfItemWithID:removedItemID];

  // Do not remove if not showing the item (i.e. showing search results).
  if (index == NSNotFound) {
    [self selectItemWithID:selectedItemID];
    return;
  }

  auto modelUpdates = ^{
    [self.items removeObjectAtIndex:index];
    self.selectedItemID = selectedItemID;
    [self deselectItemWithIDForEditing:removedItemID];
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    [self updateVisibleCellsOpacity];
  };
  auto collectionViewUpdates = ^{
    [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
    if ([self shouldShowEmptyState]) {
      [self animateEmptyStateIn];
    }
  };

  __weak __typeof(self) weakSelf = self;
  auto completion = ^(BOOL finished) {
    if (weakSelf.items.count > 0) {
      [self deselectAllCollectionViewItemsAnimated:NO];
      [self selectCollectionViewItemWithID:weakSelf.selectedItemID
                                  animated:NO
                            scrollPosition:UICollectionViewScrollPositionNone];
    }
    [weakSelf.delegate gridViewController:weakSelf
                       didChangeItemCount:weakSelf.items.count];
    [weakSelf.delegate gridViewController:weakSelf
                      didRemoveItemWIthID:removedItemID];
    [weakSelf updateFractionVisibleOfLastItem];
  };

  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
                   useSpringAnimation:NO
      collectionViewUpdatesCompletion:completion];

  [self updateVisibleCellZIndex];
  [self updateVisibleCellIdentifiers];

  if (_searchText.length)
    [self updateSearchResultsHeader];
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  if ([self.selectedItemID isEqualToString:selectedItemID])
    return;

  [self deselectAllCollectionViewItemsAnimated:NO];

  self.selectedItemID = selectedItemID;
  [self selectCollectionViewItemWithID:self.selectedItemID
                              animated:NO
                        scrollPosition:UICollectionViewScrollPositionNone];
  [self updateVisibleCellsOpacity];
}

- (void)replaceItemID:(NSString*)itemID withItem:(TabSwitcherItem*)item {
  if ([self indexOfItemWithID:itemID] == NSNotFound)
    return;
  // Consistency check: `item`'s ID is either `itemID` or not in `items`.
  DCHECK([item.identifier isEqualToString:itemID] ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);
  NSUInteger index = [self indexOfItemWithID:itemID];
  self.items[index] = item;
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)]);
  // `cell` may be nil if it is scrolled offscreen.
  if (cell)
    [self configureCell:cell withItem:item];
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
  auto modelUpdates = ^{
    TabSwitcherItem* item = self.items[fromIndex];
    [self.items removeObjectAtIndex:fromIndex];
    [self.items insertObject:item atIndex:toIndex];
  };
  auto collectionViewUpdates = ^{
    [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                                 toIndexPath:CreateIndexPath(toIndex)];
  };

  __weak __typeof(self) weakSelf = self;
  auto completion = ^(BOOL finished) {
    if (!weakSelf) {
      return;
    }

    [weakSelf.delegate gridViewController:weakSelf
                        didMoveItemWithID:itemID
                                  toIndex:toIndex];

    // Bring back selected halo only for the moved cell, which lost it during
    // the move (drag & drop).
    if (weakSelf.selectedIndex != toIndex) {
      return;
    }
    // Force reload of the selected cell now to avoid extra delay for the
    // blue halo to appear. Bring the halo in 100ms.
    [UIView animateWithDuration:0.1
                     animations:^{
                       [weakSelf.collectionView reloadItemsAtIndexPaths:@[
                         CreateIndexPath(weakSelf.selectedIndex)
                       ]];
                       [self deselectAllCollectionViewItemsAnimated:NO];
                       [self
                           selectCollectionViewItemWithID:weakSelf
                                                              .selectedItemID
                                                 animated:NO
                                           scrollPosition:
                                               UICollectionViewScrollPositionNone];
                     }
                     completion:nil];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
                   useSpringAnimation:NO
      collectionViewUpdatesCompletion:completion];

  [self updateVisibleCellZIndex];
  [self updateVisibleCellIdentifiers];
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

#pragma mark - LayoutSwitcher

- (LayoutSwitcherState)currentLayoutSwitcherState {
  return self.currentLayout == self.horizontalLayout
             ? LayoutSwitcherState::Horizontal
             : LayoutSwitcherState::Grid;
}

- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion {
  FlowLayout* nextLayout;
  BOOL thumbStripDismissEnabled;
  switch (nextState) {
    case LayoutSwitcherState::Horizontal:
      nextLayout = self.horizontalLayout;
      thumbStripDismissEnabled = YES;
      break;
    case LayoutSwitcherState::Grid:
      nextLayout = self.gridLayout;
      thumbStripDismissEnabled = NO;
      break;
  }

  // If the dismiss recognizer needs to be disabled, do it now so the user won't
  // trigger it during the transformation.
  if (!thumbStripDismissEnabled) {
    self.thumbStripDismissRecognizer.enabled = NO;
    self.thumbStripSwipeUpDismissRecognizer.enabled = NO;
  }

  __weak __typeof(self) weakSelf = self;
  auto completionBlock = ^(BOOL completed, BOOL finished) {
    weakSelf.collectionView.scrollEnabled = YES;
    if (finished) {
      weakSelf.currentLayout = nextLayout;
    }
    if (thumbStripDismissEnabled) {
      weakSelf.thumbStripDismissRecognizer.enabled = YES;
      weakSelf.thumbStripSwipeUpDismissRecognizer.enabled = YES;
    }
    weakSelf.transitionLayoutIsFinishing = NO;
    weakSelf.gridHorizontalTransitionLayout = nil;
    if (completion) {
      completion(completed, finished);
    }
  };
  self.gridHorizontalTransitionLayout = [self.collectionView
      startInteractiveTransitionToCollectionViewLayout:nextLayout
                                            completion:completionBlock];

  // Stops collectionView scrolling when the animation starts.
  [self.collectionView setContentOffset:self.collectionView.contentOffset
                               animated:NO];
}

- (void)didUpdateTransitionLayoutProgress:(CGFloat)progress {
  self.gridHorizontalTransitionLayout.transitionProgress = progress;
}

- (void)didTransitionToLayoutSuccessfully:(BOOL)success {
  // If there is no item, the transition layout might be nil, which means there
  // is no interactive transition. Return to avoid crash below.
  if (!self.gridHorizontalTransitionLayout)
    return;

  self.transitionLayoutIsFinishing = YES;
  if (success) {
    [self.collectionView finishInteractiveTransition];
  } else {
    [self.collectionView cancelInteractiveTransition];
  }
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

- (void)setFractionVisibleOfLastItem:(CGFloat)fractionVisibleOfLastItem {
  if (fractionVisibleOfLastItem == _fractionVisibleOfLastItem)
    return;
  _fractionVisibleOfLastItem = fractionVisibleOfLastItem;

  // TODO(crbug.com/1146130): Behaviour of the tab grid's bottom toolbar when
  // the plus sign cell is visible hasn't been decided yet.
  [self.delegate didChangeLastItemVisibilityInGridViewController:self];
}

- (void)setCurrentLayout:(FlowLayout*)currentLayout {
  _currentLayout = currentLayout;
  [self updateFractionVisibleOfLastItem];

  // The collection view should only always bounce horizonal when in horizontal
  // layout.
  self.collectionView.alwaysBounceHorizontal =
      self.currentLayout == self.horizontalLayout;
}

#pragma mark - Private

// Selects the collection view's item with `itemID`.
- (void)selectCollectionViewItemWithID:(NSString*)itemID
                              animated:(BOOL)animated
                        scrollPosition:
                            (UICollectionViewScrollPosition)scrollPosition {
  NSUInteger itemIndex = [self indexOfItemWithID:itemID];

  // Check `itemIndex` boundaries in order to filter out possible race
  // conditions while mutating the collection.
  if (itemIndex == NSNotFound || itemIndex >= self.items.count) {
    return;
  }

  NSIndexPath* itemIndexPath = CreateIndexPath(itemIndex);

  [self.collectionView selectItemAtIndexPath:itemIndexPath
                                    animated:animated
                              scrollPosition:scrollPosition];
}

// Selects the collection view's item with `itemID`.
- (void)selectCollectionViewItemWithID:(NSString*)itemID
                              animated:(BOOL)animated {
  UICollectionViewScrollPosition scrollPosition =
      (self.currentLayout == self.horizontalLayout)
          ? UICollectionViewScrollPositionCenteredHorizontally
          : UICollectionViewScrollPositionTop;

  [self selectCollectionViewItemWithID:itemID
                              animated:animated
                        scrollPosition:scrollPosition];
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

// Checks whether `indexPath` corresponds to the index path of the plus sign
// cell. The plus sign cell is the last cell in the collection view after all
// the items.
- (BOOL)isIndexPathForPlusSignCell:(NSIndexPath*)indexPath {
  // When items are dragged from another collection, the count of cells in the
  // collectionView is increased before self.items.count increases. That's what
  // happens when the UICollectionViewDelegate's method
  // `targetIndexPathForMoveFromItemAtIndexPath:toProposedIndexPath:` gets
  // called, and that's why indexPath.item is not being compared to
  // self.items.count here.
  return self.thumbStripEnabled &&
         indexPath.item == [self.collectionView numberOfItemsInSection:0] - 1;
}

// Performs model updates and view updates together.
- (void)performModelUpdates:(ProceduralBlock)modelUpdates
              collectionViewUpdates:(ProceduralBlock)collectionViewUpdates
                 useSpringAnimation:(BOOL)useSpringAnimation
    collectionViewUpdatesCompletion:
        (void (^)(BOOL))collectionViewUpdatesCompletion {
  auto batchUpdates = ^(void) {
    [self.collectionView
        performBatchUpdates:^{
          self.updating = YES;
          // Synchronize model and view updates.
          modelUpdates();
          collectionViewUpdates();
        }
        completion:^(BOOL completed) {
          collectionViewUpdatesCompletion(completed);
          self.updating = NO;
        }];
  };

  if (useSpringAnimation) {
    [UIView animateWithDuration:kSpringAnimationDuration
                          delay:0
         usingSpringWithDamping:kSpringAnimationDamping
          initialSpringVelocity:kSpringAnimationInitialVelocity
                        options:0
                     animations:batchUpdates
                     completion:nil];
  } else {
    batchUpdates();
  }
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
- (void)configureCell:(GridCell*)cell withItem:(TabSwitcherItem*)item {
  DCHECK(cell);
  DCHECK(item);
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
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

  __weak __typeof(self) weakSelf = self;
  [item fetchSnapshot:^(TabSwitcherItem* innerItem, UIImage* snapshot) {
    // Only update the icon if the cell is not already reused for another item.
    if ([cell hasIdentifier:innerItem.identifier]) {
      if (weakSelf.thumbStripEnabled) {
        [cell fadeInSnapshot:snapshot];
      } else {
        cell.snapshot = snapshot;
      }
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
  if (self.thumbStripEnabled &&
      ![itemIdentifier isEqualToString:self.selectedItemID]) {
    cell.opacity = self.notSelectedTabCellOpacity;
  } else {
    cell.opacity = 1.0f;
  }
  if (item.showsActivity) {
    [cell showActivityIndicator];
  } else {
    [cell hideActivityIndicator];
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

  if ([self isIndexPathForPlusSignCell:indexPath]) {
    [self.delegate didTapPlusSignInGridViewController:self];
    return;
  }

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
    [UIView performWithoutAnimation:^{
      [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
    }];
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

// Updates the value stored in `fractionVisibleOfLastItem`.
- (void)updateFractionVisibleOfLastItem {
  CGFloat offset = self.offsetPastEndOfScrollView;
  self.fractionVisibleOfLastItem = std::clamp<CGFloat>(
      1 - offset / kScrollThresholdForPlusSignButtonHide, 0, 1);
}

// Updates the ZIndex of the visible cells to have the cells of the upper rows
// be above the cell of the lower rows.
- (void)updateVisibleCellZIndex {
  for (NSIndexPath* indexPath in self.collectionView
           .indexPathsForVisibleItems) {
    // Set the z index of cells so that lower rows are moving behind the upper
    // rows during transitions between grid and horizontal layouts.
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    cell.layer.zPosition = self.items.count - indexPath.item;
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
    cell.accessibilityIdentifier = [NSString
        stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, itemIndex];
  }
}

// Setter for the not selected tab opacity. This can be used inside an
// animation block.
- (void)setNotSelectedTabCellOpacity:(CGFloat)opacity {
  _notSelectedTabCellOpacity = opacity;
  [self updateVisibleCellsOpacity];
}

// Update visible cells opacity. When thumbstrip is not enabled, all are 1.0.
// Otherwise not selected tab are `self.notSelectedTabCellOpacity`.
- (void)updateVisibleCellsOpacity {
  if (!self.thumbStripEnabled) {
    return;
  }
  for (NSIndexPath* indexPath in self.collectionView
           .indexPathsForVisibleItems) {
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    if (![cell isKindOfClass:[GridCell class]])
      continue;
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(cell);
    if (![gridCell hasIdentifier:self.selectedItemID]) {
      gridCell.opacity = self.notSelectedTabCellOpacity;
    } else {
      gridCell.opacity = 1.0f;
    }
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
  NSIndexSet* targetSections =
      [NSIndexSet indexSetWithIndex:kOpenTabsSectionIndex];
  [UIView performWithoutAnimation:^{
    // There is a collection view bug (crbug.com/1300733) that prevents
    // CollectionView's `reloadData` from working properly if its preceded by
    // CollectionView's `performBatchUpdates:` in the same UI cycle. To avoid
    // this bug, `reloadSections:` method is used instead to reload the items in
    // the tab grid.
    [self.collectionView reloadSections:targetSections];
  }];
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
  NSString* resultsCount = [NSString
      stringWithFormat:@"%ld", base::checked_cast<NSInteger>(self.items.count)];
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
  NSIndexSet* openTabsSection =
      [NSIndexSet indexSetWithIndex:kOpenTabsSectionIndex];
  // Prevent the animation, as it leads to a jarrying effect when closing all
  // inactive tabs: the inactive tabs view controller gets popped, and the
  // underlying regular Tab Grid moves tabs up.
  // Note: this could be revisited when supporting iPad, as the user could
  // have closed all inactive tabs in a different window.
  [UIView performWithoutAnimation:^{
    [self.collectionView reloadSections:openTabsSection];
  }];
  // Make sure to restore the selection. `reloadSections` cleared it.
  // https://developer.apple.com/forums/thread/656529
  [self selectCollectionViewItemWithID:self.selectedItemID
                              animated:NO
                        scrollPosition:UICollectionViewScrollPositionNone];
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
  [header configureWithDaysThreshold:self.inactiveTabsDaysThreshold];
  if (IsShowInactiveTabsCountEnabled()) {
    [header configureWithCount:self.inactiveTabsCount];
  }
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

#pragma mark Suggested Actions Section

- (void)updateSuggestedActionsSection {
  if (!self.suggestedActionsDelegate)
    return;
  // In search mode if there is already a search query, and the suggested
  // actions section section is not yet added, add it. otherwise remove the
  // section if it exists and the search mode is not active.
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
    [self.collectionView performBatchUpdates:updateSectionBlock completion:nil];
  }];
}

#pragma mark - Public Editing Mode Selection

- (void)selectAllItemsForEditing {
  if (_mode != TabGridModeSelection) {
    return;
  }

  for (TabSwitcherItem* item in self.items) {
    [self selectItemWithIDForEditing:item.identifier];
  }
  [self.collectionView reloadData];
}

- (void)deselectAllItemsForEditing {
  if (_mode != TabGridModeSelection) {
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

#pragma mark - ThumbStripSupporting

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  // Make sure the collection view isn't in the middle of a transition.
  if (self.gridHorizontalTransitionLayout) {
    if (!self.transitionLayoutIsFinishing) {
      [self didTransitionToLayoutSuccessfully:NO];
    }
    __weak GridViewController* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf thumbStripEnabledWithPanHandler:panHandler];
    });
    return;
  }

  DCHECK(!self.thumbStripEnabled);

  UICollectionView* collectionView = self.collectionView;

  // Install tap gesture recognizer in collection to handle user taps on the
  // thumb strip background, assuming it hasn't already been installed.

  if (!self.thumbStripDismissRecognizer) {
    self.thumbStripDismissRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleThumbStripBackgroundTapGesture:)];
    DCHECK(collectionView.backgroundView);
    [collectionView.backgroundView
        addGestureRecognizer:self.thumbStripDismissRecognizer];
  }
  if (!self.thumbStripSwipeUpDismissRecognizer) {
    self.thumbStripSwipeUpDismissRecognizer = [[UISwipeGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleThumbStripBackgroundSwipeUpGesture:)];
    self.thumbStripSwipeUpDismissRecognizer.direction =
        UISwipeGestureRecognizerDirectionUp;
    [collectionView.backgroundView
        addGestureRecognizer:self.thumbStripSwipeUpDismissRecognizer];
  }

  if (panHandler.currentState == ViewRevealState::Revealed) {
    self.thumbStripDismissRecognizer.enabled = NO;
    self.thumbStripSwipeUpDismissRecognizer.enabled = NO;
    collectionView.collectionViewLayout = self.gridLayout;
    self.currentLayout = self.gridLayout;
  } else {
    self.thumbStripDismissRecognizer.enabled = YES;
    self.thumbStripSwipeUpDismissRecognizer.enabled = YES;
    collectionView.collectionViewLayout = self.horizontalLayout;
    self.currentLayout = self.horizontalLayout;
  }

  [collectionView.collectionViewLayout invalidateLayout];
  [collectionView reloadData];
  _thumbStripEnabled = YES;
}

- (void)thumbStripDisabled {
  // Make sure the collection view isn't in the middle of a transition.
  if (self.gridHorizontalTransitionLayout) {
    if (!self.transitionLayoutIsFinishing) {
      [self didTransitionToLayoutSuccessfully:NO];
    }
    __weak GridViewController* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf thumbStripDisabled];
    });
    return;
  }

  DCHECK(self.thumbStripEnabled);

  UICollectionView* collectionView = self.collectionView;
  FlowLayout* gridLayout = self.gridLayout;

  [collectionView.backgroundView
      removeGestureRecognizer:self.thumbStripDismissRecognizer];
  [collectionView.backgroundView
      removeGestureRecognizer:self.thumbStripSwipeUpDismissRecognizer];
  self.thumbStripDismissRecognizer = nil;
  self.thumbStripSwipeUpDismissRecognizer = nil;

  collectionView.collectionViewLayout = gridLayout;
  self.currentLayout = gridLayout;

  [collectionView.collectionViewLayout invalidateLayout];
  [collectionView reloadData];

  _thumbStripEnabled = NO;
}

#pragma mark - Thumbstrip tap dismiss handling

- (void)handleThumbStripBackgroundTapGesture:(UIGestureRecognizer*)recognizer {
  if (recognizer.state != UIGestureRecognizerStateEnded)
    return;

  UICollectionView* collectionView = self.collectionView;
  CGPoint tapLocation = [recognizer locationInView:collectionView];

  // Check that the tap wasn't to the leading side of the trailing edge of
  // the last cell. This should exclude any cells in the collection view as
  // well as the gaps between them.
  NSIndexPath* lastCellIndex =
      CreateIndexPath([collectionView numberOfItemsInSection:0] - 1);
  UICollectionViewCell* lastCell =
      [collectionView cellForItemAtIndexPath:lastCellIndex];
  if (lastCell) {
    CGRect lastCellFrame = [recognizer.view convertRect:lastCell.bounds
                                               fromView:lastCell];
    if (EdgeLeadsEdge(tapLocation.x, CGRectGetTrailingEdge(lastCellFrame)))
      return;
  }

  // Handle the tap.
  [self.thumbStripHandler
      closeThumbStripWithTrigger:ViewRevealTrigger::BackgroundTap];
}

- (void)handleThumbStripBackgroundSwipeUpGesture:
    (UIGestureRecognizer*)recognizer {
  [self.thumbStripHandler
      closeThumbStripWithTrigger:ViewRevealTrigger::BackgroundSwipe];
}

@end
